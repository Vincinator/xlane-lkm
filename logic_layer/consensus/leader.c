#include <sassy/logger.h>
#include <sassy/sassy.h>
#include <sassy/payload_helper.h>


#include "include/leader.h"
#include <sassy/consensus.h>

#undef LOG_PREFIX
#define LOG_PREFIX "[SASSY][LE][LEADER]"

int leader_process_pkt(struct proto_instance *ins, int remote_lid, int rcluster_id, unsigned char *pkt)
{
	struct consensus_priv *priv = 
		(struct consensus_priv *)ins->proto_data;

	struct sassy_device *sdev = priv->sdev;

	u8 opcode = GET_CON_PROTO_OPCODE_VAL(pkt);
	u32 param1 = GET_CON_PROTO_PARAM1_VAL(pkt);
	u32 param2 = GET_CON_PROTO_PARAM2_VAL(pkt);

#if 1
	log_le_rx(sdev->verbose, priv->nstate, rdtsc(), priv->term, opcode, rcluster_id, param1);
#endif 

	switch(opcode){

	case VOTE:
		break;
	case NOMI:
		break;		
	case NOOP:
		break;
	case LEAD:
		if(param1 > priv->term){
#if 1
			if(sdev->verbose >= 2)
				sassy_dbg("Received message from new leader with higher or equal term=%u\n", param1);
#endif
			accept_leader(ins, remote_lid, rcluster_id, param1);
			write_log(&ins->logger, LEADER_ACCEPT_NEW_LEADER, rdtsc());


		} else {
#if 1
			if(sdev->verbose >= 2)
				sassy_dbg("Received LEAD from leader with lower or equal term=%u\n", param1);
	
			// Ignore this LEAD message, continue to send LEAD messages 
			sassy_log_le("%s, %llu, %d: Cluster node %d also claims to be leader in term %d.\n",
				nstate_string(priv->nstate),
				rdtsc(),
				rcluster_id,
				priv->term);
#endif
		}
	
		break;
	default:
		sassy_dbg("Unknown opcode received from host: %d - opcode: %d\n",remote_lid, opcode);

	}

	return 0;
}

int stop_leader(struct proto_instance *ins)
{
	struct consensus_priv *priv = 
		(struct consensus_priv *)ins->proto_data;

	return 0;
}

int start_leader(struct proto_instance *ins)
{
	struct consensus_priv *priv = 
		(struct consensus_priv *)ins->proto_data;

	setup_le_broadcast_msg(ins, LEAD);

	priv->nstate = LEADER;
	
	return 0;
}
EXPORT_SYMBOL(start_leader);
