#include <sassy/logger.h>
#include <sassy/sassy.h>

#include <linux/random.h>
#include <linux/timer.h>


#include "include/follower.h"
#include "include/sassy_consensus.h"


static enum hrtimer_restart _handle_follower_timeout(struct hrtimer *timer)
{
	int err;
	struct consensus_priv *priv = container_of(timer, struct consensus_priv, ftimer);
	struct sassy_device *sdev = priv->sdev;

	sassy_dbg("Follower Timeout occured!\n");

	err = node_transition(sdev, CANDIDATE);

	if (err){
		sassy_dbg("Error occured during the transition to candidate role\n");
		return HRTIMER_NORESTART;
	}

	return HRTIMER_NORESTART;
}

int follower_process_pkt(struct sassy_device *sdev, struct sassy_payload * pkt)
{
	sassy_dbg("%s",__FUNCTION__);
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

void init_timeout(struct sassy_device *sdev)
{
	int ftime_ns;
	ktime_t timeout;
	struct consensus_priv *priv = 
				(struct consensus_priv *)sdev->le_proto->priv;
				
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

	sassy_dbg("set follower timeout to %dns\n", timeout);
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

	init_timeout(sdev);


	sassy_dbg(" node become a follower\n");

	return 0;

error:
	sassy_error("  failed to start as follower\n");
	return err;
}
EXPORT_SYMBOL(start_follower);
