#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <net/net_namespace.h>

#include <sassy/sassy.h>
#include <sassy/logger.h>

#include "event_logger.h"


const char *el_state_string(enum le_logger_state state)
{
	switch (state) {
	case EL_RUNNING:
		return "EL_RUNNING";
	case EL_READY:
		return "EL_READY";
	case EL_UNINIT:
		return "EL_UNINIT";
	case EL_LOG_FULL:
		return "EL_LOG_FULL";
	default:
		return "UNKNOWN STATE";
	}
}

void el_state_transition_to(struct sassy_device *sdev,
			    enum le_logger_state state)
{
	sassy_dbg("State Transition from %s to %s\n",
		  el_state_string(sdev->el_state), el_state_string(state));
	sdev->el_state = state;
}

/* Get the corresponding array for type and calls write_to_logmem. */
int write_le_log(struct sassy_device *sdev,
			  enum echo_event_type type, uint64_t tcs)
{
	struct echo_event_logs *logs;
	
	if (sdev->el_state != EL_RUNNING)
		return 0;

	logs = sdev->echo_logs;

	if (unlikely(logs->current_entries > LE_EVENT_LOG_LIMIT)) {

		sassy_dbg("Logs are full! Stopped le logging.%s\n",
		       __FUNCTION__);

		sassy_le_log_stop(sdev);
		el_state_transition_to(sdev, EL_LOG_FULL);
		return -ENOMEM;
	}

	logs->events[logs->current_entries].timestamp_tcs = tcs;
	logs->events[logs->current_entries].type = type;
	logs->current_entries += 1;

	return 0;
}
EXPORT_SYMBOL(sassy_write_timestamp);

int sassy_le_log_stop(struct sassy_device *sdev)
{
	if (sdev->el_state == EL_UNINIT)
		return -EPERM;

	el_state_transition_to(sdev, EL_READY);
	return 0;
}

int sassy_le_log_start(struct sassy_device *sdev)
{

	if (sdev->el_state != EL_READY) {
		sassy_error("leader election logger is not in ready state. %s\n", __FUNCTION__);
		goto error;
	}

	el_state_transition_to(sdev, EL_RUNNING);
	return 0;

error:
	return -EPERM;
}

int sassy_le_log_reset(struct sassy_device *sdev)
{
	int err;
	int i;

	if (sdev->el_state == EL_RUNNING) {
		sassy_error(
			"can not clear leader election stats when it is active.%s\n",
			__FUNCTION__);
		err = -EPERM;
		goto error;
	}

	if (sdev->el_state != EL_READY) {
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


static ssize_t sassy_el_log_write(struct file *file, const char __user *buffer,
				size_t count, loff_t *data)
{
	/* Do nothing on write. */
	return count;
}

static int sassy_el_log_show(struct seq_file *m, void *v)
{
	int i;
	struct echo_event_logs *logs =
		(struct echo_event_logs *)m->private;

	BUG_ON(!logs);

	for (i = 0; i < logs->current_entries; i++)
		seq_printf(m, "%d, %llu\n", 
							logs->events[i].type, 
							logs->events[i].timestamp_tcs);

	return 0;
}

static int sassy_el_log_open(struct inode *inode, struct file *file)
{
	return single_open(file, sassy_le_log_show, PDE_DATA(inode));
}

static const struct file_operations sassy_le_log_ops = {
	.owner = THIS_MODULE,
	.open = sassy_el_log_open,
	.write = sassy_el_log_write,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};


static int init_el_log_ctrl(struct sassy_device *sdev)
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

	snprintf(name_buf, sizeof(name_buf), "sassy/%d/log/el_log",
		 sdev->ifindex);

	sdev->le_logs->proc_dir =
		proc_create_data(name_buf, S_IRUSR | S_IROTH, NULL,
				 &sassy_le_log_ops,
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

	el_state_transition_to(sdev, EL_READY);

error:
	return err;
}