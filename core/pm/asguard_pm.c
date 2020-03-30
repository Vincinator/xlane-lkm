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

#include <asguard/logger.h>
#include <asguard/asguard.h>
#include <asguard/payload_helper.h>
#include <asguard/consensus.h>
#include <asguard/asguard_async.h>
#include <asguard/asgard_uface.h>

#undef LOG_PREFIX
#define LOG_PREFIX "[ASGUARD][PACEMAKER]"

static struct task_struct *heartbeat_task;

static inline bool asguard_pacemaker_is_alive(struct pminfo *spminfo)
{
	return spminfo->state == ASGUARD_PM_EMITTING;
}

static inline bool scheduled_tx(uint64_t prev_time, uint64_t cur_time, uint64_t interval)
{
	return (cur_time - prev_time) >= interval;
}


static inline bool check_async_window(uint64_t prev_time, uint64_t cur_time, uint64_t interval, uint64_t sync_window)
{
	return (cur_time - prev_time) <= (interval - sync_window);
}


static inline bool check_async_door(struct pminfo *spminfo)
{
	int i;
	int doorbell = 0;

	for (i = 0; i < spminfo->num_of_targets; i++)
		doorbell += spminfo->pm_targets[i].aapriv->doorbell;

	return doorbell > 0;
}

static inline bool out_of_schedule_tx(struct asguard_device *sdev)
{
	int i, fire = 0;

	if(sdev->hold_fire)
		return 0;

	for (i = 0; i < sdev->pminfo.num_of_targets; i++)
		fire += sdev->pminfo.pm_targets[i].fire;

	return fire > 0;
}

const char *pm_state_string(enum pmstate state)
{
	switch (state) {
	case ASGUARD_PM_UNINIT:
		return "ASGUARD_PM_UNINIT";
	case ASGUARD_PM_READY:
		return "ASGUARD_PM_READY";
	case ASGUARD_PM_EMITTING:
		return "ASGUARD_PM_EMITTING";
	default:
		return "UNKNOWN STATE";
	}
}



void pm_state_transition_to(struct pminfo *spminfo,
			    const enum pmstate state)
{
#if VERBOSE_DEBUG
	asguard_dbg("State Transition from %s to %s\n",
		  pm_state_string(spminfo->state), pm_state_string(state));
#endif
	spminfo->state = state;
}

static inline void asguard_setup_hb_skbs(struct asguard_device *sdev)
{
	int i;
	struct pminfo *spminfo = &sdev->pminfo;
	struct node_addr *naddr;

    asguard_dbg("setup hb skbs. \n");

    if(!spminfo){
        asguard_error("spminfo is NULL \n");
        BUG();
    }

	// BUG_ON(spminfo->num_of_targets > MAX_REMOTE_SOURCES);

	for (i = 0; i < spminfo->num_of_targets; i++) {

		naddr = &spminfo->pm_targets[i].pkt_data.naddr;

        /* Setup SKB */
		spminfo->pm_targets[i].skb = asguard_reserve_skb(sdev->ndev, naddr->dst_ip, naddr->dst_mac, NULL);
		skb_set_queue_mapping(spminfo->pm_targets[i].skb, smp_processor_id()); // Queue mapping same for each target i

		/* Out of schedule SKB */
        spminfo->pm_targets[i].skb_oos = asguard_reserve_skb(sdev->ndev, naddr->dst_ip, naddr->dst_mac, NULL);
        skb_set_queue_mapping(spminfo->pm_targets[i].skb_oos, smp_processor_id()); // Queue mapping same for each target i

        /* Log Replication SKB */
        // spminfo->pm_targets[i].skb_logrep = asguard_reserve_skb(sdev->ndev, naddr->dst_ip, naddr->dst_mac, NULL);
        // skb_set_queue_mapping(spminfo->pm_targets[i].skb_logrep, smp_processor_id()); // Queue mapping same for each target i
	}
}

static inline void asguard_send_hbs(struct net_device *ndev, struct pminfo *spminfo, int fire, int target_fire[MAX_NODE_ID])
{
	struct netdev_queue *txq;
	struct sk_buff *skb;
	unsigned long flags;
	int tx_index = smp_processor_id();
	int i, ret;

	if (unlikely(!netif_running(ndev) ||
			!netif_carrier_ok(ndev))) {
		asguard_error("Network device offline!\n exiting pacemaker\n");
		return;
	}

	if( spminfo->num_of_targets == 0) {
		asguard_dbg("No targets for pacemaker. \n");
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
		//asguard_error("Device Busy unlocking.\n");
		goto unlock;
	}

	/* send packets in batch processing mode */
	for (i = 0; i < spminfo->num_of_targets; i++) {

		// if emission was scheduled via fire, only emit pkts marked with fire
		if(fire && !target_fire[i])
			continue;

		if(fire)
            skb = spminfo->pm_targets[i].skb_oos;
        else
            skb = spminfo->pm_targets[i].skb;


        skb_get(skb);

		/* Utilise batch process mechanism for the heartbeats.
		 * HB pkts will be transmitted to the NIC,
		 * but the NIC will only start emitting the pkts
		 * when the last HB has been successfully prepared and transmitted
		 * to the NIC.
		 *
		 * Locking and preparation overhead is reduced, because preparation
		 * work can be done once per batch process (vs. for each pkt).
		 */

		// if fire, then do not xmit in batch mode
		if(!fire)
			ret = netdev_start_xmit(skb, ndev, txq, i + 1 != spminfo->num_of_targets);
		else
			ret = netdev_start_xmit(skb, ndev, txq, 0);

		switch (ret) {
		case NETDEV_TX_OK:
			break;
		case NET_XMIT_DROP:
			asguard_error("NETDEV TX DROP\n");
			break;
		case NET_XMIT_CN:
			asguard_error("NETDEV XMIT CN\n");
			break;
		default:
			asguard_error("NETDEV UNKNOWN \n");
			/* fall through */
		case NETDEV_TX_BUSY:
			asguard_error("NETDEV TX BUSY\n");
			break;
		}

		// if(ret != NETDEV_TX_OK) {
		// 	asguard_error("netdev_start_xmit returned %d - DEBUG THIS - exiting PM now. \n", ret);
		// 	goto unlock;
		// }

		spminfo->pm_targets[i].pkt_tx_counter++;

	}

unlock:
	HARD_TX_UNLOCK(ndev, txq);

	local_bh_enable();
	local_irq_restore(flags);

}

static inline void asguard_update_skb_udp_port(struct sk_buff *skb, int udp_port)
{
	struct udphdr *uh = udp_hdr(skb);

	uh->dest = htons((u16)udp_port);
}

static inline void asguard_update_skb_payload(struct sk_buff *skb, void *payload)
{
	unsigned char *tail_ptr;
	unsigned char *data_ptr;

	tail_ptr = skb_tail_pointer(skb);
	data_ptr = (tail_ptr - ASGUARD_PAYLOAD_BYTES);

	memcpy(data_ptr, payload, ASGUARD_PAYLOAD_BYTES);
}

/*
 * Marks target with local id <i> as dead
 * if no update came in since last check
 */
void update_aliveness_states(struct asguard_device *sdev, struct pminfo *spminfo, int i)
{
	if(spminfo->pm_targets[i].cur_waiting_interval != 0){
		spminfo->pm_targets[i].cur_waiting_interval = spminfo->pm_targets[i].cur_waiting_interval - 1;
		return;
	}

	if(spminfo->pm_targets[i].lhb_ts == spminfo->pm_targets[i].chb_ts) {
		if (sdev->verbose && sdev->warmup_state == WARMED_UP){
			asguard_dbg("Node %d is considered dead - cluster_id=%d\n", i, spminfo->pm_targets[i].pkt_data.naddr.cluster_id);
		}
		spminfo->pm_targets[i].alive = 0; // Todo: remove this?
        update_cluster_member(sdev->ci, i, 0);
		spminfo->pm_targets[i].cur_waiting_interval = spminfo->pm_targets[i].resp_factor;
		return;
	}

 	// may be redundant - since we already update aliveness on reception of pkt
	spminfo->pm_targets[i].alive = 1; // Todo: remove this?

    update_cluster_member(sdev->ci, i, 1);

	spminfo->pm_targets[i].lhb_ts =  spminfo->pm_targets[i].chb_ts;
	spminfo->pm_targets[i].cur_waiting_interval = spminfo->pm_targets[i].resp_factor;
}

u32 get_lowest_alive_id(struct asguard_device *sdev, struct pminfo *spminfo)
{
	int i;
	u32 cur_low = sdev->cluster_id;


	for(i = 0; i < spminfo->num_of_targets; i++) {
		if(spminfo->pm_targets[i].alive){
			if(cur_low > spminfo->pm_targets[i].pkt_data.naddr.cluster_id) {
				cur_low = (u32) spminfo->pm_targets[i].pkt_data.naddr.cluster_id;
			}
		}
	}
	return cur_low;
}


void update_leader(struct asguard_device *sdev, struct pminfo *spminfo)
{
	int leader_lid = sdev->cur_leader_lid;
	u32 self_id = sdev->cluster_id;
	u32 lowest_follower_id = get_lowest_alive_id(sdev, spminfo);
	struct consensus_priv *priv = sdev->consensus_priv;

	if(!priv) {
		asguard_dbg("consensus priv is NULL \n");
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
			write_log(&priv->ins->logger, FOLLOWER_BECOME_CANDIDATE, RDTSC_ASGUARD);
		}
	}

}
EXPORT_SYMBOL(update_leader);

void update_alive_msg(struct asguard_device *sdev, struct asguard_payload *pkt_payload, int target_id)
{
	int j;

	// only leaders will append an ALIVE operation to the heartbeat
	if(sdev->is_leader == 0)
		return;

	// iterate through consensus protocols and include ALIVE message
	for (j = 0; j < sdev->num_of_proto_instances; j++) {

		if (sdev->protos[target_id] != NULL && sdev->protos[j]->proto_type == ASGUARD_PROTO_CONSENSUS) {

			// get corresponding local instance data for consensus
			setup_alive_msg((struct consensus_priv *)sdev->protos[j]->proto_data,
							pkt_payload, sdev->protos[j]->instance_id);
		}
	}
}
EXPORT_SYMBOL(update_alive_msg);


static inline int _emit_pkts_scheduled(struct asguard_device *sdev,
		struct pminfo *spminfo)
{
	struct asguard_payload *pkt_payload;
	int i;
	struct net_device *ndev = sdev->ndev;
	// enum tsstate ts_state = sdev->ts_state;

	/* Prepare heartbeat packets */
	for (i = 0; i < spminfo->num_of_targets; i++) {

		pkt_payload =
		     spminfo->pm_targets[i].pkt_data.hb_pkt_payload;

		if(!pkt_payload) {
		    asguard_error("packet payload is NULL! \n");
		    return -1;
		}

		asguard_update_skb_udp_port(spminfo->pm_targets[i].skb, sdev->tx_port);
		asguard_update_skb_payload(spminfo->pm_targets[i].skb,
					 pkt_payload);
	}

	/* Send heartbeats to all targets */
	asguard_send_hbs(ndev, spminfo, 0, NULL);

	// TODO: timestamp for scheduled
	//if(ts_state == ASGUARD_TS_RUNNING)
	//	asguard_write_timestamp(sdev, 0, RDTSC_ASGUARD, i);

	/* Leave Heartbeat pkts in clean state */
	for (i = 0; i < spminfo->num_of_targets; i++) {

		pkt_payload =
		     spminfo->pm_targets[i].pkt_data.hb_pkt_payload;

		if(!pkt_payload){
		    asguard_error("pkt payload become NULL! \n");
		    continue;
		}

		/* Protocols have been emitted, do not send them again ..
		 * .. and free the reservations for new protocols */
		invalidate_proto_data(sdev, pkt_payload, i);

		update_aliveness_states(sdev, spminfo, i);
		update_alive_msg(sdev, pkt_payload, i);

	}

	if(sdev->consensus_priv->nstate != LEADER) {
		update_leader(sdev, spminfo);
	}

	return 0;
}

static inline int _emit_pkts_non_scheduled(struct asguard_device *sdev,
		struct pminfo *spminfo)
{
	struct asguard_payload *pkt_payload = NULL;
	int i;
	struct net_device *ndev = sdev->ndev;
	int target_fire[MAX_NODE_ID];

	/* Storing fire and locking ALL pkt locks */
    for (i = 0; i < spminfo->num_of_targets; i++) {
		target_fire[i] = spminfo->pm_targets[i].fire;
        spin_lock(&spminfo->pm_targets[i].pkt_data.pkt_lock);
    }

	/* Prepare heartbeat packets */
	for (i = 0; i < spminfo->num_of_targets; i++) {

		if(!target_fire[i])
			continue;

		pkt_payload =
		     spminfo->pm_targets[i].pkt_data.pkt_payload;

		asguard_update_skb_payload(spminfo->pm_targets[i].skb_oos,
					 pkt_payload);
	}

	asguard_send_hbs(ndev, spminfo, 1, target_fire);

	// /* Leave Heartbeat pkts in clean state */
	 for (i = 0; i < spminfo->num_of_targets; i++) {

		if(!target_fire[i])
			continue;

         pkt_payload =
                 spminfo->pm_targets[i].pkt_data.pkt_payload;

		/* Protocols have been emitted, do not sent them again ..
		 * .. and free the reservations for new protocols */
		invalidate_proto_data(sdev, pkt_payload, i);

		memset(pkt_payload, 0, sizeof(struct asguard_payload ));

		// after alive msg has been added, the current active buffer can be used again
		spminfo->pm_targets[i].fire = 0;

	}
    /* Unlocking ALL pkt locks */
    for (i = 0; i < spminfo->num_of_targets; i++) {
        spin_unlock(&spminfo->pm_targets[i].pkt_data.pkt_lock);
    }

	return 0;
}

void emit_apkt(struct net_device *ndev, struct pminfo *spminfo, struct asguard_async_pkt *apkt)
{
	struct netdev_queue *txq;
    unsigned long flags;
    int tx_index = smp_processor_id();

    if (unlikely(!netif_running(ndev) ||
                 !netif_carrier_ok(ndev))) {
        asguard_error("Network device offline!\n exiting pacemaker\n");
        return;
    }

    /* The queue mapping is the same for each target <i>
     * Since we pinned the pacemaker to a single cpu,
     * we can use the smp_processor_id() directly.
     */
    txq = &(ndev->_tx[tx_index]);

    local_irq_save(flags);
    local_bh_disable();

    HARD_TX_LOCK(ndev, txq, smp_processor_id());

    if (unlikely(netif_xmit_frozen_or_drv_stopped(txq))) {
		asguard_error("Device Busy unlocking in async window.\n");
		goto unlock;
	}

    if(!apkt->skb){
        asguard_error("BUG! skb is not set (second check)\n");
        goto unlock;
    }

    skb_get(apkt->skb);


	netdev_start_xmit(apkt->skb, ndev, txq, 0);

unlock:
	HARD_TX_UNLOCK(ndev, txq);

	local_bh_enable();
	local_irq_restore(flags);
}

int _emit_async_pkts(struct asguard_device *sdev, struct pminfo *spminfo) 
{
	int i;
 	struct asguard_async_pkt *cur_apkt;
 	u32 num_entries;
    s32 *prev_log_idx;

    for (i = 0; i < spminfo->num_of_targets; i++) {
		if(spminfo->pm_targets[i].aapriv->doorbell > 0) {

			cur_apkt = dequeue_async_pkt(spminfo->pm_targets[i].aapriv);

            // Pkt has been handled
            spminfo->pm_targets[i].aapriv->doorbell--;

            if(!cur_apkt || !cur_apkt->skb) {
                asguard_error("pkt or skb is NULL! \n");
                continue;
            }

            skb_set_queue_mapping(cur_apkt->skb, smp_processor_id());

#if 0
            // DEBUG: print emitted pkts
            num_entries = GET_CON_AE_NUM_ENTRIES_VAL( get_payload_ptr(cur_apkt)->proto_data);
            prev_log_idx = GET_CON_AE_PREV_LOG_IDX_PTR(get_payload_ptr(cur_apkt)->proto_data);
            asguard_dbg("Node: %d - Emitting %d entries, start_idx=%d", i, num_entries, *prev_log_idx);
#endif
            // update payload
            //asguard_update_skb_payload(spminfo->pm_targets[i].skb_logrep, cur_apkt->payload);


            emit_apkt(sdev->ndev, spminfo, cur_apkt);

            spminfo->pm_targets[i].pkt_tx_counter++;


            if(cur_apkt) {
                if(cur_apkt->skb)
                    kfree_skb(cur_apkt->skb); // drop reference, and free skb if reference count hits 0
                kfree(cur_apkt);
            }

            // do not free skb!

        }
	}
	return 0;
}

static int _validate_pm(struct asguard_device *sdev,
							struct pminfo *spminfo)
{
	if (!spminfo) {
		asguard_error("No Device. %s\n", __func__);
		return -ENODEV;
	}

	if (spminfo->state != ASGUARD_PM_READY) {
		asguard_error("Pacemaker is not in ready state!\n");
		return -EPERM;
	}

	if (!sdev) {
		asguard_error("No sdev\n");
		return -ENODEV;
	}

	if (!sdev || !sdev->ndev) {
		asguard_error("netdevice is NULL\n");
		return -EINVAL;
	}

	if (spminfo->num_of_targets <= 0) {
		asguard_error("num_of_targets is invalid.\n");
		return -EINVAL;
	}

	return 0;
}

//#ifndef  CONFIG_KUNIT
static void __prepare_pm_loop(struct asguard_device *sdev, struct pminfo *spminfo)
{
	asguard_setup_hb_skbs(sdev);

    pm_state_transition_to(spminfo, ASGUARD_PM_EMITTING);

	sdev->warmup_state = WARMING_UP;

	get_cpu(); // disable preemption

}
//#endif // ! CONFIG_KUNIT

//#ifndef  CONFIG_KUNIT
static void __postwork_pm_loop(struct asguard_device *sdev)
{
	int i;

	put_cpu();

	// Stopping all protocols
	for (i = 0; i < sdev->num_of_proto_instances; i++)
		if (sdev->protos[i] != NULL && sdev->protos[i]->ctrl_ops.stop != NULL){
            sdev->protos[i]->ctrl_ops.stop(sdev->protos[i]);
        }

    // free fixed skbs again
    for(i = 0; i < sdev->pminfo.num_of_targets; i++){
        if(sdev->pminfo.pm_targets[i].skb != NULL)
            dev_kfree_skb(sdev->pminfo.pm_targets[i].skb);

        if(sdev->pminfo.pm_targets[i].skb_oos != NULL)
            dev_kfree_skb(sdev->pminfo.pm_targets[i].skb_oos);

        if(sdev->pminfo.pm_targets[i].skb_logrep != NULL)
            dev_kfree_skb(sdev->pminfo.pm_targets[i].skb_logrep);
    }
}
//#endif // ! CONFIG_KUNIT

//#ifndef CONFIG_KUNIT
static int asguard_pm_loop(void *data)
{
	uint64_t prev_time, cur_time;
	struct asguard_device *sdev = (struct asguard_device *) data;
	struct pminfo *spminfo = &sdev->pminfo;
	uint64_t interval = spminfo->hbi;
	int err;
	int scheduled_hb = 0;
	int out_of_sched_hb = 0;
	int async_pkts = 0;

    asguard_dbg(" starting pacemaker \n");

    __prepare_pm_loop(sdev, spminfo);

	prev_time = RDTSC_ASGUARD;

	while (asguard_pacemaker_is_alive(spminfo)) {

		cur_time = RDTSC_ASGUARD;

        out_of_sched_hb = 0;
        async_pkts = 0;
		scheduled_hb = scheduled_tx(prev_time, cur_time, interval);

		if(scheduled_hb)
			goto emit;

		/* If in Sync Window, do not send anything until the Heartbeat has been sent */
		if (!check_async_window(prev_time, cur_time, interval, spminfo->waiting_window)) {
            continue;
        }

		out_of_sched_hb = out_of_schedule_tx(sdev);

		if(out_of_sched_hb)
			goto emit;

		async_pkts = check_async_door(spminfo);

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
		}

		if (err) {
			asguard_pm_stop(spminfo);
			return err;
		}

		// if (sdev->ts_state == ASGUARD_TS_RUNNING)
		//	asguard_write_timestamp(sdev, 0, RDTSC_ASGUARD, 42);

	}
    asguard_dbg(" exiting pacemaker \n");

	__postwork_pm_loop(sdev);

	return 0;
}
/*#else
static int asguard_pm_loop(void *data)
{
	return 0;
}
#endif*/

#ifndef CONFIG_KUNIT
static enum hrtimer_restart asguard_pm_timer(struct hrtimer *timer)
{
	unsigned long flags;
	struct pminfo *spminfo =
			container_of(timer, struct pminfo, pm_timer);

	struct asguard_device *sdev =
			container_of(spminfo, struct asguard_device, pminfo);

	ktime_t currtime, interval;

	if (!asguard_pacemaker_is_alive(spminfo))
		return HRTIMER_NORESTART;

	currtime  = ktime_get();
	interval = ktime_set(0, 100000000);
	hrtimer_forward(timer, currtime, interval);

	asguard_setup_hb_skbs(sdev);

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
static enum hrtimer_restart asguard_pm_timer(struct hrtimer *timer)
{
	return 0;
}
#endif

int asguard_pm_start_loop(void *data)
{
	struct pminfo *spminfo =
		(struct pminfo *) data;
	struct asguard_device *sdev =
		container_of(spminfo, struct asguard_device, pminfo);
	struct cpumask mask;
	int err;

    asguard_dbg("asguard_pm_start_loop\n");

	err = _validate_pm(sdev, spminfo);

	if (err)
		return err;

	cpumask_clear(&mask);

	heartbeat_task = kthread_create(&asguard_pm_loop, sdev,
			"asguard pm loop");

	kthread_bind(heartbeat_task, spminfo->active_cpu);

	if (IS_ERR(heartbeat_task)) {
		asguard_error("Task Error. %s\n", __func__);
		return -EINVAL;
	}

	wake_up_process(heartbeat_task);

	return 0;
}

int asguard_pm_stop(struct pminfo *spminfo)
{
	if(!spminfo) {
		asguard_error("spminfo is NULL.\n");
		return -EINVAL;
	}

	pm_state_transition_to(spminfo, ASGUARD_PM_READY);

	return 0;
}
EXPORT_SYMBOL(asguard_pm_stop);

int asguard_pm_reset(struct pminfo *spminfo)
{
	struct asguard_device *sdev;

	asguard_dbg("Reset Pacemaker Configuration\n");

	if (!spminfo) {
		asguard_error("No Device. %s\n", __func__);
		return -ENODEV;
	}

	if (spminfo->state == ASGUARD_PM_EMITTING) {
		asguard_error(
			"Can not reset targets when pacemaker is running\n");
		return -EPERM;
	}

	sdev = container_of(spminfo, struct asguard_device, pminfo);

	asguard_reset_remote_host_counter(sdev->asguard_id);
	return 0;
}
