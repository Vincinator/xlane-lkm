#include <sassy/logger.h>
#include <sassy/sassy.h>


#include "include/leader.h"
#include "include/sassy_consensus.h"


int leader_process_pkt(struct sassy_device *sdev, struct sassy_payload * pkt)
{

	switch(pkt->lep.opcode){
		case VOTE:
			sassy_dbg("received vote! term=%d\n", pkt->lep.param1);
			break;
		case NOMI:
			sassy_dbg("received nomination! term=%d\n", pkt->lep.param1);
			break;		
		case NOOP:
			sassy_dbg("received NOOP! term=%d\n", pkt->lep.param1);
			break;
		default:
			sassy_dbg("Unknown opcode received: %d\n", pkt->lep.opcode);
	}

	return 0;
}

int stop_leader(struct sassy_device *sdev)
{

	return 0;
}

int start_leader(struct sassy_device *sdev)
{


	return 0;
}
EXPORT_SYMBOL(start_leader);
