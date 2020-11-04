#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <net/net_namespace.h>

#include <asgard/asgard.h>
#include <asgard/logger.h>


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


void logger_state_transition_to(struct asgard_logger *slog,
			    enum logger_state state)
{
	slog->state = state;
}


int write_log(struct asgard_logger *slog,
			  int type, uint64_t tcs)
{

	if (slog->state != LOGGER_RUNNING)
		return 0;


	if (unlikely(slog->current_entries > LOGGER_EVENT_LIMIT)) {

		asgard_dbg("Logs are full! Stopped event logging. %s\n", __func__);

		asgard_log_stop(slog);
		logger_state_transition_to(slog, LOGGER_LOG_FULL);
		return -ENOMEM;
	}

	slog->events[slog->current_entries].timestamp_tcs = tcs;
	slog->events[slog->current_entries].type = type;

	slog->current_entries += 1;

	return 0;
}
EXPORT_SYMBOL(write_log);

int asgard_log_stop(struct asgard_logger *slog)
{
	int err;

	if (!slog) {
		asgard_error("logger is not initialized\n");
		err = -EPERM;
		goto error;
	}

	if (slog->state == LOGGER_UNINIT) {
		asgard_error("logger is not in uninit state. %s\n", __func__);
		err = -EPERM;
		goto error;
	}

	logger_state_transition_to(slog, LOGGER_READY);
	return 0;

error:
	asgard_error("can not stop logger\n");
	return -EPERM;
}

int asgard_log_start(struct asgard_logger *slog)
{
	int err;

	if (!slog) {
		asgard_error("logger is not initialized.\n");
		err = -EPERM;
		goto error;
	}

	if (slog->state != LOGGER_READY) {
		asgard_error("logger is not in ready state. %s\n", __func__);
		goto error;
	}

	logger_state_transition_to(slog, LOGGER_RUNNING);
	return 0;

error:
	asgard_error("can not start logger\n");
	return -EPERM;
}

int asgard_log_reset(struct asgard_logger *slog)
{
	int err;

	if (!slog) {
		asgard_error("logger is not initialized properly.\n");
		err = -EPERM;
		goto error;
	}

	if (slog->state == LOGGER_RUNNING) {
		asgard_error(
			"can not clear logger when it is active.%s\n",
			__func__);
		err = -EPERM;
		goto error;
	}

	if (slog->state != LOGGER_READY) {
		asgard_error(
			"can not clear stats, logger is in an undefined state.%s\n",
			__func__);
		err = -EPERM;
		goto error;
	}

	slog->current_entries = 0;

	return 0;
error:
	asgard_error("Can not reset logs.\n");
	asgard_error("error code: %d for %s\n", err, __func__);
	return err;
}


static ssize_t asgard_log_write(struct file *file, const char __user *user_buffer,
				size_t count, loff_t *data)
{
    struct asgard_logger *slog =
            (struct asgard_logger *)PDE_DATA(file_inode(file));
    int err = 0;
    char kernel_buffer[MAX_PROCFS_BUF];
    int in_type = 0;

    if(slog->accept_user_ts == 0){
        asgard_error("User timestamps not enabled for logger %s!\n", slog->name);
        return count;
    }

    if (count == 0) {
        asgard_error("asgard device is NULL.\n");
        return -EINVAL;
    }

    err = copy_from_user(kernel_buffer, user_buffer, count);

    if (err) {
        asgard_error("Copy from user failed%s\n", __func__);
        return err;
    }

    kernel_buffer[count] = '\0';

    err = kstrtoint(kernel_buffer, 0, &in_type);

    if (err) {
        asgard_error("Can not convert input\n");
        return err;
    }

    write_log(slog, in_type, RDTSC_ASGARD);

	return count;
}

static int asgard_log_show(struct seq_file *m, void *v)
{
	int i;
	struct asgard_logger *slog =
		(struct asgard_logger *)m->private;

	//BUG_ON(!slog);

	for (i = 0; i < slog->current_entries; i++) {
		seq_printf(m, "%d, %llu\n",
				slog->events[i].type,
				slog->events[i].timestamp_tcs);
	}

	return 0;
}

static int asgard_log_open(struct inode *inode, struct file *file)
{
	return single_open(file, asgard_log_show, PDE_DATA(inode));
}

static const struct file_operations asgard_log_ops = {
	.owner = THIS_MODULE,
	.open = asgard_log_open,
	.write = asgard_log_write,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};


static int init_logger_out(struct asgard_logger *slog)
{
	int err;
	char name_buf[MAX_ASGARD_PROC_NAME];


	if (!slog) {
		err = -ENOMEM;
		asgard_error("Logs are not initialized!\n");
		goto error;
	}


	if(slog->log_logger_entry == NULL) {
        snprintf(name_buf, sizeof(name_buf),
                 "asgard/%d/proto_instances/%d/log_%s",
                 slog->ifindex, slog->instance_id, slog->name);

        slog->log_logger_entry = proc_create_data(name_buf, S_IRWXU | S_IRWXO, NULL, &asgard_log_ops, slog);
	} else {
        asgard_error("Log Logger procfs entry already present!\n");
    }


	return 0;

error:
	asgard_error("error code: %d for %s\n", err, __func__);
	return err;
}
int init_logger(struct asgard_logger *slog, u16 instance_id, int ifindex, char name[MAX_LOGGER_NAME], int accept_user_ts)
{
	int err;

	err = 0;

	if (!slog) {
		err = -EINVAL;
		asgard_error("logger device is NULL %s\n", __func__);
		goto error;
	}

    slog->instance_id = instance_id;
    slog->ifindex = ifindex;
    slog->log_logger_entry = NULL;
    slog->ctrl_logger_entry = NULL;
    slog->name = kmalloc(MAX_LOGGER_NAME, GFP_KERNEL);
    slog->accept_user_ts = accept_user_ts;
    asgard_dbg("Accept User TS is: %d", slog->accept_user_ts );
    strncpy(slog->name, name, MAX_LOGGER_NAME);

    // freed by clear_logger
	slog->events = kmalloc_array(LOGGER_EVENT_LIMIT, sizeof(struct logger_event), GFP_KERNEL);

	if (!slog->events) {
		err = -ENOMEM;
		asgard_error(
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

void remove_logger_ifaces(struct asgard_logger *slog)
{
	char name_buf[MAX_ASGARD_PROC_NAME];

	if(slog->log_logger_entry) {
        snprintf(name_buf, sizeof(name_buf),
                 "asgard/%d/proto_instances/%d/log_%s",
                 slog->ifindex, slog->instance_id, slog->name);

        remove_proc_entry(name_buf, NULL);
        slog->log_logger_entry = NULL;
	}

	if(slog->ctrl_logger_entry) {
        snprintf(name_buf, sizeof(name_buf),
                 "asgard/%d/proto_instances/%d/ctrl_%s",
                 slog->ifindex, slog->instance_id, slog->name);

        remove_proc_entry(name_buf, NULL);
        slog->ctrl_logger_entry = NULL;
	}




}
EXPORT_SYMBOL(remove_logger_ifaces);
