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
	char kernel_buffer[count + 1];
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
		sassy_error("Copy from user failed%s\n", __func__);
		return err;
	}

	kernel_buffer[count] = '\0';

	err = kstrtol(kernel_buffer, 0, &new_protocol);

	if (err) {
		sassy_error(" Error converting input%s\n", __func__);
		return err;
	}

	sproto = sassy_find_protocol_by_id((new_protocol & 0xFF));

	if (!sproto) {
		sassy_error("Could not find protocol\n");
		return count;
	}

	if (sproto == sdev->proto) {
		sassy_error("Protocol already enabled.\n");
		return count;
	}

	if (sdev->pminfo.state == SASSY_PM_EMITTING) {
		sassy_error("Stop pacemaker first!\n");
		return count;
	}

	if (sdev->proto) {
		sassy_dbg("Cleaning up Old Protocol\n");
		sdev->proto->ctrl_ops.clean(sdev);
	}

	// switch to new protocol
	sdev->proto = sproto;
	sdev->proto->ctrl_ops.init(sdev);

	return count;
}

static int proto_selector_show(struct seq_file *m, void *v)
{
	struct sassy_device *sdev =
		(struct sassy_device *)m->private;

	if (!sdev) {
		sassy_error(" sdev is NULL %s!\n", __func__);
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

void init_proto_selector(struct sassy_device *sdev)
{
	char name_buf[MAX_SASSY_PROC_NAME];

	snprintf(name_buf, sizeof(name_buf),
		"sassy/%d/protocol", sdev->ifindex);

	proc_create_data(name_buf, 0707, NULL, &proto_selector_ops,
			 sdev);

	sassy_dbg(" added proto selector\n");
}

void remove_proto_selector(struct sassy_device *sdev)
{
	char name_buf[MAX_SASSY_PROC_NAME];

	snprintf(name_buf, sizeof(name_buf),
			"sassy/%d/protocol", sdev->ifindex);

	remove_proc_entry(name_buf, NULL);
}
