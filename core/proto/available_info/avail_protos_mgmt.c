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

#include <linux/err.h>

#include "avail_protos_info_ops.h"


LIST_HEAD(available_protocols_l) ;


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

