
#include <linux/ip.h>
#include <linux/netdevice.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <net/ip.h>
#include <net/net_namespace.h>
#include <net/udp.h>

#include <asguard/asguard.h>
#include <asguard/logger.h>

const char *ts_state_string(enum tsstate state)
{
	switch (state) {
	case ASGUARD_TS_RUNNING:
		return "ASGUARD_TS_RUNNING";
	case ASGUARD_TS_READY:
		return "ASGUARD_TS_READY";
	case ASGUARD_TS_UNINIT:
		return "ASGUARD_TS_UNINIT";
	case ASGUARD_TS_LOG_FULL:
		return "ASGUARD_TS_LOG_FULL";
	default:
		return "UNKNOWN STATE";
	}
}

void ts_state_transition_to(struct asguard_device *sdev,
			    enum tsstate state)
{
	asguard_dbg("State Transition from %s to %s\n",
		  ts_state_string(sdev->ts_state), ts_state_string(state));
	sdev->ts_state = state;
}

static ssize_t asguard_proc_write(struct file *file, const char __user *buffer,
				size_t count, loff_t *data)
{
	/* Do nothing on write. */
	return count;
}

static int asguard_proc_show(struct seq_file *m, void *v)
{
	int i;
	struct asguard_timestamp_logs *logs =
		(struct asguard_timestamp_logs *)m->private;

	BUG_ON(!logs);

	for (i = 0; i < logs->current_timestamps; i++)
		seq_printf(m, "%llu, %d\n",
				logs->timestamp_items[i].timestamp_tcs,
				logs->timestamp_items[i].target_id);

	return 0;
}

static int asguard_proc_open(struct inode *inode, struct file *file)
{
	return single_open(file, asguard_proc_show, PDE_DATA(inode));
}

static const struct file_operations asguard_procfs_ops = {
	.owner = THIS_MODULE,
	.open = asguard_proc_open,
	.write = asguard_proc_write,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

/* Get the corresponding array for type and calls write_to_logmem. */
int asguard_write_timestamp(struct asguard_device *sdev,
			  int logid, uint64_t cycles,
			  int target_id)
{
	struct asguard_timestamp_logs *logs;


	if (!sdev||! sdev->stats||!sdev->stats->timestamp_logs[logid]) {
		asguard_dbg("Nullptr error in %s\n", __func__);
		return 0;
	}

	logs = sdev->stats->timestamp_logs[logid];

	if (unlikely(logs->current_timestamps > TIMESTAMP_ARRAY_LIMIT)) {

		asguard_dbg("Logs are full! Stopped tracking.%s\n",
		       __func__);

		asguard_ts_stop(sdev);
		ts_state_transition_to(sdev, ASGUARD_TS_LOG_FULL);
		return -ENOMEM;
	}

	logs->timestamp_items[logs->current_timestamps].timestamp_tcs = cycles;
	logs->timestamp_items[logs->current_timestamps].target_id = target_id;
	logs->current_timestamps += 1;

	return 0;
}
EXPORT_SYMBOL(asguard_write_timestamp);

int asguard_ts_stop(struct asguard_device *sdev)
{
	if (sdev->ts_state != ASGUARD_TS_RUNNING)
		return -EPERM;

	ts_state_transition_to(sdev, ASGUARD_TS_READY);
	return 0;
}

int asguard_ts_start(struct asguard_device *sdev)
{

	if (sdev->ts_state != ASGUARD_TS_READY) {
		asguard_error(" asguard is not in ready state. %s\n", __func__);
		goto error;
	}

	ts_state_transition_to(sdev, ASGUARD_TS_RUNNING);
	return 0;

error:
	return -EPERM;
}

int asguard_reset_stats(struct asguard_device *sdev)
{
	int err;
	int i;

	if (sdev == NULL||sdev->stats == NULL) {
		asguard_error(
			"can not clear stats, nullptr error.%s\n",
			__func__);
		err = -EINVAL;
		goto error;
	}

	if (sdev->ts_state == ASGUARD_TS_RUNNING) {
		asguard_error(
			" can not clear stats when timestamping is active.%s\n",
			__func__);
		err = -EPERM;
		goto error;
	}

	if (sdev->ts_state != ASGUARD_TS_READY) {
		asguard_error(
			"can not clear stats, asguard timestamping is in an undefined state.%s\n",
			__func__);
		err = -EPERM;
		goto error;
	}

	/* Reset, not free so timestamping can continue directly.*/
	for (i = 0; i < sdev->stats->timestamp_amount; i++) {
		if (!sdev->stats->timestamp_logs[i]) {
			asguard_error( "BUG! timestamp_log index does not exist. %s\n",__func__);
			err = -EPERM;
			goto error;
		} else {
			sdev->stats->timestamp_logs[i]->current_timestamps = 0;
		}

	}

	return 0;
error:
	asguard_error(" error code: %d for %s\n", err, __func__);
	return err;
}

int asguard_clean_timestamping(struct asguard_device *sdev)
{
	int err;
	int i;
	char name_buf[MAX_ASGUARD_PROC_NAME];
	int log_types = ASGUARD_NUM_TS_LOG_TYPES;

	if (!sdev->stats) {
		err = -EINVAL;
		asguard_error(" stats for active device is not valid %s\n",
			    __func__);
		goto error;
	}

	for (i = 0; i < sdev->stats->timestamp_amount; i++) {
		if (!sdev->stats->timestamp_logs[i])
			goto error;

		snprintf(name_buf, sizeof(name_buf), "asguard/%d/ts/data/%d",
			 sdev->ifindex, i);

		remove_proc_entry(name_buf, NULL);
	}

	snprintf(name_buf, sizeof(name_buf), "asguard/%d/ts/data", sdev->ifindex);
	remove_proc_entry(name_buf, NULL);

	snprintf(name_buf, sizeof(name_buf), "asguard/%d/ts/ctrl", sdev->ifindex);
	remove_proc_entry(name_buf, NULL);

	snprintf(name_buf, sizeof(name_buf), "asguard/%d/ts", sdev->ifindex);
	remove_proc_entry(name_buf, NULL);

	for (i = 0; i < log_types; i++) {
		if(!sdev->stats->timestamp_logs[i])
			continue;
		kfree(sdev->stats->timestamp_logs[i]->timestamp_items);
		kfree(sdev->stats->timestamp_logs[i]);
	}

	kfree(sdev->stats->timestamp_logs);
	kfree(sdev->stats);

	ts_state_transition_to(sdev, ASGUARD_TS_UNINIT);
	asguard_dbg(" cleanup done%s\n", __func__);
	return 0;
error:
	asguard_error(" error code: %d for %s\n", err, __func__);
	return err;
}
EXPORT_SYMBOL(asguard_clean_timestamping);

static int init_log_ctrl(struct asguard_device *sdev, int logid)
{
	int err;
	char name_buf[MAX_ASGUARD_PROC_NAME];

	if (sdev->verbose)
		asguard_dbg(" Init TS log with id: %d\n", logid);

	if (!sdev->stats) {
		err = -ENOMEM;
		asguard_error(" Stats is not initialized!%s\n", __func__);
		goto error;
	}

	/* Generate device specific proc name for asguard stats */
	snprintf(name_buf, sizeof(name_buf), "asguard/%d/ts/data/%d",
		 sdev->ifindex, logid);

	sdev->stats->timestamp_logs[logid]->proc_dir =
		proc_create_data(name_buf, S_IRUSR | S_IROTH, NULL,
				 &asguard_procfs_ops,
				 sdev->stats->timestamp_logs[logid]);

	if (!sdev->stats->timestamp_logs[logid]->proc_dir) {
		err = -ENOMEM;
		asguard_error(
			" Could not create timestamps  procfs data entry%s\n",
			__func__);
		goto error;
	}
	//asguard_dbg(" Created %d procfs %s\n",  logid, __func__);
	return 0;

error:
	asguard_error(" error code: %d for %s\n", err, __func__);
	return err;
}


int init_timestamping(struct asguard_device *sdev)
{
	int err;
	int log_types = ASGUARD_NUM_TS_LOG_TYPES;
	int i;

	if (sdev->verbose)
		asguard_dbg(" asguard device setup started %s\n", __func__);

	if (!sdev) {
		err = -EINVAL;
		asguard_error(" asguard device is NULL %s\n", __func__);
		goto error;
	}

	sdev->stats = kmalloc(sizeof(const struct asguard_stats), GFP_ATOMIC);
	if (!sdev->stats) {
		err = -ENOMEM;
		asguard_error(" Could not allocate memory for stats.%s\n",
			    __func__);
		goto error;
	}

	sdev->stats->timestamp_logs =
		kmalloc_array(log_types,
			      sizeof(struct asguard_timestamp_logs *),
			      GFP_ATOMIC);
	if (!sdev->stats->timestamp_logs) {
		err = -ENOMEM;
		asguard_error(
			" Could not allocate memory for timestamp_logs pointer.%s\n",
			__func__);
		goto error;
	}


	sdev->stats->timestamp_amount = 0;

	for (i = 0; i < log_types; i++) {
		sdev->stats->timestamp_logs[i] = kmalloc(
			sizeof(struct asguard_timestamp_logs), GFP_ATOMIC);

		if (!sdev->stats->timestamp_logs[i]) {
			err = -ENOMEM;
			asguard_error(
				"Could not allocate memory for timestamp logs struct with logid %d\n",
				i);
			goto error;
		}

		sdev->stats->timestamp_amount++;

		sdev->stats->timestamp_logs[i]->timestamp_items =
			kmalloc_array(TIMESTAMP_ARRAY_LIMIT,
				      sizeof(const struct asguard_timestamp_item),
				      GFP_ATOMIC);

		if (!sdev->stats->timestamp_logs[i]->timestamp_items) {
			err = -ENOMEM;
			asguard_error(
				" Could not allocate memory for timestamp logs with logid: %d.\n",
				i);
			goto error;
		}

		sdev->stats->timestamp_logs[i]->current_timestamps = 0;

		init_log_ctrl(sdev, i);
	}

	ts_state_transition_to(sdev, ASGUARD_TS_READY);

	return 0;

error:
	asguard_error(" Error code: %d for %s\n", err, __func__);
	if (sdev && sdev->stats) {
		kfree(sdev->stats);
		for (i = 0; i < log_types; i++) {
			kfree(sdev->stats->timestamp_logs[i]->timestamp_items);
			kfree(sdev->stats->timestamp_logs[i]->name);
			kfree(sdev->stats->timestamp_logs[i]);
		}
		sdev->stats->timestamp_amount = 0;
	}
	return err;
}

