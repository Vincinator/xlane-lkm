#include "sassy_core.h"
#include <sassy/sassy.h>
#include <sassy/logger.h>

#include <linux/list.h>


LIST_HEAD(available_protocols_l) ;


int sassy_register_protocol(struct sassy_protocol *proto) {
	if(!proto) {
		sassy_error("Protocol is NULL\n");
		return -EINVAL;
	}

	list_add(&proto->listh, &available_protocols_l);
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