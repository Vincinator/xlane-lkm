#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/slab.h>
#include <linux/uaccess.h>

#include <sassy/sassy.h>
#include <sassy/logger.h>
#include <sassy/consensus.h>

#include "include/test.h"

static ssize_t sassy_eval_ctrl_write(struct file *file,
				   const char __user *user_buffer, size_t count,
				   loff_t *data)
{
	struct consensus_priv *priv =
		(struct consensus_priv *)PDE_DATA(file_inode(file));
	struct sassy_device *sdev =
		(struct sassy_device *)PDE_DATA(file_inode(file));
	char kernel_buffer[SASSY_NUMBUF];
	int eval_selection = -3;
	size_t size;
	int err;

	size = min(sizeof(kernel_buffer) - 1, count);

	memset(kernel_buffer, 0, sizeof(kernel_buffer));

	err = copy_from_user(kernel_buffer, user_buffer, count);
	if (err) {
		sassy_error("Copy from user failed%s\n", __FUNCTION__);
		goto error;
	}

	kernel_buffer[size] = '\0';
	err = kstrtoint(kernel_buffer, 10, &eval_selection);
	if (err) {
		sassy_dbg("Error converting input buffer: %s, todevid: 0x%x\n",
		       kernel_buffer, eval_selection);
		goto error;
	}

	switch (eval_selection) {
	case 0:
		sassy_dbg("stopping eval timers\n");
		testcase_stop_timer(priv);
		break;
	case -1:
		// one shot 
		sassy_dbg("One shot eval case\n");
		testcase_one_shot_big_log(priv);
		break;
	default:
		if(eval_selection < 0 || eval_selection > MAX_ENTRIES_PER_PKT){
			sassy_error("Invalid input: %d - %s\n", eval_selection, __FUNCTION__);
			err = -EINVAL;
			goto error;
		}
		sassy_dbg("Appending %d entries to log every second\n", eval_selection);
		testcase_X_requests_per_sec(priv, eval_selection);

	}

	return count;
error:
	sassy_error("Error during parsing of input.%s\n", __FUNCTION__);
	return err;

}

static int sassy_eval_ctrl_show(struct seq_file *m, void *v)
{
	struct consensus_priv *priv =
		(struct consensus_priv *)m->private;

	if (!priv)
		return -ENODEV;

	return 0;
}

static int sassy_eval_ctrl_open(struct inode *inode, struct file *file)
{
	return single_open(file, sassy_eval_ctrl_show,
			   PDE_DATA(file_inode(file)));
}


static const struct file_operations sassy_eval_ctrl_ops = {
	.owner = THIS_MODULE,
	.open = sassy_eval_ctrl_open,
	.write = sassy_eval_ctrl_write,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

void init_eval_ctrl_interfaces(struct consensus_priv *priv)
{
	char name_buf[MAX_SASSY_PROC_NAME];

	snprintf(name_buf, sizeof(name_buf), "sassy/%d/proto_instances/%d/consensus_eval_ctrl",
			 priv->sdev->ifindex, priv->ins->instance_id);
	
	proc_create_data(name_buf, S_IRWXU | S_IRWXO, NULL, &sassy_eval_ctrl_ops, priv);
	

}
EXPORT_SYMBOL(init_eval_ctrl_interfaces);

void remove_eval_ctrl_interfaces(struct consensus_priv *priv)
{
	char name_buf[MAX_SASSY_PROC_NAME];

	snprintf(name_buf, sizeof(name_buf), "sassy/%d/proto_instances/%d/consensus_eval_ctrl",
			 priv->sdev->ifindex, priv->ins->instance_id);
	
	remove_proc_entry(name_buf, NULL);

}
EXPORT_SYMBOL(remove_eval_ctrl_interfaces);
