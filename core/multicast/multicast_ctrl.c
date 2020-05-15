//
// Created by Jahnke, Patrick on 14.05.20.
//

#include <linux/kernel.h>
#include <linux/slab.h>

#include <asguard/logger.h>
#include <asguard/multicast.h>

#include <linux/list.h>

#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/uaccess.h>
#include <linux/module.h>

#include <linux/err.h>

#include "../asguard_core.h"


#undef LOG_PREFIX
#define LOG_PREFIX "[ASGUARD][MULTICAST INSTANCE CTRL]"

static ssize_t multicast_delay_write(struct file *file,
                                    const char __user *user_buffer,
                                    size_t count, loff_t *data);
static int multicast_delay_open(struct inode *inode, struct file *file);

static ssize_t multicast_enable_write(struct file *file,
                                     const char __user *user_buffer,
                                     size_t count, loff_t *data);
static int multicast_enable_open(struct inode *inode, struct file *file);


static const struct file_operations multicast_delay_ops = {
        .owner = THIS_MODULE,
        .open = multicast_delay_open,
        .write = multicast_delay_write,
        .read = seq_read,
        .llseek = seq_lseek,
        .release = single_release,
};

static const struct file_operations multicast_enable_ops = {
        .owner = THIS_MODULE,
        .open = multicast_enable_open,
        .write = multicast_enable_write,
        .read = seq_read,
        .llseek = seq_lseek,
        .release = single_release,
};

static ssize_t multicast_delay_write(struct file *file,
                                         const char __user *user_buffer,
                                         size_t count, loff_t *data)
{
    int err;
    char kernel_buffer[ASGUARD_TARGETS_BUF];
    char *search_str;
    struct asguard_device *sdev =
            (struct asguard_device *)PDE_DATA(file_inode(file));
    size_t size = min(sizeof(kernel_buffer) - 1, count);
    long new_value = -1;

    if (!sdev) {
        asguard_error(" Could not find asguard device!\n");
        return -ENODEV;
    }

    memset(kernel_buffer, 0, sizeof(kernel_buffer));

    err = copy_from_user(kernel_buffer, user_buffer, count);

    if (err) {
        asguard_error("Copy from user failed%s\n", __func__);
        goto error;
    }

    kernel_buffer[size] = '\0';

    err = kstrtol(kernel_buffer, 0, &new_value);

    if (err) {
        asguard_error(" Error converting input%s\n", __func__);
        return err;
    }

    sdev->multicast.delay = new_value;

    return count;
    error:

    asguard_error("Error during parsing of input.%s\n", __func__);
    return err;
}

static int multicast_delay_show(struct seq_file *m, void *v)
{
    struct asguard_device *sdev =
            (struct asguard_device *)m->private;

    int i;

    if (!sdev)
        return -ENODEV;

    seq_printf(m, "Multicast delay: %d\n", sdev->multicast.delay);

    return 0;
}

static int multicast_delay_open(struct inode *inode, struct file *file)
{
    return single_open(file, multicast_delay_show,
                       PDE_DATA(file_inode(file)));
}

static ssize_t multicast_enable_write(struct file *file,
                                     const char __user *user_buffer,
                                     size_t count, loff_t *data)
{
    int err;
    char kernel_buffer[ASGUARD_TARGETS_BUF];
    char *search_str;
    struct asguard_device *sdev =
            (struct asguard_device *)PDE_DATA(file_inode(file));
    size_t size = min(sizeof(kernel_buffer) - 1, count);
    long new_value = -1;

    if (!sdev) {
        asguard_error(" Could not find asguard device!\n");
        return -ENODEV;
    }

    memset(kernel_buffer, 0, sizeof(kernel_buffer));

    err = copy_from_user(kernel_buffer, user_buffer, count);

    if (err) {
        asguard_error("Copy from user failed%s\n", __func__);
        goto error;
    }

    kernel_buffer[size] = '\0';

    err = kstrtol(kernel_buffer, 0, &new_value);

    if (err) {
        asguard_error(" Error converting input%s\n", __func__);
        return err;
    }

    sdev->multicast.enable = new_value;

    return count;
    error:

    asguard_error("Error during parsing of input.%s\n", __func__);
    return err;
}

static int multicast_enable_show(struct seq_file *m, void *v)
{
    struct asguard_device *sdev =
            (struct asguard_device *)m->private;

    int i;

    if (!sdev)
        return -ENODEV;

    seq_printf(m, "Multicast enabled: %d\n", sdev->multicast.enable);

    return 0;
}

static int multicast_enable_open(struct inode *inode, struct file *file)
{
    return single_open(file, multicast_enable_show,
                       PDE_DATA(file_inode(file)));
}


void init_multicast(struct asguard_device *sdev)
{
    char name_buf[MAX_ASGUARD_PROC_NAME];

    snprintf(name_buf, sizeof(name_buf), "asguard/%d/multicast",
             sdev->ifindex);

    proc_mkdir(name_buf, NULL);

    snprintf(name_buf, sizeof(name_buf), "asguard/%d/multicast/delay",
             sdev->ifindex);

    sdev->multicast_delay_entry = proc_create_data(name_buf, S_IRWXU | S_IRWXO, NULL, &multicast_delay_ops, sdev);

    snprintf(name_buf, sizeof(name_buf), "asguard/%d/multicast/enable",
             sdev->ifindex);

    sdev->multicast_enable_entry = proc_create_data(name_buf, S_IRWXU | S_IRWXO, NULL, &multicast_enable_ops, sdev);

}
EXPORT_SYMBOL(init_multicast);


void remove_multicast(struct asguard_device *sdev)
{
    char name_buf[MAX_ASGUARD_PROC_NAME];

    snprintf(name_buf, sizeof(name_buf), "asguard/%d/multicast/delay",
             sdev->ifindex);

    if(sdev->multicast_delay_entry) {
        remove_proc_entry(name_buf, NULL);
        sdev->multicast_delay_entry = NULL;
    }

    snprintf(name_buf, sizeof(name_buf), "asguard/%d/multicast/enable",
             sdev->ifindex);

    if(sdev->multicast_enable_entry) {
        remove_proc_entry(name_buf, NULL);
        sdev->multicast_enable_entry = NULL;
    }

    snprintf(name_buf, sizeof(name_buf), "asguard/%d/multicast",
             sdev->ifindex);

    remove_proc_entry(name_buf, NULL);
}
EXPORT_SYMBOL(remove_multicast);
