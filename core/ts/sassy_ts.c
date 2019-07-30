
#include <linux/ip.h>
#include <linux/netdevice.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <net/ip.h>
#include <net/net_namespace.h>
#include <net/udp.h>

#include <sassy/sassy.h>
#include <sassy/logger.h>
#include <sassy/sassy_ts.h>

const char *ts_state_string(sassy_timestamping_state_t state)
{
	switch (state) {
	case SASSY_TS_RUNNING:
		return "SASSY_TS_RUNNING";
	case SASSY_TS_READY:
		return "SASSY_TS_READY";
	case SASSY_TS_UNINIT:
		return "SASSY_TS_UNINIT";
	case SASSY_TS_LOG_FULL:
		return "SASSY_TS_LOG_FULL";
	default:
		return "UNKNOWN STATE";
	}
}

void ts_state_transition_to(struct sassy_device *sdev,
			    sassy_timestamping_state_t state)
{
	sassy_dbg(" State Transition from %s to %s \n",
		  ts_state_string(sdev->pm_state), ts_state_string(state));
	sdev->ts_state = state;
}

int do_sassy_print_devices(void)
{
	sassy_dbg(" Printing registered Devices. %s\n", __FUNCTION__);
	print_sassy_devices();
	return 0;
}

int do_sassy_timestamping_clear_stats(void)
{
	return 0;
}

static ssize_t sassy_proc_write(struct file *file, const char __user *buffer,
				size_t count, loff_t *data)
{
	/* Do nothing on write. */
	return count;
}

static int sassy_proc_show(struct seq_file *m, void *v)
{
	int i;
	struct sassy_timestamp_logs *logs =
		(struct sassy_timestamp_logs *)m->private;

	BUG_ON(!logs);

	for (i = 0; i < logs->current_timestamps; i++) {
		seq_printf(m, "%llu\n", logs->timestamp_items[i].timestamp_tcs);
	}
	return 0;
}

static int sassy_proc_open(struct inode *inode, struct file *file)
{
	return single_open(file, sassy_proc_show, PDE_DATA(inode));
}

static const struct file_operations sassy_procfs_ops = {
	.owner = THIS_MODULE,
	.open = sassy_proc_open,
	.write = sassy_proc_write,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

/* Get the corresponding array for type and calls write_to_logmem. */
int sassy_write_timestamp(struct sassy_device *sdev,
			  struct sassy_timestamp_logs *logs,
			  enum sassy_timestamp_type type, uint64_t cycles,
			  int target_id)
{
	uint64_t *ptr; /* Pointer for ubuf access */

	if (unlikely(logs->current_timestamps > TIMESTAMP_ARRAY_LIMIT)) {
		sassy_ts_stop(
			sdev); /* Turn off Sassy tracking, logs are full!*/
		printk(KERN_WARNING " Logs are full! Stopped tracking.%s\n",
		       __FUNCTION__);
		return -ENOMEM;
	}
	logs->timestamp_items[logs->current_timestamps].timestamp_tcs = cycles;
	logs->timestamp_items[logs->current_timestamps].target_id = target_id;
	/* Hard coded.. proc id is always 0 for this eval, offset is 8 bytes */
	ptr = (uint64_t *)(sdev->ubuf);
	*ptr = cycles;
	logs->current_timestamps += 1;
	return 0;
}
EXPORT_SYMBOL(sassy_write_timestamp);

int sassy_ts_stop(struct sassy_device *sdev)
{
	if (sdev->ts_state != SASSY_TS_RUNNING)
		return -EPERM;

	ts_state_transition_to(sdev, SASSY_TS_READY);
	return 0;
}

int sassy_ts_start(struct sassy_device *sdev)
{
	/* Transition to SASSY_TS_RUNNING is only allowed if current state is
   * SASSY_TS_READY */
	if (sdev->ts_state != SASSY_TS_READY) {
		sassy_error(" sassy is not in ready state. %s\n", __FUNCTION__);
		goto error;
	}

	ts_state_transition_to(sdev, SASSY_TS_RUNNING);
	return 0;

error:
	return -EPERM;
}

int sassy_reset_stats(struct sassy_device *sdev)
{
	int err;
	int i;

	if (sdev->ts_state == SASSY_TS_RUNNING) {
		sassy_error(
			" can not clear stats when timestamping is active.%s\n",
			__FUNCTION__);
		err = -EPERM;
		goto error;
	}

	if (sdev->ts_state != SASSY_TS_READY) {
		sassy_error(
			" can not clear stats, sassy timestamping is in an undefined "
			"state.%s\n",
			__FUNCTION__);
		err = -EPERM;
		goto error;
	}

	/* Reset, not free so timestamping can continue directly.*/
	for (i = 0; i < sdev->stats->timestamp_amount; i++)
		sdev->stats->timestamp_logs[i]->current_timestamps = 0;

	return 0;
error:
	sassy_error(" error code: %d for %s\n", err, __FUNCTION__);
	return err;
}

int sassy_clean_timestamping(struct sassy_device *sdev)
{
	int err;
	int i;
	char name_buf[MAX_SASSY_PROC_NAME];
	int log_types = SASSY_NUM_TS_LOG_TYPES;

	if (!sdev->stats) {
		err = -EINVAL;
		sassy_error(" stats for active device is not valid %s\n",
			    __FUNCTION__);
		goto error;
	}

	for (i = 0; i < sdev->stats->timestamp_amount; i++) {
		if (!sdev->stats->timestamp_logs[i])
			goto error;

		snprintf(name_buf, sizeof name_buf, "sassy/%d/ts/data/%s",
			 sdev->ifindex, sdev->stats->timestamp_logs[i]->name);

		remove_proc_entry(name_buf, NULL);
	}

	snprintf(name_buf, sizeof name_buf, "sassy/%d/ts/data", sdev->ifindex);
	remove_proc_entry(name_buf, NULL);

	snprintf(name_buf, sizeof name_buf, "sassy/%d/ts/ctrl", sdev->ifindex);
	remove_proc_entry(name_buf, NULL);

	snprintf(name_buf, sizeof name_buf, "sassy/%d/ts", sdev->ifindex);
	remove_proc_entry(name_buf, NULL);

	for (i = 0; i < log_types; i++) {
		kfree(sdev->stats->timestamp_logs[i]->timestamp_items);
		kfree(sdev->stats->timestamp_logs[i]->name);
		kfree(sdev->stats->timestamp_logs[i]);
	}

	kfree(sdev->stats->timestamp_logs);
	kfree(sdev->stats);

	ts_state_transition_to(sdev, SASSY_TS_UNINIT);
	sassy_dbg(" cleanup done%s\n", __FUNCTION__);
	return 0;
error:
	sassy_error(" error code: %d for %s\n", err, __FUNCTION__);
	return err;
}

int init_timestamping(struct sassy_device *sdev)
{
	int err;
	char name_buf[MAX_SASSY_PROC_NAME];
	int cur_log_types = 0;
	int log_types = SASSY_NUM_TS_LOG_TYPES;
	int i;

	if (sdev->verbose)
		sassy_dbg(" syncbeat device setup started %s\n", __FUNCTION__);

	if (!sdev) {
		err = -EINVAL;
		sassy_error(" syncbeat device is NULL %s\n", __FUNCTION__);
		goto error;
	}

	sdev->stats = kmalloc(sizeof(struct syncbeat_stats), GFP_ATOMIC);
	if (!sdev->stats) {
		err = -ENOMEM;
		sassy_error(" Could not allocate memory for stats.%s\n",
			    __FUNCTION__);
		goto error;
	}

	sdev->stats->timestamp_logs =
		kmalloc_array(log_types,
			      sizeof(struct syncbeat_timestamp_logs *),
			      GFP_ATOMIC);
	if (!sdev->stats->timestamp_logs) {
		err = -ENOMEM;
		sassy_error(
			" Could not allocate memory for timestamp_logs pointer.%s\n",
			__FUNCTION__);
		goto error;
	}

	sdev->stats->timestamp_amount = 0;

	for (i = 0; i < log_types; i++) {
		sdev->stats->timestamp_logs[i] = kmalloc(
			sizeof(struct syncbeat_timestamp_logs), GFP_ATOMIC);

		if (!sdev->stats->timestamp_logs[i]) {
			err = -ENOMEM;
			sassy_error(
				" Could not allocate memory for timestamp logs struct with "
				"logid %d.\n",
				i);
			goto error;
		}

		sdev->stats->timestamp_amount++;

		sdev->stats->timestamp_logs[i]->timestamp_items =
			kmalloc_array(TIMESTAMP_ARRAY_LIMIT,
				      sizeof(struct syncbeat_timestamp_item),
				      GFP_ATOMIC);

		if (!sdev->stats->timestamp_logs[i]->timestamp_items) {
			err = -ENOMEM;
			sassy_error(
				" Could not allocate memory for timestamp logs with logid: %d.\n",
				i);
			goto error;
		}

		sdev->stats->timestamp_logs[i]->current_timestamps = 0;

		init_log_ctrl(sdev, i);
	}

	ts_state_transition_to(sdev, SYNCBEAT_TS_READY);

	return 0;

error:
	sassy_error(" Error code: %d for %s\n", err, __FUNCTION__);
	if (sdev && sdev->stats) {
		kfree(sdev->stats); /* free the stats for this failed init try. */
		for (i = 0; i < log_types; i++) {
			kfree(sdev->stats->timestamp_logs[i]->timestamp_items);
			kfree(sdev->stats->timestamp_logs[i]->name);
			kfree(sdev->stats->timestamp_logs[i]);
		}
		sdev->stats->timestamp_amount = 0;
	}
	return err;
}

int init_log_ctrl(struct sassy_device *sdev, int logid)
{
	int err;
	char name_buf[MAX_SASSY_PROC_NAME];

	if (sdev->verbose)
		sassy_dbg(" Init TS log with id: %d\n", logid);

	if (!sdev->stats) {
		err = -ENOMEM;
		sassy_error(" Stats is not initialized!%s\n", __FUNCTION__);
		goto error;
	}

	/* Generate device specific proc name for sassy stats */
	snprintf(name_buf, sizeof name_buf, "sassy/%d/ts/data/%d",
		 sdev->ifindex, logid);

	sdev->stats->timestamp_logs[logid]->proc_dir =
		proc_create_data(name_buf, S_IRUSR | S_IROTH, NULL,
				 &sassy_procfs_ops,
				 sdev->stats->timestamp_logs[logid]);

	if (!sdev->stats->timestamp_logs[logid]->proc_dir) {
		err = -ENOMEM;
		sassy_error(
			" Could not create timestamps  procfs data entry%s\n",
			__FUNCTION__);
		goto error;
	}
	sassy_dbg(" Created %s procfs %s\n", logname, __FUNCTION__);
	return 0;

error:
	sassy_error(" error code: %d for %s\n", err, __FUNCTION__);
	return err;
}
