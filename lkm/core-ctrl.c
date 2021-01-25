//
// Created by vincent on 1/22/21.
//


#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/module.h>
#include <linux/init.h>

#include <linux/kernel.h>
#include <linux/types.h>

#include "../module.h"
#include "../logger.h"
#include "core-ctrl.h"

static ssize_t asgard_rx_ctrl_write(struct file *file, const char __user *user_buffer, size_t count, loff_t *data)
{
    int err;
    char kernel_buffer[MAX_PROCFS_BUF];
    struct asgard_device *sdev = (struct asgard_device *)PDE_DATA(file_inode(file));
    long new_state = -1;

    if (!sdev)
        return -ENODEV;

    if (count == 0)
        return -EINVAL;

    err = copy_from_user(kernel_buffer, user_buffer, count);

    if (err) {
        asgard_error("Copy from user failed%s\n", __func__);
        return err;
    }

    kernel_buffer[count] = '\0';

    err = kstrtol(kernel_buffer, 0, &new_state);

    if (err) {
        asgard_error(" Error converting input%s\n", __func__);
        return err;
    }

    if (new_state == 0) {
        sdev->rx_state = ASGARD_RX_DISABLED;
        asgard_dbg("RX disabled\n");

    } else {
        sdev->rx_state = ASGARD_RX_ENABLED;
        asgard_dbg("RX enabled\n");
    }
    return count;
}

static int asgard_rx_ctrl_show(struct seq_file *m, void *v)
{
    struct asgard_device *sdev = (struct asgard_device *)m->private;

    if (!sdev)
        return -ENODEV;

    seq_printf(m, "asgard core RX state: %d\n", sdev->rx_state);

    return 0;
}

static int asgard_rx_ctrl_open(struct inode *inode, struct file *file)
{
    return single_open(file, asgard_rx_ctrl_show,
                       PDE_DATA(file_inode(file)));
}

static const struct proc_ops asgard_core_ctrl_ops = {
        .proc_open = asgard_rx_ctrl_open,
        .proc_write = asgard_rx_ctrl_write,
        .proc_read = seq_read,
        .proc_lseek = seq_lseek,
        .proc_release = single_release,
};

static ssize_t asgard_verbose_ctrl_write(struct file *file, const char __user *user_buffer, size_t count, loff_t *data)
{
    int err;
    char kernel_buffer[MAX_PROCFS_BUF];
    struct asgard_device *sdev = (struct asgard_device *)PDE_DATA(file_inode(file));
    long new_state = -1;

    if (!sdev)
        return -ENODEV;

    if (count == 0)
        return -EINVAL;

    err = copy_from_user(kernel_buffer, user_buffer, count);

    if (err) {
        asgard_error("Copy from user failed%s\n", __func__);
        return err;
    }

    kernel_buffer[count] = '\0';

    err = kstrtol(kernel_buffer, 0, &new_state);

    if (err) {
        asgard_error(" Error converting input%s\n", __func__);
        return err;
    }

    sdev->verbose = new_state;

    return count;
}

static int asgard_verbose_ctrl_show(struct seq_file *m, void *v)
{
    struct asgard_device *sdev = (struct asgard_device *)m->private;

    if (!sdev)
        return -ENODEV;

    seq_printf(m, "asgard device verbosity level is set to %d\n", sdev->verbose);

    return 0;
}

static int asgard_verbose_ctrl_open(struct inode *inode, struct file *file)
{
    return single_open(file, asgard_verbose_ctrl_show,
                       PDE_DATA(file_inode(file)));
}

static const struct proc_ops asgard_verbose_ctrl_ops = {
        .proc_open = asgard_verbose_ctrl_open,
        .proc_write = asgard_verbose_ctrl_write,
        .proc_read = seq_read,
        .proc_lseek = seq_lseek,
        .proc_release = single_release,
};

void init_asgard_ctrl_interfaces(struct asgard_device *sdev)
{
    char name_buf[MAX_ASGARD_PROC_NAME];

    snprintf(name_buf, sizeof(name_buf), "asgard/%d/rx_ctrl", sdev->ifindex);
    sdev->rx_ctrl_entry = proc_create_data(name_buf, S_IRWXU | S_IRWXO, NULL, &asgard_core_ctrl_ops, sdev);

    snprintf(name_buf, sizeof(name_buf), "asgard/%d/debug", sdev->ifindex);
    sdev->debug_entry = proc_create_data(name_buf, S_IRWXU | S_IRWXO, NULL,
                                         &asgard_verbose_ctrl_ops, sdev);
}
EXPORT_SYMBOL(init_asgard_ctrl_interfaces);

void clean_asgard_ctrl_interfaces(struct asgard_device *sdev)
{
    char name_buf[MAX_ASGARD_PROC_NAME];

    snprintf(name_buf, sizeof(name_buf), "asgard/%d/rx_ctrl", sdev->ifindex);

    if(sdev->rx_ctrl_entry) {
        remove_proc_entry(name_buf, NULL);
        sdev->rx_ctrl_entry = NULL;
    }

    snprintf(name_buf, sizeof(name_buf), "asgard/%d/debug", sdev->ifindex);

    if(sdev->debug_entry) {
        remove_proc_entry(name_buf, NULL);
        sdev->debug_entry = NULL;
    }
}
EXPORT_SYMBOL(clean_asgard_ctrl_interfaces);

