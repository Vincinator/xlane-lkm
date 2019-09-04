#include <sassy/logger.h>
#include <sassy/sassy.h>
#include <sassy/payload_helper.h>


#include "include/leader.h"
#include "include/sassy_consensus.h"

#undef LOG_PREFIX
#define LOG_PREFIX "[SASSY][LE][LEADER]"

int leader_process_pkt(struct sassy_device *sdev, int remote_lid, int rcluster_id, unsigned char *pkt)
{

	struct consensus_priv *priv = 
			(struct consensus_priv *)sdev->le_proto->priv;


	u8 opcode = GET_LE_PAYLOAD(pkt, opcode);
	u32 param1 = GET_LE_PAYLOAD(pkt, param1);
	u32 param2 = GET_LE_PAYLOAD(pkt, param2);

	log_le_rx(sdev->verbose, priv->nstate, rdtsc(), priv->term, opcode, rcluster_id, param1);

	switch(opcode){

	case VOTE:
		break;
	case NOMI:
		break;		
	case NOOP:
		break;
	case LEAD:
		if(param1 > priv->term){

			if(sdev->verbose >= 2)
				sassy_dbg("Received message from new leader with higher or equal term=%u\n", param1);

			accept_leader(sdev, remote_lid, rcluster_id, param1);
			write_log(&sdev->le_logger, LEADER_ACCEPT_NEW_LEADER, rdtsc());


		} else {

			if(sdev->verbose >= 2)
				sassy_dbg("Received LEAD from leader with lower or equal term=%u\n", param1);
	
			// Ignore this LEAD message, continue to send LEAD messages 
			sassy_log_le("%s, %llu, %d: Cluster node %d also claims to be leader in term %d.\n",
				nstate_string(priv->nstate),
				rdtsc(),
				rcluster_id,
				priv->term);
		}
	
		break;
	default:
		sassy_dbg("Unknown opcode received from host: %d - opcode: %d\n",remote_lid, opcode);

	}

	return 0;
}

int stop_leader(struct sassy_device *sdev)
{

	return 0;
}

int start_leader(struct sassy_device *sdev)
{
	struct consensus_priv *priv = 
				(struct consensus_priv *)sdev->le_proto->priv;
	
	setup_le_broadcast_msg(sdev, LEAD);

	priv->nstate = LEADER;
	
	return 0;
}
EXPORT_SYMBOL(start_leader);
