#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/module.h>
#include <linux/init.h>

#include <linux/kernel.h>
#include <linux/types.h>

#include <asguard/asguard.h>
#include <asguard/logger.h>


static ssize_t asguard_rx_ctrl_write(struct file *file,
				   const char __user *user_buffer, size_t count,
				   loff_t *data)
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

static int asguard_rx_ctrl_show(struct seq_file *m, void *v)
{
	struct asguard_device *sdev = (struct asguard_device *)m->private;
	int i;

	if (!sdev)
		return -ENODEV;

	seq_printf(m, "asguard core RX state: %d\n", sdev->rx_state);

	return 0;
}

static int asguard_rx_ctrl_open(struct inode *inode, struct file *file)
{
	return single_open(file, asguard_rx_ctrl_show,
			   PDE_DATA(file_inode(file)));
}

static const struct file_operations asguard_core_ctrl_ops = {
	.owner = THIS_MODULE,
	.open = asguard_rx_ctrl_open,
	.write = asguard_rx_ctrl_write,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

static ssize_t asguard_verbose_ctrl_write(struct file *file,
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

	sdev->verbose = new_state;
	asguard_dbg("verbosity level set to %d\n", new_state);

	return count;
}

static int asguard_verbose_ctrl_show(struct seq_file *m, void *v)
{
	struct asguard_device *sdev = (struct asguard_device *)m->private;
	int i;

	if (!sdev)
		return -ENODEV;

	seq_printf(m, "asguard device verbosity level is set to %d\n", sdev->verbose);

	return 0;
}

static int asguard_verbose_ctrl_open(struct inode *inode, struct file *file)
{
	return single_open(file, asguard_verbose_ctrl_show,
			   PDE_DATA(file_inode(file)));
}

static const struct file_operations asguard_verbose_ctrl_ops = {
	.owner = THIS_MODULE,
	.open = asguard_verbose_ctrl_open,
	.write = asguard_verbose_ctrl_write,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

void init_asguard_ctrl_interfaces(struct asguard_device *sdev)
{
	char name_buf[MAX_ASGUARD_PROC_NAME];

	snprintf(name_buf, sizeof(name_buf), "asguard/%d/rx_ctrl", sdev->ifindex);
	proc_create_data(name_buf, S_IRWXU | S_IRWXO, NULL,
			 &asguard_core_ctrl_ops, sdev);

	snprintf(name_buf, sizeof(name_buf), "asguard/%d/debug", sdev->ifindex);
	proc_create_data(name_buf, S_IRWXU | S_IRWXO, NULL,
			 &asguard_verbose_ctrl_ops, sdev);
}
EXPORT_SYMBOL(init_asguard_ctrl_interfaces);

void clean_asguard_ctrl_interfaces(struct asguard_device *sdev)
{
	char name_buf[MAX_ASGUARD_PROC_NAME];

	snprintf(name_buf, sizeof(name_buf), "asguard/%d/rx_ctrl", sdev->ifindex);
	remove_proc_entry(name_buf, NULL);

	snprintf(name_buf, sizeof(name_buf), "asguard/%d/debug", sdev->ifindex);
	remove_proc_entry(name_buf, NULL);
}
EXPORT_SYMBOL(clean_asguard_ctrl_interfaces);

