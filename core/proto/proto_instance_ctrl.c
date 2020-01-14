/*
 * selecting protocol for asguard device
 */

#include <linux/kernel.h>
#include <linux/slab.h>

#include <asguard/asguard.h>
#include <asguard/logger.h>

#include <linux/list.h>

#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/module.h>

#include <linux/err.h>

#include "../asguard_core.h"

#undef LOG_PREFIX
#define LOG_PREFIX "[ASGUARD][PROTO INSTANCE CTRL]"

static ssize_t proto_instance_ctrl_write(struct file *file,
				    const char __user *user_buffer,
				    size_t count, loff_t *data)
{
	int err;
	char kernel_buffer[ASGUARD_TARGETS_BUF];
	char *search_str;
	struct asguard_device *sdev =
		(struct asguard_device *)PDE_DATA(file_inode(file));
	size_t size = min(sizeof(kernel_buffer) - 1, count);
	char *input_str;
	static const char delimiters[] = " ,;()";
	int state = 0;
	int instance_id, protocol_id;

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


	search_str = kstrdup(kernel_buffer, GFP_KERNEL);
	while ((input_str = strsep(&search_str, delimiters)) != NULL) {
		if (!input_str || strlen(input_str) <= 0)
			continue;
		if (state == 0) {
			err = kstrtoint(input_str, 10, &instance_id);


			if (err)
				goto error;

			if (instance_id == -1) {

				// clear all existing protocols and exit
				clear_protocol_instances(sdev);
				return count;
			}

			state = 1;
		} else if (state == 1) {
			err = kstrtoint(input_str, 10, &protocol_id);

			if (err)
				goto error;

			err = register_protocol_instance(sdev, instance_id, protocol_id);


			if (err)
				goto error;

		}
	}
	return count;
error:

	asguard_error("Error during parsing of input.%s\n", __func__);
	return err;
}

static int proto_instance_ctrl_show(struct seq_file *m, void *v)
{
	struct asguard_device *sdev =
		(struct asguard_device *)m->private;

	int i;

	if (!sdev ||!sdev->protos)
		return -ENODEV;

	seq_printf(m, "Total Instances: %d\n", sdev->num_of_proto_instances);

	for (i = 0; i < sdev->num_of_proto_instances; i++) {
		if (!sdev->protos[i]) {
			asguard_dbg("Proto init error detected! proto with idx %d\n", i);
			continue;
		}
		seq_printf(m, "Instance ID: %d\n", sdev->protos[i]->instance_id);
		seq_printf(m, "Protocol Type: %s\n", asguard_get_protocol_name(sdev->protos[i]->proto_type));
		seq_printf(m, "---\n");
	}

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

void init_proto_instance_ctrl(struct asguard_device *sdev)
{
	char name_buf[MAX_ASGUARD_PROC_NAME];

	snprintf(name_buf, sizeof(name_buf), "asguard/%d/proto_instances",
		 sdev->ifindex);

	proc_mkdir(name_buf, NULL);

	snprintf(name_buf, sizeof(name_buf), "asguard/%d/proto_instances/ctrl",
		 sdev->ifindex);

	proc_create_data(name_buf, S_IRWXU | S_IRWXO, NULL, &proto_instance_ctrl_ops,
			 sdev);

}
EXPORT_SYMBOL(init_proto_instance_ctrl);


void remove_proto_instance_ctrl(struct asguard_device *sdev)
{
	char name_buf[MAX_ASGUARD_PROC_NAME];

	snprintf(name_buf, sizeof(name_buf), "asguard/%d/proto_instances/ctrl",
		 sdev->ifindex);
	remove_proc_entry(name_buf, NULL);

	snprintf(name_buf, sizeof(name_buf), "asguard/%d/proto_instances",
		 sdev->ifindex);
	remove_proc_entry(name_buf, NULL);
}
EXPORT_SYMBOL(remove_proto_instance_ctrl);
