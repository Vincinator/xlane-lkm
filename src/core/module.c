
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/proc_fs.h>
#include <linux/etherdevice.h>
#include <linux/ip.h>
#include <net/ip.h>
#include <linux/netdevice.h>
#include <asm/checksum.h>
#include <linux/skbuff.h>
#include <linux/notifier.h>
#include <linux/hrtimer.h>
#include <linux/ktime.h>
#include <linux/slab.h>
#include <linux/inetdevice.h>
#include <linux/kernel.h>
#include <linux/compiler.h>


#include "module.h"


#include "logger.h"
#include "echo.h"
#include "../lkm/synbuf-chardev.h"
#include "../lkm/pm-ctrl.h"
#include "../lkm/kernel_ts.h"
#include "../lkm/core-ctrl.h"
#include "../lkm/proto-instance-ctrl.h"
#include "../lkm/multicast-ctrl.h"

#include "replication.h"
#include "membership.h"
#include "pktqueue.h"

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Distributed Systems Group");
MODULE_DESCRIPTION("ASGARD Connection Core");
MODULE_VERSION("0.01");

#undef LOG_PREFIX
#define LOG_PREFIX "[ASGARD][LKM]"


static int ifindex = -1;

module_param(ifindex, int, 0660);




static struct workqueue_struct *asgard_wq;

static int asgard_wq_lock = 0;

static struct asgard_core *score;


struct asgard_device *get_sdev(int devid)
{
    if (unlikely(devid < 0 || devid > MAX_NIC_DEVICES)) {
        asgard_error(" invalid asgard device id\n");
        return NULL;
    }

    if (unlikely(!score)) {
        asgard_error(" asgard core is not initialized\n");
        return NULL;
    }

    return score->sdevices[devid];
}
EXPORT_SYMBOL(get_sdev);




void asgard_post_ts(int asgard_id, uint64_t cycles, int ctype)
{
    struct asgard_device *sdev = get_sdev(asgard_id);
    struct echo_priv *epriv;
    //if (sdev->ts_state == ASGARD_TS_RUNNING)
    //	asgard_write_timestamp(sdev, 1, cycles, asgard_id);

    if (ctype == 2) { // channel type 2 is leader channel
        sdev->last_leader_ts = cycles;
        if (sdev->cur_leader_lid != -1) {
            sdev->pminfo.pm_targets[sdev->cur_leader_lid].chb_ts =
                    cycles;
            sdev->pminfo.pm_targets[sdev->cur_leader_lid].alive = 1;
        }
    } else if (ctype == 3) { /* Channel Type is echo channel*/
        // asgard_dbg("optimistical timestamp on channel %d received\n", ctype);

        epriv = (struct echo_priv *)sdev->echo_priv;

        if (epriv != NULL) {
            epriv->ins->ctrl_ops.post_ts(epriv->ins, NULL, cycles);
        }
    }
}

int asgard_core_register_remote_host(int asgard_id, u32 ip, char *mac,
                                     int protocol_id, int cluster_id)
{
    struct asgard_device *sdev = get_sdev(asgard_id);
    struct asgard_pm_target_info *pmtarget;

    if (!mac) {
        asgard_error("input mac is NULL!\n");
        return -1;
    }

    if (sdev->pminfo.num_of_targets >= MAX_REMOTE_SOURCES) {
        asgard_error("Reached Limit of remote hosts.\n");
        asgard_error("Limit is=%d\n", MAX_REMOTE_SOURCES);
        return -1;
    }

    pmtarget = &sdev->pminfo.pm_targets[sdev->pminfo.num_of_targets];

    if (!pmtarget) {
        asgard_error("Pacemaker target is NULL\n");
        return -1;
    }
    pmtarget->alive = 0;
    pmtarget->pkt_data.naddr.dst_ip = ip;
    pmtarget->pkt_data.naddr.cluster_id = cluster_id;
    pmtarget->pkt_data.naddr.port = 3320;
    pmtarget->lhb_ts = 0;
    pmtarget->chb_ts = 0;
    pmtarget->resp_factor = 4;
    pmtarget->cur_waiting_interval = 2;
    pmtarget->pkt_tx_counter = 0;
    pmtarget->pkt_rx_counter = 0;
    pmtarget->pkt_tx_errors = 0;

    pmtarget->scheduled_log_replications = 0;
    pmtarget->received_log_replications = 0;

    pmtarget->pkt_data.payload =
            kzalloc(sizeof(struct asgard_payload), GFP_KERNEL);

    spin_lock_init(&pmtarget->pkt_data.slock);

    memcpy(&pmtarget->pkt_data.naddr.dst_mac, mac,
           sizeof(unsigned char) * 6);

    /* Local ID is increasing with the number of targets */
    add_cluster_member(sdev->ci, cluster_id, sdev->pminfo.num_of_targets,
                       2);

    pmtarget->aapriv =
            kmalloc(sizeof(struct asgard_async_queue_priv), GFP_KERNEL);
    init_asgard_async_queue(pmtarget->aapriv);

    /* Out of schedule SKB  pre-allocation*/
    sdev->pminfo.pm_targets[sdev->pminfo.num_of_targets].pkt_data.skb =
            asgard_reserve_skb(sdev->ndev, ip, mac, NULL);
    skb_set_queue_mapping(
            sdev->pminfo.pm_targets[sdev->pminfo.num_of_targets]
                    .pkt_data.skb,
            sdev->pminfo.active_cpu); // Queue mapping same for each target i

    sdev->pminfo.num_of_targets = sdev->pminfo.num_of_targets + 1;
    asgard_error("Registered Cluster Member with ID %d\n", cluster_id);

    return 0;
}
EXPORT_SYMBOL(asgard_core_register_remote_host);


void asg_init_workqueues(struct asgard_device *sdev){
    /* Allocate Workqueues */
    sdev->asgard_wq = alloc_workqueue("asgard",
                                      WQ_HIGHPRI | WQ_CPU_INTENSIVE | WQ_UNBOUND |
                                      WQ_MEM_RECLAIM | WQ_FREEZABLE,
                                      1);
    sdev->asgard_wq_lock = 0;

}
EXPORT_SYMBOL(asg_init_workqueues);

static int __init asgard_connection_core_init(void)
{
    int i;
    int err = -EINVAL;
    int ret = 0;

    if (ifindex < 0) {
        asgard_error("ifindex parameter is missing\n");
        goto error;
    }

#if ASGARD_REAL_TEST
    err = register_asgard_at_nic(ifindex, asgard_post_ts,
                                 asgard_post_payload, asgard_force_quit);
#endif

    if (err)
        goto error;

    /* Initialize User Space interfaces
     * NOTE: BEFORE call to asgard_core_register_nic! */
    err = synbuf_bypass_init_class();

    if (err) {
        asgard_error("synbuf_bypass_init_class failed\n");
        return -ENODEV;
    }

    // freed by asgard_connection_core_exit
    score = kmalloc(sizeof(struct asgard_core), GFP_KERNEL);

    if (!score) {
        asgard_error("allocation of asgard core failed\n");
        return -1;
    }

    score->num_devices = 0;

    // freed by asgard_connection_core_exit
    score->sdevices = kmalloc_array(MAX_NIC_DEVICES, sizeof(struct asgard_device *), GFP_KERNEL);

    if (!score->sdevices) {
        asgard_error("allocation of score->sdevices failed\n");
        return -1;
    }

    for (i = 0; i < MAX_NIC_DEVICES; i++)
        score->sdevices[i] = NULL;

    proc_mkdir("asgard", NULL);

//    ret = asgard_core_register_nic(ifindex,
//                                   get_asgard_id_by_ifindex(ifindex));

    if (ret < 0) {
        asgard_error("Could not register NIC at asgard\n");
        goto reg_failed;
    }

    //init_asgard_proto_info_interfaces();



    asgard_dbg("asgard core initialized.\n");
    return 0;
reg_failed:
    // unregister_asgard();

    kfree(score->sdevices);
    kfree(score);
    remove_proc_entry("asgard", NULL);

error:
    asgard_error("Could not initialize asgard - aborting init.\n");
    return err;
}


void asgard_stop_pacemaker(struct asgard_device *adev)
{
    if (!adev) {
        asgard_error("asgard device is NULL\n");
        return;
    }

    adev->pminfo.state = ASGARD_PM_READY;
}

void asgard_stop_timestamping(struct asgard_device *adev)
{
    if (!adev) {
        asgard_error("Asgard Device is Null.\n");
        return;
    }

    asgard_ts_stop(adev);
}

int asgard_validate_asgard_device(int asgard_id)
{
    if (!score) {
        asgard_error("score is NULL!\n");
        return -1;
    }
    if (asgard_id < 0 || asgard_id > MAX_NIC_DEVICES) {
        asgard_error("invalid asgard_id! %d\n", asgard_id);
        return -1;
    }
    if (!score->sdevices || !score->sdevices[asgard_id]) {
        asgard_error("sdevices is invalid!\n");
        return -1;
    }

    return 0;
}
EXPORT_SYMBOL(asgard_validate_asgard_device);

void asgard_stop(int asgard_id)
{
    if (asgard_validate_asgard_device(asgard_id)) {
        asgard_dbg("invalid asgard id %d", asgard_id);
        return;
    }

    asgard_stop_timestamping(score->sdevices[asgard_id]);

    asgard_stop_pacemaker(score->sdevices[asgard_id]);
}

int asgard_core_remove_nic(int asgard_id)
{
    char name_buf[MAX_ASGARD_PROC_NAME];

    if (asgard_validate_asgard_device(asgard_id))
        return -1;

    asgard_clean_timestamping(score->sdevices[asgard_id]);

    /* Remove Ctrl Interfaces for NIC */
    clean_asgard_pm_ctrl_interfaces(score->sdevices[asgard_id]);
    clean_asgard_ctrl_interfaces(score->sdevices[asgard_id]);

    remove_proto_instance_ctrl(score->sdevices[asgard_id]);

    //remove_logger_ifaces(&score->sdevices[asgard_id]->le_logger);

    /* Clean Multicast*/
    remove_multicast(score->sdevices[asgard_id]);

    snprintf(name_buf, sizeof(name_buf), "asgard/%d",
             score->sdevices[asgard_id]->ifindex);

    remove_proc_entry(name_buf, NULL);

    /* Clean Cluster Membership Synbuf*/
    synbuf_chardev_exit(score->sdevices[asgard_id]->synbuf_clustermem);

    return 0;
}
EXPORT_SYMBOL(asgard_core_remove_nic);


void clear_protocol_instances(struct asgard_device *sdev)
{
    int i;

    if (!sdev) {
        asgard_error("SDEV is NULL - can not clear instances.\n");
        return;
    }

    if (sdev->num_of_proto_instances > MAX_PROTO_INSTANCES) {
        asgard_dbg(
                "num_of_proto_instances is faulty! Aborting cleanup of all instances\n");
        return;
    }

    // If pacemaker is running, do not clear the protocols!
    if (sdev->pminfo.state == ASGARD_PM_EMITTING) {
        return;
    }

    for (i = 0; i < sdev->num_of_proto_instances; i++) {
        if (!sdev->protos[i])
            continue;

        if (sdev->protos[i]->ctrl_ops.clean != NULL) {
            sdev->protos[i]->ctrl_ops.clean(sdev->protos[i]);
        }

        // timer are not finished yet!?
        if (sdev->protos[i]->proto_data)
            kfree(sdev->protos[i]->proto_data);

        if (!sdev->protos[i])
            kfree(sdev->protos[i]); // is non-NULL, see continue condition above
    }

    for (i = 0; i < MAX_PROTO_INSTANCES; i++)
        sdev->instance_id_mapping[i] = -1;
    sdev->num_of_proto_instances = 0;
}


static void __exit asgard_connection_core_exit(void)
{
    int i, j;

    asgard_wq_lock = 1;

    mb();

    if (asgard_wq)
        flush_workqueue(asgard_wq);

    /* MUST unregister asgard for drivers first */
//    unregister_asgard();

    if (!score) {
        asgard_error("score is NULL \n");
        return;
    }

    for (i = 0; i < MAX_NIC_DEVICES; i++) {
        if (!score->sdevices[i]) {
            continue;
        }

        asgard_stop(i);

        // clear all async queues
        for (j = 0; j < score->sdevices[i]->pminfo.num_of_targets; j++)
            async_clear_queue(
                    score->sdevices[i]->pminfo.pm_targets[j].aapriv);

        destroy_workqueue(score->sdevices[i]->asgard_leader_wq);
        destroy_workqueue(score->sdevices[i]->asgard_ringbuf_reader_wq);

        clear_protocol_instances(score->sdevices[i]);

        asgard_core_remove_nic(i);

        kfree(score->sdevices[i]);
    }
    if (asgard_wq)
        destroy_workqueue(asgard_wq);

    if (score->sdevices)
        kfree(score->sdevices);

    if (score)
        kfree(score);

    remove_proc_entry("asgard", NULL);

    synbuf_clean_class();
    asgard_dbg("ASGARD CORE CLEANED \n\n\n\n");
    // flush_workqueue(asgard_wq);
}




module_init(asgard_connection_core_init);
module_exit(asgard_connection_core_exit);


