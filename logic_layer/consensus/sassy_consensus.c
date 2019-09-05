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

int consensus_is_alive(struct sassy_device *sdev)
{
	struct consensus_priv *priv;

	if(!sdev || !sdev->le_proto)
		return 0;

	priv = (struct consensus_priv *)sdev->le_proto->priv;

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

void set_le_noop(struct sassy_device *sdev, unsigned char *pkt)
{
	struct consensus_priv *priv = 
				(struct consensus_priv *)sdev->le_proto->priv;

	if(priv->nstate != LEADER)
		SET_LE_PAYLOAD(pkt, opcode, NOOP);
	else
		SET_LE_PAYLOAD(pkt, opcode, LEAD);

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

struct sassy_protocol *get_consensus_proto(struct sassy_device *sdev)
{
	struct sassy_protocol *proto;
	struct consensus_priv *cpriv;

	proto = kmalloc(sizeof(struct sassy_protocol), GFP_KERNEL);

	if(!proto)
		goto error;

	proto->proto_type = SASSY_PROTO_CONSENSUS;
	proto->ctrl_ops = consensus_ops;
	proto->name = "consensus";
	proto->priv = kmalloc(sizeof(struct consensus_priv), GFP_KERNEL);
	cpriv = (struct consensus_priv *)proto->priv;
	cpriv->state = LE_UNINIT;
	priv->le_config_procfs = NULL;

	return proto;
error:
	sassy_dbg("Error in %s", __FUNCTION__);
	return NULL;
}
EXPORT_SYMBOL(get_consensus_proto);

static void __exit sassy_consensus_exit(void)
{
	
	sassy_dbg("exit consensus protocol\n");
}

module_init(sassy_consensus_init);
module_exit(sassy_consensus_exit);
