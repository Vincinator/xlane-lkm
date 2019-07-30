#include <linux/sassy.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>

#include <sassy/sassy.h>
#include <sassy/logger.h>
#include <sassy/sassy_ts.h>

static ssize_t sassy_ts_ctrl_write(struct file *file,
				   const char __user *user_buffer, size_t count,
				   loff_t *data)
{
	int err;
	struct sassy_device *sdev =
		(struct sassy_device *)PDE_DATA(file_inode(file));
	char kernel_buffer[SASSY_NUMBUF];
	int timestamping_state = -1;
	size_t size;

	size = min(sizeof(kernel_buffer) - 1, count);

	sassy_error("[SASSY] Write init count=%lu %s\n", count, __FUNCTION__);
	memset(kernel_buffer, 0, sizeof(kernel_buffer));

	err = copy_from_user(kernel_buffer, user_buffer, count);
	if (err) {
		sassy_error("[SASSY] Copy from user failed%s\n", __FUNCTION__);
		goto error;
	}

	kernel_buffer[size] = '\0';
	err = kstrtoint(kernel_buffer, 10, &timestamping_state);
	if (err) {
		printk(KERN_WARNING
		       "[SASSY] Error converting input buffer: %s, todevid: 0x%x \n",
		       kernel_buffer, timestamping_state);
		goto error;
	}

	switch (timestamping_state) {
	case 0:
		sassy_ts_stop(sdev);
		break;
	case 1:
		sassy_ts_start(sdev);
		break;
	case 2:
		sassy_reset_stats(sdev);
		break;
	default:
		sassy_error("[SASSY] Invalid input: %d - %s\n",
			    timestamping_state, __FUNCTION__);
		err = -EINVAL;
		goto error;
	}
	sassy_dbg("[SASSY] Timestamping state changed successfully.%s\n",
		  __FUNCTION__);
	return count;
error:
	sassy_error("[SASSY] Timestamping control failed.%s\n", __FUNCTION__);
	return err;
}

static int sassy_ts_ctrl_show(struct seq_file *m, void *v)
{
	struct sassy_device *sdev = (struct sassy_device *)m->private;

	if (!sdev)
		return -ENODEV;

	seq_printf(m, "%s\n", ts_state_string(sdev->ts_state));

	return 0;
}

static int sassy_ts_ctrl_open(struct inode *inode, struct file *file)
{
	return single_open(file, sassy_ts_ctrl_show,
			   PDE_DATA(file_inode(file)));
}

static const struct file_operations sassy_ts_ctrl_ops = {
	.owner = THIS_MODULE,
	.open = sassy_ts_ctrl_open,
	.write = sassy_ts_ctrl_write,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

void init_sassy_ts_ctrl_interfaces(struct sassy_device *sdev)
{
	char name_buf[MAX_SASSY_PROC_NAME];

	snprintf(name_buf, sizeof name_buf, "sassy/%d/ts", sdev->ifindex);
	proc_mkdir(name_buf, NULL);

	snprintf(name_buf, sizeof name_buf, "sassy/%d/ts/data", sdev->ifindex);
	proc_mkdir(name_buf, NULL);

	snprintf(name_buf, sizeof name_buf, "sassy/%d/ts/ctrl", sdev->ifindex);
	proc_create_data(name_buf, S_IRWXU | S_IRWXO, NULL, &sassy_ts_ctrl_ops,
			 sdev);

	sassy_dbg(
		"[SASSY] Timestamping ctrl interfaces created for device (%d)\n",
		sdev->ifindex);
}