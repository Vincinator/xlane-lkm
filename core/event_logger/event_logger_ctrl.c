#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/module.h>
#include <linux/init.h>

#include <linux/kernel.h>
#include <asgard/asgard.h>
#include <asgard/logger.h>
#include <asgard/consensus.h>

static ssize_t asgard_event_ctrl_write(struct file *file,
				   const char __user *user_buffer, size_t count,
				   loff_t *data)
{
	int err;
	struct asgard_logger *slog =
		(struct asgard_logger *)PDE_DATA(file_inode(file));
	char kernel_buffer[ASGARD_NUMBUF];
	int logging_state = -1;
	size_t size;

	size = min(sizeof(kernel_buffer) - 1, count);

	memset(kernel_buffer, 0, sizeof(kernel_buffer));

	err = copy_from_user(kernel_buffer, user_buffer, count);
	if (err) {
		asgard_error("Copy from user failed%s\n", __func__);
		goto error;
	}

	kernel_buffer[size] = '\0';
	err = kstrtoint(kernel_buffer, 10, &logging_state);
	if (err) {
		asgard_dbg("Error converting input buffer: %s, todevid: 0x%x\n",
		       kernel_buffer, logging_state);
		goto error;
	}

	switch (logging_state) {
	case 0:
		asgard_log_stop(slog);
		break;
	case 1:
		asgard_log_start(slog);
		break;
	case 2:
		asgard_log_reset(slog);
		break;
    case 3:
        dump_consensus_throughput(slog);
        break;
	default:
		asgard_error("Invalid input: %d - %s\n",
			    logging_state, __func__);
		err = -EINVAL;
		goto error;
	}
	return count;
error:
	asgard_error("Leader Election Logger control operation failed.%s\n", __func__);
	return err;
}

static int asgard_event_ctrl_show(struct seq_file *m, void *v)
{
	struct asgard_logger *slog =
		(struct asgard_logger *)m->private;

	if (!slog)
		return -ENODEV;

	seq_printf(m, "%s\n", logger_state_string(slog->state));

	return 0;
}

static int asgard_event_ctrl_open(struct inode *inode, struct file *file)
{
	return single_open(file, asgard_event_ctrl_show,
			   PDE_DATA(file_inode(file)));
}


static const struct file_operations asgard_event_ctrl_ops = {
	.owner = THIS_MODULE,
	.open = asgard_event_ctrl_open,
	.write = asgard_event_ctrl_write,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

void clear_logger(struct asgard_logger *slog)
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
        slog->ctrl_logger_entry = NULL;
        remove_proc_entry(name_buf, NULL);
    }


	if(slog->events)
	    kfree(slog->events);

}
EXPORT_SYMBOL(clear_logger);

void init_logger_ctrl(struct asgard_logger *slog)
{
	char name_buf[MAX_ASGARD_PROC_NAME];

	if (!slog) {
		asgard_error("ins or Logs are not initialized!\n");
		return;
	}

	if(slog->ctrl_logger_entry == NULL) {
        snprintf(name_buf, sizeof(name_buf),
                 "asgard/%d/proto_instances/%d/ctrl_%s",
                 slog->ifindex, slog->instance_id, slog->name);

        slog->ctrl_logger_entry = proc_create_data(name_buf, S_IRWXU | S_IRWXO, NULL, &asgard_event_ctrl_ops, slog);
    } else {
        asgard_error("Ctrl Logger Entry already present!\n");
    }

}
EXPORT_SYMBOL(init_logger_ctrl);
