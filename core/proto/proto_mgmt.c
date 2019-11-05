/*
 * SASSY protocol registration and removal
 */

#include "../asguard_core.h"
#include <asguard/asguard.h>
#include <asguard/logger.h>
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
#define LOG_PREFIX "[SASSY][CORE]"

static LIST_HEAD(available_protocols_l);


struct proto_instance *generate_protocol_instance(struct asguard_device *sdev, int protocol_id)
{
	struct proto_instance *sproto;
	enum asguard_protocol_type proto_type = (enum asguard_protocol_type)protocol_id;

	sproto = NULL;
	

	switch(proto_type)
	{
		case SASSY_PROTO_FD:
			sproto = get_fd_proto_instance(sdev);
			break;
		case SASSY_PROTO_ECHO:
			sproto = get_echo_proto_instance(sdev);
			break;
		case SASSY_PROTO_CONSENSUS:
			

			sproto = get_consensus_proto_instance(sdev);
			

			break;
		default:
			asguard_error("not a known protocol id\n");
			break;
	}
	

	return sproto;
}
EXPORT_SYMBOL(generate_protocol_instance);