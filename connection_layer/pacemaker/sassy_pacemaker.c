#include <sassy/logger.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/kthread.h>
#include <linux/sched.h>

#include <linux/netdevice.h>
#include <linux/skbuff.h>
#include <linux/udp.h>
#include <linux/etherdevice.h>
#include <linux/ip.h>
#include <net/ip.h>

#include <sassy/sassy_pm.h>
#include <sassy/sassy_net.h>
#include <sassy/sassy_dev.h>
#include <sassy/sassy_hb.h>
#include <sassy/logger.h>


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
    sassy_dbg("State Transition from %s to %s \n", pm_state_string(spminfo->state), pm_state_string(state));
    spminfo->state = state;
}


int sassy_heart(void *data)
{
    uint64_t prev_time, cur_time;
    unsigned long flags;
    struct sassy_pacemaker_info *spminfo = (struct sassy_pacemaker_info *)data;

    sassy_dbg("Enter %s", __FUNCTION__);
    
    pm_state_transition_to(spminfo, SASSY_PM_EMITTING);

    sassy_setup_skbs(spminfo);

    get_cpu();                      /* disable preemption */
    local_irq_save(flags);          /* Disable hard interrupts on the local CPU */
    
    prev_time = rdtsc();

    local_bh_disable();
    while (sassy_pacemaker_is_alive(spminfo)) {

        cur_time = rdtsc();

        /* Loop until pacemaker can fire*/
        if (!can_fire(prev_time, cur_time))
            continue;

        prev_time = cur_time;

        sassy_send_all_heartbeats(spminfo);
    }

    local_bh_enable();

    sassy_dbg(" Exit Heartbeat at device");
    local_irq_restore(flags);
    put_cpu();
    sassy_dbg(" leaving heart..\n");
    return 0;
}

void sassy_send_all_heartbeats(struct sassy_pacemaker_info *spminfo) {
    int i;
    unsigned char* tail_ptr;
    unsigned char* data_ptr;
    int err;
    uint64_t ts1, ts2;
    int counter = 0; /* update counter for each hb destination - just for testing.. */

    for(i = 0; i < spminfo->num_of_targets; i++) {
        tail_ptr = skb_tail_pointer(tdata[i].skb);
        data_ptr = (tail_ptr - sizeof(struct sassy_heartbeat_packet));
        skb_get(tdata[i].skb);
        //skb->tstamp = current_time;

        data_ptr[0] = (counter >> 24) & 0xFF;
        data_ptr[1] = (counter >> 16) & 0xFF;
        data_ptr[2] = (counter >> 8) & 0xFF;
        data_ptr[3] = counter & 0xFF;
        counter = counter + 1;

        HARD_TX_LOCK(ndev, tdata[i].txq, smp_processor_id());
        if (unlikely(netif_xmit_frozen_or_drv_stopped(tdata[i].txq))) {
            err = NETDEV_TX_BUSY;
            sassy_error("Device Busy unlocking.\n");
            ts1 = rdtsc();
            goto unlock;
        }
        ts1 = rdtsc();
        err = netdev_start_xmit(tdata[i].skb, ndev, tdata[i].txq, 0);
unlock:
        ts2 = rdtsc();
        HARD_TX_UNLOCK(ndev, tdata[i].txq);
    }
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

    return compose_heartbeat_skb(ndev, spminfo->targets[host_number].dst_mac, spminfo->targets[host_number].dst_ip);
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
