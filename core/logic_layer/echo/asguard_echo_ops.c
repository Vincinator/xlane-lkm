#include <asguard/logger.h>
#include <asguard/asguard.h>

#include "include/asguard_echo.h"
#include <asguard/payload_helper.h>
#include <linux/proc_fs.h>


int echo_init(struct proto_instance *ins)
{
    struct asguard_echo_priv *priv =
            (struct asguard_echo_priv *)ins->proto_data;

    char name_buf[MAX_ASGUARD_PROC_NAME];

    asguard_dbg("echo init");


    snprintf(name_buf, sizeof(name_buf), "asguard/%d/proto_instances/%d",
             priv->sdev->ifindex, ins->instance_id);

    proc_mkdir(name_buf, NULL);

    init_ping_ctrl_interfaces(priv);

    // requires "proto_instances/%d"
    init_logger(&ins->logger, ins->instance_id, priv->sdev->ifindex, "echo_log", 0);

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

    struct asguard_echo_priv *priv =
            (struct asguard_echo_priv *)ins->proto_data;
    char name_buf[MAX_ASGUARD_PROC_NAME];

    asguard_dbg("echo clean");

    remove_ping_ctrl_interfaces(priv);

    clear_logger(&ins->logger);

    snprintf(name_buf, sizeof(name_buf), "asguard/%d/proto_instances/%d",
             priv->sdev->ifindex, ins->instance_id);

    remove_proc_entry(name_buf, NULL);

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
	enum echo_opcode opcode;

	struct asguard_echo_priv *epriv =
			(struct asguard_echo_priv *)ins->proto_data;

    struct asguard_device *sdev = epriv->sdev;

    s32 param1_sender_cluster_id, param2_receiver_cluster_id;

    // Note ts2 and ts3 are not used in the current implementation
    s64 ts1 = 0, ts2 = 0, ts3 = 0, ts4 = 0;

    opcode = GET_ECHO_PROTO_OPCODE_VAL(payload);

	switch (opcode) {

    case ASGUARD_PING_REQ_MULTI:

        param1_sender_cluster_id = GET_ECHO_PROTO_SENDER_ID_VAL(payload);
        ts1 = GET_ECHO_PROTO_TS1_VAL(payload);

        write_log(&ins->logger, LOG_ECHO_RX_PING_MULTI, RDTSC_ASGUARD);

            setup_echo_msg_multi(ins, &epriv->sdev->pminfo,
                                 sdev->pminfo.cluster_id, param1_sender_cluster_id,
                                 ts1, ts2, ts3, ASGUARD_PONG_MULTI);

        break;
    case ASGUARD_PING_REQ_UNI:
        param1_sender_cluster_id = GET_ECHO_PROTO_SENDER_ID_VAL(payload);
        ts1 = GET_ECHO_PROTO_TS1_VAL(payload);

        write_log(&ins->logger, LOG_ECHO_RX_PING_UNI, RDTSC_ASGUARD);

            setup_echo_msg_uni(ins, &epriv->sdev->pminfo, remote_lid,
                               sdev->pminfo.cluster_id, param1_sender_cluster_id,
                               ts1, ts2, ts3, ASGUARD_PONG_UNI);
        break;
    case ASGUARD_PONG_MULTI:
        ts4 = RDTSC_ASGUARD;
        param2_receiver_cluster_id = GET_ECHO_PROTO_RECEIVER_ID_VAL(payload);

        ts1 = GET_ECHO_PROTO_TS1_VAL(payload);

        if(param2_receiver_cluster_id == sdev->pminfo.cluster_id) {
            write_log(&ins->logger, LOG_ECHO_MULTI_LATENCY_DELTA, ts1 - ts4);
            asguard_dbg("Received Multicast Pong. ts1=%lld ts4=%lld",ts1, ts4);
        }else {
            asguard_error("Received Multicast Pong for cluster id=%d, but my cluster id is %d\n",
                          param2_receiver_cluster_id, sdev->pminfo.cluster_id);
        }

        break;

	case ASGUARD_PONG_UNI:

        ts4 = RDTSC_ASGUARD;
        param2_receiver_cluster_id = GET_ECHO_PROTO_RECEIVER_ID_VAL(payload);
        ts1 = GET_ECHO_PROTO_TS1_VAL(payload);

        if(param2_receiver_cluster_id == sdev->pminfo.cluster_id) {
            write_log(&ins->logger, LOG_ECHO_UNI_LATENCY_DELTA, ts1 - ts4);
            asguard_dbg("Received unicast pong. ts1=%lld ts4=%lld",ts1, ts4);
        } else {
            asguard_error("Received Unicast for cluster id=%d, but my cluster id is %d\n",
                    param2_receiver_cluster_id, sdev->pminfo.cluster_id);
        }

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
	asguard_dbg("echo post optimistical ts %lld", RDTSC_ASGUARD);
	return 0;
}

