#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/slab.h>
#include <linux/uaccess.h>

#include <sassy/sassy.h>
#include <sassy/logger.h>
#include <sassy/sassy_ts.h>

#include "include/sassy_consensus.h"

static ssize_t sassy_le_config_write(struct file *file,
				   const char __user *user_buffer, size_t count,
				   loff_t *data)
{
	struct consensus_priv *priv =
		(struct consensus_priv *)PDE_DATA(file_inode(file));

	int err;
	char kernel_buffer[SASSY_TARGETS_BUF];
	char *search_str;
	size_t size = min(sizeof(kernel_buffer) - 1, count);
	char *input_str;
	static const char delimiters[] = " ,;()";
	int state = 0;
	int fmin_tmp, fmax_tmp, cmin_tmp, cmax_tmp;
	int tmp;

	if (!priv)
		return -ENODEV;

	memset(kernel_buffer, 0, sizeof(kernel_buffer));

	err = copy_from_user(kernel_buffer, user_buffer, count);
	if (err) {
		sassy_error("Copy from user failed%s\n", __FUNCTION__);
		goto error;
	}

	kernel_buffer[size] = '\0';
	

	search_str = kstrdup(kernel_buffer, GFP_KERNEL);
	while ((input_str = strsep(&search_str, delimiters)) != NULL) {
		sassy_dbg(" reading: '%s'", input_str);
		if (!input_str || strlen(input_str) <= 0)
			continue;

		err = kstrtoint(input_str, 10, &tmp);

		if(err) {
			sassy_error("error converting '%s' to an integer", input_str);
			goto error;
		}

		if (state == 0) {
			fmin_tmp = tmp;
			sassy_dbg("fmin: %d\n", tmp);
			state = 1;
		} else if (state == 1) {
			fmax_tmp = tmp;
			sassy_dbg("fmax: %d\n", tmp);
			state = 2;
		} else if (state == 2) {
			cmin_tmp = tmp;
			sassy_dbg("cmin: %d\n", tmp);
			state = 3;
		} else if (state == 3){
			cmax_tmp = tmp;
			sassy_dbg("cmax: %d\n", tmp);
			break;
		}
	}

	if(fmin_tmp < fmax_tmp && cmin_tmp < cmax_tmp){

		priv->ft_min = fmin_tmp;
		priv->ft_max = fmax_tmp;
		priv->ct_min = cmin_tmp;
		priv->ct_max = cmax_tmp;
	} else {
		sassy_error("Invalid Ranges! Must assure that fmin < fmax and cmin < cmax \n");
		sassy_error("input order: fmin, fmax, cmin, cmax \n");
	}

	return count;
error:
	sassy_error("Error during parsing of input.%s\n", __FUNCTION__);
	return err;

}

static int sassy_le_config_show(struct seq_file *m, void *v)
{
	struct consensus_priv *priv =
		(struct consensus_priv *)m->private;

	if (!priv)
		return -ENODEV;

	//seq_printf(m, "fmin,fmax,cmin,cmax (in ns) \n");
	seq_printf(m, "%d,%d,%d,%d\n",priv->ft_min, priv->ft_max, priv->ct_min, priv->ct_max);

	return 0;
}

static int sassy_le_config_open(struct inode *inode, struct file *file)
{
	return single_open(file, sassy_le_config_show,
			   PDE_DATA(file_inode(file)));
}


static const struct file_operations sassy_le_config_ops = {
	.owner = THIS_MODULE,
	.open = sassy_le_config_open,
	.write = sassy_le_config_write,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

void init_le_config_ctrl_interfaces(struct consensus_priv *priv)
{
	char name_buf[MAX_SASSY_PROC_NAME];

	snprintf(name_buf, sizeof(name_buf), "sassy/%d/le_config", priv->sdev->ifindex);
	
	priv->le_config_procfs =
					proc_create_data(name_buf, S_IRWXU | S_IRWXO, NULL, 
							&sassy_le_config_ops, priv);
	

}
EXPORT_SYMBOL(init_le_config_ctrl_interfaces);

void remove_le_config_ctrl_interfaces(struct consensus_priv *priv)
{
	char name_buf[MAX_SASSY_PROC_NAME];

	snprintf(name_buf, sizeof(name_buf), "sassy/%d/le_config", priv->sdev->ifindex);
	
	remove_proc_entry(name_buf, NULL);

}
EXPORT_SYMBOL(remove_le_config_ctrl_interfaces);
