#include <sassy/logger.h>
#include <sassy/sassy.h>

#include "include/sassy_echo.h"
#include "include/sassy_echo_ops.h"

int echo_init(struct proto_instance *ins)
{
	struct sassy_echo_priv *epriv = 
			(struct sassy_echo_priv *)ins->proto_data;

	sassy_dbg("echo init");


    //init_logger(&ins->logger);

	return 0;
}

int echo_start(struct proto_instance *ins)
{
	sassy_dbg("echo start");
	return 0;
}

int echo_stop(struct proto_instance *ins)
{
	sassy_dbg("echo stop");
	return 0;
}

int echo_us_update(struct proto_instance *ins)
{
	sassy_dbg("echo us update");
	return 0;
}

int echo_clean(struct proto_instance *ins)
{
	struct sassy_echo_priv *epriv =
		(struct sassy_echo_priv *)ins->proto_data;

	sassy_dbg("echo clean");
	//clear_logger(epriv);

	return 0;
}

int echo_info(struct proto_instance *ins)
{
	sassy_dbg("echo info");
	return 0;
}

int echo_post_payload(struct proto_instance *ins, unsigned char *remote_mac,
		      void *payload)
{
	int remote_lid, rcluster_id;
	uint64_t tx_ts;
	enum echo_opcode opcode;

	struct sassy_echo_priv *epriv = 
			(struct sassy_echo_priv *)ins->proto_data;

	tx_ts = GET_ECHO_PAYLOAD(payload, tx_ts);
	get_cluster_ids(epriv->sdev, remote_mac, &remote_lid, &rcluster_id);
	
	switch(opcode){
		case SASSY_PING:
			tx_ts = GET_ECHO_PAYLOAD(payload, tx_ts);
			write_log(&ins->logger, LOG_ECHO_RX_PING, rdtsc());

			// reply back to sender
			setup_echo_msg(&epriv->sdev->pminfo, remote_lid, tx_ts, SASSY_PONG);
			break;
		case SASSY_PONG:
			tx_ts = GET_ECHO_PAYLOAD(payload, tx_ts);

			write_log(&ins->logger, LOG_ECHO_PINGPONG_LATENCY, rdtsc() - tx_ts);

			break;
		default:
			sassy_error("Unknown echo opcode!");

	}


	return 0;
}

int echo_init_payload(void *payload)
{

	return 0;
}

int echo_post_ts(struct proto_instance *ins, unsigned char *remote_mac,
		 uint64_t ts)
{
	struct sassy_echo_priv *epriv = 
		(struct sassy_echo_priv *)ins->proto_data;

	sassy_dbg("SRC MAC=%pM", remote_mac);
	sassy_dbg("echo post optimistical ts");
	return 0;
}

