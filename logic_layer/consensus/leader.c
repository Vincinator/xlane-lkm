#include <sassy/logger.h>
#include <sassy/sassy.h>


#include "include/leader.h"
#include "include/sassy_consensus.h"

#undef LOG_PREFIX
#define LOG_PREFIX "[SASSY][LE][LEADER]"

int leader_process_pkt(struct sassy_device *sdev, int remote_lid, struct sassy_payload * pkt)
{

	switch(pkt->lep.opcode){
	case VOTE:
		sassy_dbg("received vote from host: %d - term=%u\n",remote_lid, pkt->lep.param1);
		break;
	case NOMI:
		sassy_dbg("received nomination from host: %d - term=%u\n",remote_lid, pkt->lep.param1);
		break;		
	case NOOP:
		if(sdev->verbose >= 3)
			sassy_dbg("received NOOP from host: %d - term=%u\n", remote_lid, pkt->lep.param1);
		break;
	default:
		sassy_dbg("Unknown opcode received from host: %d - opcode: %d\n",remote_lid, pkt->lep.opcode);

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

	priv->nstate = LEADER;

	return 0;
}
EXPORT_SYMBOL(start_leader);
