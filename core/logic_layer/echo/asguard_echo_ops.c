#include <asguard/logger.h>
#include <asguard/asguard.h>

#include "include/asguard_echo.h"
#include "include/asguard_echo_ops.h"
#include "payload_helper.h"

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


// TODO: continue here ..
void setup_msg_multi_pong(struct pminfo *spminfo,int remote_lid,
        s32 sender_cluster_id, s32 receiver_cluster_id,
        uint64_t send_ts, uint64_t received_ts, enum echo_opcode opcode) {

    struct asguard_payload *pkt_payload;
    char *pkt_payload_sub;


    spin_lock(&spminfo->pm_targets[remote_lid].pkt_data.pkt_lock);

    pkt_payload =
            spminfo->pm_targets[remote_lid].pkt_data.pkt_payload;

    pkt_payload_sub =
            asguard_reserve_proto(ins->instance_id, pkt_payload, ASGUARD_PROTO_CON_PAYLOAD_SZ);

    if (!pkt_payload_sub) {
        asguard_error("Sassy packet full!\n");
        goto unlock;
    }

    set_le_opcode((unsigned char *)pkt_payload_sub, opcode, param1, param2, param3, param4);

unlock:
    spin_unlock(&spminfo->pm_targets[target_id].pkt_data.pkt_lock);
    return;
}

int echo_post_payload(struct proto_instance *ins, int remote_lid, int rcluster_id,
                      void *payload)
{
	uint64_t ts;
	enum echo_opcode opcode;

	struct asguard_echo_priv *epriv =
			(struct asguard_echo_priv *)ins->proto_data;

    struct asguard_device *sdev = epriv->sdev;

    s32 param1_sender_cluster_id, param2_receiver_cluster_id;
    s64 param3_echo_ts;


    opcode = GET_ECHO_PROTO_OPCODE_VAL(payload);

	switch (opcode) {

    case ASGUARD_PING_REQ_MULTI:

        param1_sender_cluster_id = GET_ECHO_PROTO_PARAM1_VAL(payload);
        param2_receiver_cluster_id = GET_ECHO_PROTO_PARAM2_VAL(payload);
        param3_echo_ts = GET_ECHO_PROTO_PARAM3_VAL(payload);

        ts = RDTSC_ASGUARD;

        write_log(&ins->logger, LOG_ECHO_RX_PING_MULTI, ts);

        setup_msg_multi_pong(&epriv->sdev->pminfo, remote_lid,
                param1_sender_cluster_id, param2_receiver_cluster_id,
                param3_echo_ts, ts, ASGUARD_PONG_MULTI);

        break;
    case ASGUARD_PONG_MULTI:
        ts = RDTSC_ASGUARD;
        param2_receiver_cluster_id = GET_ECHO_PROTO_PARAM2_VAL(payload);
        param3_echo_ts = GET_ECHO_PROTO_PARAM3_VAL(payload);


        if(param2_receiver_cluster_id == sdev->pminfo.cluster_id) {
            write_log(&ins->logger, LOG_ECHO_MULTI_LATENCY_DELTA, ts - param3_echo_ts);
            asguard_dbg("Received Multicast Pong. Timestamp is: %lld", ts);
        }


        break;
	case ASGUARD_PING_REQ_UNI:
        param1_sender_cluster_id = GET_ECHO_PROTO_PARAM1_VAL(payload);
        param2_receiver_cluster_id = GET_ECHO_PROTO_PARAM2_VAL(payload);
        param3_echo_ts = GET_ECHO_PROTO_PARAM3_VAL(payload);

        ts = RDTSC_ASGUARD;

		write_log(&ins->logger, LOG_ECHO_RX_PING_UNI, ts);

        setup_msg_uni_pong(&epriv->sdev->pminfo, remote_lid,
                             param1_sender_cluster_id, param2_receiver_cluster_id,
                             param3_echo_ts, ts, ASGUARD_PONG_UNI);
		break;
	case ASGUARD_PONG_UNI:
        ts = RDTSC_ASGUARD;
        param2_receiver_cluster_id = GET_ECHO_PROTO_PARAM2_VAL(payload);
        param3_echo_ts = GET_ECHO_PROTO_PARAM3_VAL(payload);

        if(param2_receiver_cluster_id == sdev->pminfo.cluster_id) {
            write_log(&ins->logger, LOG_ECHO_UNI_LATENCY_DELTA, ts - param3_echo_ts);
            asguard_dbg("Received Unicast Pong. Timestamp is: %lld", ts);
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
	asguard_dbg("echo post optimistical ts");
	return 0;
}

