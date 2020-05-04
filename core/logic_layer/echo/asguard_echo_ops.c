#include <asguard/logger.h>
#include <asguard/asguard.h>

#include "include/asguard_echo.h"
#include <asguard/payload_helper.h>


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
void setup_msg_multi_pong(struct proto_instance *ins, struct pminfo *spminfo,
        s32 sender_cluster_id, s32 receiver_cluster_id,
        uint64_t ts1, uint64_t ts2,  uint64_t ts3,
        enum echo_opcode opcode)
{

    struct asguard_payload *pkt_payload;
    char *pkt_payload_sub;

    spin_lock(&spminfo->multicast_pkt_data_oos.lock);

    pkt_payload =
            spminfo->multicast_pkt_data_oos.payload;

    pkt_payload_sub =
            asguard_reserve_proto(ins->instance_id, pkt_payload, ASGUARD_PROTO_CON_PAYLOAD_SZ);

    if (!pkt_payload_sub) {
        asguard_error("asgard packet full!\n");
        goto unlock;
    }

    set_echo_opcode((unsigned char *)pkt_payload_sub, opcode,
            sender_cluster_id, receiver_cluster_id, ts1, ts2, ts3);

    spminfo->multicast_pkt_data_oos_fire = 1;

unlock:
    spin_unlock(&spminfo->multicast_pkt_data_oos.lock);
}

void setup_msg_uni_pong(struct proto_instance *ins, struct pminfo *spminfo,
                int remote_lid, s32 sender_cluster_id, s32 receiver_cluster_id,
                uint64_t ts1, uint64_t ts2,  uint64_t ts3,
                enum echo_opcode opcode)
{
    struct asguard_payload *pkt_payload;
    char *pkt_payload_sub;

    spin_lock(&spminfo->multicast_pkt_data_oos.lock);

    pkt_payload =
            spminfo->pm_targets[remote_lid].pkt_data.payload;

    pkt_payload_sub =
            asguard_reserve_proto(ins->instance_id, pkt_payload, ASGUARD_PROTO_ECHO_PAYLOAD_SZ);

    if (!pkt_payload_sub) {
        asguard_error("asgard packet full!\n");
        goto unlock;
    }

    set_echo_opcode((unsigned char *)pkt_payload_sub, opcode,
                    sender_cluster_id, receiver_cluster_id, ts1, ts2, ts3);

    spminfo->pm_targets[remote_lid].fire = 1;

unlock:
    spin_unlock(&spminfo->multicast_pkt_data_oos.lock);

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

        setup_msg_multi_pong(ins, &epriv->sdev->pminfo,
                         sdev->pminfo.cluster_id, param1_sender_cluster_id,
                         ts1, ts2, ts3, ASGUARD_PONG_MULTI);

        break;
    case ASGUARD_PING_REQ_UNI:
        param1_sender_cluster_id = GET_ECHO_PROTO_SENDER_ID_VAL(payload);
        ts1 = GET_ECHO_PROTO_TS1_VAL(payload);

        write_log(&ins->logger, LOG_ECHO_RX_PING_UNI, RDTSC_ASGUARD);

        setup_msg_uni_pong(ins, &epriv->sdev->pminfo, remote_lid,
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
	asguard_dbg("echo post optimistical ts");
	return 0;
}

