#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/slab.h>
#include <linux/uaccess.h>

#include <asgard/asgard.h>
#include <asgard/logger.h>

static ssize_t asgard_ts_ctrl_write(struct file *file,
				   const char __user *user_buffer, size_t count,
				   loff_t *data)
{
	int err;
	struct asgard_device *sdev =
		(struct asgard_device *)PDE_DATA(file_inode(file));
	char kernel_buffer[ASGARD_NUMBUF];
	int timestamping_state = -1;
	size_t size;

	size = min(sizeof(kernel_buffer) - 1, count);

	memset(kernel_buffer, 0, sizeof(kernel_buffer));

	err = copy_from_user(kernel_buffer, user_buffer, count);
	if (err) {
		asgard_error("[ASGARD] Copy from user failed%s\n", __func__);
		goto error;
	}

	kernel_buffer[size] = '\0';
	err = kstrtoint(kernel_buffer, 10, &timestamping_state);
	if (err) {
		asgard_dbg("[ASGARD] Error converting input buffer: %s, todevid: 0x%x\n",
		       kernel_buffer, timestamping_state);
		goto error;
	}

	switch (timestamping_state) {
	case 0:
		asgard_ts_stop(sdev);
		break;
	case 1:
		asgard_ts_start(sdev);
		break;
	case 2:
		asgard_reset_stats(sdev);
		break;
	default:
		asgard_error("[ASGARD] Invalid input: %d - %s\n",
			    timestamping_state, __func__);
		err = -EINVAL;
		goto error;
	}

	return count;
error:
	asgard_error("[ASGARD] Timestamping control failed.%s\n", __func__);
	return err;
}

static int asgard_ts_ctrl_show(struct seq_file *m, void *v)
{
	struct asgard_device *sdev =
		(struct asgard_device *)m->private;

	if (!sdev)
		return -ENODEV;

	seq_printf(m, "%s\n", ts_state_string(sdev->ts_state));

	return 0;
}

static int asgard_ts_ctrl_open(struct inode *inode, struct file *file)
{
	return single_open(file, asgard_ts_ctrl_show,
			   PDE_DATA(file_inode(file)));
}


static const struct file_operations asgard_ts_ctrl_ops = {
	.owner = THIS_MODULE,
	.open = asgard_ts_ctrl_open,
	.write = asgard_ts_ctrl_write,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

void init_asgard_ts_ctrl_interfaces(struct asgard_device *sdev)
{
	char name_buf[MAX_ASGARD_PROC_NAME];

	snprintf(name_buf, sizeof(name_buf), "asgard/%d/ts", sdev->ifindex);
	proc_mkdir(name_buf, NULL);

	snprintf(name_buf, sizeof(name_buf), "asgard/%d/ts/data", sdev->ifindex);
	proc_mkdir(name_buf, NULL);

	snprintf(name_buf, sizeof(name_buf), "asgard/%d/ts/ctrl", sdev->ifindex);

	proc_create_data(name_buf, S_IRWXU | S_IRWXO, NULL, &asgard_ts_ctrl_ops,
			 sdev);


}

