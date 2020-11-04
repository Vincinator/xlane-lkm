/*
 * ASGARD protocol registration and removal
 */

#include "../asgard_core.h"
#include <asgard/asgard.h>
#include <asgard/logger.h>
#include <linux/list.h>
#include <linux/kernel.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/err.h>

#include <linux/list.h>

//#include "available_info/avail_protos_mgmt.h"

#undef LOG_PREFIX
#define LOG_PREFIX "[ASGARD][CORE]"

static LIST_HEAD(available_protocols_l);


struct proto_instance *generate_protocol_instance(struct asgard_device *sdev, int protocol_id)
{
	struct proto_instance *sproto;
	enum asgard_protocol_type proto_type = (enum asgard_protocol_type)protocol_id;

	sproto = NULL;

	switch (proto_type) {
	case ASGARD_PROTO_FD:
		// sproto = get_fd_proto_instance(sdev);
		break;
	case ASGARD_PROTO_ECHO:
		sproto = get_echo_proto_instance(sdev);
		break;
	case ASGARD_PROTO_CONSENSUS:
		sproto = get_consensus_proto_instance(sdev);
		break;
	default:
		asgard_error("not a known protocol id\n");
		break;
	}

	return sproto;
}
EXPORT_SYMBOL(generate_protocol_instance);
