#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/module.h>
#include <linux/init.h>

#include <linux/kthread.h>
#include <linux/sched.h>

#include <linux/kernel.h>

#include "../core/module.h"
#include "../core/libasraft.h"
#include "../core/types.h"
#include "../core/logger.h"
#include "../core/pacemaker.h"
#include "../core/membership.h"


static ssize_t asgard_hb_ctrl_proc_write(struct file *file, const char __user *buffer, size_t count, loff_t *data)
{
    int err;
    char kernel_buffer[MAX_PROCFS_BUF];
    struct pminfo *spminfo =
            (struct pminfo *)PDE_DATA(file_inode(file));
    long new_hb_state = -1;

    asgard_error("hb ctrl interface accessed!\n");

    if (!spminfo) {
        asgard_error("pacemaker info is not initialized!\n");
        return -ENODEV;
    }
    if (count == 0) {
        err = -EINVAL;
        asgard_error("invalid interface access\n");
        goto error;
    }

    err = copy_from_user(kernel_buffer, buffer, count);
    asgard_error("copy_from_user!\n");

    if (err) {
        asgard_error("Copy from user failed%s\n", __func__);
        goto error;
    }

    kernel_buffer[count] = '\0';

    err = kstrtol(kernel_buffer, 0, &new_hb_state);
    asgard_error("kstrtol!\n");

    if (err) {
        asgard_error("Error converting input%s\n", __func__);
        goto error;
    }

    switch (new_hb_state) {
        case 0:
            asgard_error("0\n");

            asgard_pm_stop(spminfo);
            break;
        case 1:
            asgard_error("pm loop start\n");
            asgard_pm_start_loop(spminfo);
            break;
        case 2:
            asgard_error("2\n");
            asgard_pm_reset(spminfo);
            break;
        default:
            asgard_error("Unknown action!\n");
            err = -EINVAL;
            goto error;
    }

    return count;
error:
    asgard_error("Heartbeat control failed.%s\n", __func__);
    return err;
}

static int asgard_hb_ctrl_proc_show(struct seq_file *m, void *v)
{
    struct pminfo *spminfo =
            (struct pminfo *)m->private;

    if (!spminfo)
        return -ENODEV;

    if (spminfo->state == ASGARD_PM_EMITTING)
        seq_puts(m, "emitting");
    else if (spminfo->state == ASGARD_PM_UNINIT)
        seq_puts(m, "uninit");
    else if (spminfo->state == ASGARD_PM_READY)
        seq_puts(m, "ready");

    return 0;
}

static int asgard_hb_ctrl_proc_open(struct inode *inode, struct file *file)
{
    return single_open(file, asgard_hb_ctrl_proc_show,
                       PDE_DATA(file_inode(file)));
}

static ssize_t asgard_cpumgmt_write(struct file *file, const char __user *user_buffer, size_t count, loff_t *data)
{
    int err;
    char kernel_buffer[ASGARD_NUMBUF];
    int tocpu = -1;
    struct pminfo *spminfo = (struct pminfo *)PDE_DATA(file_inode(file));
    size_t size;

    if (!spminfo)
        return -ENODEV;

    size = min(sizeof(kernel_buffer) - 1, count);

    memset(kernel_buffer, 0, sizeof(kernel_buffer));

    err = copy_from_user(kernel_buffer, user_buffer, count);
    if (err) {
        asgard_error("Copy from user failed%s\n", __func__);
        goto error;
    }

    kernel_buffer[size] = '\0';
    err = kstrtoint(kernel_buffer, 10, &tocpu);
    if (err) {
        asgard_error(
        "Error converting input buffer: %s, tocpu: 0x%x\n",
        kernel_buffer, tocpu);
        goto error;
    }

    if (tocpu > MAX_CPU_NUMBER || tocpu < 0)
        return -ENODEV;

    spminfo->active_cpu = tocpu;
    pm_state_transition_to(spminfo, ASGARD_PM_READY);
    asgard_error("Pacemaker CPU is set\n");

    return count;
error:
    asgard_error("Could not set cpu.%s\n", __func__);
    return err;
}

static int asgard_cpumgmt_show(struct seq_file *m, void *v)
{
    struct pminfo *spminfo =
            (struct pminfo *)m->private;

    if (!spminfo)
        return -ENODEV;

    seq_printf(m, "%d\n", spminfo->active_cpu);
    return 0;
}

static int asgard_cpumgmt_open(struct inode *inode, struct file *file)
{
    return single_open(file, asgard_cpumgmt_show,
                       PDE_DATA(file_inode(file)));
}

static ssize_t asgard_cluster_id_write(struct file *file, const char __user *user_buffer, size_t count, loff_t *data)
{
    int err;
    char kernel_buffer[ASGARD_NUMBUF];
    int to_cluster_id = -1;
    struct pminfo *spminfo =
            (struct pminfo *)PDE_DATA(file_inode(file));
    size_t size;

    if (!spminfo)
        return -ENODEV;

    size = min(sizeof(kernel_buffer) - 1, count);

    memset(kernel_buffer, 0, sizeof(kernel_buffer));

    err = copy_from_user(kernel_buffer, user_buffer, count);

    if (err) {
        asgard_error("Copy from user failed%s\n", __func__);
        goto error;
    }

    kernel_buffer[size] = '\0';
    err = kstrtoint(kernel_buffer, 10, &to_cluster_id);
    if (err) {
        asgard_error(
        "Error converting input buffer: %s, cluster id: 0x%x\n",
        kernel_buffer, to_cluster_id);
        goto error;
    }

    if (to_cluster_id > MAX_CLUSTER_MEMBER || to_cluster_id < 0)
        return -ENODEV;

    spminfo->cluster_id = to_cluster_id;
    pm_state_transition_to(spminfo, ASGARD_PM_READY);
    asgard_error("Pacemaker Cluster ID is set to %d\n", spminfo->cluster_id);

    return count;
error:
    asgard_error("Could not set cpu.%s\n", __func__);
    return err;
}

static int asgard_cluster_id_show(struct seq_file *m, void *v)
{
    struct pminfo *spminfo =
            (struct pminfo *)m->private;

    if (!spminfo)
        return -ENODEV;

    seq_printf(m, "%d\n", spminfo->cluster_id);


    return 0;
}

static int asgard_cluster_id_open(struct inode *inode, struct file *file)
{
    return single_open(file, asgard_cluster_id_show,
                       PDE_DATA(file_inode(file)));
}




static ssize_t asgard_hbi_write(struct file *file, const char __user *buffer, size_t count, loff_t *data)
{
    int err;
    char kernel_buffer[MAX_PROCFS_BUF];
    struct pminfo *spminfo = (struct pminfo *)PDE_DATA(file_inode(file));
    long new_hbi = -1;

    if (!spminfo)
    return -ENODEV;

    if (count == 0) {
        err = -EINVAL;
        goto error;
    }

    err = copy_from_user(kernel_buffer, buffer, count);

    if (err) {
        asgard_error("Copy from user failed%s\n", __func__);
        goto error;
    }

    kernel_buffer[count] = '\0';

    err = kstrtol(kernel_buffer, 0, &new_hbi);

    if (err) {
        asgard_error("Error converting input%s\n", __func__);
        goto error;
    }

    if ( new_hbi < MIN_HB_CYCLES || new_hbi > MAX_HB_CYCLES) {
        asgard_error("Invalid heartbeat interval! range is %d to %d, but got %ld",
        MIN_HB_CYCLES, MAX_HB_CYCLES, new_hbi);
        err = -EINVAL;
        goto error;
    }

    spminfo->hbi = new_hbi;

    return count;
error:
    asgard_error("Heartbeat control failed.%s\n", __func__);
    return err;
}

static int asgard_hbi_show(struct seq_file *m, void *v)
{
    struct pminfo *spminfo =
            (struct pminfo *)m->private;

    if (!spminfo)
        return -ENODEV;

    seq_printf(m, "%llu\n", spminfo->hbi);
    return 0;
}

static int asgard_hbi_open(struct inode *inode, struct file *file)
{
    return single_open(file, asgard_hbi_show,
                       PDE_DATA(file_inode(file)));
}


static ssize_t asgard_ww_write(struct file *file,const char __user *buffer, size_t count, loff_t *data)
{
    int err;
    char kernel_buffer[MAX_PROCFS_BUF];
    struct pminfo *spminfo = (struct pminfo *)PDE_DATA(file_inode(file));
    long new_ww = -1;

    if (!spminfo)
        return -ENODEV;

    if (count == 0) {
        err = -EINVAL;
        goto error;
    }

    err = copy_from_user(kernel_buffer, buffer, count);

    if (err) {
        asgard_error("Copy from user failed%s\n", __func__);
        goto error;
    }

    kernel_buffer[count] = '\0';

    err = kstrtol(kernel_buffer, 0, &new_ww);

    if (err) {
        asgard_error("Error converting input %s\n", __func__);
        goto error;
    }

    spminfo->waiting_window = new_ww;

    return count;
error:
    asgard_error("Setting of waiting Window failed\n");
    return err;
}

static int asgard_ww_show(struct seq_file *m, void *v)
{
    struct pminfo *spminfo =
            (struct pminfo *)m->private;

    if (!spminfo)
        return -ENODEV;

    seq_printf(m, "%llu\n", spminfo->waiting_window);
    return 0;
}

static int asgard_ww_open(struct inode *inode, struct file *file)
{
    return single_open(file, asgard_ww_show,
                       PDE_DATA(file_inode(file)));
}


static ssize_t asgard_payload_write(struct file *file,  const char __user *user_buffer, size_t count, loff_t *data)
{
    int ret = 0;
    char kernel_buffer[ASGARD_TARGETS_BUF];
    struct pminfo *spminfo =
            (struct pminfo *)PDE_DATA(file_inode(file));
    size_t size = min(sizeof(kernel_buffer) - 1, count);
    static const char delimiters[] = " ,;";
    char *input_str;
    char *search_str;
    int i = 0;

    if (!spminfo) {
        asgard_error("spminfo is NULL.\n");
        ret = -ENODEV;
        goto out;
    }

    memset(kernel_buffer, 0, sizeof(kernel_buffer));

    ret = copy_from_user(kernel_buffer, user_buffer, count);
    if (ret) {
        asgard_error("Copy from user failed%s\n", __func__);
        goto out;
    }

    kernel_buffer[size] = '\0';

    search_str = kstrdup(kernel_buffer, GFP_KERNEL);
    while ((input_str = strsep(&search_str, delimiters)) != NULL) {
        if (strcmp(input_str, "") == 0 || strlen(input_str) == 0)
            break;

        if (i >= MAX_REMOTE_SOURCES) {
            asgard_error(
            "exceeded max of remote targets %d >= %d\n",
            i, MAX_REMOTE_SOURCES);
            break;
        }

        //asgard_dbg(" payload message: %02X\n", input_str[0] & 0xFF);
        i++;
    }

out:
    return count;
}

static int asgard_payload_show(struct seq_file *m, void *v)
{
    struct pminfo *spminfo =
            (struct pminfo *)m->private;
    int i;
    // freed in this function
    char *current_payload = kmalloc(16, GFP_KERNEL);
    // freed in this function
    char *current_ip =
            kmalloc(16, GFP_KERNEL); /* strlen of 255.255.255.255 is 15*/
    int ret = 0;

    if (!spminfo) {
        asgard_error("spminfo is NULL.\n");
        ret = -ENODEV;
        goto out;
    }

    for (i = 0; i < spminfo->num_of_targets; i++) {
        asgard_hex_to_ip(current_ip,
                         spminfo->pm_targets[i].pkt_data.naddr.dst_ip);
        seq_printf(m, "%s:\n", current_ip);
        seq_hex_dump(m, "	", DUMP_PREFIX_OFFSET, 32, 1,
                     spminfo->pm_targets[i].pkt_data.payload,
                     ASGARD_PAYLOAD_BYTES, false);
    }

    out:
    kfree(current_ip);
    kfree(current_payload);

    return 0;
}

static int asgard_payload_open(struct inode *inode, struct file *file)
{
    return single_open(file, asgard_payload_show,
                       PDE_DATA(file_inode(file)));
}

static ssize_t asgard_target_write(struct file *file, const char __user *user_buffer, size_t count, loff_t *data)
{
    int err;
    char kernel_buffer[ASGARD_TARGETS_BUF];
    char *search_str;
    struct pminfo *spminfo =
            (struct pminfo *)PDE_DATA(file_inode(file));
    struct asgard_device *sdev;
    size_t size = min(sizeof(kernel_buffer) - 1, count);
    char *input_str;
    static const char delimiters[] = " ,;()";
    int i = 0;
    int state = 0; /* first element of tuple is ip address, second is mac */
    u32 current_ip;
    unsigned char *current_mac = NULL;
    int current_protocol;
    int cluster_id;

    current_ip = -1;

    if (!spminfo)
        return -ENODEV;

    sdev = container_of(spminfo, struct asgard_device, pminfo);

    if (!sdev) {
        asgard_error(" Could not find asgard device!\n");
        return -ENODEV;
    }

    memset(kernel_buffer, 0, sizeof(kernel_buffer));

    err = copy_from_user(kernel_buffer, user_buffer, count);
    if (err) {
        asgard_error("Copy from user failed%s\n", __func__);
        goto error;
    }

    kernel_buffer[size] = '\0';
    if (spminfo->num_of_targets < 0 ||
        spminfo->num_of_targets > ASGARD_TARGETS_BUF) {
        asgard_error("num_of_targets is invalid! Have you set the target hosts?\n");
        return -EINVAL;
    }

    i = 0;
    sdev->pminfo.num_of_targets = 0;
    search_str = kstrdup(kernel_buffer, GFP_KERNEL);
    while ((input_str = strsep(&search_str, delimiters)) != NULL) {

        if (!input_str || strlen(input_str) <= 0)
            continue;

        if (i > ASGARD_TARGETS_BUF) {
            asgard_error(
            "Target buffer full! Not all targets are applied, increase buffer in asgard source.\n");
            break;
        }

        if (state == 0) {
            current_ip = asgard_ip_convert(input_str);
            if (current_ip == -EINVAL) {
                asgard_error("Error formating IP address. %s\n",
                __func__);
                return -EINVAL;
            }
            state = 1;
        } else if (state == 1) {
            current_mac = asgard_convert_mac(input_str);
            if (!current_mac) {
                asgard_error(
                "Invalid MAC. Failed to convert to byte string.\n");
                return -EINVAL;
            }
        state = 2;

        } else if (state == 2) {
            err = kstrtoint(input_str, 10, &current_protocol);
            if (err) {
                asgard_error(
                "Expected Porotocol Number. Error converting input buffer: %s\n",
                input_str);
                goto error;
            }

            state = 3;
        } else if (state == 3) {
            err = kstrtoint(input_str, 10, &cluster_id);

        if (is_ip_local(sdev->ndev, current_ip)) {
            sdev->pminfo.cluster_id = cluster_id;

        if(sdev->ci)
            sdev->ci->cluster_self_id = cluster_id;

        } else {
            asgard_core_register_remote_host(sdev->asgard_id,
            current_ip, current_mac,
            current_protocol, cluster_id);
            i++;
        }
        state = 0;
        if (current_mac)
        kfree(current_mac);
        }
    }
    spminfo->num_of_targets = i;

    return count;
error:
    if (current_mac)
        kfree(current_mac);
    asgard_error("Error during parsing of input.%s\n", __func__);
    return err;
}

static int asgard_target_show(struct seq_file *m, void *v)
{
    struct pminfo *spminfo =
            (struct pminfo *)m->private;
    int i;
    // freed in this function
    char *current_ip =
            kmalloc(16, GFP_KERNEL); /* strlen of 255.255.255.255 is 15*/

    if (!spminfo)
        return -ENODEV;

    for (i = 0; i < spminfo->num_of_targets; i++) {
        asgard_hex_to_ip(current_ip,
                         spminfo->pm_targets[i].pkt_data.naddr.dst_ip);
        seq_printf(m, "(%s,", current_ip);
        seq_printf(m, "%x:%x:%x:%x:%x:%x)\n",
                   spminfo->pm_targets[i].pkt_data.naddr.dst_mac[0],
                   spminfo->pm_targets[i].pkt_data.naddr.dst_mac[1],
                   spminfo->pm_targets[i].pkt_data.naddr.dst_mac[2],
                   spminfo->pm_targets[i].pkt_data.naddr.dst_mac[3],
                   spminfo->pm_targets[i].pkt_data.naddr.dst_mac[4],
                   spminfo->pm_targets[i].pkt_data.naddr.dst_mac[5]);
    }
    kfree(current_ip);

    return 0;
}

static int asgard_target_open(struct inode *inode, struct file *file)
{
    return single_open(file, asgard_target_show, PDE_DATA(file_inode(file)));
}

static ssize_t asgard_test_ctrl_write(struct file *file, const char __user *user_buffer, size_t count, loff_t *data)
{
    int err;
    char kernel_buffer[MAX_PROCFS_BUF];
    struct pminfo *spminfo = (struct pminfo *)PDE_DATA(file_inode(file));
    long new_active_processes = -1;

    if (!spminfo)
        return -ENODEV;

    if (count == 0)
        return -EINVAL;

    err = copy_from_user(kernel_buffer, user_buffer, count);

    if (err) {
        asgard_error("Copy from user failed%s\n", __func__);
        return err;
    }

    kernel_buffer[count] = '\0';

    err = kstrtol(kernel_buffer, 0, &new_active_processes);

    if (err) {
        asgard_error("Error converting input%s\n", __func__);
        return err;
    }

    asgard_dbg("created %ld active user space\n", new_active_processes);
}

static int asgard_test_ctrl_show(struct seq_file *m, void *v)
{
    struct pminfo *spminfo =
            (struct pminfo *)m->private;

    if (!spminfo)
        return -ENODEV;

    if (spminfo->tdata.state == ASGARD_PM_TEST_UNINIT)
        seq_printf(m, "Not Initialized\n");
    else
        seq_printf(m, "active dummy user space processes: %d",
                   spminfo->tdata.active_processes);

    return 0;
}

static int asgard_test_ctrl_open(struct inode *inode, struct file *file)
{
    return single_open(file, asgard_test_ctrl_show,
                       PDE_DATA(file_inode(file)));
}

static const struct proc_ops asgard_target_ops = {
        .proc_open = asgard_target_open,
        .proc_write = asgard_target_write,
        .proc_read = seq_read,
        .proc_lseek = seq_lseek,
        .proc_release = single_release,
};

static const struct proc_ops asgard_hb_ctrl_ops = {
        .proc_open = asgard_hb_ctrl_proc_open,
        .proc_write = asgard_hb_ctrl_proc_write,
        .proc_read = seq_read,
        .proc_lseek = seq_lseek,
        .proc_release = single_release,
};

static const struct proc_ops asgard_cpumgmt_ops = {
        .proc_open = asgard_cpumgmt_open,
        .proc_write = asgard_cpumgmt_write,
        .proc_read = seq_read,
        .proc_lseek = seq_lseek,
        .proc_release = single_release,
};

static const struct proc_ops asgard_cluster_id_ops = {
        .proc_open = asgard_cluster_id_open,
        .proc_write = asgard_cluster_id_write,
        .proc_read = seq_read,
        .proc_lseek = seq_lseek,
        .proc_release = single_release,
};

static const struct proc_ops asgard_hbi_ops = {
        .proc_open = asgard_hbi_open,
        .proc_write = asgard_hbi_write,
        .proc_read = seq_read,
        .proc_lseek = seq_lseek,
        .proc_release = single_release,
};

static const struct proc_ops asgard_ww_ops = {
        .proc_open = asgard_ww_open,
        .proc_write = asgard_ww_write,
        .proc_read = seq_read,
        .proc_lseek = seq_lseek,
        .proc_release = single_release,
};

static const struct proc_ops asgard_payload_ops = {
        .proc_open = asgard_payload_open,
        .proc_write = asgard_payload_write,
        .proc_read = seq_read,
        .proc_lseek = seq_lseek,
        .proc_release = single_release,
};

static const struct proc_ops asgard_test_ctrl_ops = {
        .proc_open = asgard_test_ctrl_open,
        .proc_write = asgard_test_ctrl_write,
        .proc_read = seq_read,
        .proc_lseek = seq_lseek,
        .proc_release = single_release,
};

void init_asgard_pm_ctrl_interfaces(struct asgard_device *sdev)
{
    char name_buf[MAX_ASGARD_PROC_NAME];
    if(!sdev) {
        asgard_error("Catched NUll pointer in %s\n", __FUNCTION__);
        return;
    }


    snprintf(name_buf, sizeof(name_buf), "asgard/%d/pacemaker",
             sdev->ifindex);
    proc_mkdir(name_buf, NULL);

    snprintf(name_buf, sizeof(name_buf), "asgard/%d/pacemaker/payload",
             sdev->ifindex);

    sdev->pacemaker_payload_entry = proc_create_data(name_buf, S_IRWXU | S_IRWXO, NULL, &asgard_payload_ops,
                                                     &sdev->pminfo);

    snprintf(name_buf, sizeof(name_buf), "asgard/%d/pacemaker/ctrl",
             sdev->ifindex);

    sdev->pacemaker_ctrl_entry = proc_create_data(name_buf, S_IRWXU | S_IRWXO, NULL, &asgard_hb_ctrl_ops,
                                                  &sdev->pminfo);

    snprintf(name_buf, sizeof(name_buf), "asgard/%d/pacemaker/hbi",
             sdev->ifindex);

    sdev->pacemaker_hbi_entry = proc_create_data(name_buf, S_IRWXU | S_IRWXO, NULL, &asgard_hbi_ops,
                                                 &sdev->pminfo);

    snprintf(name_buf, sizeof(name_buf), "asgard/%d/pacemaker/waiting_window",
             sdev->ifindex);

    sdev->pacemaker_waiting_window_entry = proc_create_data(name_buf, S_IRWXU | S_IRWXO, NULL, &asgard_ww_ops,
                                                            &sdev->pminfo);

    snprintf(name_buf, sizeof(name_buf), "asgard/%d/pacemaker/targets",
             sdev->ifindex);
    sdev->pacemaker_targets_entry = proc_create_data(name_buf, S_IRWXU | S_IRWXO, NULL, &asgard_target_ops,
                                                     &sdev->pminfo);

    snprintf(name_buf, sizeof(name_buf), "asgard/%d/pacemaker/cpu",
             sdev->ifindex);

    sdev->pacemaker_cpu_entry = proc_create_data(name_buf, S_IRWXU | S_IRWXO, NULL, &asgard_cpumgmt_ops,
                                                 &sdev->pminfo);


    snprintf(name_buf, sizeof(name_buf), "asgard/%d/pacemaker/cluster_id",
             sdev->ifindex);

    sdev->pacemaker_cluster_id_entry = proc_create_data(name_buf, S_IRWXU | S_IRWXO, NULL, &asgard_cluster_id_ops,
                                                        &sdev->pminfo);


}
EXPORT_SYMBOL(init_asgard_pm_ctrl_interfaces);

void clean_asgard_pm_ctrl_interfaces(struct asgard_device *sdev)
{
    char name_buf[MAX_ASGARD_PROC_NAME];

    snprintf(name_buf, sizeof(name_buf), "asgard/%d/pacemaker/payload",
             sdev->ifindex);

    if(sdev->pacemaker_payload_entry) {
        remove_proc_entry(name_buf, NULL);
        sdev->pacemaker_payload_entry = NULL;
    }

    snprintf(name_buf, sizeof(name_buf), "asgard/%d/pacemaker/cpu",
             sdev->ifindex);

    if(sdev->pacemaker_cpu_entry){
        remove_proc_entry(name_buf, NULL);
        sdev->pacemaker_cpu_entry = NULL;
    }

    snprintf(name_buf, sizeof(name_buf), "asgard/%d/pacemaker/ctrl",
             sdev->ifindex);

    if(sdev->pacemaker_ctrl_entry){
        remove_proc_entry(name_buf, NULL);
        sdev->pacemaker_ctrl_entry = NULL;
    }

    snprintf(name_buf, sizeof(name_buf), "asgard/%d/pacemaker/hbi",
             sdev->ifindex);

    if(sdev->pacemaker_hbi_entry) {
        remove_proc_entry(name_buf, NULL);
        sdev->pacemaker_hbi_entry = NULL;
    }

    snprintf(name_buf, sizeof(name_buf), "asgard/%d/pacemaker/waiting_window",
             sdev->ifindex);

    if(sdev->pacemaker_waiting_window_entry) {
        remove_proc_entry(name_buf, NULL);
        sdev->pacemaker_waiting_window_entry = NULL;
    }

    snprintf(name_buf, sizeof(name_buf), "asgard/%d/pacemaker/targets",
             sdev->ifindex);

    if(sdev->pacemaker_targets_entry) {
        remove_proc_entry(name_buf, NULL);
        sdev->pacemaker_targets_entry = NULL;
    }

    snprintf(name_buf, sizeof(name_buf), "asgard/%d/pacemaker/cluster_id",
             sdev->ifindex);

    if(sdev->pacemaker_cluster_id_entry) {
        remove_proc_entry(name_buf, NULL);
        sdev->pacemaker_cluster_id_entry = NULL;
    }


    snprintf(name_buf, sizeof(name_buf), "asgard/%d/pacemaker",
             sdev->ifindex);

    remove_proc_entry(name_buf, NULL);
}
EXPORT_SYMBOL(clean_asgard_pm_ctrl_interfaces);
