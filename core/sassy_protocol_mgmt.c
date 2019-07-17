#include "sassy_core.h"
#include <sassy/sassy.h>
#include <sassy/logger.h>

#include <linux/list.h>

#include <linux/kernel.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/module.h>

LIST_HEAD(available_protocols_l) ;


static ssize_t proto_info_write(struct file *file, const char __user *user_buffer, size_t count, loff_t *data)
{
	sassy_dbg("Nothing to write here!\n");
	return count;
}

static int proto_info_show(struct seq_file *m, void *v)
{
	struct sassy_protocol *sproto = (struct sassy_protocol*) m->private;

	seq_printf(m, "Protocol %s has id %d\n", sproto->name, sproto->protocol_id);
	return 0;
}

static int proto_info_open(struct inode *inode, struct file *file)
{
	return single_open(file, proto_info_show,PDE_DATA(file_inode(file)));
}


static const struct file_operations sassy_proto_info_ops = {
		.owner	= THIS_MODULE,
		.open	= proto_info_open,
		.write	= proto_info_write,
		.read	= seq_read,
		.llseek	= seq_lseek,
		.release = single_release,
};


void init_sassy_proto_info_interfaces(void)
{
	proc_mkdir("sassy/protocols", NULL);

}


void clean_sassy_proto_info_interfaces(void)
{

	remove_proc_entry("sassy/protocols", NULL);
}

void sassy_register_protocol_info_iface(struct sassy_protocol *proto) 
{
	char name_buf[MAX_SYNCBEAT_PROC_NAME];


	snprintf(name_buf, sizeof name_buf, "sassy/protocols/%s", proto->name);
	proc_create_data(name_buf, S_IRWXU|S_IRWXO, NULL, &sassy_proto_info_ops, &proto);

}

void sassy_remove_protocol_info_iface(struct sassy_protocol *proto) 
{
	char name_buf[MAX_SYNCBEAT_PROC_NAME];

	snprintf(name_buf, sizeof name_buf, "sassy/protocols/%s", proto->name);
	remove_proc_entry(name_buf, NULL);
}


int sassy_register_protocol(struct sassy_protocol *proto)
{
	char name_buf[MAX_SYNCBEAT_PROC_NAME];

	if(!proto) {
		sassy_error("Protocol is NULL\n");
		return -EINVAL;
	}

	list_add(&proto->listh, &available_protocols_l);

	/* Initialize /proc/sassy/protocols/<name> interface */
	sassy_register_protocol_info_iface(proto);

	sassy_dbg("Added protocol: %s",proto->name);

	return 0;
}

EXPORT_SYMBOL(sassy_register_protocol);


int sassy_remove_protocol(struct sassy_protocol *proto) {

	if(!proto) {
		sassy_error("Protocol is NULL\n");
		return -EINVAL;
	}
	sassy_dbg("Remove protocol: %s", proto->name);

	sassy_remove_protocol_info_iface(proto);

	list_del(&proto->listh);

	return 0;
}
EXPORT_SYMBOL(sassy_remove_protocol);