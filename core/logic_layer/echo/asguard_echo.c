#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <asguard/logger.h>

#include <linux/random.h>
#include <linux/timer.h>

#include <asguard/asguard.h>
#include <linux/slab.h>

#include <asguard/payload_helper.h>

#include "include/asguard_echo.h"



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

