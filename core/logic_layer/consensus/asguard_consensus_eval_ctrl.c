#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/slab.h>
#include <linux/uaccess.h>

#include <asguard/asguard.h>
#include <asguard/logger.h>
#include <asguard/consensus.h>

#include "include/test.h"

static ssize_t asguard_eval_ctrl_write(struct file *file,
				   const char __user *user_buffer, size_t count,
				   loff_t *data)
{
	struct consensus_priv *priv =
		(struct consensus_priv *)PDE_DATA(file_inode(file));
	char kernel_buffer[ASGUARD_NUMBUF];
	int eval_selection = -3;
	size_t size;
	int err;

	size = min(sizeof(kernel_buffer) - 1, count);

	memset(kernel_buffer, 0, sizeof(kernel_buffer));

	err = copy_from_user(kernel_buffer, user_buffer, count);
	if (err) {
		asguard_error("Copy from user failed%s\n", __func__);
		goto error;
	}

	kernel_buffer[size] = '\0';
	err = kstrtoint(kernel_buffer, 10, &eval_selection);
	if (err) {
		asguard_dbg("Error converting input buffer: %s, todevid: 0x%x\n",
		       kernel_buffer, eval_selection);
		goto error;
	}

	switch (eval_selection) {
	case 0:
		testcase_stop_timer(priv);
		break;
	case -1:
		// one shot
		testcase_one_shot_big_log(priv);
		break;
	default:
		if (eval_selection < 0 || eval_selection > MAX_CONSENSUS_LOG) {
			asguard_error("Invalid input: %d - %s\n", eval_selection, __func__);
			err = -EINVAL;
			goto error;
		}
		// asguard_dbg("Appending %d entries to log every second\n", eval_selection);
		testcase_X_requests_per_sec(priv, eval_selection);

	}

	return count;
error:
	asguard_error("Error during parsing of input.%s\n", __func__);
	return err;

}

static int asguard_eval_ctrl_show(struct seq_file *m, void *v)
{
	struct consensus_priv *priv =
		(struct consensus_priv *)m->private;

	if (!priv)
		return -ENODEV;

	// todo output uuid of consensus eval
    seq_printf(m, "%pUB\n", priv->uuid);

	return 0;
}

static int asguard_eval_ctrl_open(struct inode *inode, struct file *file)
{
	return single_open(file, asguard_eval_ctrl_show,
			   PDE_DATA(file_inode(file)));
}


static const struct file_operations asguard_eval_ctrl_ops = {
	.owner = THIS_MODULE,
	.open = asguard_eval_ctrl_open,
	.write = asguard_eval_ctrl_write,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

void init_eval_ctrl_interfaces(struct consensus_priv *priv)
{
	char name_buf[MAX_ASGUARD_PROC_NAME];

	snprintf(name_buf, sizeof(name_buf),
			"asguard/%d/proto_instances/%d/consensus_eval_ctrl",
			priv->sdev->ifindex, priv->ins->instance_id);

	proc_create_data(name_buf, S_IRWXU | S_IRWXO, NULL, &asguard_eval_ctrl_ops, priv);


}
EXPORT_SYMBOL(init_eval_ctrl_interfaces);

void remove_eval_ctrl_interfaces(struct consensus_priv *priv)
{
	char name_buf[MAX_ASGUARD_PROC_NAME];

	snprintf(name_buf, sizeof(name_buf),
			"asguard/%d/proto_instances/%d/consensus_eval_ctrl",
			priv->sdev->ifindex, priv->ins->instance_id);

	remove_proc_entry(name_buf, NULL);

}
EXPORT_SYMBOL(remove_eval_ctrl_interfaces);
