#include <asguard/logger.h>
#include <asguard/asguard.h>
#include <asguard/consensus.h>
#include <asguard/payload_helper.h>


#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/random.h>
#include <linux/timer.h>

#include "include/asguard_consensus_ops.h"
#include "include/candidate.h"
#include "include/follower.h"
#include "include/leader.h"
#include "include/test.h"


#define MAX_VALUE_SM_VALUE_SPACE 1024
#define MAX_VALUE_SM_ID_SPACE 255

void testcase_stop_timer(struct consensus_priv *priv)
{
	priv->test_data.running = 0;

	asguard_dbg("Timer will be stopped directly after next timeout\n");
	asguard_dbg("Actions of next timeout will not be performed.\n");

}

/* Fully saturates the local log (if leader)  
 *
 */
void testcase_one_shot_big_log(struct consensus_priv *priv)
{
	struct state_machine_cmd_log *log = &priv->sm_log;
	int i, err;
	struct sm_command *cur_cmd;
	err = 0;
	u32 rand_value, rand_id;

	for(i = 0; i < MAX_CONSENSUS_LOG; i++) {

		rand_value = prandom_u32_max(MAX_VALUE_SM_VALUE_SPACE);
		rand_id = prandom_u32_max(MAX_VALUE_SM_ID_SPACE);
		cur_cmd = kmalloc(sizeof(struct sm_command), GFP_KERNEL);

		if (!cur_cmd) {
			err = -ENOMEM;
			goto error;
		}
		cur_cmd->sm_logvar_id = rand_id;
		cur_cmd->sm_logvar_value = rand_value;
		err = append_command(&priv->sm_log, cur_cmd, priv->term);
		
		if (err)
			goto error;
	}

	return 0;

error:
	asguard_error("Evaluation Crashed errorcode=%d\n", err);
	return err;

}

static enum hrtimer_restart testcase_timer(struct hrtimer *timer)
{
	unsigned long flags;
	struct consensus_test_container *test_data =
			container_of(timer, struct consensus_test_container, timer);

	struct consensus_priv *priv = test_data->priv;
	struct asguard_device *sdev = priv->sdev;
	u32 rand_value, rand_id;
	ktime_t currtime, interval;
	struct sm_command *cur_cmd;
	struct pminfo *spminfo = &sdev->pminfo;
	struct asguard_payload *spay;
	int err = 0;
	int i, tar, hb_passive_idx;

	if (test_data->running == 0)
		return HRTIMER_NORESTART;

	currtime  = ktime_get();
	interval = ktime_set(1, 0);
	hrtimer_forward(timer, currtime, interval);

	if (priv->nstate != LEADER)
		return HRTIMER_RESTART; // nothing to do, node is not a leader.

	asguard_dbg("Incoming Client requests.. \n");
	
	// write x random entries to local log (if node is leader)
	for(i = 0; i < test_data->x; i++) {

		rand_value = prandom_u32_max(MAX_VALUE_SM_VALUE_SPACE);
		rand_id = prandom_u32_max(MAX_VALUE_SM_ID_SPACE);
		cur_cmd = kmalloc(sizeof(struct sm_command), GFP_KERNEL);

		if (!cur_cmd) {
			err = -ENOMEM;
			goto error;
		}
		cur_cmd->sm_logvar_id = rand_id;
		cur_cmd->sm_logvar_value = rand_value;
		err = append_command(&priv->sm_log, cur_cmd, priv->term);
		write_log(&priv->ins->logger, CONSENSUS_REQUEST, rdtsc());

		if (err)
			goto error;
	}

	return HRTIMER_RESTART;
error:
	asguard_error("Evaluation Crashed errorcode=%d\n", err);
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

	if (x < 0)  {
		asguard_dbg("Invalid Input \n");
		return;
	}

	// start hrtimer!
	_init_testcase_timeout(&priv->test_data);

	asguard_dbg("Appending %d consensus requests every second, if node is currently the leader\n", x);

}