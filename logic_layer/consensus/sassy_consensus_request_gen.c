#include <sassy/logger.h>
#include <sassy/sassy.h>
#include <sassy/consensus.h>

#include "include/sassy_consensus_ops.h"
#include "include/candidate.h"
#include "include/follower.h"
#include "include/leader.h"
#include "include/test.h"


#define MAX_VALUE_SM_VALUE_SPACE 1024
#define MAX_VALUE_SM_ID_SPACE 255



void testcase_stop_timer(struct consensus_priv *priv)
{
	priv->test_data.running = 0;

	sassy_dbg("Actions will not be performed on next timeout.\n");
	sassy_dbg("Timer will be stopped directly on next timeout\n");
}



/* Fully saturates the local log (if leader)  
 *
 */
void testcase_one_shot_big_log(struct consensus_priv *priv)
{
	struct state_machine_cmd_log *log = &priv->sm_log;


}


static enum hrtimer_restart testcase_timer(struct hrtimer *timer)
{
	unsigned long flags;
	struct consensus_test_container *test_data =
			container_of(timer, struct consensus_test_container, timer);

	struct consensus_priv *priv = test_data->priv;
	u32 rand_value, rand_id;
	ktime_t currtime, interval;
	struct sm_command *cur_cmd;
	int err = 0;

	if (test_data->running == 0)
		return HRTIMER_NORESTART;

	currtime  = ktime_get();
	interval = ktime_set(1, 0);
	hrtimer_forward(timer, currtime, interval);

	if(priv->nstate != LEADER)
		return HRTIMER_RESTART; // nothing to do, node is not a leader.

	// write x random entries to local log (if node is leader)
	for(i = 0, i < test_data.x; i++){
		rand_value = prandom_u32_max(MAX_VALUE_SM_VALUE_SPACE);
		rand_id = prandom_u32_max(MAX_VALUE_SM_ID_SPACE);
		cur_cmd = kmalloc(sizeof(struct sm_command), GFP_KERNEL);
		cur_cmd->sm_logvar_id = rand_id;
		cur_cmd->sm_logvar_value = rand_value;
		err = append_command(&priv->sm_log, cur_cmd);
		if(err)
			goto error;
	}

	return HRTIMER_RESTART;
error:
	sassy_error("Evaluation Crashed errorcode=%d\n", err);
	return HRTIMER_NORESTART;

}

/* 
 *
 */
void _init_testcase_timeout(struct consensus_test_container *test_data)
{
	ktime_t timeout;	

	// timeout to 1 second
	timeout = ktime_set(1, 0);

	hrtimer_init(&test_data->timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL_PINNED);

	test_data->timer.function = &testcase_timer;
	test_data->running = 1;
	hrtimer_start(&test_data->timer, timeout, HRTIMER_MODE_REL_PINNED);
}


void testcase_X_requests_per_sec(struct consensus_priv *priv, int x)
{

	priv->test_data.priv = priv;
	priv->test_data.x = x;

	if(x < 0)  {
		sassy_dbg("Invalid Input \n");
		return;
	}

	// start hrtimer!
	_init_testcase_timeout(&priv->test_data);

	sassy_dbg("Appending %d consensus requests every second, if node is currently the leader\n", x);

}
