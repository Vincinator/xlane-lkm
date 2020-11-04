#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/module.h>
#include <linux/init.h>

#include <linux/kernel.h>

#include <asgard/asgard.h>
#include <asgard/logger.h>

#include "asgard_fd_tx_procfs.h"


static ssize_t asgard_fdus_reg_write(struct file *file,
				    const char __user *user_buffer,
				    size_t count, loff_t *data)
{
	int err;
	char kernel_buffer[MAX_PROCFS_BUF];
	struct asgard_device *sdev =
		(struct asgard_device *)PDE_DATA(file_inode(file));
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

static int asgard_fdus_reg_show(struct seq_file *m, void *v)
{
	struct asgard_device *sdev = (struct asgard_device *)m->private;
	int i;

	if (!sdev)
		return -ENODEV;

	seq_printf(m, "asgard core RX state: %d\n", sdev->rx_state);

	return 0;
}

static int asgard_fdus_reg_open(struct inode *inode, struct file *file)
{
	return single_open(file, asgard_fdus_reg_show,
			   PDE_DATA(file_inode(file)));
}

static const struct file_operations asgard_fdus_reg_ops = {
	.owner = THIS_MODULE,
	.open = asgard_fdus_reg_open,
	.write = asgard_fdus_reg_write,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

void init_asgard_fdus_interfaces(struct asgard_device *sdev)
{
	char name_buf[MAX_ASGARD_PROC_NAME];

	snprintf(name_buf, sizeof(name_buf), "asgard/%d/fd", sdev->ifindex);
	proc_mkdir(name_buf, NULL);

	snprintf(name_buf, sizeof(name_buf), "asgard/%d/fd/register_proc",
		 sdev->ifindex);
	proc_create_data(name_buf, S_IRWXU | S_IRWXO, NULL, &asgard_fdus_reg_ops,
			 sdev);
}
EXPORT_SYMBOL(init_asgard_fdus_interfaces);

void clean_asgard_fdus_interfaces(struct asgard_device *sdev)
{
	char name_buf[MAX_ASGARD_PROC_NAME];

	snprintf(name_buf, sizeof(name_buf), "asgard/%d/fd/register_proc",
		 sdev->ifindex);
	remove_proc_entry(name_buf, NULL);

	snprintf(name_buf, sizeof(name_buf), "asgard/%d/fd", sdev->ifindex);
	remove_proc_entry(name_buf, NULL);
}
EXPORT_SYMBOL(clean_asgard_fdus_interfaces);

