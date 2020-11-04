#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/kthread.h>
#include <linux/sched.h>

#include <linux/kernel.h>

#include <linux/netdevice.h>
#include <linux/skbuff.h>
#include <linux/udp.h>
#include <linux/etherdevice.h>
#include <linux/ip.h>
#include <net/ip.h>
#include <linux/slab.h>
#include <linux/inetdevice.h>

#include <linux/hrtimer.h>
#include <linux/ktime.h>
#include <linux/time.h>

#include <asgard/logger.h>
#include <asgard/asgard.h>
#include <asgard/payload_helper.h>
#include <asgard/consensus.h>
#include <asgard/asgard_async.h>
#include <asgard/asgard_uface.h>
#include <asgard/asgard_echo.h>

#undef LOG_PREFIX
#define LOG_PREFIX "[ASGARD][PACEMAKER]"

static struct task_struct *heartbeat_task;

static inline bool asgard_pacemaker_is_alive(struct pminfo *spminfo)
{
	return spminfo->state == ASGARD_PM_EMITTING;
}

static inline bool scheduled_tx(uint64_t prev_time, uint64_t cur_time, uint64_t interval)
{
	return (cur_time - prev_time) >= interval;
}

static inline bool check_async_window(uint64_t prev_time, uint64_t cur_time, uint64_t interval, uint64_t sync_window)
{
	return (cur_time - prev_time) <= (interval - sync_window);
}

static inline bool check_async_door(struct asgard_device *sdev)
{
	int i;

    if(sdev->multicast.enable)
        return sdev->multicast.aapriv->doorbell;

	for (i = 0; i < sdev->pminfo.num_of_targets; i++){

	    if(sdev->pminfo.pm_targets[i].aapriv->doorbell)
            return 1;

	}

	return 0;
}

static inline bool out_of_schedule_multi_tx(struct asgard_device *sdev)
{

    struct echo_priv *epriv = (struct echo_priv*) sdev->echo_priv;

    if(sdev->hold_fire)
        return 0;


    /* ---------  ONLY FOR ECHO TEST  --------- */

    if(epriv == NULL)
        return 0;


    if(sdev->pminfo.multicast_pkt_data_oos_fire == 0)
        return 0;


    /* Only applicable if this multicast is a ping */
    if(epriv->fire_ping){
        epriv->fire_ping = 0;
        return 1;
    }

    /* Timestamp is set to 0 again after pong is emitted */
    if(epriv->last_echo_ts == 0){
        asgard_dbg("Last echo ts is 0\n");
        return 0;

    }

    if(epriv->pong_waiting_interval >= RDTSC_ASGARD - epriv->last_echo_ts){
        // asgard_dbg("Blocking emission\n");
        return 0;
    }

    epriv->last_echo_ts = 0;
    /* ---------  ONLY FOR ECHO TEST  --------- */


    /* ONLY FOR ECHO TEST COMMENT: return will be evaluated to true if we got here,
     * leave this in case we remove the echo test again */
    return sdev->pminfo.multicast_pkt_data_oos_fire != 0;
}


static inline bool out_of_schedule_tx(struct asgard_device *sdev)
{
	int i;

	if(sdev->hold_fire)
		return 0;

	for (i = 0; i < sdev->pminfo.num_of_targets; i++){

	    if( sdev->pminfo.pm_targets[i].fire)
	        return 1;
	}

	return 0;
}

const char *pm_state_string(enum pmstate state)
{
	switch (state) {
	case ASGARD_PM_UNINIT:
		return "ASGARD_PM_UNINIT";
	case ASGARD_PM_READY:
		return "ASGARD_PM_READY";
	case ASGARD_PM_EMITTING:
		return "ASGARD_PM_EMITTING";
    case ASGARD_PM_FAILED:
        return "ASGARD_PM_FAILED";
	default:
		return "UNKNOWN STATE";
	}
}



void pm_state_transition_to(struct pminfo *spminfo,
			    const enum pmstate state)
{
#if VERBOSE_DEBUG
	asgard_dbg("State Transition from %s to %s\n",
		  pm_state_string(spminfo->state), pm_state_string(state));
#endif
	spminfo->state = state;
}

static inline int asgard_setup_hb_skbs(struct asgard_device *sdev)
{
	int i;
	struct pminfo *spminfo = &sdev->pminfo;
	struct node_addr *naddr;

    asgard_dbg("setup hb skbs. \n");

    if(!spminfo){
        asgard_error("spminfo is NULL \n");
        BUG();
        return -1;
    }

	// BUG_ON(spminfo->num_of_targets > MAX_REMOTE_SOURCES);

    /* Setup Multicast SKB */
    if(!sdev->multicast_mac) {
        asgard_error("Multicast MAC is NULL");
        return -1;
    }

    asgard_dbg("broadcast ip: %x  mac: %pM", sdev->multicast_ip, sdev->multicast_mac);

    spminfo->multicast_pkt_data_oos.skb = asgard_reserve_skb(sdev->ndev, sdev->multicast_ip, sdev->multicast_mac, NULL);
    skb_set_queue_mapping(spminfo->multicast_pkt_data_oos.skb, smp_processor_id()); // Queue mapping same for each target i
    spminfo->multicast_pkt_data_oos.port = 3321; /* TODO */

    spminfo->multicast_skb = asgard_reserve_skb(sdev->ndev, sdev->multicast_ip, sdev->multicast_mac, NULL);
    skb_set_queue_mapping(spminfo->multicast_skb, smp_processor_id()); // Queue mapping same for each target i

    return 0;
}

int asg_xmit_skb(const struct net_device *ndev, const struct netdev_queue *txq, const struct sk_buff *skb) {
    int ret = 0;

    skb_get(skb);

    ret = netdev_start_xmit(skb, ndev, txq, 0);

    switch (ret) {
        case NETDEV_TX_OK:
            break;
        case NET_XMIT_DROP:
            asgard_error("NETDEV TX DROP\n");
            break;
        case NET_XMIT_CN:
            asgard_error("NETDEV XMIT CN\n");
            break;
        default:
            asgard_error("NETDEV UNKNOWN \n");
            /* fall through */
        case NETDEV_TX_BUSY:
            asgard_error("NETDEV TX BUSY\n");
            break;
    }
    return ret;
}

static inline void asgard_send_multicast_hb(struct net_device *ndev, struct pminfo *spminfo)
{
    struct netdev_queue *txq;
    struct sk_buff *skb;
    unsigned long flags;
    int tx_index = smp_processor_id();

    if (unlikely(!netif_running(ndev) ||
                 !netif_carrier_ok(ndev))) {
        asgard_error("Network device offline!\n exiting pacemaker\n");
        spminfo->errors++;
        return;
    }
    spminfo->errors = 0;

    local_irq_save(flags);
    local_bh_disable();

    /* The queue mapping is the same for each target <i>
     * Since we pinned the pacemaker to a single cpu,
     * we can use the smp_processor_id() directly.
     */
    txq = &ndev->_tx[tx_index];

    HARD_TX_LOCK(ndev, txq, tx_index);

    if (unlikely(netif_xmit_frozen_or_drv_stopped(txq))) {
        //asgard_error("Device Busy unlocking.\n");
        goto unlock;
    }

    skb = spminfo->multicast_skb;

    asg_xmit_skb(ndev, txq, skb);

unlock:
    HARD_TX_UNLOCK(ndev, txq);

    local_bh_enable();
    local_irq_restore(flags);

}



static inline void asgard_send_oos_multi_pkts(struct net_device *ndev, struct pminfo *spminfo)
{
    struct netdev_queue *txq;
    struct sk_buff *skb;
    unsigned long flags;
    int tx_index = smp_processor_id();

    if (unlikely(!netif_running(ndev) ||
                 !netif_carrier_ok(ndev))) {
        asgard_error("Network device offline!\n exiting pacemaker\n");
        spminfo->errors++;
        return;
    }
    spminfo->errors = 0;

    local_irq_save(flags);
    local_bh_disable();

    txq = &ndev->_tx[tx_index];

    HARD_TX_LOCK(ndev, txq, tx_index);

    if (unlikely(netif_xmit_frozen_or_drv_stopped(txq)))
        goto unlock;

    skb = spminfo->multicast_pkt_data_oos.skb;

    asg_xmit_skb(ndev, txq, skb);

unlock:
    HARD_TX_UNLOCK(ndev, txq);
    local_bh_enable();
    local_irq_restore(flags);
}



static inline void asgard_send_oos_pkts(struct net_device *ndev, struct pminfo *spminfo,  int *target_fire)
{
	struct netdev_queue *txq;
	struct sk_buff *skb;
	unsigned long flags;
	int tx_index = smp_processor_id();
	int i;

	if (unlikely(!netif_running(ndev) ||
			!netif_carrier_ok(ndev))) {
		asgard_error("Network device offline!\n exiting pacemaker\n");
		spminfo->errors++;
		return;
	}
    spminfo->errors = 0;
	if( spminfo->num_of_targets == 0) {
		asgard_dbg("No targets for pacemaker. \n");
		return;
	}

	local_irq_save(flags);
	local_bh_disable();

	/* The queue mapping is the same for each target <i>
	 * Since we pinned the pacemaker to a single cpu,
	 * we can use the smp_processor_id() directly.
	 */
	txq = &ndev->_tx[tx_index];

	HARD_TX_LOCK(ndev, txq, tx_index);

	if (unlikely(netif_xmit_frozen_or_drv_stopped(txq))) {
		//asgard_error("Device Busy unlocking.\n");
		goto unlock;
	}

	/* send packets in batch processing mode */
	for (i = 0; i < spminfo->num_of_targets; i++) {

		// only emit pkts marked with fire
		if(!target_fire[i])
			continue;

        skb = spminfo->pm_targets[i].pkt_data.skb;

        asg_xmit_skb(ndev, txq, skb);

        spminfo->pm_targets[i].pkt_tx_counter++;

	}

unlock:
	HARD_TX_UNLOCK(ndev, txq);

	local_bh_enable();
	local_irq_restore(flags);

}

static inline void asgard_update_skb_udp_port(struct sk_buff *skb, int udp_port)
{
	struct udphdr *uh = udp_hdr(skb);

	uh->dest = htons((u16)udp_port);
}

static inline void asgard_update_skb_payload(struct sk_buff *skb, void *payload)
{
	unsigned char *tail_ptr;
	unsigned char *data_ptr;

	tail_ptr = skb_tail_pointer(skb);
	data_ptr = (tail_ptr - ASGARD_PAYLOAD_BYTES);

	memcpy(data_ptr, payload, ASGARD_PAYLOAD_BYTES);
}

/*
 * Marks target with local id <i> as dead
 * if no update came in since last check
 */
void update_aliveness_states(struct asgard_device *sdev, struct pminfo *spminfo, int i)
{
	if(spminfo->pm_targets[i].cur_waiting_interval != 0){
		spminfo->pm_targets[i].cur_waiting_interval = spminfo->pm_targets[i].cur_waiting_interval - 1;
		return;
	}

	if(spminfo->pm_targets[i].lhb_ts == spminfo->pm_targets[i].chb_ts) {
		spminfo->pm_targets[i].alive = 0; // Todo: remove this?
        update_cluster_member(sdev->ci, i, 0);
		spminfo->pm_targets[i].cur_waiting_interval = spminfo->pm_targets[i].resp_factor;
		return;
	}

 	// may be redundant - since we already update aliveness on reception of pkt
	//spminfo->pm_targets[i].alive = 1; // Todo: remove this?

    //update_cluster_member(sdev->ci, i, 1);

	spminfo->pm_targets[i].lhb_ts =  spminfo->pm_targets[i].chb_ts;
	spminfo->pm_targets[i].cur_waiting_interval = spminfo->pm_targets[i].resp_factor;
}

u32 get_lowest_alive_id(struct asgard_device *sdev, struct pminfo *spminfo)
{
	int i;
	u32 cur_low = sdev->pminfo.cluster_id;


	for(i = 0; i < spminfo->num_of_targets; i++) {
		if(spminfo->pm_targets[i].alive){
			if(cur_low > spminfo->pm_targets[i].pkt_data.naddr.cluster_id) {
				cur_low = (u32) spminfo->pm_targets[i].pkt_data.naddr.cluster_id;
			}
		}
	}
	return cur_low;
}


void update_leader(struct asgard_device *sdev, struct pminfo *spminfo)
{
	int leader_lid = sdev->cur_leader_lid;
	u32 self_id = sdev->pminfo.cluster_id;
	u32 lowest_follower_id = get_lowest_alive_id(sdev, spminfo);
	struct consensus_priv *priv = sdev->consensus_priv;

	if(!priv) {
		asgard_dbg("consensus priv is NULL \n");
		return;
	}

	if(unlikely(sdev->warmup_state == WARMING_UP))
		return;


	if(leader_lid == -1 || spminfo->pm_targets[leader_lid].alive == 0) {
		if(lowest_follower_id == self_id) {
			/* TODO: parameterize candidate_counter check */
			if(priv->nstate == CANDIDATE && priv->candidate_counter < 50) {
				priv->candidate_counter++;
				return;
			}

			node_transition(priv->ins, CANDIDATE);
			write_log(&priv->ins->logger, FOLLOWER_BECOME_CANDIDATE, RDTSC_ASGARD);
		}
	}

}
EXPORT_SYMBOL(update_leader);

void update_alive_msg(struct asgard_device *sdev, struct asgard_payload *pkt_payload)
{
	int j;

	/* Not a member of a cluster yet - thus, append advertising messages */
	if(unlikely(sdev->warmup_state == WARMING_UP)) {
	    if(sdev->self_mac)
	        setup_cluster_join_advertisement(pkt_payload, sdev->pminfo.cluster_id, sdev->self_ip, sdev->self_mac);
	}

	// only leaders will append an ALIVE operation to the heartbeat
	if(sdev->is_leader == 0)
		return;

	// iterate through consensus protocols and include ALIVE message
	for (j = 0; j < sdev->num_of_proto_instances; j++) {

		if (sdev->protos[j]->proto_type == ASGARD_PROTO_CONSENSUS) {

			// get corresponding local instance data for consensus
			setup_alive_msg((struct consensus_priv *)sdev->protos[j]->proto_data,
							pkt_payload, sdev->protos[j]->instance_id);
		}
	}
}
EXPORT_SYMBOL(update_alive_msg);


static inline int _emit_pkts_scheduled(struct asgard_device *sdev,
		struct pminfo *spminfo)
{
	struct asgard_payload *pkt_payload;
	int i;

    pkt_payload = spminfo->multicast_pkt_data.payload;

    if(!pkt_payload) {
        asgard_error("packet payload is NULL! \n");
        return -1;
    }

    asgard_update_skb_udp_port(spminfo->multicast_skb, sdev->tx_port);
    asgard_update_skb_payload(spminfo->multicast_skb, pkt_payload);

	/* Send heartbeats to all targets */
    asgard_send_multicast_hb(sdev->ndev, spminfo);

	/* Leave Heartbeat multicast pkt in clean state */
    pkt_payload = spminfo->multicast_pkt_data.payload;

    if(!pkt_payload){
        asgard_error("pkt payload become NULL! \n");
        return -1;
    }

    /* Protocols have been emitted, do not send them again ..
     * .. and free the reservations for new protocols */
    invalidate_proto_data(sdev, pkt_payload);

    for(i = 0; i < spminfo->num_of_targets; i++)
        update_aliveness_states(sdev, spminfo, i); // check if we received messages since last call of this check

    update_alive_msg(sdev, pkt_payload);  // Setup next HB Message

	if(sdev->consensus_priv->nstate != LEADER) {
		update_leader(sdev, spminfo);
	}

	return 0;
}

static inline int _emit_pkts_non_scheduled_multi(struct asgard_device *sdev,
                                           struct pminfo *spminfo)
{
    struct asgard_payload *pkt_payload = NULL;

    spin_lock(&spminfo->multicast_pkt_data_oos.lock);

    pkt_payload = spminfo->multicast_pkt_data_oos.payload;

    asgard_update_skb_udp_port(spminfo->multicast_pkt_data_oos.skb, spminfo->multicast_pkt_data_oos.port);

    asgard_update_skb_payload(spminfo->multicast_pkt_data_oos.skb, pkt_payload);

    asgard_send_oos_multi_pkts(sdev->ndev, spminfo);

    memset(pkt_payload, 0, sizeof(struct asgard_payload ));

    spminfo->multicast_pkt_data_oos_fire = 0;

    spin_unlock(&spminfo->multicast_pkt_data_oos.lock);

    return 0;
}


static inline int _emit_pkts_non_scheduled(struct asgard_device *sdev,
		struct pminfo *spminfo)
{
	struct asgard_payload *pkt_payload = NULL;
	int i;
	struct net_device *ndev = sdev->ndev;
	int target_fire[MAX_NODE_ID];

    for (i = 0; i < spminfo->num_of_targets; i++) {
		target_fire[i] = spminfo->pm_targets[i].fire;
        spin_lock(&spminfo->pm_targets[i].pkt_data.lock);
    }

	for (i = 0; i < spminfo->num_of_targets; i++) {

		if(!target_fire[i])
			continue;

		pkt_payload =
		     spminfo->pm_targets[i].pkt_data.payload;

        asgard_update_skb_udp_port(spminfo->pm_targets[i].pkt_data.skb,
                                    spminfo->pm_targets[i].pkt_data.port);

        /* use default port for next transmission again */
        spminfo->pm_targets[i].pkt_data.port= sdev->tx_port;

		asgard_update_skb_payload(spminfo->pm_targets[i].pkt_data.skb,
					 pkt_payload);
	}

    asgard_send_oos_pkts(ndev, spminfo, target_fire);

	 /* Leave pkts in clean state */
	 for (i = 0; i < spminfo->num_of_targets; i++) {

		if(!target_fire[i])
			continue;

         pkt_payload =
                 spminfo->pm_targets[i].pkt_data.payload;

		memset(pkt_payload, 0, sizeof(struct asgard_payload ));

		// after alive msg has been added, the current active buffer can be used again
		spminfo->pm_targets[i].fire = 0;

	}

    for (i = 0; i < spminfo->num_of_targets; i++) {
        spin_unlock(&spminfo->pm_targets[i].pkt_data.lock);
    }

	return 0;
}

int emit_apkt(struct net_device *ndev, struct pminfo *spminfo, struct asgard_async_pkt *apkt)
{
	struct netdev_queue *txq;
    unsigned long flags;
    int cpuid = smp_processor_id();
    netdev_tx_t ret;

    if (unlikely(!netif_running(ndev) ||
                 !netif_carrier_ok(ndev))) {
        asgard_error("Network device offline!\n exiting pacemaker\n");
        spminfo->errors++;
        return NETDEV_TX_BUSY;
    }

    if(!apkt->skb){
        asgard_error("BUG! skb is not set (second check)\n");
        spminfo->errors++;
        return NET_XMIT_DROP;
    }

    txq = &(ndev->_tx[cpuid]);
    local_irq_save(flags);
    local_bh_disable();

    HARD_TX_LOCK(ndev, txq, cpuid);

    if (unlikely(netif_xmit_frozen_or_drv_stopped(txq))) {
		asgard_error("Device Busy unlocking in async window.\n");
		ret = NETDEV_TX_BUSY;
		goto unlock;
	}

    skb_get(apkt->skb);

	ret = netdev_start_xmit(apkt->skb, ndev, txq, 0);

    spminfo->errors = 0;

unlock:
	HARD_TX_UNLOCK(ndev, txq);

	local_bh_enable();
	local_irq_restore(flags);
out:
	return ret;
}

int _emit_async_unicast_pkts(struct asgard_device *sdev, struct pminfo *spminfo)
{
    int i;
    struct asgard_async_pkt *cur_apkt;
    int ret;

    for (i = 0; i < spminfo->num_of_targets; i++) {
        if(spminfo->pm_targets[i].aapriv->doorbell > 0) {

            cur_apkt = dequeue_async_pkt(spminfo->pm_targets[i].aapriv);

            // consider packet already handled
            spminfo->pm_targets[i].aapriv->doorbell--;

            if(!cur_apkt || !cur_apkt->skb) {
                asgard_error("pkt or skb is NULL! \n");
                continue;
            }

            skb_set_queue_mapping(cur_apkt->skb, smp_processor_id());

#if 0
            // DEBUG: print emitted pkts
            num_entries = GET_CON_AE_NUM_ENTRIES_VAL( get_payload_ptr(cur_apkt)->proto_data);
            prev_log_idx = GET_CON_AE_PREV_LOG_IDX_PTR(get_payload_ptr(cur_apkt)->proto_data);
            asgard_dbg("Node: %d - Emitting %d entries, start_idx=%d", i, num_entries, *prev_log_idx);
#endif
            // emit pkt, and check if transmission failed
            ret = emit_apkt(sdev->ndev, spminfo, cur_apkt);

            switch(ret){
                case NETDEV_TX_OK:

                    /* packet is not needed anymore */
                    if(cur_apkt->skb)
                        kfree_skb(cur_apkt->skb);
                    kfree(cur_apkt);

                    break;
                case NET_XMIT_DROP:
                    break;
                case NETDEV_TX_BUSY:

                    /* requeue packet */
                    push_front_async_pkt(spminfo->pm_targets[i].aapriv, cur_apkt);
                    spminfo->pm_targets[i].pkt_tx_errors++;

                    return -1;
                default:
                    asgard_error("Unhandled Return code in async pkt handler (%d)\n", ret);
            }
            spminfo->pm_targets[i].pkt_tx_counter++;
        }
    }

    return 0;
}

int _emit_async_multicast_pkt(struct asgard_device *sdev, struct pminfo *spminfo)
{
    struct asgard_async_pkt *cur_apkt;
    int ret;

    if(sdev->multicast.aapriv->doorbell > 0) {

        cur_apkt = dequeue_async_pkt(sdev->multicast.aapriv);

        // consider packet already handled
        sdev->multicast.aapriv->doorbell--;

        if(!cur_apkt || !cur_apkt->skb) {
            asgard_error("pkt or skb is NULL! \n");
            return -1;
        }

        skb_set_queue_mapping(cur_apkt->skb, smp_processor_id());

        // emit pkt, and check if transmission failed
        ret = emit_apkt(sdev->ndev, spminfo, cur_apkt);

        switch(ret){
            case NETDEV_TX_OK:

                /* packet is not needed anymore */
                if(cur_apkt->skb)
                    kfree_skb(cur_apkt->skb);
                kfree(cur_apkt);

                break;
            case NET_XMIT_DROP:
                break;
            case NETDEV_TX_BUSY:
                /* requeue packet */
                push_front_async_pkt(sdev->multicast.aapriv, cur_apkt);
                return -1;
            default:
                asgard_error("Unhandled Return code in async pkt handler (%d)\n", ret);
        }
    }

    return 0;
}


int _emit_async_pkts(struct asgard_device *sdev, struct pminfo *spminfo) 
{
    if (sdev->multicast.enable)
        return _emit_async_multicast_pkt(sdev, spminfo);
    else
        return _emit_async_unicast_pkts(sdev, spminfo);
}

static int _validate_pm(struct asgard_device *sdev,
							struct pminfo *spminfo)
{
	if (!spminfo) {
		asgard_error("No Device. %s\n", __func__);
		return -ENODEV;
	}

	if (spminfo->state != ASGARD_PM_READY) {
		asgard_error("Pacemaker is not in ready state!\n");
		return -EPERM;
	}

	if (!sdev) {
		asgard_error("No sdev\n");
		return -ENODEV;
	}

	if (!sdev->ndev) {
		asgard_error("netdevice is NULL\n");
		return -EINVAL;
	}

	if(!sdev->self_mac) {
	    asgard_error("self mac is NULL\n");
	    return -EINVAL;
	}

    if(!sdev->multicast_mac) {
        asgard_error("multicast mac is NULL\n");
        return -EINVAL;
    }

	return 0;
}

//#ifndef  CONFIG_KUNIT
static int __prepare_pm_loop(struct asgard_device *sdev, struct pminfo *spminfo)
{
	if(asgard_setup_hb_skbs(sdev))
	    return -1;

    pm_state_transition_to(spminfo, ASGARD_PM_EMITTING);

	sdev->warmup_state = WARMING_UP;

	get_cpu(); // disable preemption

	return 0;
}
//#endif // ! CONFIG_KUNIT

//#ifndef  CONFIG_KUNIT
static void __postwork_pm_loop(struct asgard_device *sdev)
{
	int i;

	put_cpu();

	// Stopping all protocols
	for (i = 0; i < sdev->num_of_proto_instances; i++)
		if (sdev->protos[i] != NULL && sdev->protos[i]->ctrl_ops.stop != NULL){
            sdev->protos[i]->ctrl_ops.stop(sdev->protos[i]);
        }

    if(sdev->pminfo.multicast_skb != NULL)
        kfree_skb(sdev->pminfo.multicast_skb);

    // free fixed skbs again
    for(i = 0; i < sdev->pminfo.num_of_targets; i++){
        if(sdev->pminfo.pm_targets[i].pkt_data.skb != NULL)
            kfree_skb(sdev->pminfo.pm_targets[i].pkt_data.skb);
    }
}
//#endif // ! CONFIG_KUNIT

//#ifndef CONFIG_KUNIT
static int asgard_pm_loop(void *data)
{
	uint64_t prev_time, cur_time;
	struct asgard_device *sdev = (struct asgard_device *) data;
	struct pminfo *spminfo = &sdev->pminfo;
	uint64_t interval = spminfo->hbi;
	int err;
	int scheduled_hb = 0;
	int out_of_sched_hb = 0;
	int async_pkts = 0;
	int out_of_sched_multi = 0;

	spminfo->errors = 0;

    asgard_dbg("Starting Pacemaker\n");

    if(!sdev->ndev || !sdev->ndev->ip_ptr || !sdev->ndev->ip_ptr->ifa_list || !sdev->ndev->ip_ptr->ifa_list->ifa_address){
        asgard_error("Initialization Failed. Aborting Pacemaker now\n");
        return -ENODEV;
    }

    if(__prepare_pm_loop(sdev, spminfo))
        return -1;

	prev_time = RDTSC_ASGARD;

	while (asgard_pacemaker_is_alive(spminfo)) {

		cur_time = RDTSC_ASGARD;

        out_of_sched_hb = 0;
        async_pkts = 0;
        out_of_sched_multi = 0;

        /* Scheduled Multicast Heartbeats */
		scheduled_hb = scheduled_tx(prev_time, cur_time, interval);

		if(spminfo->errors > 1000) {
            pm_state_transition_to(spminfo, ASGARD_PM_FAILED);
            break;
		}

		if(scheduled_hb)
			goto emit;

		/* If in Sync Window, do not send anything until the Heartbeat has been sent */
		if (!check_async_window(prev_time, cur_time, interval, spminfo->waiting_window)) {
            continue;
        }

		/* including Leader Election Messages  */
		out_of_sched_hb = out_of_schedule_tx(sdev);

		if(out_of_sched_hb)
			goto emit;

        out_of_sched_multi = out_of_schedule_multi_tx(sdev);

        if(out_of_sched_multi)
            goto emit;

		/* including Log Replication Messages */
		async_pkts = check_async_door(sdev);

		if(async_pkts)
			goto emit;

		continue;

emit:
		if(scheduled_hb) {
            prev_time = cur_time;
            err = _emit_pkts_scheduled(sdev, spminfo);
            sdev->hb_interval++; // todo: how to handle overflow?
		} else if (out_of_sched_hb){
			err = _emit_pkts_non_scheduled(sdev, spminfo);
        } else if (async_pkts) {
			err = _emit_async_pkts(sdev, spminfo);
		} else if (out_of_sched_multi) {
            err = _emit_pkts_non_scheduled_multi(sdev, spminfo);
        }

		if (err) {
			asgard_pm_stop(spminfo);
			return err;
		}


		// if (sdev->ts_state == ASGARD_TS_RUNNING)
		//	asgard_write_timestamp(sdev, 0, RDTSC_ASGARD, 42);

	}
    asgard_dbg(" exiting pacemaker \n");

	__postwork_pm_loop(sdev);

	return 0;
}
/*#else
static int asgard_pm_loop(void *data)
{
	return 0;
}
#endif*/

#ifndef CONFIG_KUNIT
static enum hrtimer_restart asgard_pm_timer(struct hrtimer *timer)
{
	unsigned long flags;
	struct pminfo *spminfo =
			container_of(timer, struct pminfo, pm_timer);

	struct asgard_device *sdev =
			container_of(spminfo, struct asgard_device, pminfo);

	ktime_t currtime, interval;

	if (!asgard_pacemaker_is_alive(spminfo))
		return HRTIMER_NORESTART;

	currtime  = ktime_get();
	interval = ktime_set(0, 100000000);
	hrtimer_forward(timer, currtime, interval);

	asgard_setup_hb_skbs(sdev);

	get_cpu(); /* disable preemption */

	local_irq_save(flags);
	local_bh_disable();

	//_emit_pkts(sdev, spminfo, sdev->fire);

	local_bh_enable();
	local_irq_restore(flags);

	put_cpu();
	return HRTIMER_RESTART;
}
#else
static enum hrtimer_restart asgard_pm_timer(struct hrtimer *timer)
{
	return 0;
}
#endif

int asgard_pm_start_loop(void *data)
{
	struct pminfo *spminfo =
		(struct pminfo *) data;
	struct asgard_device *sdev =
		container_of(spminfo, struct asgard_device, pminfo);
	struct cpumask mask;
	int err;

    asgard_dbg("asgard_pm_start_loop\n");

	err = _validate_pm(sdev, spminfo);

	if (err)
		return err;

	cpumask_clear(&mask);

	heartbeat_task = kthread_create(&asgard_pm_loop, sdev,
			"asgard pm loop");

	kthread_bind(heartbeat_task, spminfo->active_cpu);

	if (IS_ERR(heartbeat_task)) {
		asgard_error("Task Error. %s\n", __func__);
		return -EINVAL;
	}

	wake_up_process(heartbeat_task);

	return 0;
}

int asgard_pm_stop(struct pminfo *spminfo)
{
	if(!spminfo) {
		asgard_error("spminfo is NULL.\n");
		return -EINVAL;
	}

	pm_state_transition_to(spminfo, ASGARD_PM_READY);

	return 0;
}
EXPORT_SYMBOL(asgard_pm_stop);

int asgard_pm_reset(struct pminfo *spminfo)
{
	struct asgard_device *sdev;

	asgard_dbg("Reset Pacemaker Configuration\n");

	if (!spminfo) {
		asgard_error("No Device. %s\n", __func__);
		return -ENODEV;
	}

	if (spminfo->state == ASGARD_PM_EMITTING) {
		asgard_error(
			"Can not reset targets when pacemaker is running\n");
		return -EPERM;
	}

	sdev = container_of(spminfo, struct asgard_device, pminfo);

	asgard_reset_remote_host_counter(sdev->asgard_id);
	return 0;
}