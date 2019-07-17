#include "sassy_core.h"
#include <sassy/sassy.h>
#include <sassy/logger.h>

#include <linux/list.h>


LIST_HEAD(available_protocols_l) ;


void init_sassy_proto_info_interfaces(struct sassy_device *sdev)
{
	proc_mkdir("sassy/protocols", NULL);

}


void clean_sassy_proto_info_interfaces()
{

	remove_proc_entry("sassy/protocols", NULL);
}



int sassy_register_protocol(struct sassy_protocol *proto) {
	char name_buf[MAX_SYNCBEAT_PROC_NAME];

	if(!proto) {
		sassy_error("Protocol is NULL\n");
		return -EINVAL;
	}

	list_add(&proto->listh, &available_protocols_l);

	/* Initialize protocol handler and ctrl interfaces */
	proto->init();	

	sassy_dbg("Added protocol: %s"m proto->name);

	return 0;
}

EXPORT_SYMBOL(sassy_register_protocol);


int sassy_remove_protocol(struct sassy_protocol *proto) {

	if(!proto) {
		sassy_error("Protocol is NULL\n");
		return -EINVAL;
	}
	sassy_dbg("Remove protocol: %s"m proto->name);

	list_del(&proto->listh, &available_protocols_l);

	return 0;
}
EXPORT_SYMBOL(sassy_remove_protocol);