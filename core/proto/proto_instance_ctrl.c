/*
 * selecting protocol for sassy device
 */

#include <linux/kernel.h>
#include <linux/slab.h>

#include <sassy/sassy.h>
#include <sassy/logger.h>

#include <linux/list.h>

#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/module.h>

#include <linux/err.h>

#include "../sassy_core.h"

#undef LOG_PREFIX
#define LOG_PREFIX "[SASSY][PROTO INSTANCE CTRL]"

static ssize_t proto_instance_ctrl_write(struct file *file,
				    const char __user *user_buffer,
				    size_t count, loff_t *data)
{
	
	return count;
}

static int proto_instance_ctrl_show(struct seq_file *m, void *v)
{
	struct sassy_device *sdev =
		(struct sassy_device *)m->private;

	return 0;
}

static int proto_instance_ctrl_open(struct inode *inode, struct file *file)
{
	return single_open(file, proto_instance_ctrl_show,
			   PDE_DATA(file_inode(file)));
}

static const struct file_operations proto_instance_ctrl_ops = {
	.owner = THIS_MODULE,
	.open = proto_instance_ctrl_open,
	.write = proto_instance_ctrl_write,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

void init_proto_instance_ctrl(struct sassy_device *sdev)
{
	char name_buf[MAX_SASSY_PROC_NAME];

	snprintf(name_buf, sizeof(name_buf), "sassy/%d/proto_instances",
		 sdev->ifindex);
	proc_mkdir(name_buf, NULL);

	snprintf(name_buf, sizeof(name_buf), "sassy/%d/proto_instances/ctrl",
		 sdev->ifindex);

	proc_create_data(name_buf, S_IRWXU | S_IRWXO, NULL, &proto_instance_ctrl_ops,
			 &sdev);

	
}
EXPORT_SYMBOL(init_proto_instance_ctrl);


void remove_proto_instance_ctrl(struct sassy_device *sdev)
{
	char name_buf[MAX_SASSY_PROC_NAME];

	snprintf(name_buf, sizeof(name_buf), "sassy/%d/proto_instances/ctrl",
		 sdev->ifindex);
	remove_proc_entry(name_buf, NULL);

	snprintf(name_buf, sizeof(name_buf), "sassy/%d/proto_instances",
		 sdev->ifindex);
	remove_proc_entry(name_buf, NULL);
}
EXPORT_SYMBOL(remove_proto_instance_ctrl);
