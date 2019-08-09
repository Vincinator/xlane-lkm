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

static inline bool
sassy_pacemaker_is_alive(struct pminfo *spminfo)
{
	return spminfo->state == SASSY_PM_EMITTING;
}


static inline bool can_fire(uint64_t prev_time, uint64_t cur_time)
{
	return (cur_time - prev_time) >= CYCLES_PER_100MS;
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
	sassy_dbg("State Transition from %s to %s\n",
		  pm_state_string(spminfo->state), pm_state_string(state));
	spminfo->state = state;
}

static inline void sassy_setup_skbs(struct pminfo *spminfo)
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
			smp_processor_id());
	}
}

static inline void sassy_send_hb(struct net_device *ndev, struct sk_buff *skb)
{
	int ret;
	const struct netdev_queue *txq;

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

static inline int _emit_pkts(struct sassy_device *sdev,
		struct pminfo *spminfo)
{
	void *pkt_payload;
	int i;
	int ret;
	int hb_active_ix;
	struct net_device *ndev = sdev->ndev;
	enum tsstate ts_state = sdev->ts_state;

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
		if (sdev->proto->ctrl_ops.us_update != NULL)
			sdev->proto->ctrl_ops.us_update(sdev, pkt_payload);

		sassy_update_skb_payload(spminfo->pm_targets[i].skb,
					 pkt_payload);

		if (sdev->verbose)
			print_hex_dump(KERN_DEBUG,
				"Payload: ", DUMP_PREFIX_NONE,
				16, 1, pkt_payload, SASSY_PAYLOAD_BYTES, 0);

		sassy_send_hb(ndev, spminfo->pm_targets[i].skb);

		if (ts_state == SASSY_TS_RUNNING) {
			sassy_write_timestamp(sdev, 0, rdtsc(), i);
			sassy_write_timestamp(sdev, 4, ktime_get(), i);
		}

	}
	return 0;
}

static inline _validate_pm(struct sassy_device *sdev, struct pminfo *spminfo)
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

	if (!sdev->proto) {
		sassy_error("No Protocol is selected. Aborting.\n");
		return -EPERM;
	}

	if (!sdev || !sdev->ndev) {
		sassy_error("netdevice is NULL\n");
		return -EINVAL;
	}

	if (spminfo->num_of_targets <= 0) {
		sassy_error("num_of_targets is invalid\n");
		return -EINVAL;
	}

	return 0;
}
int sassy_pm_loop(void *data)
{
	uint64_t prev_time, cur_time;
	unsigned long flags;
	struct sassy_device *sdev = (struct sassy_device *) data;
	struct pminfo *spminfo = &sdev->pminfo;

	void *pkt_payload;
	int i;
	int ret;
	int hb_active_ix;
	ktime_t currtime, interval;
	int err;

	sassy_setup_skbs(spminfo);

	pm_state_transition_to(spminfo, SASSY_PM_EMITTING);

	get_cpu(); /* disable preemption */

	prev_time = rdtsc();

	while (sassy_pacemaker_is_alive(spminfo)) {
		cur_time = rdtsc();

		if (!can_fire(prev_time, cur_time))
			continue;

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
	}
	put_cpu();
	sassy_dbg(" leaving heart..\n");
	return HRTIMER_RESTART;
}

enum hrtimer_restart sassy_pm_timer(struct hrtimer *timer)
{
	unsigned long flags;
	struct pminfo *spminfo =
			container_of(timer, struct pminfo, pm_timer);

	struct sassy_device *sdev =
			container_of(spminfo, struct sassy_device, pminfo);

	void *pkt_payload;
	int i;
	int ret;
	int hb_active_ix;
	ktime_t currtime, interval;

	if (!sassy_pacemaker_is_alive(spminfo))
		return HRTIMER_NORESTART;

	currtime  = ktime_get();
	interval = ktime_set(0, 100000000);
	hrtimer_forward(timer, currtime, interval);

	sassy_setup_skbs(spminfo);

	get_cpu(); /* disable preemption */

	local_irq_save(flags);
	local_bh_disable();

	_emit_pkts(sdev, spminfo);

	local_bh_enable();
	local_irq_restore(flags);

	put_cpu();
	return HRTIMER_RESTART;
}

struct sk_buff *sassy_setup_hb_packet(struct pminfo *si,
				 int host_number)
{
	struct sassy_device *sdev =
		container_of(si, struct sassy_device, pminfo);

	if (!si) {
		sassy_error(
			"Could not setup skb, pminfo is NULL\n");
		return NULL;
	}

	sassy_dbg("Composing skb.\n");

	return compose_heartbeat_skb(sdev->ndev, si, host_number);
}

int sassy_pm_start_timer(void *data)
{
	struct pminfo *spminfo =
		(struct pminfo *) data;
	struct sassy_device *sdev =
		container_of(spminfo, struct sassy_device, pminfo);
	ktime_t interval;
	struct cpumask mask;
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
	ktime_t interval;
	int err;

	err = _validate_pm(sdev, spminfo);

	if (err)
		return err;

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
