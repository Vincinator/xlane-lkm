#include <sassy/logger.h>
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

#include <sassy/sassy.h>
#include <sassy/payload_helper.h>

#undef LOG_PREFIX
#define LOG_PREFIX "[SASSY][PACEMAKER]"

static struct task_struct *heartbeat_task;

static inline bool

sassy_pacemaker_is_alive(struct pminfo *spminfo)
{
	return spminfo->state == SASSY_PM_EMITTING;
}

static inline bool can_fire(uint64_t prev_time, uint64_t cur_time, uint64_t interval)
{
	return (cur_time - prev_time) >= interval;
}

const char *pm_state_string(pmstate_t state)
{
	switch (state) {
	case SASSY_PM_UNINIT:
		return "SASSY_PM_UNINIT";
	case SASSY_PM_READY:
		return "SASSY_PM_READY";
	case SASSY_PM_EMITTING:
		return "SASSY_PM_EMITTING";
	default:
		return "UNKNOWN STATE";
	}
}

void pm_state_transition_to(struct pminfo *spminfo,
			    const enum pmstate state)
{
#if 1
	sassy_dbg("State Transition from %s to %s\n",
		  pm_state_string(spminfo->state), pm_state_string(state));
#endif
	spminfo->state = state;
}

static inline void sassy_setup_hb_skbs(struct sassy_device *sdev)
{
	int i;
	struct pminfo *spminfo = &sdev->pminfo;
	int hb_active_ix;
	struct sassy_payload *pkt_payload;
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
		skb_set_queue_mapping(spminfo->pm_targets[i].skb, smp_processor_id());
	}
}

static inline void sassy_send_hb(struct net_device *ndev, struct sk_buff *skb)
{
	int ret;
	struct netdev_queue *txq;

	txq = skb_get_tx_queue(ndev, skb);
	skb_get(skb); /* keep this. otherwise this thread locks the system */

	HARD_TX_LOCK(ndev, txq, smp_processor_id());

	if (unlikely(netif_xmit_frozen_or_drv_stopped(txq))) {
		sassy_error("Device Busy unlocking.\n");
		goto unlock;
	}

	//skb_queue_head
	ret = netdev_start_xmit(skb, ndev, txq, 0);
unlock:
	HARD_TX_UNLOCK(ndev, txq);
}

static inline void sassy_update_skb_payload(struct sk_buff *skb, void *payload)
{
	unsigned char *tail_ptr;
	unsigned char *data_ptr;

	tail_ptr = skb_tail_pointer(skb);
	data_ptr = (tail_ptr - SASSY_PAYLOAD_BYTES);

	// TODO: directly write in skb, and use skb dual-buffer!?
	// TODO: check if the skb memory is moved/freed by the NIC
	memcpy(data_ptr, payload, SASSY_PAYLOAD_BYTES);
}

static inline int _emit_pkts(struct sassy_device *sdev,
		struct pminfo *spminfo)
{
	struct sassy_payload *pkt_payload;
	int i;
	int hb_active_ix;
	struct net_device *ndev = sdev->ndev;
	// enum tsstate ts_state = sdev->ts_state;

	/* If netdev is offline, then stop pacemaker */
	if (unlikely(!netif_running(ndev) ||
		     !netif_carrier_ok(ndev))) {
		return -1;
	}

	for (i = 0; i < spminfo->num_of_targets; i++) {
 
		// Always update payload to avoid jitter!
		hb_active_ix =
		     spminfo->pm_targets[i].pkt_data.hb_active_ix;

		pkt_payload =
		     spminfo->pm_targets[i].pkt_data.pkt_payload[hb_active_ix];

		//Direct Updates - No double buffer
		//if (sdev->proto->ctrl_ops.us_update != NULL)
		//	sdev->proto->ctrl_ops.us_update(sdev, pkt_payload);

		sassy_update_skb_payload(spminfo->pm_targets[i].skb,
					 pkt_payload);

		// if (sdev->verbose >= 4)
		// 	print_hex_dump(KERN_DEBUG,
		// 		"TX Payload: ", DUMP_PREFIX_NONE,
		// 		16, 1, pkt_payload, SASSY_PAYLOAD_BYTES, 0);

		sassy_send_hb(ndev, spminfo->pm_targets[i].skb);

		// if (ts_state == SASSY_TS_RUNNING) {
		// 	sassy_write_timestamp(sdev, 0, rdtsc(), i);
		// 	sassy_write_timestamp(sdev, 4, ktime_get(), i);
		// }
		

		// Protocols have been emitted, do not sent them again ..
		// .. and free the reservations for new protocols
		invalidate_proto_data(sdev, pkt_payload, i);
		

	}
	return 0;
}

static int _validate_pm(struct sassy_device *sdev,
							struct pminfo *spminfo)
{
	if (!spminfo) {
		sassy_error("No Device. %s\n", __FUNCTION__);
		return -ENODEV;
	}

	if (spminfo->state != SASSY_PM_READY) {
		sassy_error("Pacemaker is not in ready state!\n");
		return -EPERM;
	}

	if (!sdev) {
		sassy_error("No sdev\n");
		return -ENODEV;
	}

	if (!sdev || !sdev->ndev) {
		sassy_error("netdevice is NULL\n");
		return -EINVAL;
	}

	if (spminfo->num_of_targets <= 0) {
		sassy_error("num_of_targets is invalid! Have you set the target hosts?\n");
		return -EINVAL;
	}

	return 0;
}


static void __prepare_pm_loop(struct sassy_device *sdev, struct pminfo *spminfo)
{
	sassy_setup_hb_skbs(sdev);

	pm_state_transition_to(spminfo, SASSY_PM_EMITTING);

	sdev->warmup_state = WARMING_UP;

	get_cpu(); // disable preemption

}

static void __postwork_pm_loop(struct sassy_device *sdev)
{
	int i;

	put_cpu();

	// Stopping all protocols 
	for(i = 0; i < sdev->num_of_proto_instances; i++)
		if(sdev->protos[i] != NULL && sdev->protos[i]->ctrl_ops.stop != NULL)
			sdev->protos[i]->ctrl_ops.stop(sdev->protos[i]);

}

static int sassy_pm_loop(void *data)
{
	uint64_t prev_time, cur_time;
	struct sassy_device *sdev = (struct sassy_device *) data;
	struct pminfo *spminfo = &sdev->pminfo;
	uint64_t interval = spminfo->hbi;	
	unsigned long flags;
	int err;

	__prepare_pm_loop(sdev, spminfo);
	
	prev_time = rdtsc();

	while (sassy_pacemaker_is_alive(spminfo)) {

		cur_time = rdtsc();

		// if (!can_fire(prev_time, cur_time, interval))
		// 	continue;

		if (!sdev->fire&&!can_fire(prev_time, cur_time, interval))
			continue;

		sdev->fire = !sdev->fire;

		prev_time = cur_time;
		
		local_irq_save(flags);
		local_bh_disable();

		err = _emit_pkts(sdev, spminfo);

		if (err) {
			sassy_pm_stop(spminfo);
			local_bh_enable();
			local_irq_restore(flags);
			return err;
		}

		local_bh_enable();
		local_irq_restore(flags);
	
		// if (sdev->ts_state == SASSY_TS_RUNNING)
		// 	sassy_write_timestamp(sdev, 0, rdtsc(), 42);

	}

	__postwork_pm_loop(sdev);

	return 0;
}

static enum hrtimer_restart sassy_pm_timer(struct hrtimer *timer)
{
	unsigned long flags;
	struct pminfo *spminfo =
			container_of(timer, struct pminfo, pm_timer);

	struct sassy_device *sdev =
			container_of(spminfo, struct sassy_device, pminfo);

	ktime_t currtime, interval;

	if (!sassy_pacemaker_is_alive(spminfo)){
		return HRTIMER_NORESTART;
	}

	currtime  = ktime_get();
	interval = ktime_set(0, 100000000);
	hrtimer_forward(timer, currtime, interval);

	sassy_setup_hb_skbs(sdev);

	get_cpu(); /* disable preemption */

	local_irq_save(flags);
	local_bh_disable();

	_emit_pkts(sdev, spminfo);

	local_bh_enable();
	local_irq_restore(flags);

	put_cpu();
	return HRTIMER_RESTART;
}

int sassy_pm_start_timer(void *data)
{
	struct pminfo *spminfo =
		(struct pminfo *) data;
	struct sassy_device *sdev =
		container_of(spminfo, struct sassy_device, pminfo);
	ktime_t interval;
	int active_cpu;
	int err;

	err = _validate_pm(sdev, spminfo);

	if (err)
		return err;

	pm_state_transition_to(spminfo, SASSY_PM_EMITTING);

	interval = ktime_set(0, 100000000);

	hrtimer_init(&spminfo->pm_timer, CLOCK_MONOTONIC,
		HRTIMER_MODE_REL_PINNED);

	spminfo->pm_timer.function = &sassy_pm_timer;

	hrtimer_start(&spminfo->pm_timer, interval,
		HRTIMER_MODE_REL_PINNED);

	return 0;
}


int sassy_pm_start_loop(void *data)
{
	struct pminfo *spminfo =
		(struct pminfo *) data;
	struct sassy_device *sdev =
		container_of(spminfo, struct sassy_device, pminfo);
	struct cpumask mask;
	int err, i;

	err = _validate_pm(sdev, spminfo);

	if (err)
		return err;

	sassy_dbg("protocol instances: %d", sdev->num_of_proto_instances);

	cpumask_clear(&mask);

	heartbeat_task = kthread_create(&sassy_pm_loop, sdev, "sassy pm loop");

	kthread_bind(heartbeat_task, spminfo->active_cpu);

	if (IS_ERR(heartbeat_task)) {
		sassy_error("Task Error. %s\n", __FUNCTION__);
		return -EINVAL;
	}

	wake_up_process(heartbeat_task);

	return 0;
}

int sassy_pm_stop(struct pminfo *spminfo)
{
	pm_state_transition_to(spminfo, SASSY_PM_READY);

	return 0;
}

int sassy_pm_reset(struct pminfo *spminfo)
{
	struct sassy_device *sdev;

	sassy_dbg("Reset Pacemaker Configuration\n");

	if (!spminfo) {
		sassy_error("No Device. %s\n", __FUNCTION__);
		return -ENODEV;
	}

	if (spminfo->state == SASSY_PM_EMITTING) {
		sassy_error(
			"Can not reset targets when pacemaker is running\n");
		return -EPERM;
	}

	sdev = container_of(spminfo, struct sassy_device, pminfo);

	sassy_reset_remote_host_counter(sdev->sassy_id);
	return 0;
}
