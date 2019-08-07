#include <sassy/logger.h>
#include <sassy/sassy.h>

#include <linux/random.h>
#include <linux/timer.h>


#include "include/follower.h"


static u32 follower_timeout_ms;

#define MIN_FOLLOWER_TIMEOUT_MS 150
#define MAX_FOLLOWER_TIMEOUT_MS 300

struct timer_list ftimer;


void _handle_follower_timeout(unsigned long data)
{
	sassy_dbg(" Follower Timeout occured! \n");
}

int follower_process_pkt(struct sassy_device *sdev, void* pkt)
{

	// Check if pkt is from leader
	reset_timeout(0);

	return 0;
}


/*
 * Resets the current ftimer to start (again) with a new random timeout in 
 * the intervall of [MIN_FOLLOWER_TIMEOUT_MS, MAX_FOLLOWER_TIMEOUT_MS]
 */
int reset_timeout(int sassy_id)
{
	follower_timeout_ms = MIN_FOLLOWER_TIMEOUT_MS + 
						prandom_u32_max(MAX_FOLLOWER_TIMEOUT_MS -
				   				   		MIN_FOLLOWER_TIMEOUT_MS);

    sassy_dbg(" set follower timeout to %d", follower_timeout_ms);

    mod_timer(&ftimer, jiffies + msecs_to_jiffies(follower_timeout_ms));


	return 0;
}

int stop_follower(int sassy_id)
{
	del_timer(&ftimer);

	sassy_dbg(" node stopped beeing a follower\n");
	return 0;
}


/*
 * ftimer gets initialized and is valid as long as this node is a follower
 */
int start_follower(int sassy_id)
{
	int err;

	setup_timer(&ftimer, _handle_follower_timeout, 0);

	err = reset_timeout(sassy_id);		

	if(err)
		goto error;

	sassy_dbg(" node become a follower\n");

	return 0;

error:
	sassy_error("  failed to start as follower\n");
	return err;
}
EXPORT_SYMBOL(start_follower);