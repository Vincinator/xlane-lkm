#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <asguard/logger.h>

#include <linux/random.h>
#include <linux/timer.h>

#include <asguard/asguard.h>
#include <linux/slab.h>

#include <asguard/payload_helper.h>

#include "include/consensus_helper.h"
#include "include/asguard_consensus_ops.h"
#include <asguard/consensus.h>

// Default Values for timeouts
#define MIN_FTIMEOUT_NS 10000000
#define MAX_FTIMEOUT_NS 20000000
#define MIN_CTIMEOUT_NS 20000000
#define MAX_CTIMEOUT_NS 40000000


#undef LOG_PREFIX
#define LOG_PREFIX "[ASGUARD][CONSENSUS]"

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
EXPORT_SYMBOL(nstate_string);

const char *opcode_string(enum le_opcode opcode)
{
	switch (opcode) {
	case VOTE:
		return "Vote";
	case NOMI:
		return "Nomination";
	case NOOP:
		return "Noop";
	case APPEND:
		return "Append";
	case APPEND_REPLY:
		return "Append Reply";
	case ALIVE:
		return "Alive";
	default:
		return "Unknown State ";
	}
}

int consensus_is_alive(struct consensus_priv *priv)
{
    if(!priv)
        return 0;

    if (priv->state != LE_RUNNING)
		return 0;

	return 1;
}
EXPORT_SYMBOL(consensus_is_alive);

void log_le_rx(int verbose, enum node_state nstate, uint64_t ts, int term, enum le_opcode opcode, int rcluster_id, int rterm)
{

	if (opcode == NOOP)
		return;

	if (opcode == ALIVE)
		return;

    if (opcode == ADVERTISE)
        return;

	if(verbose)
		asguard_log_le("%s, %llu, %d: %s from %d with term %d\n",
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
EXPORT_SYMBOL(get_rnd_timeout);

void set_ae_data(unsigned char *pkt,
				 s32 in_term,
				 s32 in_leader_id,
				 s32 first_idx,
				 s32 in_prevLogTerm,
				 s32 in_leaderCommitIdx,
				 struct consensus_priv *priv,
				 s32 num_of_entries,
				 int more)
{
	struct sm_log_entry **entries = priv->sm_log.entries;
	u16 *opcode;
	s32 *prev_log_idx, *leader_commit_idx;
	u32 *included_entries, *term, *prev_log_term, *leader_id;
	int i, buf_idx;
	u32 *cur_ptr;

	opcode = GET_CON_AE_OPCODE_PTR(pkt);
	*opcode = (u16) APPEND;

	term = GET_CON_AE_TERM_PTR(pkt);
	*term = in_term;

	leader_id = GET_CON_AE_LEADER_ID_PTR(pkt);
	*leader_id = in_leader_id;

	prev_log_idx = GET_CON_AE_PREV_LOG_IDX_PTR(pkt);
	*prev_log_idx = first_idx - 1;

	prev_log_term = GET_CON_AE_PREV_LOG_TERM_PTR(pkt);
	*prev_log_term = in_prevLogTerm;

	included_entries = GET_CON_AE_NUM_ENTRIES_PTR(pkt);
	*included_entries = num_of_entries;

	leader_commit_idx = GET_CON_AE_PREV_LEADER_COMMIT_IDX_PTR(pkt);
	*leader_commit_idx = in_leaderCommitIdx;

	// done if there are no entries to include
	if (num_of_entries == 0)
		return;

	if (first_idx > priv->sm_log.last_idx) {
		asguard_error("Nothing to send, first_idx > priv->sm_log.last_idx %d, %d", first_idx, priv->sm_log.last_idx);
		*included_entries = 0;
		return;
	}

	//check if num_of_entries would exceed actual entries
	if ((first_idx + (num_of_entries - 1)) > priv->sm_log.last_idx) {
		asguard_error("BUG! can not send more entries than available... %d, %d, %d\n",
					first_idx, num_of_entries, priv->sm_log.last_idx);
		*included_entries = 0;
		return;
	}

	cur_ptr = GET_CON_PROTO_ENTRIES_START_PTR(pkt);

	for (i = first_idx; i < first_idx + num_of_entries; i++) {

	    /* Converting consensus idx to buffer idx */
        buf_idx = consensus_idx_to_buffer_idx(&priv->sm_log, i);

        if( buf_idx < 0 ){
            asguard_dbg("Could not translate idx=%d to buffer index ", i);
            return;
        }

        if (!entries[buf_idx]) {
			asguard_dbg("BUG! - entries at %d is null", i);
			return;
		}

		if (!entries[buf_idx]->dataChunk) {
			asguard_dbg("BUG! - entries dataChunk at %d is null", i);
			return;
		}

        //print_hex_dump(KERN_DEBUG, "writing to pkt: ", DUMP_PREFIX_NONE, 16,1,
         //              entries[buf_idx]->dataChunk->data, sizeof(struct data_chunk), 0);

        memcpy(cur_ptr, entries[buf_idx]->dataChunk->data, sizeof(struct data_chunk));

		// TODO: u32bit ptr increased twice to land at next data_chunk..
		//         ... however, we need a stable method if we change the data_chunk size!!
		cur_ptr++;
		cur_ptr++;


	}

}

int check_handle_nomination(struct consensus_priv *priv, u32 param1, u32 param2, u32 param3, u32 param4, int rcluster_id, int remote_lid)
{
    u32 buf_lastidx;

	if (priv->term < param1) {
		if (priv->voted == param1) {
#if VERBOSE_DEBUG
		asguard_dbg("Voted already. Waiting for ftimeout or HB from voted leader.\n");
#endif
			return 0;
		} else {

			if(priv->sdev->cur_leader_lid >= 0 && priv->sdev->cur_leader_lid < priv->sdev->pminfo.num_of_targets)
				if(priv->sdev->pminfo.pm_targets[priv->sdev->cur_leader_lid].alive
					&& priv->leader_id < rcluster_id)
					return 0; // current leader is alive and has better id

			// if local log is empty, just grant the vote!
			if (priv->sm_log.last_idx == -1)
				return 1;

            buf_lastidx = consensus_idx_to_buffer_idx(&priv->sm_log, priv->sm_log.last_idx);

            if(buf_lastidx == -1) {
                asguard_error("Invalid idx. could not convert to buffer idx in %s",__FUNCTION__);
                return -1;
            }


			// Safety Check during development & Debugging..
			if (priv->sm_log.entries[buf_lastidx] == NULL) {
				asguard_dbg("BUG! Log is faulty can not grant any votes. \n");
				return 0;
			}

			// candidates log is at least as up to date as the local log!
			if (param3 >= priv->sm_log.last_idx){
                // Terms of previous log item must match with lastLogTerm of Candidate
                if (priv->sm_log.entries[buf_lastidx]->term == param4)
                    return 1;
			}

		}
	}
	return 0; // got request of invalid term! (lower or equal current term)
}
EXPORT_SYMBOL(check_handle_nomination);


void set_le_opcode_ad(unsigned char *pkt, enum le_opcode opco, s32 cluster_id, s32 self_ip, unsigned char *self_mac)
{
    u16 *opcode;
    u32 *param1;
    s32 *param2;
    char *param3;

    opcode = GET_CON_PROTO_OPCODE_PTR(pkt);
    *opcode = (u16) opco;

    param1 = GET_CON_PROTO_PARAM1_PTR(pkt);
    *param1 = (u32) cluster_id;

    param2 = GET_CON_PROTO_PARAM2_PTR(pkt);
    *param2 = (s32) self_ip;

    param3 = GET_CON_PROTO_PARAM3_MAC_PTR(pkt);

    memcpy(param3, self_mac, 6);

}

void set_le_opcode(unsigned char *pkt, enum le_opcode opco, s32 p1, s32 p2, s32 p3, s32 p4)
{
	u16 *opcode;
	u32 *param1;
	s32 *param2, *param3, *param4;

	opcode = GET_CON_PROTO_OPCODE_PTR(pkt);
	*opcode = (u16) opco;

	param1 = GET_CON_PROTO_PARAM1_PTR(pkt);
	*param1 = (u32) p1;

	param2 = GET_CON_PROTO_PARAM2_PTR(pkt);
	*param2 = (s32) p2;

	param3 = GET_CON_PROTO_PARAM3_PTR(pkt);
	*param3 = (s32) p3;

	param4 = GET_CON_PROTO_PARAM4_PTR(pkt);
	*param4 = (s32) p4;
}

static const struct asguard_protocol_ctrl_ops consensus_ops = {
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

struct proto_instance *get_consensus_proto_instance(struct asguard_device *sdev)
{
    struct consensus_priv *cpriv;
    struct proto_instance *ins;

    // freed by get_echo_proto_instance
    ins = kmalloc (sizeof(struct proto_instance), GFP_KERNEL);

    if (!ins)
        goto error;

    ins->proto_type = ASGUARD_PROTO_CONSENSUS;
    ins->ctrl_ops = consensus_ops;

    ins->logger.name = "consensus_le";
    ins->logger.instance_id = ins->instance_id;
    ins->logger.ifindex = sdev->ifindex;

    // freed by clear_protocol_instances
    ins->proto_data = kmalloc(sizeof(struct consensus_priv), GFP_KERNEL);

    cpriv = (struct consensus_priv *)ins->proto_data;

    if (!cpriv)
        goto error;

    cpriv->throughput_logger.instance_id = ins->instance_id;
    cpriv->throughput_logger.ifindex = sdev->ifindex;
    cpriv->throughput_logger.name = "consensus_throughput";

    cpriv->state = LE_UNINIT;
    cpriv->ft_min = MIN_FTIMEOUT_NS;
    cpriv->ft_max = MAX_FTIMEOUT_NS;
    cpriv->ct_min = MIN_CTIMEOUT_NS;
    cpriv->ct_max = MAX_CTIMEOUT_NS;
    cpriv->max_entries_per_pkt = MAX_AE_ENTRIES_PER_PKT;
    cpriv->sdev = sdev;
    cpriv->ins = ins;
    cpriv->llts_before_ftime = 0;

    return ins;

error:
    asguard_dbg("Error in %s", __func__);
    return NULL;
}
EXPORT_SYMBOL(get_consensus_proto_instance);