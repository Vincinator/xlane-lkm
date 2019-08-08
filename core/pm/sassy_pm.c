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

#undef LOG_PREFIX
#define LOG_PREFIX "[SASSY][PACEMAKER]"

struct task_struct *heartbeat_task;

#define NSEC_PER_MSEC   1000000L

static struct hrtimer hr_timer;


static inline bool
sassy_pacemaker_is_alive(struct sassy_pacemaker_info *spminfo)
{
	return spminfo->state == SASSY_PM_EMITTING;
}


static inline bool can_fire(uint64_t prev_time, uint64_t cur_time)
{
	return (cur_time - prev_time) >= CYCLES_PER_100MS;
}

const char *pm_state_string(sassy_pacemaker_state_t state)
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

void pm_state_transition_to(struct sassy_pacemaker_info *spminfo,
			    enum sassy_pacemaker_state state)
{
	sassy_dbg(" State Transition from %s to %s \n",
		  pm_state_string(spminfo->state), pm_state_string(state));
	spminfo->state = state;
}

static inline void sassy_setup_skbs(struct sassy_pacemaker_info *spminfo)
{
	int i;
	struct sassy_device *sdev =
		container_of(spminfo, struct sassy_device, pminfo);

	BUG_ON(spminfo->num_of_targets > MAX_REMOTE_SOURCES);

	for (i = 0; i < spminfo->num_of_targets; i++) {
		/* Setup SKB */
		spminfo->pm_targets[i].skb = sassy_setup_hb_packet(spminfo, i);
		skb_set_queue_mapping(
			spminfo->pm_targets[i].skb,
			smp_processor_id()); /* Use the queue active_cpu for sending the hb packet */
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

	memcpy(data_ptr, payload, SASSY_PAYLOAD_BYTES);
}

enum hrtimer_restart sassy_heart(struct hrtimer *timer_for_restart)
{
	uint64_t prev_time, cur_time;
	unsigned long flags;
	struct sassy_pacemaker_info *spminfo;
	struct sassy_device *sdev = (struct sassy_device *)data;
    struct net_device *ndev = sdev->ndev;
	void *pkt_payload;
	int i;
	int ret;
	int hb_active_ix;
	enum sassy_ts_state ts_state = sdev->ts_state;
  	ktime_t currtime , interval;

  	currtime  = ktime_get();
  	interval = ktime_set(0, 100000); 
  	hrtimer_forward(timer_for_restart, currtime , interval);

	sassy_dbg("Enter %s", __FUNCTION__);

	if (!sdev || !sdev->ndev) {
		sassy_error("netdevice is NULL\n");
		return HRTIMER_NORESTART;
	}

	spminfo = &(sdev->pminfo);

	if (!spminfo) {
		sassy_error("spminfo is NULL\n");
		return HRTIMER_NORESTART;
	}

	if (spminfo->num_of_targets <= 0) {
		sassy_error("num_of_targets is invalid\n");
		return HRTIMER_NORESTART;
	}

	sassy_setup_skbs(spminfo);

	pm_state_transition_to(spminfo, SASSY_PM_EMITTING);

	get_cpu(); /* disable preemption */

	local_irq_save(flags);
	local_bh_disable();

	/* If netdev is offline, then stop pacemaker */
	if (unlikely(!netif_running(ndev) ||
		     !netif_carrier_ok(ndev))) {
		sassy_pm_stop(spminfo);
		local_bh_enable();
		local_irq_restore(flags);
		return HRTIMER_NORESTART;
	}

	for (i = 0; i < spminfo->num_of_targets; i++) {
		
		// Always update payload to avoid jitter!
		hb_active_ix =
		     spminfo->pm_targets[i].pkt_data.hb_active_ix;
		pkt_payload =
		     spminfo->pm_targets[i].pkt_data.pkt_payload[hb_active_ix];

		//Direct Updates - No double buffer
        if (sdev->proto->ctrl_ops.us_update != NULL)
		 sdev->proto->ctrl_ops.us_update(sdev,
		 					pkt_payload);

		sassy_update_skb_payload(spminfo->pm_targets[i].skb,
					 pkt_payload);

		if (sdev->verbose)
		     print_hex_dump(KERN_DEBUG,
		 	       "Payload: ", DUMP_PREFIX_NONE,
		 		       16, 1, pkt_payload,
		 		       SASSY_PAYLOAD_BYTES, 0);
        
		sassy_send_hb(ndev, spminfo->pm_targets[i].skb);

        if(ts_state == SASSY_TS_RUNNING)
            sassy_write_timestamp(sdev, 0, rdtsc(), i);

	}
	local_bh_enable();
	local_irq_restore(flags);
	//}

	put_cpu();
	sassy_dbg(" leaving heart..\n");
	return HRTIMER_RESTART;
}

struct sk_buff *sassy_setup_hb_packet(struct sassy_pacemaker_info *spminfo,
				      int host_number)
{
	struct sassy_device *sdev =
		container_of(spminfo, struct sassy_device, pminfo);

	if (!spminfo) {
		sassy_error(
			"Could not setup skb, sassy_pacemaker_info is NULL\n");
		return NULL;
	}

	sassy_dbg("Composing skb.\n");

	return compose_heartbeat_skb(sdev->ndev, spminfo, host_number);
}

int sassy_pm_start(struct sassy_pacemaker_info *spminfo)
{
	struct cpumask mask;
	int active_cpu;
	struct sassy_device *sdev;
	ktime_t interval;

	sassy_dbg(" Start Heartbeat thread, %s\n", __FUNCTION__);

	if (!spminfo) {
		sassy_error("No Device. %s\n", __FUNCTION__);
		return -ENODEV;
	}

	if (spminfo->state != SASSY_PM_READY) {
		sassy_error(" Pacemaker is not in ready state! \n");
		return -EPERM;
	}

	sdev = container_of(spminfo, struct sassy_device, pminfo);

	if (!sdev) {
		sassy_error("No sdev \n");
		return -ENODEV;
	}

	if (!sdev->proto) {
		sassy_error("No Protocol is selected. Aborting.\n");
		return -EPERM;
	}

	if (spminfo->active_cpu > MAX_CPU_NUMBER || spminfo->active_cpu < 0) {
		sassy_error(
			" Invalid CPU Number. set via /proc/sassy/<devid>/active_cpu.\n");
		return -EINVAL;
	}

	active_cpu = spminfo->active_cpu;
	sassy_dbg("num of hb targets: %d", spminfo->num_of_targets);


	interval = ktime_set(0, 100000);

	hrtimer_init( &hr_timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	hr_timer.function = &sassy_heart;
 	hrtimer_start( &hr_timer, interval, HRTIMER_MODE_REL);



	/* cpumask_clear(&mask);
	heartbeat_task =
		kthread_create(&sassy_heart, sdev, "sassy Heartbeat thread");
	kthread_bind(heartbeat_task, active_cpu);

	if (IS_ERR(heartbeat_task)) {
		sassy_error(" Task Error. %s\n", __FUNCTION__);
		return -EINVAL;
	}
	sassy_dbg(" Start Thread now: %s\n", __FUNCTION__);
	wake_up_process(heartbeat_task);
	*/


	return 0;
}

int sassy_pm_stop(struct sassy_pacemaker_info *spminfo)
{
	pm_state_transition_to(spminfo, SASSY_PM_READY);
 	hrtimer_cancel(&hr_timer);
	return 0;
}

int sassy_pm_reset(struct sassy_pacemaker_info *spminfo)
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