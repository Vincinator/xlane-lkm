#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <net/net_namespace.h>

#include <sassy/sassy.h>
#include <sassy/logger.h>

void logger_state_transition_to(struct logger *slog,
			    enum le_logger_state state)
{
	slog->state = state;
}

int write_log(struct logger *slog,
			  int type, uint64_t tcs)
{
	struct event_logs *logs;
	
	if (slog->state != LOGGER_RUNNING)
		return 0;

	logs = slog->logs;

	if (unlikely(logs->current_entries > LOGGER_EVENT_LIMIT)) {

		sassy_dbg("Logs are full! Stopped event logging. %s\n", __FUNCTION__);

		sassy_le_log_stop(sdev);
		lel_state_transition_to(sdev, LOGGER_LOG_FULL);
		return -ENOMEM;
	}

	logs->events[logs->current_entries].timestamp_tcs = tcs;
	logs->events[logs->current_entries].type = type;
	logs->current_entries += 1;

	return 0;
}
EXPORT_SYMBOL(sassy_write_timestamp);

int sassy_log_stop(struct logger *slog)
{
	if (slog->state == LOGGER_UNINIT)
		return -EPERM;

	logger_state_transition_to(sdev, LOGGER_READY);
	return 0;
}

int sassy_log_start(struct logger *slog)
{

	if (slog->state != LOGGER_READY) {
		sassy_error("logger is not in ready state. %s\n", __FUNCTION__);
		goto error;
	}

	logger_state_transition_to(sdev, LOGGER_RUNNING);
	return 0;

error:
	return -EPERM;
}

int sassy_log_reset(struct logger *slog)
{
	int err;
	int i;

	if (slog->state == LOGGER_RUNNING) {
		sassy_error(
			"can not clear logger when it is active.%s\n",
			__FUNCTION__);
		err = -EPERM;
		goto error;
	}

	if (sdev->lel_state != LOGGER_READY) {
		sassy_error(
			"can not clear stats, logger is in an undefined state.%s\n",
			__FUNCTION__);
		err = -EPERM;
		goto error;
	}

	slog->logs->current_entries = 0;

	return 0;
error:
	sassy_error(" error code: %d for %s\n", err, __FUNCTION__);
	return err;
}


static ssize_t sassy_log_write(struct file *file, const char __user *buffer,
				size_t count, loff_t *data)
{
	/* Do nothing on write. */
	return count;
}

static int sassy_log_show(struct seq_file *m, void *v)
{
	int i;
	struct event_logs *logs =
		(struct event_logs *)m->private;

	BUG_ON(!logs);

	for (i = 0; i < logs->current_entries; i++)
		seq_printf(m, "%d, %llu\n", 
							logs->events[i].type, 
							logs->events[i].timestamp_tcs);

	return 0;
}

static int sassy_le_log_open(struct inode *inode, struct file *file)
{
	return single_open(file, sassy_le_log_show, PDE_DATA(inode));
}

static const struct file_operations sassy_le_log_ops = {
	.owner = THIS_MODULE,
	.open = sassy_le_log_open,
	.write = sassy_le_log_write,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};


static int init_logger_ctrl(struct logger *slog)
{
	int err;
	char name_buf[MAX_SASSY_PROC_NAME];

	if (!slog->logs) {
		err = -ENOMEM;
		sassy_error("Logs are not initialized!\n");
		goto error;
	}

	snprintf(name_buf, sizeof(name_buf), "sassy/%d/log/%s",
		 slog->ifindex, slog->name);

	sdev->le_logs->proc_dir =
		proc_create_data(name_buf, S_IRUSR | S_IROTH, NULL,
				 &sassy_log_ops,
				 slog->logs);

	if (!slog->logs->proc_dir) {
		err = -ENOMEM;
		sassy_error(" Could not create leader election log procfs data entry%s\n",
			__FUNCTION__);
		goto error;
	}
	return 0;

error:
	sassy_error(" error code: %d for %s\n", err, __FUNCTION__);
	return err;
}


int init_logger(struct logger *slog) 
{
	char name_buf[MAX_SASSY_PROC_NAME];
	int err;
	int i;
	

	if (!slog) {
		err = -EINVAL;
		sassy_error(" sassy device is NULL %s\n", __FUNCTION__);
		goto error;
	}

	slog->logs = kmalloc(sizeof(struct event_logs), GFP_KERNEL);

	if (!slog->le_logs) {
		err = -ENOMEM;
		sassy_error(" Could not allocate memory for logs. %s\n",
			    __FUNCTION__);
		goto error;
	}

	slog->logs->events = kmalloc_array(LOGGER_EVENT_LIMIT,
									sizeof(struct event),
									GFP_KERNEL);

	if (!slog->logs->events) {
		err = -ENOMEM;
		sassy_error(
			"Could not allocate memory for leader election logs items\n");
		goto error;
	}

	slog->logs->current_entries = 0;

	init_logger_ctrl(slog);

	logger_state_transition_to(sdev, LOGGER_READY);

error:
	return err;
}