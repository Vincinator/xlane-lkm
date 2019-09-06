#include <sassy/logger.h>
#include <sassy/sassy.h>

#include "include/sassy_echo.h"
#include "include/sassy_echo_ops.h"

int echo_init(struct sassy_device *sdev)
{
	struct sassy_echo_priv *epriv = (struct sassy_echo_priv *)sdev->proto->priv;

	sassy_dbg("echo init");

    init_logger(&epriv->echo_logger);

	return 0;
}

int echo_start(struct sassy_device *sdev)
{
	sassy_dbg("echo start");
	return 0;
}

int echo_stop(struct sassy_device *sdev)
{
	sassy_dbg("echo stop");
	return 0;
}

int echo_us_update(struct sassy_device *sdev)
{
	sassy_dbg("echo us update");
	return 0;
}

int echo_clean(struct sassy_device *sdev)
{
	struct sassy_echo_priv *epriv = (struct sassy_echo_priv *)sdev->proto->priv;

	sassy_dbg("echo clean");
	clear_logger(&epriv->echo_logger);

	return 0;
}

int echo_info(struct sassy_device *sdev)
{
	sassy_dbg("echo info");
	return 0;
}

int echo_post_payload(struct sassy_device *sdev, unsigned char *remote_mac,
		      void *payload)
{
	int remote_lid, rcluster_id;
	uint64_t tx_ts;
	enum echo_opcode opcode;

	struct sassy_echo_priv *epriv = (struct sassy_echo_priv *)sdev->proto->priv;

	tx_ts = GET_ECHO_PAYLOAD(payload, tx_ts);
	get_cluster_ids(sdev, remote_mac, &remote_lid, &rcluster_id);
	
	switch(opcode){
		case SASSY_PING:
			tx_ts = GET_ECHO_PAYLOAD(payload, tx_ts);
			write_log(&epriv->echo_logger, LOG_ECHO_RX_PING, rdtsc());

			// reply back to sender
			setup_echo_msg(&sdev->pminfo, remote_lid, tx_ts, SASSY_PONG);
			break;
		case SASSY_PONG:
			tx_ts = GET_ECHO_PAYLOAD(payload, tx_ts);

			write_log(&epriv->echo_logger, LOG_ECHO_PINGPONG_LATENCY, rdtsc() - tx_ts);

			break;
		default:
			sassy_error("Unknown echo opcode!");

	}


	return 0;
}

int echo_init_payload(struct sassy_payload *payload)
{

	return 0;
}

int echo_post_ts(struct sassy_device *sdev, unsigned char *remote_mac,
		 uint64_t ts)
{
	sassy_dbg("SRC MAC=%pM", remote_mac);
	sassy_dbg("echo post optimistical ts");
	return 0;
}

