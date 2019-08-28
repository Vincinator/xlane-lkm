#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <net/net_namespace.h>

#include <sassy/sassy.h>
#include <sassy/logger.h>

#include "event_logger.h"


const char *lel_state_string(enum tsstate state)
{
	switch (state) {
	case LEL_RUNNING:
		return "LEL_RUNNING";
	case LEL_READY:
		return "LEL_READY";
	case LEL_UNINIT:
		return "LEL_UNINIT";
	case LEL_LOG_FULL:
		return "LEL_LOG_FULL";
	default:
		return "UNKNOWN STATE";
	}
}

void lel_state_transition_to(struct sassy_device *sdev,
			    enum le_logger_state state)
{
	sassy_dbg("State Transition from %s to %s\n",
		  lel_state_string(sdev->ts_state), lel_state_string(state));
	sdev->lel_state = state;
}

/* Get the corresponding array for type and calls write_to_logmem. */
int write_le_log(struct sassy_device *sdev,
			  enum le_event_type type, uint64_t tcs)
{
	struct le_event_logs *logs;

	logs = sdev->le_logs;

	if (unlikely(logs->current_entries > LE_EVENT_LOG_LIMIT)) {

		sassy_dbg("Logs are full! Stopped le logging.%s\n",
		       __FUNCTION__);

		sassy_le_log_stop(sdev);

		return -ENOMEM;
	}

	logs->events[logs->current_timestamps].timestamp_tcs = cycles;
	logs->events[logs->current_timestamps].type = type;
	logs->current_timestamps += 1;

	return 0;
}
EXPORT_SYMBOL(sassy_write_timestamp);

int sassy_le_log_stop(struct sassy_device *sdev)
{
	if (sdev->lel_state-> != LEL_RUNNING)
		return -EPERM;

	lel_state_transition_to(sdev, LEL_READY);
	return 0;
}

int sassy_le_log_start(struct sassy_device *sdev)
{

	if (sdev->lel_state != SASSY_TS_READY) {
		sassy_error("leader election logger is not in ready state. %s\n", __FUNCTION__);
		goto error;
	}

	lel_state_transition_to(sdev, LEL_RUNNING);
	return 0;

error:
	return -EPERM;
}

int sassy_le_log_reset(struct sassy_device *sdev)
{
	int err;
	int i;

	if (sdev->lel_state == LEL_RUNNING) {
		sassy_error(
			"can not clear leader election stats when it is active.%s\n",
			__FUNCTION__);
		err = -EPERM;
		goto error;
	}

	if (sdev->lel_state != LEL_READY) {
		sassy_error(
			"can not clear stats, leader election is in an undefined state.%s\n",
			__FUNCTION__);
		err = -EPERM;
		goto error;
	}

	sdev->le_logs->current_entries = 0;

	return 0;
error:
	sassy_error(" error code: %d for %s\n", err, __FUNCTION__);
	return err;
}


static ssize_t sassy_le_log_write(struct file *file, const char __user *buffer,
				size_t count, loff_t *data)
{
	/* Do nothing on write. */
	return count;
}

static int sassy_le_log_show(struct seq_file *m, void *v)
{
	int i;
	struct le_event_logs *logs =
		(struct le_event_logs *)m->private;

	BUG_ON(!logs);

	for (i = 0; i < logs->current_timestamps; i++)
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


static int init_le_log_ctrl(struct sassy_device *sdev)
{
	int err;
	char name_buf[MAX_SASSY_PROC_NAME];

	if (sdev->verbose)
		sassy_dbg("Init leader election event log\n");

	if (!sdev->le_logs) {
		err = -ENOMEM;
		sassy_error("Leader Election logs are not initialized!\n");
		goto error;
	}

	snprintf(name_buf, sizeof(name_buf), "sassy/%d/log/le_log",
		 sdev->ifindex, logid);

	sdev->le_logs->proc_dir =
		proc_create_data(name_buf, S_IRUSR | S_IROTH, NULL,
				 &sassy_procfs_ops,
				 sdev->le_logs);

	if (!sdev->le_logs->proc_dir) {
		err = -ENOMEM;
		sassy_error(
			" Could not create leader election log procfs data entry%s\n",
			__FUNCTION__);
		goto error;
	}
	return 0;

error:
	sassy_error(" error code: %d for %s\n", err, __FUNCTION__);
	return err;
}


int init_le_logging(struct sassy_device *sdev) 
{
	char name_buf[MAX_SASSY_PROC_NAME];
	int err;
	int i;
	
	if (sdev->verbose)
		sassy_dbg(" sassy device setup started %s\n", __FUNCTION__);

	if (!sdev) {
		err = -EINVAL;
		sassy_error(" sassy device is NULL %s\n", __FUNCTION__);
		goto error;
	}

	sdev->le_logs = kmalloc(sizeof(struct le_event_logs), GFP_KERNEL);

	if (!sdev->le_logs) {
		err = -ENOMEM;
		sassy_error(" Could not allocate memory for leader election stats. %s\n",
			    __FUNCTION__);
		goto error;
	}

	sdev->le_logs->events = kmalloc_array(LE_EVENT_LOG_LIMIT,
									sizeof(struct le_event),
									GFP_KERNEL);

	if (!sdev->le_logs->events) {
		err = -ENOMEM;
		sassy_error(
			"Could not allocate memory for leader election logs intems: %d.\n", i);
		goto error;
	}

	sdev->le_logs->current_entries = 0;

	init_le_log_ctrl(sdev);


error:
	return err;
}