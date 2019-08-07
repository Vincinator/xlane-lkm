#include <sassy/logger.h>
#include <sassy/sassy.h>

#include <linux/random.h>

static u32 follower_timeout;


#define MIN_FOLLOWER_TIMEOUT_MS 150
#define MAX_FOLLOWER_TIMEOUT_MS 300

int start_follower(int sassy_id)
{
	follower_timeout = 	MIN_FOLLOWER_TIMEOUT_MS +
					   	prandom_u32_max(MAX_FOLLOWER_TIMEOUT_MS -
				   				   		MIN_FOLLOWER_TIMEOUT_MS);
				
	

	return 0;
}
EXPORT_SYMBOL(start_follower);