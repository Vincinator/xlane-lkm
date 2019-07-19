/*
 * SASSY protocol registration and removal
 */


#include "../sassy_core.h"
#include <sassy/sassy.h>
#include <sassy/logger.h>
#include <linux/list.h>
#include <linux/kernel.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/err.h>

#include <linux/list.h>


#include "available_info/avail_protos_mgmt.h"


#undef LOG_PREFIX
#define LOG_PREFIX "[SASSY][CORE]"


LIST_HEAD(available_protocols_l);


struct sassy_protocol* sassy_find_protocol_by_id(int protocol_id) 
{
	struct sassy_protocol *sproto, *tmp_proto;
	sproto = NULL;

	list_for_each_entry (tmp_proto, &available_protocols_l, listh) 
    { 
    	if(tmp_proto->proto_type == protocol_id){
    		sproto = tmp_proto;
    		break;
    	}
    }
    return sproto;
}
EXPORT_SYMBOL(sassy_find_protocol_by_id);


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

	sassy_dbg("Added protocol: %s", sassy_get_protocol_name(proto->proto_type));

	return 0;
}

EXPORT_SYMBOL(sassy_register_protocol);


int sassy_remove_protocol(struct sassy_protocol *proto) {

	if(!proto) {
		sassy_error("Protocol is NULL\n");
		return -EINVAL;
	}

	sassy_dbg("Remove protocol: %s",sassy_get_protocol_name(proto->proto_type));

	/* Remove /proc/sassy/protocols/<name> interface */
	sassy_remove_protocol_info_iface(proto);

	list_del(&proto->listh);

	return 0;
}
EXPORT_SYMBOL(sassy_remove_protocol);