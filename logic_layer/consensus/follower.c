#include <sassy/logger.h>
#include <sassy/sassy.h>

#include <linux/random.h>
#include <linux/timer.h>


#include "include/follower.h"


static u32 follower_timeout_ms;

#define MIN_FTIMEOUT_NS 150000000
#define MAX_FTIMEOUT_NS 300000000

static struct hrtimer ftimer;


void _handle_follower_timeout(unsigned long data)
{
	sassy_dbg(" Follower Timeout occured!\n");
}

int follower_process_pkt(struct sassy_device *sdev, void *pkt)
{

	// Check if pkt is from leader
	reset_timeout();
	sassy_dbg("Timeout reset!\n");
	return 0;
}

void init_timeout(void)
{
	int ftime_ns;
	ktime_t timeout;

	timeout = get_rnd_timeout();

	hrtimer_init(&ftimer, CLOCK_MONOTONIC, HRTIMER_MODE_REL_PINNED);

	ftimer.function = &_handle_follower_timeout;

	hrtimer_start(&ftimer, timeout, HRTIMER_MODE_REL_PINNED);
}

ktime_t get_rnd_timeout(void)
{
	return ktime_set(0, MIN_FTIMEOUT_NS +
			prandom_u32_max(MAX_FTIMEOUT_NS - MIN_FTIMEOUT_NS));
}


void reset_timeout(void)
{
	ktime_t now;
	ktime_t timeout;

	now = ktime_get();
	timeout = get_rnd_timeout();

	hrtimer_forward(&ftimer, now, timeout);

	sassy_dbg("set follower timeout to %dns\n", timeout);
}

int stop_follower(int sassy_id)
{
	hrtimer_try_to_cancel(&ftimer);
	return 0;
}

int start_follower(int sassy_id)
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
