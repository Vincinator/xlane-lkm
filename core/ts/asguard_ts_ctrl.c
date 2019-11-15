#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/slab.h>
#include <linux/uaccess.h>

#include <asguard/asguard.h>
#include <asguard/logger.h>

static ssize_t asguard_ts_ctrl_write(struct file *file,
				   const char __user *user_buffer, size_t count,
				   loff_t *data)
{
	int err;
	struct asguard_device *sdev =
		(struct asguard_device *)PDE_DATA(file_inode(file));
	char kernel_buffer[ASGUARD_NUMBUF];
	int timestamping_state = -1;
	size_t size;

	size = min(sizeof(kernel_buffer) - 1, count);

	memset(kernel_buffer, 0, sizeof(kernel_buffer));

	err = copy_from_user(kernel_buffer, user_buffer, count);
	if (err) {
		asguard_error("[ASGUARD] Copy from user failed%s\n", __func__);
		goto error;
	}

	kernel_buffer[size] = '\0';
	err = kstrtoint(kernel_buffer, 10, &timestamping_state);
	if (err) {
		asguard_dbg("[ASGUARD] Error converting input buffer: %s, todevid: 0x%x\n",
		       kernel_buffer, timestamping_state);
		goto error;
	}

	switch (timestamping_state) {
	case 0:
		asguard_ts_stop(sdev);
		break;
	case 1:
		asguard_ts_start(sdev);
		break;
	case 2:
		asguard_reset_stats(sdev);
		break;
	default:
		asguard_error("[ASGUARD] Invalid input: %d - %s\n",
			    timestamping_state, __func__);
		err = -EINVAL;
		goto error;
	}
	asguard_dbg("[ASGUARD] Timestamping state changed successfully.%s\n",
		  __func__);
	return count;
error:
	asguard_error("[ASGUARD] Timestamping control failed.%s\n", __func__);
	return err;
}

static int asguard_ts_ctrl_show(struct seq_file *m, void *v)
{
	struct asguard_device *sdev =
		(struct asguard_device *)m->private;

	if (!sdev)
		return -ENODEV;

	seq_printf(m, "%s\n", ts_state_string(sdev->ts_state));

	return 0;
}

static int asguard_ts_ctrl_open(struct inode *inode, struct file *file)
{
	return single_open(file, asguard_ts_ctrl_show,
			   PDE_DATA(file_inode(file)));
}


static const struct file_operations asguard_ts_ctrl_ops = {
	.owner = THIS_MODULE,
	.open = asguard_ts_ctrl_open,
	.write = asguard_ts_ctrl_write,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

void init_asguard_ts_ctrl_interfaces(struct asguard_device *sdev)
{
	char name_buf[MAX_ASGUARD_PROC_NAME];

	snprintf(name_buf, sizeof(name_buf), "asguard/%d/ts", sdev->ifindex);
	proc_mkdir(name_buf, NULL);

	snprintf(name_buf, sizeof(name_buf), "asguard/%d/ts/data", sdev->ifindex);
	proc_mkdir(name_buf, NULL);

	snprintf(name_buf, sizeof(name_buf), "asguard/%d/ts/ctrl", sdev->ifindex);

	proc_create_data(name_buf, S_IRWXU | S_IRWXO, NULL, &asguard_ts_ctrl_ops,
			 sdev);

	asguard_dbg(
		"[ASGUARD] Timestamping ctrl interfaces created for device (%d)\n",
		sdev->ifindex);
}

