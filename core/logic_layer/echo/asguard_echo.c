#include <linux/module.h>
#include <linux/kernel.h>
#include <asguard/logger.h>
#include <linux/timer.h>
#include <asguard/asguard.h>
#include <asguard/payload_helper.h>
#include <asguard/asguard_echo.h>


// TODO: continue here ..
void setup_echo_msg_multi(struct proto_instance *ins, struct pminfo *spminfo,
                          s32 sender_cluster_id, s32 receiver_cluster_id,
                          uint64_t ts1, uint64_t ts2, uint64_t ts3,
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

void setup_echo_msg_uni(struct proto_instance *ins, struct pminfo *spminfo,
                        int remote_lid, s32 sender_cluster_id, s32 receiver_cluster_id,
                        uint64_t ts1, uint64_t ts2, uint64_t ts3,
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



void set_echo_opcode(unsigned char *pkt, enum echo_opcode opco,
        s32 sender_id, s32 receiver_id,
        s64 ts1, s64 ts2, s64 ts3)
{
    u16 *opcode;
    u32 *sender_id_pptr, *receiver_id_pptr;

    s64 *ts1_pptr, *ts2_pptr, *ts3_pptr;

    opcode = GET_ECHO_PROTO_OPCODE_PTR(pkt);
    *opcode = (u16) opco;

    sender_id_pptr = GET_ECHO_PROTO_SENDER_ID_PTR(pkt);
    *sender_id_pptr = (u32) sender_id;

    receiver_id_pptr = GET_ECHO_PROTO_RECEIVER_ID_PTR(pkt);
    *receiver_id_pptr = (s32) receiver_id;

    ts1_pptr = GET_ECHO_PROTO_TS1_PTR(pkt);
    *ts1_pptr = ts1;

    ts2_pptr = GET_ECHO_PROTO_TS2_PTR(pkt);
    *ts2_pptr = ts2;

    ts3_pptr = GET_ECHO_PROTO_TS3_PTR(pkt);
    *ts3_pptr = ts3;

}

void set_echo_tx_ts(unsigned char *pkt, uint64_t ts)
{
	SET_ECHO_PAYLOAD(pkt, tx_ts, ts);
}

