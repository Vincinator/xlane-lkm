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

static LIST_HEAD(available_protocols_l);


struct sassy_protocol *sassy_find_protocol_by_id(u8 protocol_id)
{
	struct sassy_protocol *sproto;
	enum sassy_protocol_type proto_type = (enum sassy_protocol_type)protocol_id;

	sproto = NULL;

	switch(proto_type)
	{
		case SASSY_PROTO_CONSENSUS:
			sproto = get_consensus_proto();
			break;
		case SASSY_PROTO_FD:
			sproto = get_fd_proto();
			break;
		case SASSY_PROTO_ECHO:
			sproto = get_echo_proto();
			break;
		default:
			sassy_error("not a known protocol id\n");
			break;
	}

	return sproto;
}
EXPORT_SYMBOL(sassy_find_protocol_by_id);

int sassy_register_protocol(struct sassy_protocol *proto)
{
	char name_buf[MAX_SASSY_PROC_NAME];

	struct sassy_core *score = sassy_core();

	if (!proto) {
		sassy_error("Protocol is NULL\n");
		return -EINVAL;
	}

	list_add(&proto->listh, &available_protocols_l);

	/* Initialize /proc/sassy/protocols/<name> interface */
	sassy_register_protocol_info_iface(proto);

	/* Add protocol ptr to score array */
	if (proto->proto_type < 0 || proto->proto_type > MAX_PROTOCOLS)
		return -EINVAL;

	if (!score->protocols) {
		sassy_error("protocols is not initialized\n");
		return -EPERM;
	}

	score->protocols[proto->proto_type] = proto;

	sassy_dbg("Added protocol: %s",
		  sassy_get_protocol_name(proto->proto_type));

	return 0;
}
EXPORT_SYMBOL(sassy_register_protocol);

int sassy_remove_protocol(struct sassy_protocol *proto)
{
	if (!proto) {
		sassy_error("Protocol is NULL\n");
		return -EINVAL;
	}

	sassy_dbg("Remove protocol: %s",
		  sassy_get_protocol_name(proto->proto_type));

	/* Remove /proc/sassy/protocols/<name> interface */
	sassy_remove_protocol_info_iface(proto);

	list_del(&proto->listh);

	return 0;
}
EXPORT_SYMBOL(sassy_remove_protocol);
