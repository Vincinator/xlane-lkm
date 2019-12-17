#include <asguard/logger.h>
#include <asguard/asguard.h>

#include "include/asguard_echo.h"
#include "include/asguard_echo_ops.h"

int echo_init(struct proto_instance *ins)
{
	asguard_dbg("echo init");
    //init_logger(&ins->logger);

	return 0;
}

int echo_start(struct proto_instance *ins)
{
	asguard_dbg("echo start");
	return 0;
}

int echo_stop(struct proto_instance *ins)
{
	asguard_dbg("echo stop");
	return 0;
}

int echo_us_update(struct proto_instance *ins)
{
	asguard_dbg("echo us update");
	return 0;
}

int echo_clean(struct proto_instance *ins)
{

	asguard_dbg("echo clean");
	//clear_logger(epriv);

	return 0;
}

int echo_info(struct proto_instance *ins)
{
	asguard_dbg("echo info");
	return 0;
}

int echo_post_payload(struct proto_instance *ins, int remote_lid, int rcluster_id,
		      void *payload)
{
	uint64_t tx_ts;
	enum echo_opcode opcode;

	struct asguard_echo_priv *epriv =
			(struct asguard_echo_priv *)ins->proto_data;

	tx_ts = GET_ECHO_PAYLOAD(payload, tx_ts);
	opcode = GET_ECHO_PAYLOAD(payload, opcode);

	switch (opcode) {
	case ASGUARD_PING:
		tx_ts = GET_ECHO_PAYLOAD(payload, tx_ts);
		write_log(&ins->logger, LOG_ECHO_RX_PING, RDTSC_ASGUARD);

		// reply back to sender
		setup_echo_msg(&epriv->sdev->pminfo, remote_lid, tx_ts, ASGUARD_PONG);
		break;
	case ASGUARD_PONG:
		tx_ts = GET_ECHO_PAYLOAD(payload, tx_ts);

		write_log(&ins->logger, LOG_ECHO_PINGPONG_LATENCY, RDTSC_ASGUARD - tx_ts);

		break;
	default:
		asguard_error("Unknown echo opcode!");

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

	asguard_dbg("SRC MAC=%pM", remote_mac);
	asguard_dbg("echo post optimistical ts");
	return 0;
}

