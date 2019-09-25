#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <sassy/logger.h>

#include <linux/random.h>
#include <linux/timer.h>

#include <sassy/sassy.h>
#include <linux/slab.h>

#include <sassy/payload_helper.h>

#include "include/log.h"
#include "include/consensus_helper.h"
#include "include/sassy_consensus_ops.h"
#include <sassy/consensus.h>

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Vincent Riesop");
MODULE_DESCRIPTION("SASSY consensus");
MODULE_VERSION("0.01");

// Default Values for timeouts
#define MIN_FTIMEOUT_NS 10000000
#define MAX_FTIMEOUT_NS 20000000
#define MIN_CTIMEOUT_NS 20000000
#define MAX_CTIMEOUT_NS 40000000


#undef LOG_PREFIX
#define LOG_PREFIX "[SASSY][CONSENSUS]"

const char *nstate_string(enum node_state state)
{
	switch (state) {
	case FOLLOWER:
		return "Follower";
	case CANDIDATE:
		return "Candidate";
	case LEADER:
		return "Leader";
	default:
		return "Unknown State ";
	}
}
const char *opcode_string(enum le_opcode opcode)
{
	switch (opcode) {
	case VOTE:
		return "Vote";
	case NOMI:
		return "Nomination";
	case NOOP:
		return "Noop";
	case LEAD:
		return "Lead";
	default:
		return "Unknown State ";
	}
}

int consensus_is_alive(struct consensus_priv *priv)
{

	if(priv->state != LE_RUNNING)
		return 0;

	return 1;
}
EXPORT_SYMBOL(consensus_is_alive);

void log_le_rx(int verbose, enum node_state nstate, uint64_t ts, int term, enum le_opcode opcode, int rcluster_id, int rterm)
{

	if(opcode == NOOP && verbose < 4)
		return;

	if(opcode == LEAD && verbose < 1)
		return;

	sassy_log_le("%s, %llu, %d: %s from %d with term %d\n",
					nstate_string(nstate),
					ts,
					term,
					opcode_string(opcode),
					rcluster_id,
					rterm);
}

ktime_t get_rnd_timeout(int min, int max)
{
	return ktime_set(0, min +
			prandom_u32_max(max - min));
}

void set_ae_data(unsigned char *pkt, 
				 u32 in_term, 
				 u32 in_leader_id,
				 u32 in_prevLogIndex,
				 u32 in_prevLogTerm,
				 u32 in_leaderCommitIdx,
				 struct sm_log_entry **entries, 
				 int num_of_entries)
{
	u16 *opcode;
	u32 *term, *leader_id, *prev_log_idx, *prev_log_term, *leader_commit_idx;
	u32 *included_entries;
	int i;
	u32 *cur_ptr;

	opcode = GET_CON_AE_OPCODE_PTR(pkt);
	*opcode = (u16) APPEND;

	term = GET_CON_AE_TERM_PTR(pkt);
	*term = in_term;

	leader_id = GET_CON_AE_LEADER_ID_PTR(pkt);
	*leader_id = in_leader_id;

	prev_log_idx = GET_CON_AE_PREV_LOG_IDX_PTR(pkt);
	*prev_log_idx = in_prevLogIndex;

	prev_log_term = GET_CON_AE_PREV_LOG_TERM_PTR(pkt);
	*prev_log_term = in_prevLogTerm;

	included_entries = GET_CON_AE_NUM_ENTRIES_PTR(pkt);
	*included_entries = num_of_entries;

	leader_commit_idx = GET_CON_AE_PREV_LEADER_COMMIT_IDX_PTR(pkt);
	*leader_commit_idx = in_leaderCommitIdx;

	cur_ptr = GET_CON_PROTO_ENTRIES_START_PTR(pkt);

	for(i = in_prevLogIndex + 1; i < num_of_entries; i++){
		*cur_ptr = entries[i]->cmd->sm_logvar_id;
		cur_ptr++;
		*cur_ptr = entries[i]->cmd->sm_logvar_value;
		cur_ptr++;
	}


}

int check_handle_nomination(struct consensus_priv *priv, u32 param1, u32 param2, u32 param3, u32 param4)
{
	if(priv->term < param1) {
		if (priv->voted == param1) {
#if 1
		sassy_dbg("Voted already. Waiting for ftimeout or HB from voted leader.\n");
#endif	
			return 0;
		} else {
			// if local log is empty, just accept the vote!
			if(priv->sm_log.last_idx == -1)
				return 1;

			// candidates log is at least as up to date as the local log!
			if(param3 >= priv->sm_log.last_idx){
				// Terms of previous log item must match with lastLogTerm of Candidate
				if(priv->sm_log.entries[param3]->term == param4){
					return 1;
				}
			}

		}
	}
}

void set_le_opcode(unsigned char *pkt, enum le_opcode opco, u32 p1, u32 p2, u32 p3, u32 p4)
{
	u16 *opcode;
	u32 *param1, *param2, *param3, *param4;

	opcode = GET_CON_PROTO_OPCODE_PTR(pkt);
	*opcode = (u16) opco;

	param1 = GET_CON_PROTO_PARAM1_PTR(pkt);
	*param1 = (u32) p1;

	param2 = GET_CON_PROTO_PARAM2_PTR(pkt);
	*param2 = (u32) p2;
	
	param3 = GET_CON_PROTO_PARAM3_PTR(pkt);
	*param3 = (u32) p3;

	param4 = GET_CON_PROTO_PARAM4_PTR(pkt);
	*param4 = (u32) p4;

}

static const struct sassy_protocol_ctrl_ops consensus_ops = {
	.init = consensus_init,
	.start = consensus_start,
	.stop = consensus_stop,
	.clean = consensus_clean,
	.info = consensus_info,
	.post_payload = consensus_post_payload,
	.post_ts = consensus_post_ts,
	.init_payload = consensus_init_payload,
	.us_update = consensus_us_update,
};


static int __init sassy_consensus_init(void)
{
	sassy_dbg("init consensus protocol\n");
	return 0;
}

struct proto_instance *get_consensus_proto_instance(struct sassy_device *sdev)
{
	struct consensus_priv *cpriv;
	struct proto_instance *ins;

	ins = kmalloc (sizeof(struct proto_instance), GFP_KERNEL);
	sassy_dbg("%s %i",__FUNCTION__, __LINE__);

	if(!ins)
		goto error;
	sassy_dbg("%s %i",__FUNCTION__, __LINE__);

	ins->proto_type = SASSY_PROTO_CONSENSUS;
	ins->ctrl_ops = consensus_ops;
	ins->name = "consensus";
	ins->logger.name = "consensus";
	ins->logger.ifindex = sdev->ifindex;
	sassy_dbg("%s %i",__FUNCTION__, __LINE__);

	ins->proto_data = kmalloc(sizeof(struct consensus_priv), GFP_KERNEL);
	sassy_dbg("%s %i",__FUNCTION__, __LINE__);

	cpriv = (struct consensus_priv *)ins->proto_data;
	cpriv->state = LE_UNINIT;
	cpriv->ft_min = MIN_FTIMEOUT_NS;
	cpriv->ft_max = MAX_FTIMEOUT_NS;
	cpriv->ct_min = MIN_CTIMEOUT_NS;
	cpriv->ct_max = MAX_CTIMEOUT_NS;
	cpriv->le_config_procfs = NULL;
	cpriv->sdev = sdev;
	cpriv->ins = ins;
	sassy_dbg("%s %i",__FUNCTION__, __LINE__);

	return ins;
error:
	sassy_dbg("Error in %s", __FUNCTION__);
	return NULL;
}
EXPORT_SYMBOL(get_consensus_proto_instance);

static void __exit sassy_consensus_exit(void)
{
	
	sassy_dbg("exit consensus protocol\n");
}

module_init(sassy_consensus_init);
module_exit(sassy_consensus_exit);
