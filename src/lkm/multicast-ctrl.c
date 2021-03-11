
#include "../core/module.h"
#include "multicast-ctrl.h"


#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/list.h>

#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/uaccess.h>
#include <linux/module.h>

#include <linux/err.h>

#include "../core/libasraft.h"
#include "../core/types.h"
#include "../core/logger.h"

#undef LOG_PREFIX
#define LOG_PREFIX "[ASGARD][MULTICAST INSTANCE CTRL]"

static ssize_t multicast_delay_write(struct file *file,
                                     const char __user *user_buffer,
size_t count, loff_t *data);
static int multicast_delay_open(struct inode *inode, struct file *file);

static ssize_t multicast_enable_write(struct file *file,
                                      const char __user *user_buffer,
size_t count, loff_t *data);
static int multicast_enable_open(struct inode *inode, struct file *file);


static const struct proc_ops multicast_delay_ops = {
        .proc_open = multicast_delay_open,
        .proc_write = multicast_delay_write,
        .proc_read = seq_read,
        .proc_lseek = seq_lseek,
        .proc_release = single_release,
};

static const struct proc_ops multicast_enable_ops = {
        .proc_open = multicast_enable_open,
        .proc_write = multicast_enable_write,
        .proc_read = seq_read,
        .proc_lseek = seq_lseek,
        .proc_release = single_release,
};

static ssize_t multicast_delay_write(struct file *file, const char __user *user_buffer, size_t count, loff_t *data)
{
    int err;
    char kernel_buffer[ASGARD_TARGETS_BUF];
    struct asgard_device *sdev = (struct asgard_device *)PDE_DATA(file_inode(file));
    size_t size = min(sizeof(kernel_buffer) - 1, count);
    long new_value = -1;

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

    err = kstrtol(kernel_buffer, 0, &new_value);

    if (err) {
        asgard_error(" Error converting input%s\n", __func__);
        return err;
    }

    sdev->multicast.delay = new_value;

    return count;
error:
    asgard_error("Error during parsing of input.%s\n", __func__);
    return err;
}

static int multicast_delay_show(struct seq_file *m, void *v)
{
    struct asgard_device *sdev =
            (struct asgard_device *)m->private;

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

static ssize_t multicast_enable_write(struct file *file, const char __user *user_buffer,
size_t count, loff_t *data)
{
int err;
char kernel_buffer[ASGARD_TARGETS_BUF];
struct asgard_device *sdev =
        (struct asgard_device *)PDE_DATA(file_inode(file));
size_t size = min(sizeof(kernel_buffer) - 1, count);
long new_value = -1;

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

err = kstrtol(kernel_buffer, 0, &new_value);

if (err) {
asgard_error(" Error converting input%s\n", __func__);
return err;
}

sdev->multicast.enable = new_value;

return count;
error:

asgard_error("Error during parsing of input.%s\n", __func__);
return err;
}

static int multicast_enable_show(struct seq_file *m, void *v)
{
    struct asgard_device *sdev =
            (struct asgard_device *)m->private;

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


void init_multicast(struct asgard_device *sdev)
{
    char name_buf[MAX_ASGARD_PROC_NAME];

    if(!sdev) {
        asgard_error("Catched NUll pointer in %s\n", __FUNCTION__);
        return;
    }

    snprintf(name_buf, sizeof(name_buf), "asgard/%d/multicast",
             sdev->ifindex);

    proc_mkdir(name_buf, NULL);

    snprintf(name_buf, sizeof(name_buf), "asgard/%d/multicast/delay",
             sdev->ifindex);

    sdev->multicast_delay_entry = proc_create_data(name_buf, S_IRWXU | S_IRWXO, NULL, &multicast_delay_ops, sdev);

    snprintf(name_buf, sizeof(name_buf), "asgard/%d/multicast/enable",
             sdev->ifindex);

    sdev->multicast_enable_entry = proc_create_data(name_buf, S_IRWXU | S_IRWXO, NULL, &multicast_enable_ops, sdev);

}
EXPORT_SYMBOL(init_multicast);


void remove_multicast(struct asgard_device *sdev)
{
    char name_buf[MAX_ASGARD_PROC_NAME];

    snprintf(name_buf, sizeof(name_buf), "asgard/%d/multicast/delay",
             sdev->ifindex);

    if(sdev->multicast_delay_entry) {
        remove_proc_entry(name_buf, NULL);
        sdev->multicast_delay_entry = NULL;
    }

    snprintf(name_buf, sizeof(name_buf), "asgard/%d/multicast/enable",
             sdev->ifindex);

    if(sdev->multicast_enable_entry) {
        remove_proc_entry(name_buf, NULL);
        sdev->multicast_enable_entry = NULL;
    }

    snprintf(name_buf, sizeof(name_buf), "asgard/%d/multicast",
             sdev->ifindex);

    remove_proc_entry(name_buf, NULL);
}
EXPORT_SYMBOL(remove_multicast);
