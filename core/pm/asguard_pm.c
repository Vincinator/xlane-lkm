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

#undef LOG_PREFIX
#define LOG_PREFIX "[ASGUARD][PACEMAKER]"

static struct task_struct *heartbeat_task;

static inline bool

asguard_pacemaker_is_alive(struct pminfo *spminfo)
{
	return spminfo->state == ASGUARD_PM_EMITTING;
}

static inline bool can_fire(uint64_t prev_time, uint64_t cur_time, uint64_t interval)
{
	return (cur_time - prev_time) >= interval;
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
	int hb_active_ix;
	struct asguard_payload *pkt_payload;
	struct node_addr *naddr;

	// BUG_ON(spminfo->num_of_targets > MAX_REMOTE_SOURCES);

	for (i = 0; i < spminfo->num_of_targets; i++) {

		hb_active_ix =
		     spminfo->pm_targets[i].pkt_data.hb_active_ix;

		pkt_payload =
			spminfo->pm_targets[i].pkt_data.pkt_payload[hb_active_ix];

		naddr = &spminfo->pm_targets[i].pkt_data.naddr;

		/* Setup SKB */
		spminfo->pm_targets[i].skb = compose_skb(sdev, naddr, pkt_payload);
		skb_set_queue_mapping(spminfo->pm_targets[i].skb, smp_processor_id()); // Queue mapping same for each target i
	}
}

static inline void asguard_send_hbs(struct net_device *ndev, struct pminfo *spminfo)
{
	struct netdev_queue *txq;
	struct sk_buff *skb;
	unsigned long flags;
	int tx_index = smp_processor_id();
	int i, ret;

	if (unlikely(!netif_running(ndev) ||
			!netif_carrier_ok(ndev))) {
		asguard_error("Network device offline!\n exiting pacemaker\n");
		return -1;
	}

	if( spminfo->num_of_targets == 0) {
		asguard_dbg("No targets for pacemaker. \n");
		return 0;
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
		asguard_error("Device Busy unlocking.\n");
		goto unlock;
	}

	/* send packets in batch processing mode */
	for (i = 0; i < spminfo->num_of_targets; i++) {
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
		ret = netdev_start_xmit(skb, ndev, txq, i + 1 != spminfo->num_of_targets);

		if(ret != NETDEV_TX_OK) {
			asguard_error("netdev_start_xmit returned %d - DEBUG THIS - exiting PM now. \n", ret);
			goto unlock;
		}
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

/* TODO: directly write in skb, and use skb dual-buffer!?
 * Update: when we write directly to the skb, then we need to
 *			implement a locking mechanism for NIC/CPU accessing the
 *			skb's.. Which would introduce potential jitter sources..
 *			This memcpy makes sure, that we only access the next skb
 *			if the applications are done with the logic..
 */
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
		if (sdev->verbose){
			asguard_dbg("Node %d is considered dead. \n lhb_ts = %llu \n chb_ts = %llu \n, cluster_id=%d",
						i, spminfo->pm_targets[i].lhb_ts, spminfo->pm_targets[i].chb_ts, spminfo->pm_targets[i].pkt_data.naddr.cluster_id );

			asguard_dbg("Processing Info\n
						proc counter: %llu\n
			 			start ts: %llu\n
						end ts: %llu",
						sdev->pkt_proc_ctr,
						sdev->pkt_proc_sts,
						sdev->pkt_proc_ets);


		}
		spminfo->pm_targets[i].alive = 0;
		spminfo->pm_targets[i].cur_waiting_interval = spminfo->pm_targets[i].resp_factor;
		return;
	}

 	// may be redundant - since we already update aliveness on reception of pkt
	spminfo->pm_targets[i].alive = 1;

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
			asguard_dbg("Start self nomination - this node has the lowest id %d\n", lowest_follower_id);

			node_transition(priv->ins, CANDIDATE);
			write_log(&priv->ins->logger, FOLLOWER_BECOME_CANDIDATE, RDTSC_ASGUARD);
		}
	}


}
EXPORT_SYMBOL(update_leader);



static inline int _emit_pkts(struct asguard_device *sdev,
		struct pminfo *spminfo)
{
	struct asguard_payload *pkt_payload;
	int i;
	int hb_active_ix;
	struct net_device *ndev = sdev->ndev;
	int batch = spminfo->num_of_targets;
	enum tsstate ts_state = sdev->ts_state;
	int ret;


	/* Prepare heartbeat packets */
	for (i = 0; i < spminfo->num_of_targets; i++) {

		// Always update payload to avoid jitter!
		hb_active_ix =
		     spminfo->pm_targets[i].pkt_data.hb_active_ix;

		pkt_payload =
		     spminfo->pm_targets[i].pkt_data.pkt_payload[hb_active_ix];

		//asguard_update_skb_udp_port(spminfo->pm_targets[i].skb, sdev->tx_port);
		//asguard_dbg("sending to cluster node %d to udp target port %d", i, sdev->tx_port);

		asguard_update_skb_payload(spminfo->pm_targets[i].skb,
					 pkt_payload);
	}

	/* Send heartbeats to all targets */
	asguard_send_hbs(ndev, spminfo);

	if(ts_state == ASGUARD_TS_RUNNING) {
		asguard_write_timestamp(sdev, 0, RDTSC_ASGUARD, i);
	}

	/* Leave Heartbeat pkts in clean state */
	for (i = 0; i < spminfo->num_of_targets; i++) {
		hb_active_ix =
		     spminfo->pm_targets[i].pkt_data.hb_active_ix;

		pkt_payload =
		     spminfo->pm_targets[i].pkt_data.pkt_payload[hb_active_ix];

		/* Protocols have been emitted, do not sent them again ..
		 * .. and free the reservations for new protocols */
		invalidate_proto_data(sdev, pkt_payload, i);
		spminfo->pm_targets[i].pkt_data.active_dirty = 0;

		update_aliveness_states(sdev, spminfo, i);
	}

	if(sdev->consensus_priv->nstate != LEADER)
		update_leader(sdev, spminfo);
	else
		prepare_log_replication(sdev);

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

#ifndef  CONFIG_KUNIT
static void __prepare_pm_loop(struct asguard_device *sdev, struct pminfo *spminfo)
{
	asguard_setup_hb_skbs(sdev);

	pm_state_transition_to(spminfo, ASGUARD_PM_EMITTING);

	sdev->warmup_state = WARMING_UP;

	get_cpu(); // disable preemption

}
#endif // ! CONFIG_KUNIT

#ifndef  CONFIG_KUNIT
static void __postwork_pm_loop(struct asguard_device *sdev)
{
	int i;

	put_cpu();

	// Stopping all protocols
	for (i = 0; i < sdev->num_of_proto_instances; i++)
		if (sdev->protos[i] != NULL && sdev->protos[i]->ctrl_ops.stop != NULL)
			sdev->protos[i]->ctrl_ops.stop(sdev->protos[i]);

}
#endif // ! CONFIG_KUNIT


#ifndef CONFIG_KUNIT
static int asguard_pm_loop(void *data)
{
	uint64_t prev_time, cur_time;
	struct asguard_device *sdev = (struct asguard_device *) data;
	struct pminfo *spminfo = &sdev->pminfo;
	uint64_t interval = spminfo->hbi;
	int err;

	__prepare_pm_loop(sdev, spminfo);

	prev_time = RDTSC_ASGUARD;

	while (asguard_pacemaker_is_alive(spminfo)) {

		cur_time = RDTSC_ASGUARD;

		// if (!can_fire(prev_time, cur_time, interval))
		//	continue;

		if (!sdev->fire && !can_fire(prev_time, cur_time, interval))
			continue;

		sdev->fire = !sdev->fire;

		prev_time = cur_time;

		err = _emit_pkts(sdev, spminfo);

		if (err) {
			asguard_pm_stop(spminfo);
			return err;
		}

		// if (sdev->ts_state == ASGUARD_TS_RUNNING)
		//	asguard_write_timestamp(sdev, 0, RDTSC_ASGUARD, 42);

	}

	__postwork_pm_loop(sdev);

	return 0;
}
#else
static int asguard_pm_loop(void *data)
{
	return 0;
}
#endif

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

	_emit_pkts(sdev, spminfo);

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

int asguard_pm_start_timer(void *data)
{
	struct pminfo *spminfo =
		(struct pminfo *) data;
	struct asguard_device *sdev =
		container_of(spminfo, struct asguard_device, pminfo);
	ktime_t interval;
	int err;

	err = _validate_pm(sdev, spminfo);

	if (err)
		return err;

	pm_state_transition_to(spminfo, ASGUARD_PM_EMITTING);

	interval = ktime_set(0, 100000000);

	hrtimer_init(&spminfo->pm_timer, CLOCK_MONOTONIC,
		HRTIMER_MODE_REL_PINNED);

	spminfo->pm_timer.function = &asguard_pm_timer;

	hrtimer_start(&spminfo->pm_timer, interval,
		HRTIMER_MODE_REL_PINNED);

	return 0;
}


int asguard_pm_start_loop(void *data)
{
	struct pminfo *spminfo =
		(struct pminfo *) data;
	struct asguard_device *sdev =
		container_of(spminfo, struct asguard_device, pminfo);
	struct cpumask mask;
	int err;

	err = _validate_pm(sdev, spminfo);

	if (err)
		return err;

	asguard_dbg("protocol instances: %d", sdev->num_of_proto_instances);

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
