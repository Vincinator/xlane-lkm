#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/module.h>
#include <linux/init.h>

#include <linux/kernel.h>

#include <asguard/asguard.h>
#include <asguard/logger.h>

#include "asguard_fd_tx_procfs.h"


static ssize_t asguard_fdus_reg_write(struct file *file,
				    const char __user *user_buffer,
				    size_t count, loff_t *data)
{
	int err;
	char kernel_buffer[MAX_PROCFS_BUF];
	struct asguard_device *sdev =
		(struct asguard_device *)PDE_DATA(file_inode(file));
	long new_state = -1;

	if (!sdev)
		return -ENODEV;

	if (count == 0)
		return -EINVAL;

	err = copy_from_user(kernel_buffer, user_buffer, count);

	if (err) {
		asguard_error("Copy from user failed%s\n", __FUNCTION__);
		return err;
	}

	kernel_buffer[count] = '\0';

	err = kstrtol(kernel_buffer, 0, &new_state);

	if (err) {
		asguard_error(" Error converting input%s\n", __FUNCTION__);
		return err;
	}

	if (new_state == 0) {
		sdev->rx_state = ASGUARD_RX_DISABLED;
		asguard_dbg("RX disabled\n");

	} else {
		sdev->rx_state = ASGUARD_RX_ENABLED;
		asguard_dbg("RX enabled\n");
	}
	return count;
}

static int asguard_fdus_reg_show(struct seq_file *m, void *v)
{
	struct asguard_device *sdev = (struct asguard_device *)m->private;
	int i;

	if (!sdev)
		return -ENODEV;

	seq_printf(m, "asguard core RX state: %d\n", sdev->rx_state);

	return 0;
}

static int asguard_fdus_reg_open(struct inode *inode, struct file *file)
{
	return single_open(file, asguard_fdus_reg_show,
			   PDE_DATA(file_inode(file)));
}

static const struct file_operations asguard_fdus_reg_ops = {
	.owner = THIS_MODULE,
	.open = asguard_fdus_reg_open,
	.write = asguard_fdus_reg_write,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

void init_asguard_fdus_interfaces(struct asguard_device *sdev)
{
	char name_buf[MAX_ASGUARD_PROC_NAME];

	snprintf(name_buf, sizeof(name_buf), "asguard/%d/fd", sdev->ifindex);
	proc_mkdir(name_buf, NULL);

	snprintf(name_buf, sizeof(name_buf), "asguard/%d/fd/register_proc",
		 sdev->ifindex);
	proc_create_data(name_buf, S_IRWXU | S_IRWXO, NULL, &asguard_fdus_reg_ops,
			 sdev);
}
EXPORT_SYMBOL(init_asguard_fdus_interfaces);

void clean_asguard_fdus_interfaces(struct asguard_device *sdev)
{
	char name_buf[MAX_ASGUARD_PROC_NAME];

	snprintf(name_buf, sizeof(name_buf), "asguard/%d/fd/register_proc",
		 sdev->ifindex);
	remove_proc_entry(name_buf, NULL);

	snprintf(name_buf, sizeof(name_buf), "asguard/%d/fd", sdev->ifindex);
	remove_proc_entry(name_buf, NULL);
}
EXPORT_SYMBOL(clean_asguard_fdus_interfaces);

