#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <net/net_namespace.h>

#include <sassy/sassy.h>
#include <sassy/logger.h>


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


void logger_state_transition_to(struct sassy_logger *slog,
			    enum logger_state state)
{
	slog->state = state;
}

int write_log(struct sassy_logger *slog,
			  int type, uint64_t tcs)
{
	
	if (slog->state != LOGGER_RUNNING)
		return 0;


	if (unlikely(slog->current_entries > LOGGER_EVENT_LIMIT)) {

		sassy_dbg("Logs are full! Stopped event logging. %s\n", __FUNCTION__);

		sassy_log_stop(slog);
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

int sassy_log_stop(struct sassy_logger *slog)
{
	int err;

	if(!slog) {
		sassy_error("logger is not initialized, can not stop logger.\n");
		err = -EPERM;
		goto error;
	}

	if (slog->state == LOGGER_UNINIT){
		err = -EPERM;
		goto error;
	}

	logger_state_transition_to(slog, LOGGER_READY);
	return 0;

error:
	return -EPERM;
}

int sassy_log_start(struct sassy_logger *slog)
{
	int err;

	if(!slog) {
		sassy_error("logger is not initialized, can not start logger.\n");
		err = -EPERM;
		goto error;
	}

	if (slog->state != LOGGER_READY) {
		sassy_error("logger is not in ready state. %s\n", __FUNCTION__);
		goto error;
	}

	logger_state_transition_to(slog, LOGGER_RUNNING);
	return 0;

error:
	return -EPERM;
}

int sassy_log_reset(struct sassy_logger *slog)
{
	int err;
	int i;

	if(!slog) {
		sassy_error("logger is not initialized properly! Can not clear logs.\n");
		err = -EPERM;
		goto error;
	}

	if (slog->state == LOGGER_RUNNING) {
		sassy_error(
			"can not clear logger when it is active.%s\n",
			__FUNCTION__);
		err = -EPERM;
		goto error;
	}

	if (slog->state != LOGGER_READY) {
		sassy_error(
			"can not clear stats, logger is in an undefined state.%s\n",
			__FUNCTION__);
		err = -EPERM;
		goto error;
	}

	slog->current_entries = 0;

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
	struct sassy_logger *slog =
		(struct sassy_logger *)m->private;

	//BUG_ON(!slog);

	for (i = 0; i < slog->current_entries; i++){
		seq_printf(m, "%d, %llu\n", 
				slog->events[i].type, 
				slog->events[i].timestamp_tcs);
	}

	return 0;
}

static int sassy_log_open(struct inode *inode, struct file *file)
{
	return single_open(file, sassy_log_show, PDE_DATA(inode));
}

static const struct file_operations sassy_log_ops = {
	.owner = THIS_MODULE,
	.open = sassy_log_open,
	.write = sassy_log_write,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};


static int init_logger_out(struct sassy_logger *slog)
{
	int err;
	char name_buf[MAX_SASSY_PROC_NAME];


	if (!slog) {
		err = -ENOMEM;
		sassy_error("Logs are not initialized!\n");
		goto error;
	}

	snprintf(name_buf, sizeof(name_buf), "sassy/%d/proto_instances/%d/log_%s",
		 slog->ifindex, slog->instance_id, slog->name);


	proc_create_data(name_buf, S_IRUSR | S_IROTH, NULL, &sassy_log_ops, slog);
		
	return 0;

error:
	sassy_error(" error code: %d for %s\n", err, __FUNCTION__);
	return err;
}


int init_logger(struct sassy_logger *slog) 
{
	int err;
	int i;

	err = 0;

	if (!slog) {
		err = -EINVAL;
		sassy_error("logger device is NULL %s\n", __FUNCTION__);
		goto error;
	}

	slog->events = kmalloc_array(LOGGER_EVENT_LIMIT, sizeof(struct logger_event), GFP_KERNEL);
	
	if (!slog->events) {
		err = -ENOMEM;
		sassy_error(
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

void remove_logger_ifaces(struct sassy_logger *slog)
{
	char name_buf[MAX_SASSY_PROC_NAME];

	snprintf(name_buf, sizeof(name_buf), "sassy/%d/proto_instances/%d/log_%s",
			 slog->ifindex, slog->instance_id, slog->name);
	
	remove_proc_entry(name_buf, NULL);

	snprintf(name_buf, sizeof(name_buf), "sassy/%d/proto_instances/%d/ctrl_%s",
			 slog->ifindex, slog->instance_id, slog->name);

	remove_proc_entry(name_buf, NULL);
}
EXPORT_SYMBOL(remove_logger_ifaces);