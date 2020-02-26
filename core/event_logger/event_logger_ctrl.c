#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/module.h>
#include <linux/init.h>

#include <linux/kernel.h>
#include <asguard/asguard.h>
#include <asguard/logger.h>

static ssize_t asguard_event_ctrl_write(struct file *file,
				   const char __user *user_buffer, size_t count,
				   loff_t *data)
{
	int err;
	struct asguard_logger *slog =
		(struct asguard_logger *)PDE_DATA(file_inode(file));
	char kernel_buffer[ASGUARD_NUMBUF];
	int logging_state = -1;
	size_t size;

	size = min(sizeof(kernel_buffer) - 1, count);

	memset(kernel_buffer, 0, sizeof(kernel_buffer));

	err = copy_from_user(kernel_buffer, user_buffer, count);
	if (err) {
		asguard_error("Copy from user failed%s\n", __func__);
		goto error;
	}

	kernel_buffer[size] = '\0';
	err = kstrtoint(kernel_buffer, 10, &logging_state);
	if (err) {
		asguard_dbg("Error converting input buffer: %s, todevid: 0x%x\n",
		       kernel_buffer, logging_state);
		goto error;
	}

	switch (logging_state) {
	case 0:
		asguard_log_stop(slog);
		break;
	case 1:
		asguard_log_start(slog);
		break;
	case 2:
		asguard_log_reset(slog);
		break;
	default:
		asguard_error("Invalid input: %d - %s\n",
			    logging_state, __func__);
		err = -EINVAL;
		goto error;
	}
	return count;
error:
	asguard_error("Leader Election Logger control operation failed.%s\n", __func__);
	return err;
}

static int asguard_event_ctrl_show(struct seq_file *m, void *v)
{
	struct asguard_logger *slog =
		(struct asguard_logger *)m->private;

	if (!slog)
		return -ENODEV;

	seq_printf(m, "%s\n", logger_state_string(slog->state));

	return 0;
}

static int asguard_event_ctrl_open(struct inode *inode, struct file *file)
{
	return single_open(file, asguard_event_ctrl_show,
			   PDE_DATA(file_inode(file)));
}


static const struct file_operations asguard_event_ctrl_ops = {
	.owner = THIS_MODULE,
	.open = asguard_event_ctrl_open,
	.write = asguard_event_ctrl_write,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

void clear_logger(struct asguard_logger *slog)
{
	char name_buf[MAX_ASGUARD_PROC_NAME];
    int i;

	snprintf(name_buf, sizeof(name_buf),
			"asguard/%d/proto_instances/%d/log_%s",
			slog->ifindex, slog->instance_id, slog->name);

	remove_proc_entry(name_buf, NULL);

	snprintf(name_buf, sizeof(name_buf),
			"asguard/%d/proto_instances/%d/ctrl_%s",
			slog->ifindex, slog->instance_id, slog->name);

	remove_proc_entry(name_buf, NULL);
    asguard_error("%s - %d\n",__FUNCTION__,  __LINE__);

	if(slog->events)
	    kfree(slog->events);

    asguard_error("%s - %d\n",__FUNCTION__,  __LINE__);

}
EXPORT_SYMBOL(clear_logger);

void init_logger_ctrl(struct asguard_logger *slog)
{
	char name_buf[MAX_ASGUARD_PROC_NAME];

	if (!slog) {
		asguard_error("ins or Logs are not initialized!\n");
		return;
	}

	snprintf(name_buf, sizeof(name_buf),
			"asguard/%d/proto_instances/%d/ctrl_%s",
			slog->ifindex, slog->instance_id, slog->name);

	proc_create_data(name_buf, S_IRWXU | S_IRWXO, NULL, &asguard_event_ctrl_ops, slog);

	return;
}
EXPORT_SYMBOL(init_logger_ctrl);
