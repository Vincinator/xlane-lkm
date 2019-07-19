/*
 * selecting protocol for sassy device
 */

#include <linux/kernel.h>
#include <linux/slab.h>

#include <sassy/sassy.h>
#include <sassy/logger.h>

#include <linux/list.h>

#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/module.h>

#include <linux/err.h>

#undef LOG_PREFIX
#define LOG_PREFIX "[SASSY][CORE]"

/* 
 * Selects protocol <sproto> for sassy device <sdev>. 
 * Includes safety checks before selecting protocol.
 */
int sassy_select_protocol(struct sassy_device *sdev, struct sassy_protocol *sproto)
{

	if(!sdev || !sproto){
		sassy_error(" Invalid Input - NULL reference. \n");
		return -EINVAL;
	}

	if(sdev->proto != NULL){
		sassy_error(" sdev %d uses protocol %d - clean up old protocol first. BUG.\n");
		return -EPERM;
	}

	sdev->proto = sproto;
	sassy_dbg(" successfully switchted to protocol: %s with id %d\n", sassy_get_protocol_name(sproto->proto_type), sproto->proto_type);
	return 0;
}


static ssize_t proto_selector_write(struct file *file, const char __user *user_buffer, size_t count, loff_t *data)
{
	sassy_dbg("Nothing to write here!\n");
	return count;
}

static int proto_selector_show(struct seq_file *m, void *v)
{
	struct sassy_device *sdev = (struct sassy_device*) m->private;
	char name_buf[MAX_SYNCBEAT_PROC_NAME];

	if(!sdev){
		sassy_error(" sdev is NULL %s!\n", __FUNCTION__);
		return -EINVAL;
	}
	if(!sdev->proto ){
		seq_printf(m, "sdev does not use a protocol yet\n");
		return -1;
	}

	seq_printf(m, "sdev uses protocol %s with id %d\n", 
		sassy_get_protocol_name(sdev->proto->proto_type), sdev->proto->proto_type);

	return 0;
}

static int proto_selector_open(struct inode *inode, struct file *file)
{
	return single_open(file, proto_selector_show,PDE_DATA(file_inode(file)));
}


static const struct file_operations proto_selector_ops = {
		.owner	= THIS_MODULE,
		.open	= proto_selector_open,
		.write	= proto_selector_write,
		.read	= seq_read,
		.llseek	= seq_lseek,
		.release = single_release,
};



void init_proto_selector(struct sassy_device  *sdev) 
{
	char name_buf[MAX_SYNCBEAT_PROC_NAME];


	snprintf(name_buf, sizeof name_buf, "sassy/%d/protocol", sdev->ifindex);
	proc_create_data(name_buf, S_IRWXU|S_IRWXO, NULL, &proto_selector_ops, sdev);

	sassy_dbg(" added proto selector\n");

}

void remove_proto_selector(struct sassy_device *sdev)
{
	char name_buf[MAX_SYNCBEAT_PROC_NAME];

	snprintf(name_buf, sizeof name_buf, "sassy/%d/protocol", sdev->ifindex);
	remove_proc_entry(name_buf, NULL);

}