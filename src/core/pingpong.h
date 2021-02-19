#pragma once

#include "types.h"


// protoid(uint16_t) + offset(uint16_t)
#define GET_PP_PROTO_OPCODE_VAL(p) (*(uint16_t *)((char *) p + 2 + 2))
#define GET_PP_PROTO_OPCODE_PTR(p) (uint16_t *)((char *) p + 2 + 2)

// protoid(uint16_t) + offset(uint16_t) + opcode(uint16_t)
#define GET_PP_N_VAL(p) (*(uint64_t *)((char *) p + 2 + 2 + 2))
#define GET_PP_N_PTR(p) (uint64_t *)((char *) p + 2 + 2 + 2)

// protoid(uint16_t) + offset(uint16_t) + opcode(uint16_t) + pp_n(uint64_t)
#define GET_PP_T1_VAL(p) (*(uint64_t *)((char *) p + 2 + 2 + 2 + 8))
#define GET_PP_T1_PTR(p) (uint64_t *)((char *) p + 2 + 2 + 2 + 8)

// protoid(uint16_t) + offset(uint16_t) + opcode(uint16_t) + pp_n(uint64_t) + pp_t1(uint64_t)
#define GET_PP_T2_VAL(p) (*(uint64_t *)((char *) p + 2 + 2 + 2 + 8 + 8))
#define GET_PP_T2_PTR(p) (uint64_t *)((char *) p + 2 + 2 + 2 + 8 + 8)



// 3* 2byte + 3* 8byte = 30bytes
// protoid, offset, opcode each 2byte
// pp_n, pp_t1, pp_t2 each 8 byte
#define ASGARD_PROTO_PP_PAYLOAD_SZ 30


#define MAX_PING_PONG_ROUND_TRIPS 256




typedef enum pingpong_state {

    PP_UNINIT = 0,
    PP_READY = 1,
    PP_RUNNING = 2,
    PP_STOPPED = 3,
    PP_FULL = 4,
} pingpong_state_t;

typedef enum pp_opcode {
    PING = 10,
    PONG = 11,
} pp_opcode_t;


struct ping_round_trip {

    /* starts at 0, increments by 1 for each ping */
    uint64_t n;

    /* Timestamp of ping emission */
    uint64_t ts1;

    /* Timestamp of pong reception */
    uint64_t ts4;

};


struct pingpong_priv {

    struct asgard_device *sdev;
    struct proto_instance *ins;

    pingpong_state_t state;

    /* Array of Buffers allocated in pingpong_init function of pingpong.c
     *
     * Once the buffer is full, the latency benchmark will stop.
     * It is up to the benchmark to start a new ping pong session. */
    struct ping_round_trip **round_trip_local_stores;




    int num_of_rounds;

};


struct proto_instance *get_pp_proto_instance(struct asgard_device *sdev);