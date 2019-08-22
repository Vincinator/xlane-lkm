#include <sassy/logger.h>
#include <sassy/sassy.h>
#include <sassy/payload_helper.h>


#include "include/leader.h"
#include "include/sassy_consensus.h"

#undef LOG_PREFIX
#define LOG_PREFIX "[SASSY][LE][LEADER]"

int leader_process_pkt(struct sassy_device *sdev, int remote_lid, unsigned char *pkt)
{

	struct consensus_priv *priv = 
			(struct consensus_priv *)sdev->le_proto->priv;


	u8 opcode = GET_LE_PAYLOAD(pkt, opcode);
	u32 param1 = GET_LE_PAYLOAD(pkt, param1);
	u32 param2 = GET_LE_PAYLOAD(pkt, param2);

	switch(opcode){

	case VOTE:

		if(sdev->verbose >= 1)
			sassy_dbg("received vote from host: %d - term=%u\n",remote_lid, param1);
	
		break;
	
	case NOMI:

		if(sdev->verbose >= 1)
			sassy_dbg("received nomination from host: %d - term=%u\n",remote_lid, param1);
	
		break;		
	
	case NOOP:
	
		if(sdev->verbose >= 1)
			sassy_dbg("received NOOP from host: %d - term=%u\n", remote_lid, param1);
	
		break;
	
	case LEAD:

		if(param1 > priv->term){

			if(sdev->verbose >= 1)
				sassy_dbg("Received message from new leader with higher or equal term=%u\n", param1);

			accept_leader(sdev, remote_lid, param1);

		} else {

			if(sdev->verbose >= 1)
				sassy_dbg("Received LEAD from leader with lower or equal term=%u\n", param1);
	
			// Ignore this LEAD message, continue to send LEAD messages 
	
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
