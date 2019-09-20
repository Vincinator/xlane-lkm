#include <sassy/logger.h>
#include <sassy/sassy.h>

#include <linux/random.h>
#include <linux/timer.h>

#include <sassy/payload_helper.h>

#include "include/follower.h"
#include "include/sassy_consensus.h"

#undef LOG_PREFIX
#define LOG_PREFIX "[SASSY][LE][FOLLOWER]"


static enum hrtimer_restart _handle_follower_timeout(struct hrtimer *timer)
{
	int err;
	struct consensus_priv *priv = container_of(timer, struct consensus_priv, ftimer);
	struct sassy_device *sdev = priv->sdev;

	if(priv->ftimer_init == 0 || priv->nstate != FOLLOWER)
		return HRTIMER_NORESTART;

	write_log(&priv->ins->logger, FOLLOWER_TIMEOUT, rdtsc());

#if 0
	if(sdev->verbose >= 1)
		sassy_dbg("Follower Timeout occured!\n");


	sassy_log_le("%s, %llu, %d: Follower timer timed out\n",
			nstate_string(priv->nstate),
			rdtsc(),
			priv->term);
#endif

	err = node_transition(priv->ins, CANDIDATE);
	write_log(&priv->ins->logger, FOLLOWER_BECOME_CANDIDATE, rdtsc());

	if (err){
		sassy_dbg("Error occured during the transition to candidate role\n");
		return HRTIMER_NORESTART;
	}

	return HRTIMER_NORESTART;
}

void reply_vote(struct proto_instance *ins, int remote_lid, int rcluster_id, int param1, int param2)
{
	struct consensus_priv *priv = 
		(struct consensus_priv *)ins->proto_data;
#if 0

	sassy_log_le("%s, %llu, %d: voting for cluster node %d with term %d\n",
			nstate_string(priv->nstate),
			rdtsc(),
			priv->term,
			rcluster_id,
			param1);
#endif

	setup_le_msg(ins, &priv->sdev->pminfo, VOTE, remote_lid, param1);
	priv->voted = param1;

	write_log(&ins->logger, VOTE_FOR_CANDIDATE, rdtsc());

}

int follower_process_pkt(struct proto_instance *ins, int remote_lid, int rcluster_id, unsigned char *pkt)
{
	struct consensus_priv *priv = 
		(struct consensus_priv *)ins->proto_data;

	u8 opcode = GET_LE_PAYLOAD(pkt, opcode);
	u32 param1 = GET_LE_PAYLOAD(pkt, param1);
	u32 param2 = GET_LE_PAYLOAD(pkt, param2);

#if 0
	log_le_rx(sdev->verbose, priv->nstate, rdtsc(), priv->term, opcode, rcluster_id, param1);
#endif

	switch(opcode){
	case VOTE:
		break;
	case NOMI:	
		if(priv->term < param1) {
	 		if (priv->voted == param1) {
#if 0
				sassy_dbg("Voted already. Waiting for ftimeout or HB from voted leader.\n");
#endif	
			} else {
				reply_vote(ins, remote_lid, rcluster_id, param1, param2);
				reset_ftimeout(ins);
			}
		}

		break;	
	case NOOP:
		break;
	case LEAD:

		/* Received a LEAD operation from a node with a higher term, 
		 * thus this node is accepting the node as new leader.
		 */
		if(param1 > priv->term){

#if 0
			if(sdev->verbose >= 2)
				sassy_dbg("Received message from new leader with higher term=%u local term=%u\n", param1, priv->term);
#endif

			accept_leader(ins, remote_lid, rcluster_id, param1);
			write_log(&ins->logger, FOLLOWER_ACCEPT_NEW_LEADER, rdtsc());
			reset_ftimeout(ins);

		} 

		/* Received a LEAD operation from a node with the same term,
		 * thus, this node has to check whether it is already following 
		 * that node (continue to follow), or if it is a LEAD operation 
		 * from a node that is currently not the follower (Ignore and let 
		 * the timeout continue to go down).
		 */
		else if(param1 == priv->term) {

			if(priv->leader_id == remote_lid){
#if 0
				if(sdev->verbose >= 2)
					sassy_dbg("Received message from known leader term=%u\n", param1);
#endif
				reset_ftimeout(ins);

			}else {
#if 0
				if(sdev->verbose >= 2)
					sassy_dbg("Received message from new leader term=%u\n", param1);
#endif
				// Ignore this LEAD message, let the ftimer continue. 
			}
		}
		/* Received a LEAD operation from a node with a lower term.
		 * Ignoring this LEAD operation and let the countdown continue to go down.
		 */
		else {
#if 0
			if(sdev->verbose >= 2)
				sassy_dbg("Received LEAD from leader with lower term=%u\n", param1);
#endif
			// Ignore this LEAD message, let the ftimer continue. 
		}
		break;
	default:
		sassy_dbg("Unknown opcode received from host: %d - opcode: %d\n",remote_lid, opcode);

	}

	return 0;
}

void init_timeout(struct proto_instance *ins)
{
	int ftime_ns;
	ktime_t timeout;	
	struct consensus_priv *priv = 
		(struct consensus_priv *)ins->proto_data;

	if(priv->ftimer_init == 1) {
		reset_ftimeout(ins);
		return;
	}

	timeout = get_rnd_timeout(priv->ft_min, priv->ft_max);

	hrtimer_init(&priv->ftimer, CLOCK_MONOTONIC, HRTIMER_MODE_REL_PINNED);
	priv->ftimer_init = 1;

	priv->ftimer.function = &_handle_follower_timeout;

#if 0
	sassy_log_le("%s, %llu, %d: Init follower timeout to %lld ms. \n",
		nstate_string(priv->nstate),
		rdtsc(),
		priv->term,
		ktime_to_ms(timeout));
#endif

	hrtimer_start_range_ns(&priv->ftimer, timeout, TOLERANCE_FTIMEOUT_NS, HRTIMER_MODE_REL_PINNED);
}

void reset_ftimeout(struct proto_instance *ins)
{
	ktime_t timeout;
	struct consensus_priv *priv = 
		(struct consensus_priv *)ins->proto_data;

	timeout = get_rnd_timeout(priv->ft_min, priv->ft_max);

	hrtimer_cancel(&priv->ftimer);
	hrtimer_set_expires_range_ns(&priv->ftimer, timeout, TOLERANCE_FTIMEOUT_NS);
	hrtimer_start_expires(&priv->ftimer, HRTIMER_MODE_REL_PINNED);

#if 0

	sassy_log_le("%s, %llu, %d: Set follower timeout to %lld ms.\n",
			nstate_string(priv->nstate),
			rdtsc(),
			priv->term,
			ktime_to_ms(timeout));
#endif
}

int stop_follower(struct proto_instance *ins)
{
	struct consensus_priv *priv = 
		(struct consensus_priv *)ins->proto_data;

	if(priv->ftimer_init == 0)
		return 0;

	priv->ftimer_init = 0;

	return hrtimer_cancel(&priv->ftimer) == 1;
}

int start_follower(struct proto_instance *ins)
{
	int err;
	struct consensus_priv *priv = 
			(struct consensus_priv *)ins->proto_data;
	
	err = setup_le_broadcast_msg(ins, NOOP);
	
	if(err)
		goto error;

	priv->votes = 0;
	priv->nstate = FOLLOWER;

	init_timeout(ins);

#if 0
	sassy_dbg("Node became a follower\n");
#endif

	return 0;

error:
	sassy_error("Failed to start as follower\n");
	return err;
}
EXPORT_SYMBOL(start_follower);
