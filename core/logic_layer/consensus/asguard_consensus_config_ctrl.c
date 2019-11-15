#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/slab.h>
#include <linux/uaccess.h>

#include <asguard/asguard.h>
#include <asguard/logger.h>
#include <asguard/consensus.h>

// /proc/asguard/<ifindex/some_blah_
static ssize_t asguard_le_config_write(struct file *file,
				   const char __user *user_buffer, size_t count,
				   loff_t *data)
{
	struct consensus_priv *priv =
		(struct consensus_priv *)PDE_DATA(file_inode(file));

	int err;
	char kernel_buffer[ASGUARD_TARGETS_BUF];
	char *search_str;
	size_t size = min(sizeof(kernel_buffer) - 1, count);
	char *input_str;
	static const char delimiters[] = " ,;()";
	int state = 0;
	int fmin_tmp, fmax_tmp, cmin_tmp, cmax_tmp, max_entries_per_pkt_tmp;
	int tmp;

	if (!priv)
		return -ENODEV;

	memset(kernel_buffer, 0, sizeof(kernel_buffer));

	err = copy_from_user(kernel_buffer, user_buffer, count);
	if (err) {
		asguard_error("Copy from user failed%s\n", __func__);
		goto error;
	}

	kernel_buffer[size] = '\0';

	search_str = kstrdup(kernel_buffer, GFP_KERNEL);
	while ((input_str = strsep(&search_str, delimiters)) != NULL) {
		if (!input_str || strlen(input_str) <= 0)
			continue;

		err = kstrtoint(input_str, 10, &tmp);

		if (err) {
			asguard_error("error converting '%s' to an integer", input_str);
			goto error;
		}

		if (state == 0) {
			fmin_tmp = tmp;
			state = 1;
		} else if (state == 1) {
			fmax_tmp = tmp;
			state = 2;
		} else if (state == 2) {
			cmin_tmp = tmp;
			state = 3;
		} else if (state == 3) {
			cmax_tmp = tmp;
			state = 4;
		} else if (state == 4) {
			max_entries_per_pkt_tmp = tmp;
			break;
		}
	}

	if (!(fmin_tmp < fmax_tmp && cmin_tmp < cmax_tmp)) {
		asguard_error("Invalid Ranges! Must assure that fmin < fmax and cmin < cmax\n");
		asguard_error("input order: fmin, fmax, cmin, cmax, max_entries_per_pkt_tmp\n");
		goto error;
	}

	if (!(max_entries_per_pkt_tmp > 0 && max_entries_per_pkt_tmp < MAX_AE_ENTRIES_PER_PKT)) {
		asguard_error("Invalid for entries per consensus payload!\n");
		asguard_error("Must be in (0,%d) interval!\n", MAX_AE_ENTRIES_PER_PKT);
		goto error;
	}

	priv->ft_min = fmin_tmp;
	priv->ft_max = fmax_tmp;
	priv->ct_min = cmin_tmp;
	priv->ct_max = cmax_tmp;
	priv->max_entries_per_pkt = max_entries_per_pkt_tmp;


	return count;
error:
	asguard_error("Error during parsing of input.%s\n", __func__);
	return err;

}

static int asguard_le_config_show(struct seq_file *m, void *v)
{
	struct consensus_priv *priv =
		(struct consensus_priv *)m->private;

	if (!priv)
		return -ENODEV;

	seq_printf(m, "%d,%d,%d,%d,%d\n",
		priv->ft_min,
		priv->ft_max,
		priv->ct_min,
		priv->ct_max,
		priv->max_entries_per_pkt);

	return 0;
}

static int asguard_le_config_open(struct inode *inode, struct file *file)
{
	return single_open(file, asguard_le_config_show,
			   PDE_DATA(file_inode(file)));
}

static const struct file_operations asguard_le_config_ops = {
	.owner = THIS_MODULE,
	.open = asguard_le_config_open,
	.write = asguard_le_config_write,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

void init_le_config_ctrl_interfaces(struct consensus_priv *priv)
{
	char name_buf[MAX_ASGUARD_PROC_NAME];

	snprintf(name_buf, sizeof(name_buf),
			"asguard/%d/proto_instances/%d/le_config",
			priv->sdev->ifindex, priv->ins->instance_id);

	proc_create_data(name_buf, S_IRWXU | S_IRWXO, NULL, &asguard_le_config_ops, priv);


}
EXPORT_SYMBOL(init_le_config_ctrl_interfaces);

void remove_le_config_ctrl_interfaces(struct consensus_priv *priv)
{
	char name_buf[MAX_ASGUARD_PROC_NAME];

	snprintf(name_buf, sizeof(name_buf),
			"asguard/%d/proto_instances/%d/le_config",
			priv->sdev->ifindex, priv->ins->instance_id);

	remove_proc_entry(name_buf, NULL);

}
EXPORT_SYMBOL(remove_le_config_ctrl_interfaces);
