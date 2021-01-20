
#define ASGARD_KERNEL_MODULE 1
#define ASGARD_DPDK 0


#include <linux/module.h>


#include "logger.h"
#include "module.h"

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Distributed Systems Group");
MODULE_DESCRIPTION("ASGARD Connection Core");
MODULE_VERSION("0.01");

#undef LOG_PREFIX
#define LOG_PREFIX "[ASGARD][LKM]"



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


static int __init asgard_connection_core_init(void)
{
    int i;
    int err = -EINVAL;
    int ret = 0;

    if (ifindex < 0) {
        asgard_error("ifindex parameter is missing\n");
        goto error;
    }

    err = register_asgard_at_nic(ifindex, asgard_post_ts,
                                 asgard_post_payload, asgard_force_quit);

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

    ret = asgard_core_register_nic(ifindex,
                                   get_asgard_id_by_ifindex(ifindex));

    if (ret < 0) {
        asgard_error("Could not register NIC at asgard\n");
        goto reg_failed;
    }

    //init_asgard_proto_info_interfaces();

    /* Allocate Workqueues */
    asgard_wq = alloc_workqueue("asgard",
                                WQ_HIGHPRI | WQ_CPU_INTENSIVE | WQ_UNBOUND |
                                WQ_MEM_RECLAIM | WQ_FREEZABLE,
                                1);
    asgard_wq_lock = 0;

    asgard_dbg("asgard core initialized.\n");
    return 0;
reg_failed:
    unregister_asgard();

    kfree(score->sdevices);
    kfree(score);
    remove_proc_entry("asgard", NULL);

error:
    asgard_error("Could not initialize asgard - aborting init.\n");
    return err;
}


static void __exit asgard_connection_core_exit(void)
{
    int i, j;

    asgard_wq_lock = 1;

    mb();

    if (asgard_wq)
        flush_workqueue(asgard_wq);

    /* MUST unregister asgard for drivers first */
    unregister_asgard();

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


