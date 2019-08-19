#include <sassy/logger.h>
#include <sassy/sassy.h>

#include <linux/random.h>
#include <linux/timer.h>


#include "include/follower.h"
#include "include/sassy_consensus.h"


int follower_process_pkt(struct sassy_device *sdev, void *pkt)
{
	sassy_dbg("Timeout reset!\n");
	return 0;
}

void init_timeout(void)
{

}

void reset_ftimeout(void)
{

}

int stop_follower(void)
{
	return 0;
}

int start_follower(void)
{

	sassy_dbg(" node become a follower\n");

	return 0;
}
EXPORT_SYMBOL(start_follower);
