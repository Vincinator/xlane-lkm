#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <net/net_namespace.h>

#include <asguard/asguard.h>
#include <asguard/logger.h>


const char *logger_state_string(enum logger_state state)
{
	switch (state) {
	case LOGGER_RUNNING:
		return "LOGGER_RUNNING";
	case LOGGER_READY:
		return "LOGGER_READY";
	case LOGGER_UNINIT:
		return "LOGGER_UNINIT";
	case LOGGER_LOG_FULL:
		return "LOGGER_LOG_FULL";
	default:
		return "UNKNOWN STATE";
	}
}


void logger_state_transition_to(struct asguard_logger *slog,
			    enum logger_state state)
{
	slog->state = state;
}

int write_log(struct asguard_logger *slog,
			  int type, uint64_t tcs)
{

	if (slog->state != LOGGER_RUNNING)
		return 0;


	if (unlikely(slog->current_entries > LOGGER_EVENT_LIMIT)) {

		asguard_dbg("Logs are full! Stopped event logging. %s\n", __func__);

		asguard_log_stop(slog);
		logger_state_transition_to(slog, LOGGER_LOG_FULL);
		return -ENOMEM;
	}

	slog->events[slog->current_entries].timestamp_tcs = tcs;
	slog->events[slog->current_entries].type = type;
	slog->events[slog->current_entries].accu_random_timeouts = 0;

	slog->current_entries += 1;

	return 0;
}
EXPORT_SYMBOL(write_log);

int asguard_log_stop(struct asguard_logger *slog)
{
	int err;

	if (!slog) {
		asguard_error("logger is not initialized\n");
		err = -EPERM;
		goto error;
	}

	if (slog->state == LOGGER_UNINIT) {
		asguard_error("logger is not in uninit state. %s\n", __func__);
		err = -EPERM;
		goto error;
	}

	logger_state_transition_to(slog, LOGGER_READY);
	return 0;

error:
	asguard_error("can not stop logger\n");
	return -EPERM;
}

int asguard_log_start(struct asguard_logger *slog)
{
	int err;

	if (!slog) {
		asguard_error("logger is not initialized.\n");
		err = -EPERM;
		goto error;
	}

	if (slog->state != LOGGER_READY) {
		asguard_error("logger is not in ready state. %s\n", __func__);
		goto error;
	}

	logger_state_transition_to(slog, LOGGER_RUNNING);
	return 0;

error:
	asguard_error("can not start logger\n");
	return -EPERM;
}

int asguard_log_reset(struct asguard_logger *slog)
{
	int err;

	if (!slog) {
		asguard_error("logger is not initialized properly.\n");
		err = -EPERM;
		goto error;
	}

	if (slog->state == LOGGER_RUNNING) {
		asguard_error(
			"can not clear logger when it is active.%s\n",
			__func__);
		err = -EPERM;
		goto error;
	}

	if (slog->state != LOGGER_READY) {
		asguard_error(
			"can not clear stats, logger is in an undefined state.%s\n",
			__func__);
		err = -EPERM;
		goto error;
	}

	slog->current_entries = 0;

	return 0;
error:
	asguard_error("Can not reset logs.\n");
	asguard_error("error code: %d for %s\n", err, __func__);
	return err;
}


static ssize_t asguard_log_write(struct file *file, const char __user *buffer,
				size_t count, loff_t *data)
{
	/* Do nothing on write. */
	return count;
}

static int asguard_log_show(struct seq_file *m, void *v)
{
	int i;
	struct asguard_logger *slog =
		(struct asguard_logger *)m->private;

	//BUG_ON(!slog);

	for (i = 0; i < slog->current_entries; i++) {
		seq_printf(m, "%d, %llu\n",
				slog->events[i].type,
				slog->events[i].timestamp_tcs);
	}

	return 0;
}

static int asguard_log_open(struct inode *inode, struct file *file)
{
	return single_open(file, asguard_log_show, PDE_DATA(inode));
}

static const struct file_operations asguard_log_ops = {
	.owner = THIS_MODULE,
	.open = asguard_log_open,
	.write = asguard_log_write,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};


static int init_logger_out(struct asguard_logger *slog)
{
	int err;
	char name_buf[MAX_ASGUARD_PROC_NAME];


	if (!slog) {
		err = -ENOMEM;
		asguard_error("Logs are not initialized!\n");
		goto error;
	}

	snprintf(name_buf, sizeof(name_buf),
			"asguard/%d/proto_instances/%d/log_%s",
			slog->ifindex, slog->instance_id, slog->name);


	proc_create_data(name_buf, S_IRUSR | S_IROTH, NULL, &asguard_log_ops, slog);

	return 0;

error:
	asguard_error("error code: %d for %s\n", err, __func__);
	return err;
}


int init_logger(struct asguard_logger *slog)
{
	int err;

	err = 0;

	if (!slog) {
		err = -EINVAL;
		asguard_error("logger device is NULL %s\n", __func__);
		goto error;
	}

	slog->events = kmalloc_array(LOGGER_EVENT_LIMIT, sizeof(struct logger_event), GFP_KERNEL);

	if (!slog->events) {
		err = -ENOMEM;
		asguard_error(
			"Could not allocate memory for leader election logs items\n");
		goto error;
	}

	slog->current_entries = 0;

	init_logger_out(slog);

	init_logger_ctrl(slog);

	logger_state_transition_to(slog, LOGGER_READY);

error:
	return err;
}

void remove_logger_ifaces(struct asguard_logger *slog)
{
	char name_buf[MAX_ASGUARD_PROC_NAME];

	snprintf(name_buf, sizeof(name_buf),
			"asguard/%d/proto_instances/%d/log_%s",
			slog->ifindex, slog->instance_id, slog->name);

	remove_proc_entry(name_buf, NULL);

	snprintf(name_buf, sizeof(name_buf),
			"asguard/%d/proto_instances/%d/ctrl_%s",
			slog->ifindex, slog->instance_id, slog->name);

	remove_proc_entry(name_buf, NULL);
}
EXPORT_SYMBOL(remove_logger_ifaces);
