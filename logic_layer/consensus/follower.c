#include <sassy/logger.h>
#include <sassy/sassy.h>

#include <linux/random.h>
#include <linux/timer.h>


#include "include/follower.h"
#include "include/sassy_consensus.h"


static enum hrtimer_restart _handle_follower_timeout(struct hrtimer *timer)
{
	int err;
	
	if(priv->ftimer_init = 0) {
		sassy_dbg("ftimer was stopped before - quiting ftimer without restart now");
		return HRTIMER_NORESTART;
	}

	sassy_dbg("Follower Timeout occured!\n");

	err = node_transition(CANDIDATE);

	if (err){
		sassy_dbg("Error occured during the transition to candidate role\n");
		return HRTIMER_NORESTART;
	}

	return HRTIMER_NORESTART;
}

int follower_process_pkt(struct sassy_device *sdev, void *pkt)
{

	reset_ftimeout();
	sassy_dbg("Timeout reset!\n");
	return 0;
}

void init_timeout(void)
{
	int ftime_ns;
	ktime_t timeout;
	struct consensus_priv *priv = con_priv();

	timeout = get_rnd_timeout();

	hrtimer_init(&priv->ftimer, CLOCK_MONOTONIC, HRTIMER_MODE_REL_PINNED);
	priv->ftimer_init = 1;

	priv->ftimer.function = &_handle_follower_timeout;

	hrtimer_start(&priv->ftimer, timeout, HRTIMER_MODE_REL_PINNED);
}

void reset_ftimeout(void)
{
	ktime_t now;
	ktime_t timeout;
	struct consensus_priv *priv = con_priv();

	now = ktime_get();
	timeout = get_rnd_timeout();

	hrtimer_forward(&priv->ftimer, now, timeout);

	sassy_dbg("set follower timeout to %dns\n", timeout);
}

int stop_follower(void)
{
	struct consensus_priv *priv = con_priv();

	if(priv->ftimer_init == 0)
		return 0;

	priv->ftimer_init = 0;
	return hrtimer_try_to_cancel(&priv->ftimer) != -1;
}

int start_follower(void)
{
	int err;

	init_timeout();


	sassy_dbg(" node become a follower\n");

	return 0;

error:
	sassy_error("  failed to start as follower\n");
	return err;
}
EXPORT_SYMBOL(start_follower);
