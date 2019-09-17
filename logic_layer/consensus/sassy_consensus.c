#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <sassy/logger.h>

#include <linux/random.h>
#include <linux/timer.h>

#include <sassy/sassy.h>
#include <linux/slab.h>

#include <sassy/payload_helper.h>


#include "include/sassy_consensus_ops.h"
#include "include/sassy_consensus.h"

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Vincent Riesop");
MODULE_DESCRIPTION("SASSY consensus");
MODULE_VERSION("0.01");

// Default Values for timeouts
#define MIN_FTIMEOUT_NS 10000000
#define MAX_FTIMEOUT_NS 20000000
#define MIN_CTIMEOUT_NS 20000000
#define MAX_CTIMEOUT_NS 40000000


#undef LOG_PREFIX
#define LOG_PREFIX "[SASSY][CONSENSUS]"

const char *nstate_string(enum node_state state)
{
	switch (state) {
	case FOLLOWER:
		return "Follower";
	case CANDIDATE:
		return "Candidate";
	case LEADER:
		return "Leader";
	default:
		return "Unknown State ";
	}
}
const char *opcode_string(enum le_opcode opcode)
{
	switch (opcode) {
	case VOTE:
		return "Vote";
	case NOMI:
		return "Nomination";
	case NOOP:
		return "Noop";
	case LEAD:
		return "Lead";
	default:
		return "Unknown State ";
	}
}

int consensus_is_alive(struct consensus_priv *priv)
{

	if(priv->state != LE_RUNNING)
		return 0;

	return 1;
}
EXPORT_SYMBOL(consensus_is_alive);

void log_le_rx(int verbose, enum node_state nstate, uint64_t ts, int term, enum le_opcode opcode, int rcluster_id, int rterm)
{

	if(opcode == NOOP && verbose < 4)
		return;

	if(opcode == LEAD && verbose < 1)
		return;

	sassy_log_le("%s, %llu, %d: %s from %d with term %d\n",
					nstate_string(nstate),
					ts,
					term,
					opcode_string(opcode),
					rcluster_id,
					rterm);
}

ktime_t get_rnd_timeout(int min, int max)
{
	return ktime_set(0, min +
			prandom_u32_max(max - min));
}

void set_le_term(unsigned char *pkt, u32 p1)
{
	SET_LE_PAYLOAD(pkt, param1, p1);
}


void set_le_opcode(unsigned char *pkt, enum le_opcode opcode, u32 p1, u32 p2)
{
	SET_LE_PAYLOAD(pkt, opcode, opcode);
	SET_LE_PAYLOAD(pkt, param1, p1);
	SET_LE_PAYLOAD(pkt, param2, p2);
}

static const struct sassy_protocol_ctrl_ops consensus_ops = {
	.init = consensus_init,
	.start = consensus_start,
	.stop = consensus_stop,
	.clean = consensus_clean,
	.info = consensus_info,
	.post_payload = consensus_post_payload,
	.post_ts = consensus_post_ts,
	.init_payload = consensus_init_payload,
	.us_update = consensus_us_update,
};


static int __init sassy_consensus_init(void)
{
	sassy_dbg("init consensus protocol\n");
	return 0;
}

struct proto_instance *get_consensus_instance(struct sassy_device *sdev)
{
	struct consensus_priv *cpriv;
	struct proto_instance *ins;

	ins = kmalloc (sizeof(struct proto_instance), GFP_KERNEL);

	if(!ins)
		goto error;

	ins->proto_type = SASSY_PROTO_CONSENSUS;
	ins->ctrl_ops = consensus_ops;
	ins->name = "consensus";
	ins->priv = kmalloc(sizeof(struct consensus_priv), GFP_KERNEL);

	cpriv = (struct consensus_priv *)ins->priv;
	cpriv->state = LE_UNINIT;
	cpriv->ft_min = MIN_FTIMEOUT_NS;
	cpriv->ft_max = MAX_FTIMEOUT_NS;
	cpriv->ct_min = MIN_CTIMEOUT_NS;
	cpriv->ct_max = MAX_CTIMEOUT_NS;
	cpriv->le_config_procfs = NULL;
	cpriv->sdev = sdev;
	cpriv->ins = ins;

	return ins;
error:
	sassy_dbg("Error in %s", __FUNCTION__);
	return NULL;
}
EXPORT_SYMBOL(get_consensus_proto_instance);

static void __exit sassy_consensus_exit(void)
{
	
	sassy_dbg("exit consensus protocol\n");
}

module_init(sassy_consensus_init);
module_exit(sassy_consensus_exit);
