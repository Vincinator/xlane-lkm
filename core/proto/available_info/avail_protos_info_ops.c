#include "../../sassy_core.h"
#include <sassy/sassy.h>
#include <sassy/logger.h>

#include <linux/list.h>

#include <linux/kernel.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/module.h>

#include <linux/err.h>

#undef LOG_PREFIX
#define LOG_PREFIX "[SASSY][CORE]"

ssize_t proto_info_write(struct file *file, const char __user *user_buffer, size_t count, loff_t *data)
{
	sassy_dbg("Nothing to write here!\n");
	return count;
}

int proto_info_show(struct seq_file *m, void *v)
{
	struct sassy_protocol *sproto = (struct sassy_protocol*) m->private;

	seq_printf(m, "Protocol %s has id %d\n", sproto->name, sproto->protocol_id);
	return 0;
}

int proto_info_open(struct inode *inode, struct file *file)
{
	return single_open(file, proto_info_show,PDE_DATA(file_inode(file)));
}
