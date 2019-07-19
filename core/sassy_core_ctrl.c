#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/module.h>
#include <linux/init.h>

#include <linux/kernel.h>

#include <sassy/sassy.h>
#include <sassy/logger.h>

static ssize_t sassy_rx_ctrl_write(struct file *file, const char __user *user_buffer, size_t count, loff_t *data)
{
	int err;
	char kernel_buffer[count+1];
	struct sassy_device *sdev = (struct sassy_device*)PDE_DATA(file_inode(file));
	long new_state = -1;

	if (!sdev)
		return -ENODEV;

	if (count == 0)
		return -EINVAL;

	err = copy_from_user(kernel_buffer, user_buffer, count);

	if (err) {
		sassy_error("Copy from user failed%s\n", __FUNCTION__);
		return err;
	}

	kernel_buffer[count] = '\0';

	err = kstrtol(kernel_buffer, 0, &new_state);

	if (err) {
		sassy_error(" Error converting input%s\n", __FUNCTION__);
		return err;
	}

	if(new_state == 0) {
		sdev->rx_state = SASSY_RX_DISABLED;
		sassy_dbg("RX disabled\n");

	} else {
		sdev->rx_state = SASSY_RX_ENABLED;
		sassy_dbg("RX enabled\n");
	}
}

static int sassy_rx_ctrl_show(struct seq_file *m, void *v)
{
	struct sassy_device *sdev = (struct sassy_device*) m->private;
	int i;

	if (!score)
		return -ENODEV;

	seq_printf(m, "sassy core RX state: %d\n", sdev->rx_state);
	
	return 0;
}

static int sassy_rx_ctrl_open(struct inode *inode, struct file *file)
{
	return single_open(file, sassy_rx_ctrl_show,PDE_DATA(file_inode(file)));
}


static const struct file_operations sassy_core_ctrl_ops = {
		.owner	= THIS_MODULE,
		.open	= sassy_rx_ctrl_open,
		.write	= sassy_rx_ctrl_write,
		.read	= seq_read,
		.llseek	= seq_lseek,
		.release = single_release,
};


void init_sassy_rx_ctrl_interfaces(struct sassy_device *sdev)
{
	char name_buf[MAX_SYNCBEAT_PROC_NAME];

	snprintf(name_buf, sizeof name_buf, "sassy/%d/rx_ctrl", sdev->ifindex);
	proc_create_data(name_buf, S_IRWXU|S_IRWXO, NULL, &sassy_core_ctrl_ops, sdev);

}
EXPORT_SYMBOL(init_sassy_rx_ctrl_interfaces);


void clean_sassy_rx_ctrl_interfaces(struct sassy_device *sdev)
{
	char name_buf[MAX_SYNCBEAT_PROC_NAME];

	snprintf(name_buf, sizeof name_buf, "sassy/%d/rx_ctrl", sdev->ifindex);
	remove_proc_entry(name_buf, NULL);

}
EXPORT_SYMBOL(clean_sassy_rx_ctrl_interfaces);
