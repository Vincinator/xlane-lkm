#include <sassy/logger.h>
#include <sassy/sassy.h>
#include <sassy/payload_helper.h>

#include "include/consensus_helper.h"
#include "include/leader.h"
#include <sassy/consensus.h>

#undef LOG_PREFIX
#define LOG_PREFIX "[SASSY][LE][LEADER]"

void initialze_indices(struct consensus_priv *priv)
{
	int i;

	for(i = 0; i < priv->sdev->pminfo.num_of_targets; i++){
		// initialize to leader last log index + 1
		priv->sm_log.next_index[i] = priv->sm_log.last_idx + 1;
		priv->sm_log.match_index[i] = 0;
	}
}

int _is_potential_commit_idx(struct consensus_priv *priv, int N)
{
	int i, hits,double_majority;

	hits = 0;

	double_majority = priv->sdev->pminfo.num_of_targets + 1;

	for(i = 0; i < priv->sdev->pminfo.num_of_targets; i++){
		if(priv->sm_log.match_index[i] >= N) {
			hits++;
		}
	}
	return hits * 2 >= double_majority;
}

void update_commit_idx(struct consensus_priv *priv)
{
	int N, current_N, i;

	if(!priv){
		sassy_error("prib is NULL!\n");
		return;
	}

	N = priv->sm_log.match_index[0];

	// each match_index is a potential new commit_idx candidate
	for(i = 0; i < priv->sdev->pminfo.num_of_targets; i++){

		current_N = priv->sm_log.match_index[i];

		if(current_N >= MAX_CONSENSUS_LOG){
			sassy_error("\n");
			return;
		}

		if(priv->sm_log.entries[current_N]->term == priv->term)
			// majority of match_index[j] >= N and sm_log.entries[N]->term == currentTerm
			if(_is_potential_commit_idx(priv, current_N))
				if(current_N > N){
					write_log(&priv->ins->logger, GOT_CONSENSUS_ON_VALUE, rdtsc());
					N = current_N;
				}
	}

	if(N > priv->sm_log.commit_idx){
		priv->sm_log.commit_idx = N;
		// sassy_dbg("found new commit_idx %d", N);
	}

}

int leader_process_pkt(struct proto_instance *ins, int remote_lid, int rcluster_id, unsigned char *pkt)
{
	struct consensus_priv *priv = 
		(struct consensus_priv *)ins->proto_data;

	struct sassy_device *sdev = priv->sdev;

	u8 opcode = GET_CON_PROTO_OPCODE_VAL(pkt);
	s32 param1, param2, param3;
	s32 param4 = GET_CON_PROTO_PARAM4_VAL(pkt);
	
	switch(opcode){
	case VOTE:
		break;
	case NOMI:
		break;		
	case NOOP:
		break;
	case APPEND_REPLY:
		// param1 intepreted as last term of follower
		// param2 interpreted as success 
		// param3 contains last idx in follower log 
		param2 = GET_CON_PROTO_PARAM2_VAL(pkt);
		param3 = GET_CON_PROTO_PARAM3_VAL(pkt);

		// check if success

		if(param2 == 1){
			// append rpc success!
			//sassy_dbg("Received Reply with State=success param3=%d\n",param3);
			// update match Index for follower with <remote_lid> 
			
			// Asguard can potentially send multiple appendEntries RPCs, and after each RPC
			// the next_index must be updated to indicate wich entries to send next..
			// But the replies to the appendEntries RPC will indicate that only to certain index the
			// follower log was updated. Thus, the follower must include the information to which 
			// index it has updated the follower log. As an alternative, the leader could remember a state
			// including the index after emitting the udp packet.. 
			//priv->sm_log.match_index[remote_lid] = priv->sm_log.next_index[remote_lid] - 1;

			priv->sm_log.match_index[remote_lid] = param3;

			update_commit_idx(priv);


		} else {
			// append rpc failed!
			//sassy_dbg("Received Reply with State=failed\n");

			// decrement nextIndex for follower with <remote_lid>
			priv->sm_log.next_index[remote_lid] = param3 + 1;

		}

		break;

	case APPEND:
		param1 = GET_CON_PROTO_PARAM1_VAL(pkt);

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
			sassy_log_le("%s, %llu, %d: Cluster node %d also claims to be leader in term %u.\n",
				nstate_string(priv->nstate),
				rdtsc(),
				priv->term,
				rcluster_id,
				param1);
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

	return 0;
}

int start_leader(struct proto_instance *ins)
{
	struct consensus_priv *priv = (struct consensus_priv *)ins->proto_data;
	struct sassy_payload *pkt_payload;
	struct pminfo *spminfo = &priv->sdev->pminfo;
	int hb_passive_ix, i;
	
	initialze_indices(priv);

	for(i = 0; i < priv->sdev->pminfo.num_of_targets; i++){
		hb_passive_ix =
		     !!!spminfo->pm_targets[i].pkt_data.hb_active_ix;

		pkt_payload =
	     	spminfo->pm_targets[i].pkt_data.pkt_payload[hb_passive_ix];

		setup_append_msg(priv, pkt_payload, ins->instance_id, i);
	}

	return 0;
}
EXPORT_SYMBOL(start_leader);
