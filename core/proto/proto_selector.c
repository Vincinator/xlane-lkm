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
#define LOG_PREFIX "[SASSY][CORE]"

/* Only used for debugging. The goal is to load all protocols and select based on the protocol id in the payload.
 * For now, we use only one protocol at a time - to eliminate potential error sources.
 * ... and to see if this protocol multiplexing introduces latency or (worse) jitter.
 */
static ssize_t proto_selector_write(struct file *file,
				    const char __user *user_buffer,
				    size_t count, loff_t *data)
{
	int err;
	char kernel_buffer[MAX_PROCFS_BUF];
	struct sassy_device *sdev =
		(struct sassy_device *)PDE_DATA(file_inode(file));
	struct sassy_protocol *sproto = NULL;
	long new_protocol = -1;

	if (!sdev)
		return -ENODEV;

	if (count == 0) {
		sassy_error("sassy device is NULL.\n");
		return -EINVAL;
	}

	err = copy_from_user(kernel_buffer, user_buffer, count);

	if (err) {
		sassy_error("Copy from user failed%s\n", __FUNCTION__);
		return err;
	}

	kernel_buffer[count] = '\0';

	err = kstrtol(kernel_buffer, 0, &new_protocol);

	if (err) {
		sassy_error(" Error converting input%s\n", __FUNCTION__);
		return err;
	}

	sproto = generate_protocol(sdev, (new_protocol & 0xFF));

	if (!sproto) {
		sassy_error("Could not find protocol\n");
		return count;
	}

	if (sdev->pminfo.state == SASSY_PM_EMITTING) {
		sassy_error("Stop pacemaker first!\n");
		return count;
	}

	if (sdev->proto) {
		sassy_error("Stop and Clean previous protocol\n");
		sdev->proto->ctrl_ops.stop(sdev);
		sdev->proto->ctrl_ops.clean(sdev);
		kfree(sdev->proto); // free old protocol data
	}

	// switch to new protocol
	sdev->proto = sproto;
	sdev->proto->ctrl_ops.init(sdev);

	// Initialize leader election protocol implicitly
	sdev->le_proto = get_consensus_proto(sdev);
	sdev->le_proto->ctrl_ops.init(sdev);

	return count;
}

static int proto_selector_show(struct seq_file *m, void *v)
{
	struct sassy_device *sdev =
		(struct sassy_device *)m->private;

	if (!sdev) {
		sassy_error(" sdev is NULL %s!\n", __FUNCTION__);
		return -EINVAL;
	}
	if (!sdev->proto) {
		seq_puts(m, "sdev does not use a protocol yet\n");
		return -1;
	}

	seq_printf(m, "sdev uses protocol %s with id %d\n",
		   sassy_get_protocol_name(sdev->proto->proto_type),
		   sdev->proto->proto_type);

	return 0;
}

static int proto_selector_open(struct inode *inode, struct file *file)
{
	return single_open(file, proto_selector_show,
			   PDE_DATA(file_inode(file)));
}

static const struct file_operations proto_selector_ops = {
	.owner = THIS_MODULE,
	.open = proto_selector_open,
	.write = proto_selector_write,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};


static void __clear_previous_le_proto(struct sassy_device *sdev)
{
	if (sdev->le_proto && sdev->pminfo.state != SASSY_PM_EMITTING) {
		sassy_dbg("Stop and Clean le protocol\n");
		sdev->le_proto->ctrl_ops.stop(sdev);
		sdev->le_proto->ctrl_ops.clean(sdev);
	} else {
		sassy_dbg("Leader election protocol was not loaded, or PM is emitting! \n");
	}

}

static ssize_t proto_le_selector_write(struct file *file,
				    const char __user *user_buffer,
				    size_t count, loff_t *data)
{
	int err;
	char kernel_buffer[MAX_PROCFS_BUF];
	struct sassy_device *sdev =
		(struct sassy_device *)PDE_DATA(file_inode(file));
	long new_protocol = -1;

	if (!sdev)
		return -ENODEV;

	if (count == 0) {
		sassy_error("sassy device is NULL.\n");
		return -EINVAL;
	}

	err = copy_from_user(kernel_buffer, user_buffer, count);

	if (err) {
		sassy_error("Copy from user failed%s\n", __FUNCTION__);
		return err;
	}

	kernel_buffer[count] = '\0';

	err = kstrtol(kernel_buffer, 0, &new_protocol);
	
	if (err) {
		sassy_error(" Error converting input%s\n", __FUNCTION__);
		return err;
	}

	if (sdev->pminfo.state == SASSY_PM_EMITTING) {
		sassy_error("Stop pacemaker first!\n");
		return -ENODEV;
	}

	switch(new_protocol){
		case 0: // Stop (if initialized)
			if(!consensus_is_alive(sdev)){
				sassy_dbg("Leader Election is not running\n");
				break;
			}
			sdev->le_proto->ctrl_ops.stop(sdev);

			break;
		case 1: // Start (if initialized)
			if(consensus_is_alive(sdev)){
				sassy_dbg("Leader Election is already running\n");
				break;
			}

			sdev->le_proto->ctrl_ops.start(sdev);
			
			break;
		case 2: // Reset data
			if(consensus_is_alive(sdev))
				sdev->le_proto->ctrl_ops.stop(sdev);
			
			sdev->le_proto->ctrl_ops.clean(sdev);

			break;

		default:
			sassy_error("Invalid Input.\n");
			sassy_error("Valid inputs are:\n");
			sassy_error("     0 for clearing leader election protocol\n");
			sassy_error("     1 for initializing leader election protocol\n");
			break;
	}
	return count;
}

static int proto_le_selector_show(struct seq_file *m, void *v)
{
	struct sassy_device *sdev =
		(struct sassy_device *)m->private;

	if (!sdev) {
		sassy_error(" sdev is NULL %s!\n", __FUNCTION__);
		return -EINVAL;
	}
	if (!sdev->le_proto) {
		seq_puts(m, "sdev does not use a protocol yet\n");
		return -1;
	}

	seq_printf(m, "State is %s\n", le_state_name(sdev));

	return 0;
}

static int proto_le_selector_open(struct inode *inode, struct file *file)
{
	return single_open(file, proto_le_selector_show,
			   PDE_DATA(file_inode(file)));
}

static const struct file_operations proto_le_selector_ops = {
	.owner = THIS_MODULE,
	.open = proto_le_selector_open,
	.write = proto_le_selector_write,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

void init_proto_selector(struct sassy_device *sdev)
{
	char name_buf[MAX_SASSY_PROC_NAME];

	snprintf(name_buf, sizeof(name_buf),
		"sassy/%d/protocol", sdev->ifindex);

	proc_create_data(name_buf, 0707, NULL, &proto_selector_ops,
			 sdev);

	snprintf(name_buf, sizeof(name_buf),
		"sassy/%d/le_protocol", sdev->ifindex);

	proc_create_data(name_buf, 0707, NULL, &proto_le_selector_ops,
			 sdev);

	sassy_dbg(" added proto selector\n");
}

void remove_proto_selector(struct sassy_device *sdev)
{
	char name_buf[MAX_SASSY_PROC_NAME];

	snprintf(name_buf, sizeof(name_buf),
		"sassy/%d/protocol", sdev->ifindex);

	remove_proc_entry(name_buf, NULL);

	snprintf(name_buf, sizeof(name_buf),
		"sassy/%d/le_protocol", sdev->ifindex);

	remove_proc_entry(name_buf, NULL);
}
