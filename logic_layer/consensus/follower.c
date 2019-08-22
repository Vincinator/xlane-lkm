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

	if(sdev->verbose >= 1)
		sassy_dbg("Follower Timeout occured!\n");

	err = node_transition(sdev, CANDIDATE);

	if (err){
		sassy_dbg("Error occured during the transition to candidate role\n");
		return HRTIMER_NORESTART;
	}

	return HRTIMER_NORESTART;
}

void reply_vote(struct sassy_device *sdev, int remote_lid, int param1, int param2)
{
	struct consensus_priv *priv = 
				(struct consensus_priv *)sdev->le_proto->priv;

	if(sdev->verbose >= 1)
		sassy_dbg("Preparing vote for next hb interval.\n");

	setup_le_msg(&priv->sdev->pminfo, VOTE, remote_lid, param1);
	priv->voted = param1;

}

int follower_process_pkt(struct sassy_device *sdev, int remote_lid, unsigned char *pkt)
{
	struct consensus_priv *priv = 
			(struct consensus_priv *)sdev->le_proto->priv;

	u8 opcode = GET_LE_PAYLOAD(pkt, opcode);
	u32 param1 = GET_LE_PAYLOAD(pkt, param1);
	u32 param2 = GET_LE_PAYLOAD(pkt, param2);

	if(sdev->verbose >= 3)
		sassy_dbg("received packet to process\n");

	switch(opcode){
	case VOTE:

		sassy_error("BUG! This node is a Follower," \
				  "but it received a VOTE from host: %d - term=%u\n", remote_lid, param1);

		break;
	case NOMI:

		if(sdev->verbose >= 1)
			sassy_dbg("received NOMI from host: %d - term=%u\n", remote_lid, param1);
		
		if(priv->term > param1) {
	 		if (priv->voted == param1) {
				sassy_dbg("Voted already. Waiting for ftimeout or HB from voted leader.\n");
			} else {
				reply_vote(sdev, remote_lid, param1, param2);
				reset_ftimeout(sdev);
			}
		}

		break;	
	case NOOP:
		
		if(sdev->verbose >= 1)
			sassy_dbg("received NOOP from host: %d - term=%u\n", remote_lid, param1);
		
		break;
	case LEAD:

		/* Received a LEAD operation from a node with a higher term, 
		 * thus this node is accepting the node as new leader.
		 */
		if(param1 > priv->term){

			if(sdev->verbose >= 1)
				sassy_dbg("Received message from new leader with higher term=%u\n", param1);

			accept_leader(sdev, remote_lid, param1);

		} 

		/* Received a LEAD operation from a node with the same term,
		 * thus, this node has to check whether it is already following 
		 * that node (continue to follow), or if it is a LEAD operation 
		 * from a node that is currently not the follower (Ignore and let 
		 * the timeout continue to go down).
		 */
		else if(param1 == priv->term) {

			if(priv->leader_id == remote_lid){

				if(sdev->verbose >= 1)
					sassy_dbg("Received message from known leader term=%u\n", param1);

				reset_ftimeout(sdev);

			}else {
				if(sdev->verbose >= 1)
					sassy_dbg("Received message from new leader term=%u\n", param1);

				// Ignore this LEAD message, let the ftimer continue. 
			}
		}
		/* Received a LEAD operation from a node with a lower term.
		 * Ignoring this LEAD operation and let the countdown continue to go down.
		 */
		else {

			if(sdev->verbose >= 1)
				sassy_dbg("Received LEAD from leader with lower term=%u\n", param1);

			// Ignore this LEAD message, let the ftimer continue. 
		}
		break;
	default:
		sassy_dbg("Unknown opcode received from host: %d - opcode: %d\n",remote_lid, opcode);

	}

	return 0;
}

void init_timeout(struct sassy_device *sdev)
{
	int ftime_ns;
	ktime_t timeout;
	struct consensus_priv *priv = 
				(struct consensus_priv *)sdev->le_proto->priv;

	if(priv->ftimer_init == 1) {
		reset_ftimeout(sdev);
		return;
	}

	timeout = get_rnd_timeout();

	hrtimer_init(&priv->ftimer, CLOCK_MONOTONIC, HRTIMER_MODE_REL_PINNED);
	priv->ftimer_init = 1;

	priv->ftimer.function = &_handle_follower_timeout;

	hrtimer_start(&priv->ftimer, timeout, HRTIMER_MODE_REL_PINNED);
}

void reset_ftimeout(struct sassy_device *sdev)
{
	ktime_t now;
	ktime_t timeout;
	struct consensus_priv *priv = 
				(struct consensus_priv *)sdev->le_proto->priv;

	now = ktime_get();
	timeout = get_rnd_timeout();

	hrtimer_forward(&priv->ftimer, now, timeout);

	if(sdev->verbose >= 1)
		sassy_dbg("set candidate timeout to %dns\n", timeout);
}

int stop_follower(struct sassy_device *sdev)
{
	struct consensus_priv *priv = 
				(struct consensus_priv *)sdev->le_proto->priv;

	if(priv->ftimer_init == 0)
		return 0;

	priv->ftimer_init = 0;

	return hrtimer_try_to_cancel(&priv->ftimer) != -1;
}

int start_follower(struct sassy_device *sdev)
{
	int err;
	struct consensus_priv *priv = 
				(struct consensus_priv *)sdev->le_proto->priv;


	err = setup_le_broadcast_msg(sdev, NOOP);
	
	if(err)
		goto error;

	priv->votes = 0;
	priv->nstate = FOLLOWER;

	init_timeout(sdev);

	sassy_dbg(" node become a follower\n");

	return 0;

error:
	sassy_error("  failed to start as follower\n");
	return err;
}
EXPORT_SYMBOL(start_follower);
