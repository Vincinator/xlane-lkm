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


#include <sassy/sassy.h>

#undef LOG_PREFIX
#define LOG_PREFIX "[SASSY][PACEMAKER]"



struct target_data {
    struct sk_buff *skb;
    struct netdev_queue *txq;
};

struct target_data tdata[MAX_REMOTE_SOURCES]; /* pre-packed heartbeat messages for all target hosts */

struct net_device *ndev;


static inline bool sassy_pacemaker_is_alive(struct sassy_pacemaker_info *spminfo)
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
    case SASSY_PM_UNINIT: return "SASSY_PM_UNINIT";
    case SASSY_PM_READY: return "SASSY_PM_READY";
    case SASSY_PM_EMITTING: return "SASSY_PM_EMITTING";
    default: return "UNKNOWN STATE";
    }
}

void pm_state_transition_to(struct sassy_pacemaker_info *spminfo, enum sassy_pacemaker_state state)
{
    sassy_dbg(" State Transition from %s to %s \n", pm_state_string(spminfo->state), pm_state_string(state));
    spminfo->state = state;
}

static inline void sassy_send_all_heartbeats(struct sassy_pacemaker_info *spminfo) {
    int i;
    int err;
    uint64_t ts1, ts2;

   

}

int sassy_heart(void *data)
{
    uint64_t prev_time, cur_time;
    unsigned long flags;
    struct sassy_pacemaker_info *spminfo = (struct sassy_pacemaker_info *)data;
    int i;

    sassy_dbg("Enter %s", __FUNCTION__);
    
    pm_state_transition_to(spminfo, SASSY_PM_EMITTING);

    sassy_setup_skbs(spminfo);

    //get_cpu();                      /* disable preemption */
    //local_irq_save(flags);          /* Disable hard interrupts on the local CPU */
    
    prev_time = rdtsc();

    while (sassy_pacemaker_is_alive(spminfo)) {

        cur_time = rdtsc();

        /* Loop until pacemaker can fire*/
        if (!can_fire(prev_time, cur_time))
            continue;

        prev_time = cur_time;

        for(i = 0; i < spminfo->num_of_targets; i++) {
            local_bh_disable();
            HARD_TX_LOCK(ndev, tdata[i].txq, smp_processor_id());

            if (unlikely(netif_xmit_frozen_or_drv_stopped(tdata[i].txq))) {
                sassy_error("Device Busy unlocking.\n");
                goto unlock;
            }
            netdev_start_xmit(tdata[i].skb, ndev, tdata[i].txq, 0);
        unlock:
            HARD_TX_UNLOCK(ndev, tdata[i].txq);
            local_bh_enable();
        }
    }
    sassy_dbg(" exit loop\n");

    sassy_dbg(" Exit Heartbeat at device\n");
    //local_irq_restore(flags);
    //put_cpu();
    sassy_dbg(" leaving heart..\n");
    return 0;
}



void sassy_setup_skbs(struct sassy_pacemaker_info *spminfo) {
    int i;

    BUG_ON(spminfo->num_of_targets > MAX_REMOTE_SOURCES);

    for(i = 0; i < spminfo->num_of_targets; i++) {
        /* Setup SKB */
        tdata[i].skb = sassy_setup_hb_packet(spminfo, i);
        skb_set_queue_mapping(tdata[i].skb, smp_processor_id()); /* Use the queue active_cpu for sending the hb packet */
        tdata[i].txq = skb_get_tx_queue(ndev, tdata[i].skb); 
    }
}

struct sk_buff *sassy_setup_hb_packet(struct sassy_pacemaker_info *spminfo, int host_number)
{
    struct sassy_device *sdev = container_of(spminfo, struct sassy_device, pminfo);

    if (!spminfo) {
        sassy_error("Could not setup skb, sassy_pacemaker_info is NULL\n");
        return NULL;
    }

    ndev = first_net_device(&init_net);

    /* Find ifindex of NIX to use to send heartbeats */
    while (ndev != NULL) {
        if (ndev->ifindex == sdev->ifindex)
            break;
        ndev = next_net_device(ndev);
    }

    if (!ndev) {
        sassy_error("Netdevice is NULL %s\n", __FUNCTION__);
        return NULL;
    }

    sassy_dbg("Composing skb.\n");

    return compose_heartbeat_skb(ndev, spminfo, host_number);
}

int sassy_pm_start(struct sassy_pacemaker_info *spminfo)
{
    struct task_struct *heartbeat_task;
    struct cpumask mask;
    int active_cpu;

    sassy_dbg(" Start Heartbeat thread, %s\n", __FUNCTION__);

    if (!spminfo) {
        sassy_error("No Device. %s\n", __FUNCTION__);
        return -ENODEV;
    }

    if (spminfo->active_cpu > MAX_CPU_NUMBER || spminfo->active_cpu < 0) {
        sassy_error(" Invalid CPU Number. set via /proc/sassy/<devid>/active_cpu.\n");
        return -EINVAL;
    }

    active_cpu = spminfo->active_cpu;
    sassy_dbg("num of hb targets: %d", spminfo->num_of_targets);

    cpumask_clear(&mask);
    heartbeat_task = kthread_create(&sassy_heart, spminfo, "sassy Heartbeat thread");
    kthread_bind(heartbeat_task, active_cpu);

    if (IS_ERR(heartbeat_task)) {
        sassy_error(" Task Error. %s\n", __FUNCTION__);
        return -EINVAL;
    }
    sassy_dbg(" Start Thread now: %s\n", __FUNCTION__);
    wake_up_process(heartbeat_task);
    return 0;
}

int sassy_pm_stop(struct sassy_pacemaker_info *spminfo)
{
    pm_state_transition_to(spminfo, SASSY_PM_READY);
    return 0;
}

int sassy_pm_reset(struct sassy_pacemaker_info *spminfo) 
{
    sassy_dbg("Reset Pacemaker Configuration\n");

    if (!spminfo) {
        sassy_error("No Device. %s\n", __FUNCTION__);
        return -ENODEV;
    }

    /* no need to overwrite old values*/
    spminfo->num_of_targets = 0;

    return 0;

}