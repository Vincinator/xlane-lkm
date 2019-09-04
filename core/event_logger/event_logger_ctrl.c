#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/slab.h>
#include <linux/uaccess.h>

#include <sassy/sassy.h>
#include <sassy/logger.h>
#include <sassy/sassy_ts.h>

static ssize_t sassy_event_ctrl_write(struct file *file,
				   const char __user *user_buffer, size_t count,
				   loff_t *data)
{
	int err;
	struct logger *slog =
		(struct logger *)PDE_DATA(file_inode(file));
	char kernel_buffer[SASSY_NUMBUF];
	int logging_state = -1;
	size_t size;

	size = min(sizeof(kernel_buffer) - 1, count);

	sassy_error("Write init count=%lu %s\n", count, __FUNCTION__);
	memset(kernel_buffer, 0, sizeof(kernel_buffer));

	err = copy_from_user(kernel_buffer, user_buffer, count);
	if (err) {
		sassy_error("Copy from user failed%s\n", __FUNCTION__);
		goto error;
	}

	kernel_buffer[size] = '\0';
	err = kstrtoint(kernel_buffer, 10, &logging_state);
	if (err) {
		sassy_dbg("Error converting input buffer: %s, todevid: 0x%x\n",
		       kernel_buffer, logging_state);
		goto error;
	}

	switch (logging_state) {
	case 0:
		sassy_log_stop(slog);
		break;
	case 1:
		sassy_log_start(slog);
		break;
	case 2:
		sassy_log_reset(slog);
		break;
	default:
		sassy_error("Invalid input: %d - %s\n",
			    logging_state, __FUNCTION__);
		err = -EINVAL;
		goto error;
	}
	sassy_dbg("Leader Election Logger state changed successfully.%s\n",
		  __FUNCTION__);
	return count;
error:
	sassy_error("Leader Election Logger control operation failed.%s\n", __FUNCTION__);
	return err;
}

static int sassy_event_ctrl_show(struct seq_file *m, void *v)
{
	struct logger *slog =
		(struct logger *)m->private;

	if (!slog)
		return -ENODEV;

	seq_printf(m, "%s\n", logger_state_string(slog->state));

	return 0;
}

static int sassy_event_ctrl_open(struct inode *inode, struct file *file)
{
	return single_open(file, sassy_event_ctrl_show,
			   PDE_DATA(file_inode(file)));
}


static const struct file_operations sassy_event_ctrl_ops = {
	.owner = THIS_MODULE,
	.open = sassy_event_ctrl_open,
	.write = sassy_event_ctrl_write,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

void init_logger_ctrl(struct logger *slog)
{
	char name_buf[MAX_SASSY_PROC_NAME];

	if (!slog->logs) {
		sassy_error("Logs are not initialized!\n");
		return -ENOMEM;
	}

	snprintf(name_buf, sizeof(name_buf), "sassy/%d/log/ctrl_%s",
		 slog->ifindex, slog->name);

	proc_create_data(name_buf, S_IRUSR | S_IROTH, NULL, &sassy_event_ctrl_ops, slog->logs);

	return 0;
}
EXPORT_SYMBOL(init_logger_ctrl);

void init_log_ctrl_base(struct sassy_device *sdev)
{
	char name_buf[MAX_SASSY_PROC_NAME];

	snprintf(name_buf, sizeof(name_buf), "sassy/%d/log", sdev->ifindex);
	proc_mkdir(name_buf, NULL);

}

