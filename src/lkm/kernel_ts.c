//
// Created by vincent on 1/20/21.
//

#include "../core/module.h"
#include "kernel_ts.h"

#include <linux/ip.h>
#include <linux/netdevice.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <net/ip.h>
#include <net/net_namespace.h>
#include <net/udp.h>



const char *ts_state_string(enum tsstate state)
{
    switch (state) {
        case ASGARD_TS_RUNNING:
            return "ASGARD_TS_RUNNING";
        case ASGARD_TS_READY:
            return "ASGARD_TS_READY";
        case ASGARD_TS_UNINIT:
            return "ASGARD_TS_UNINIT";
        case ASGARD_TS_LOG_FULL:
            return "ASGARD_TS_LOG_FULL";
        default:
            return "UNKNOWN STATE";
    }
}

void ts_state_transition_to(struct asgard_device *sdev,
                            enum tsstate state)
{
    sdev->ts_state = state;
}

static ssize_t asgard_proc_write(struct file *file, const char __user *buffer,
size_t count, loff_t *data)
{
/* Do nothing on write. */
return count;
}

static int asgard_proc_show(struct seq_file *m, void *v)
{
    int i;
    struct asgard_timestamp_logs *logs =
            (struct asgard_timestamp_logs *)m->private;

    BUG_ON(!logs);

    for (i = 0; i < logs->current_timestamps; i++)
        seq_printf(m, "%llu, %d\n",
                   logs->timestamp_items[i].timestamp_tcs,
                   logs->timestamp_items[i].target_id);

    return 0;
}

static int asgard_proc_open(struct inode *inode, struct file *file)
{
    return single_open(file, asgard_proc_show, PDE_DATA(inode));
}

static const struct proc_ops asgard_procfs_ops = {
        .proc_open = asgard_proc_open,
        .proc_write = asgard_proc_write,
        .proc_read = seq_read,
        .proc_lseek = seq_lseek,
        .proc_release = single_release,
};

/* Get the corresponding array for type and calls write_to_logmem. */
int asgard_write_timestamp(struct asgard_device *sdev,
                           int logid, uint64_t cycles,
                           int target_id)
{
    struct asgard_timestamp_logs *logs;


    if (!sdev||!sdev->stats||!sdev->stats->timestamp_logs[logid]) {
        asgard_dbg("Nullptr error in %s\n", __func__);
        return 0;
    }

    logs = sdev->stats->timestamp_logs[logid];

    if (unlikely(logs->current_timestamps > TIMESTAMP_ARRAY_LIMIT)) {

        asgard_dbg("Logs are full! Stopped tracking.%s\n",
                   __func__);

        asgard_ts_stop(sdev);
        ts_state_transition_to(sdev, ASGARD_TS_LOG_FULL);
        return -ENOMEM;
    }

    logs->timestamp_items[logs->current_timestamps].timestamp_tcs = cycles;
    logs->timestamp_items[logs->current_timestamps].target_id = target_id;
    logs->current_timestamps += 1;

    return 0;
}
EXPORT_SYMBOL(asgard_write_timestamp);

int asgard_ts_stop(struct asgard_device *sdev)
{
    if (sdev->ts_state != ASGARD_TS_RUNNING)
        return -EPERM;

    ts_state_transition_to(sdev, ASGARD_TS_READY);
    return 0;
}

int asgard_ts_start(struct asgard_device *sdev)
{

    if (sdev->ts_state != ASGARD_TS_READY) {
        asgard_error(" asgard is not in ready state. %s\n", __func__);
        goto error;
    }

    ts_state_transition_to(sdev, ASGARD_TS_RUNNING);
    return 0;

    error:
    return -EPERM;
}

int asgard_reset_stats(struct asgard_device *sdev)
{
    int err;
    int i;

    if (sdev == NULL||sdev->stats == NULL) {
        asgard_error(
                "can not clear stats, nullptr error.%s\n",
                __func__);
        err = -EINVAL;
        goto error;
    }

    if (sdev->ts_state == ASGARD_TS_RUNNING) {
        asgard_error(
                " can not clear stats when timestamping is active.%s\n",
                __func__);
        err = -EPERM;
        goto error;
    }

    if (sdev->ts_state != ASGARD_TS_READY) {
        asgard_error(
                "can not clear stats, asgard timestamping is in an undefined state.%s\n",
                __func__);
        err = -EPERM;
        goto error;
    }

    /* Reset, not free so timestamping can continue directly.*/
    for (i = 0; i < sdev->stats->timestamp_amount; i++) {
        if (!sdev->stats->timestamp_logs[i]) {
            asgard_error( "BUG! timestamp_log index does not exist. %s\n",__func__);
            err = -EPERM;
            goto error;
        } else {
            sdev->stats->timestamp_logs[i]->current_timestamps = 0;
        }

    }

    return 0;
    error:
    asgard_error(" error code: %d for %s\n", err, __func__);
    return err;
}

int asgard_clean_timestamping(struct asgard_device *sdev)
{
    int err;
    int i;
    char name_buf[MAX_ASGARD_PROC_NAME];
    int log_types = ASGARD_NUM_TS_LOG_TYPES;

    if (!sdev->stats) {
        err = -EINVAL;
        asgard_error(" stats for active device is not valid %s\n",
                     __func__);
        goto error;
    }

    for (i = 0; i < sdev->stats->timestamp_amount; i++) {
        if (!sdev->stats->timestamp_logs[i])
            goto error;

        snprintf(name_buf, sizeof(name_buf), "asgard/%d/ts/data/%d",
                 sdev->ifindex, i);

        remove_proc_entry(name_buf, NULL);
    }

    snprintf(name_buf, sizeof(name_buf), "asgard/%d/ts/data", sdev->ifindex);
    remove_proc_entry(name_buf, NULL);

    snprintf(name_buf, sizeof(name_buf), "asgard/%d/ts/ctrl", sdev->ifindex);
    remove_proc_entry(name_buf, NULL);

    snprintf(name_buf, sizeof(name_buf), "asgard/%d/ts", sdev->ifindex);
    remove_proc_entry(name_buf, NULL);

    for (i = 0; i < log_types; i++) {
        if(!sdev->stats->timestamp_logs[i])
            continue;
        //kfree(sdev->stats->timestamp_logs[i]->timestamp_items);
        kfree(sdev->stats->timestamp_logs[i]);
    }

    kfree(sdev->stats->timestamp_logs);
    kfree(sdev->stats);

    ts_state_transition_to(sdev, ASGARD_TS_UNINIT);
    return 0;
    error:
    asgard_error(" error code: %d for %s\n", err, __func__);
    return err;
}
EXPORT_SYMBOL(asgard_clean_timestamping);

static int init_log_ctrl(struct asgard_device *sdev, int logid)
{
    int err;
    char name_buf[MAX_ASGARD_PROC_NAME];

    if (sdev->verbose)
        asgard_dbg(" Init TS log with id: %d\n", logid);

    if (!sdev->stats) {
        err = -ENOMEM;
        asgard_error(" Stats is not initialized!%s\n", __func__);
        goto error;
    }

    /* Generate device specific proc name for asgard stats */
    snprintf(name_buf, sizeof(name_buf), "asgard/%d/ts/data/%d",
             sdev->ifindex, logid);

    sdev->stats->timestamp_logs[logid]->proc_dir =
            proc_create_data(name_buf, S_IRUSR | S_IROTH, NULL,
                             &asgard_procfs_ops,
                             sdev->stats->timestamp_logs[logid]);

    if (!sdev->stats->timestamp_logs[logid]->proc_dir) {
        err = -ENOMEM;
        asgard_error(
                " Could not create timestamps  procfs data entry%s\n",
                __func__);
        goto error;
    }
    //asgard_dbg(" Created %d procfs %s\n",  logid, __func__);
    return 0;

    error:
    asgard_error(" error code: %d for %s\n", err, __func__);
    return err;
}


int init_timestamping(struct asgard_device *sdev)
{
    int err;
    int log_types = ASGARD_NUM_TS_LOG_TYPES;
    int i;

    if (sdev->verbose)
        asgard_dbg(" asgard device setup started %s\n", __func__);

    if (!sdev) {
        err = -EINVAL;
        asgard_error(" asgard device is NULL %s\n", __func__);
        goto error;
    }

    // freed by asgard_clean_timestamping
    sdev->stats = kmalloc(sizeof(const struct asgard_stats), GFP_KERNEL);
    if (!sdev->stats) {
        err = -ENOMEM;
        asgard_error(" Could not allocate memory for stats.%s\n",
                     __func__);
        goto error;
    }


    // freed by asgard_clean_timestamping
    sdev->stats->timestamp_logs =
            kmalloc_array(log_types,
                          sizeof(struct asgard_timestamp_logs *),
                          GFP_KERNEL);
    if (!sdev->stats->timestamp_logs) {
        err = -ENOMEM;
        asgard_error(
                " Could not allocate memory for timestamp_logs pointer.%s\n",
                __func__);
        goto error;
    }


    sdev->stats->timestamp_amount = 0;

    for (i = 0; i < log_types; i++) {
        // freed by asgard_clean_timestamping
        sdev->stats->timestamp_logs[i] = kmalloc(
                sizeof(struct asgard_timestamp_logs), GFP_KERNEL);

        if (!sdev->stats->timestamp_logs[i]) {
            err = -ENOMEM;
            asgard_error(
                    "Could not allocate memory for timestamp logs struct with logid %d\n",
                    i);
            goto error;
        }

        sdev->stats->timestamp_amount++;


        // freed by asgard_clean_timestamping
        sdev->stats->timestamp_logs[i]->timestamp_items =
                kmalloc_array(TIMESTAMP_ARRAY_LIMIT,
                              sizeof(const struct asgard_timestamp_item),
                              GFP_KERNEL);

        if (!sdev->stats->timestamp_logs[i]->timestamp_items) {
            err = -ENOMEM;
            asgard_error(
                    " Could not allocate memory for timestamp logs with logid: %d.\n",
                    i);
            goto error;
        }

        sdev->stats->timestamp_logs[i]->current_timestamps = 0;

        init_log_ctrl(sdev, i);
    }

    ts_state_transition_to(sdev, ASGARD_TS_READY);

    return 0;

    error:
    asgard_error(" Error code: %d for %s\n", err, __func__);
    if (sdev && sdev->stats) {
        kfree(sdev->stats);
        for (i = 0; i < log_types; i++) {
            kfree(sdev->stats->timestamp_logs[i]->timestamp_items);
            kfree(sdev->stats->timestamp_logs[i]->name);
            kfree(sdev->stats->timestamp_logs[i]);
        }
        sdev->stats->timestamp_amount = 0;
    }
    return err;
}

