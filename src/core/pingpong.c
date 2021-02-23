
#include "pingpong.h"
#include "libasraft.h"
#include "payload.h"
#include "types.h"


#ifndef ASGARD_KERNEL_MODULE
#include <sys/stat.h>
#endif

#ifndef ASGARD_KERNEL_MODULE

uint64_t calculate_latencies(uint64_t ts1, uint64_t ts4, uint64_t y, uint64_t z){
    uint64_t res = ts4 - ts1 - (z-y);
    res = res/2;
    return res;
}

void dump_ping_pong_to_file(struct pingpong_priv *pPriv, const char *filename, int self_id){
    FILE *fp;
    int i, r;
    struct ping_round_trip *cur_rt, *last_rt;
    fp = fopen(filename, "a");
    uint64_t z,y;


    for(i = 0; i < pPriv->sdev->pminfo.num_of_targets; i++){
        fprintf(fp, "# latency from node %d to node %d\n", self_id, i);
        fprintf(fp, "# scheduled pings: %d, received pongs: %d\n", pPriv->targets_rt_meta[i].scheduled_pings, pPriv->targets_rt_meta[i].received_pongs);

        fprintf(fp, "# round, ts1_1, ts4_1, ts1_2, ts4_2, latency in ns, latency in ms\n");

        for(r = 1; r < pPriv->targets_rt_meta[i].received_pongs; r++){
            last_rt = &pPriv->targets_rt_meta[i].round_trip_data[r-1];
            cur_rt = &pPriv->targets_rt_meta[i].round_trip_data[r];


            // Delta between ping pong emissions
            y = cur_rt->ts1 - last_rt->ts1;

            // Delta between pong receptions
            z = cur_rt->ts4 - last_rt->ts4;

            fprintf(fp, "%d, %lu, %lu, %lu, %lu, %fms\n",
                    r,
                    last_rt->ts1, last_rt->ts4,
                    cur_rt->ts1, cur_rt->ts4,
                    // convert from nanoseconds to milliseconds
                    0.000001 * calculate_latencies(cur_rt->ts1, cur_rt->ts4, y, z));

        }

    }


    fclose(fp);
}


void dump_ping_pong_raw_timestamps(struct asgard_device *sdev, struct pingpong_priv *pPriv){
    int i;
    struct tm *timenow;
    time_t now = time(NULL);
    struct stat st = {0};
    char filename[80];
    char foldername[80];
    char timestring[40];

    timenow = gmtime(&now);

    strftime(timestring, sizeof(timestring), "%Y-%m-%d_%H:%M:%S", timenow);
    sprintf(foldername, "logs/%s", timestring);

    if (stat("logs", &st) == -1) {
        mkdir("logs", 0777);
    }

    if (stat(foldername, &st) == -1) {
        mkdir(foldername, 0777);
    }

    sprintf(filename, "%s/RAW_Ping_pong_latencies", foldername);
    asgard_dbg("Writing raw ping pong timestamps to %s\n",filename);
    dump_ping_pong_to_file( pPriv, filename, sdev->pminfo.cluster_id);


}
#endif


void set_pp_opcode(unsigned char *pkt, pp_opcode_t opco, uint64_t id, uint64_t t1, uint64_t t2) {
    uint16_t *opcode;
    uint64_t *id_pkt_ptr, *t1_pkt_ptr, *t2_pkt_ptr;

    opcode = GET_PP_PROTO_OPCODE_PTR(pkt);
    *opcode = (uint16_t) opco;

    id_pkt_ptr = GET_PP_N_PTR(pkt);
    *id_pkt_ptr = (uint64_t) id;

    t1_pkt_ptr = GET_PP_T1_PTR(pkt);
    *t1_pkt_ptr = (uint64_t) t1;

    t2_pkt_ptr = GET_PP_T2_PTR(pkt);
    *t2_pkt_ptr = (uint64_t) t2;

}


int setup_pp_msg(struct proto_instance *ins, struct pminfo *spminfo, uint16_t opcode,
                 int32_t target_id, uint64_t id, uint64_t t1, int32_t t2) {
    struct asgard_payload *pkt_payload;
    unsigned char *pkt_payload_sub;

    asg_mutex_lock(&spminfo->pm_targets[target_id].pkt_data.mlock);

    pkt_payload =
            spminfo->pm_targets[target_id].pkt_data.payload;

    pkt_payload_sub =
            asgard_reserve_proto(ins->instance_id, pkt_payload, ASGARD_PROTO_PP_PAYLOAD_SZ);

    if (!pkt_payload_sub) {
        asgard_error("Leader Election packet full!\n");
        goto unlock;
    }

    set_pp_opcode((unsigned char *) pkt_payload_sub, opcode, id, t1, t2);

unlock:
    asg_mutex_unlock(&spminfo->pm_targets[target_id].pkt_data.mlock);
    return 0;
}


char *pingpong_state_name(pingpong_state_t state) {
    switch (state) {
        case PP_UNINIT:
            return "UNINIT";
        case PP_READY:
            return "READY";
        case PP_RUNNING:
            return "RUNNING";
        case PP_STOPPED:
            return "STOPPED";
        case PP_FULL:
            return "FULL";
        default:
            return "UNKNOWN STATE";
    }
}

void pingpong_state_transition_to(struct pingpong_priv *priv, pingpong_state_t state) {
    asgard_dbg("Ping Pong state transition from %s to %s\n", pingpong_state_name(priv->state), pingpong_state_name(state));
    priv->state = state;
}


/* Initializes data and user space interfaces */
int pingpong_init(struct proto_instance *ins, int verbosity){
    struct pingpong_priv *priv = (struct pingpong_priv *)ins->proto_data;
    struct asgard_device *sdev = priv->sdev;
    int  i, j;

    priv->verbosity = verbosity;

    priv->targets_rt_meta = AMALLOC( CLUSTER_SIZE * sizeof(struct ping_pong_round_trip *), GFP_KERNEL);

    for(i = 0; i < CLUSTER_SIZE; i++){
        priv->targets_rt_meta[i].round_trip_data = AMALLOC(MAX_PING_PONG_ROUND_TRIPS * sizeof(struct ping_round_trip), GFP_KERNEL);
        priv->targets_rt_meta[i].received_pongs = 0;
        priv->targets_rt_meta[i].scheduled_pings = 0;
        if(verbosity)
            asgard_dbg("allocated local ping pong store for target %d\n",i );
    }


    pingpong_state_transition_to(priv, PP_READY);
    return 0;
}

int pingpong_init_payload(void *payload){

    asgard_error("NOT IMPLEMENTED %s\n", __FUNCTION__ );
    return 0;
}



int pingpong_start(struct proto_instance *ins){
    struct pingpong_priv *priv = (struct pingpong_priv *)ins->proto_data;

    pingpong_state_transition_to(priv, PP_RUNNING);
    return 0;
}

int pingpong_stop(struct proto_instance *ins){
    struct pingpong_priv *priv = (struct pingpong_priv *)ins->proto_data;

#ifndef ASGARD_KERNEL_MODULE
    dump_ping_pong_raw_timestamps(priv->sdev, priv);
#endif

    pingpong_state_transition_to(priv, PP_STOPPED);
    return 0;
}

int pingpong_us_update(struct proto_instance *ins, void *payload){

    asgard_error("NOT IMPLEMENTED %s\n", __FUNCTION__ );
    return 0;
}

/* free memory of app and remove user space interfaces */
int pingpong_clean(struct proto_instance *ins){
    struct pingpong_priv *priv = (struct pingpong_priv *)ins->proto_data;
    struct asgard_device *sdev = priv->sdev;
    int i;

    for(i = 0; i < sdev->pminfo.num_of_targets; i++){
        AFREE(priv->targets_rt_meta[i].round_trip_data);
    }
    AFREE(priv->targets_rt_meta);


    return 0;
}




void handle_pong(struct pingpong_priv *pPriv, int remote_lid, uint16_t round_id, uint16_t t1, uint64_t ots) {
    struct ping_pong_round_trip *cur_target_trip;

    if(remote_lid < 0 || remote_lid > pPriv->sdev->pminfo.num_of_targets){
        asgard_error("invalid target_lid. could not add ts1 to local store\n");
        return;
    }
    cur_target_trip = &pPriv->targets_rt_meta[remote_lid];

    if(!cur_target_trip){
        asgard_error("Could not access rountrip target store in %s\n", __FUNCTION__ );
        return;
    }

    if(round_id < 0 || round_id > MAX_PING_PONG_ROUND_TRIPS){
        asgard_error("Invalid ping pong id (%d)\n", round_id);
        return;
    }


    if(!&cur_target_trip->round_trip_data[round_id]) {
        asgard_error("could not access round trip local store with remote id: %d\n", remote_lid);
        return;
    }
    cur_target_trip->round_trip_data[round_id].ts4 = ots;
    cur_target_trip->round_trip_data[round_id].n = round_id;
    asgard_dbg("received pong %d\n", round_id);

    cur_target_trip->received_pongs++;

}

int pingpong_post_payload(struct proto_instance *ins, int remote_lid, int cluster_id, void *payload, uint64_t ots){
    uint16_t opcode, pp_id, pp_t1;
    struct pingpong_priv *priv = (struct pingpong_priv *)ins->proto_data;

    if(priv->state != PP_RUNNING)
        return 0;

#ifndef ASGARD_KERNEL_MODULE
   // DumpHex(payload, ASGARD_PROTO_PP_PAYLOAD_SZ);
#endif
    opcode = GET_PP_PROTO_OPCODE_VAL(payload);
    pp_id = GET_PP_N_VAL(payload);


    switch(opcode){
        case PING:
            pp_t1 = GET_PP_T1_VAL(payload);
            setup_pp_msg(ins, &priv->sdev->pminfo, PONG, remote_lid, pp_id, pp_t1, 0);
            break;
        case PONG:
            pp_t1 = GET_PP_T1_VAL(payload);

            handle_pong(priv, remote_lid, pp_id, pp_t1, ots);

            break;
        default:
            asgard_error("Received unknown opcode for Ping pong protocol %d!\n", opcode);
            break;
    }

    return 0;
}

int pingpong_post_ts(struct proto_instance *ins, unsigned char *remote_mac, uint64_t ts){

    asgard_error("NOT IMPLEMENTED %s\n", __FUNCTION__ );
    return 0;

}

/* Write statistics to debug console  */
int pingpong_info(struct proto_instance *ins){

    asgard_error("NOT IMPLEMENTED %s\n", __FUNCTION__ );
    return 0;
}




static const struct asgard_protocol_ctrl_ops pingpong_ops = {
        .init = pingpong_init,
        .start = pingpong_start,
        .stop = pingpong_stop,
        .clean = pingpong_clean,
        .info = pingpong_info,
        .post_payload = pingpong_post_payload,
        .post_ts = pingpong_post_ts,
        .init_payload = pingpong_init_payload,
        .us_update = pingpong_us_update,
};



struct proto_instance *get_pp_proto_instance(struct asgard_device *sdev)
{
    struct pingpong_priv *ppriv;
    struct proto_instance *ins;

    // freed by get_echo_proto_instance
    ins = AMALLOC(sizeof(struct proto_instance), GFP_KERNEL);

    if (!ins)
        goto error;

    ins->proto_type = ASGARD_PROTO_PP;
    ins->ctrl_ops = pingpong_ops;

    ins->logger.name = "pingpong";
    ins->logger.instance_id = ins->instance_id;
    ins->logger.ifindex = sdev->ifindex;

    ins->proto_data = AMALLOC(sizeof(struct pingpong_priv), GFP_KERNEL);

    ppriv = (struct pingpong_priv *)ins->proto_data;

    if (!ppriv)
        goto error;


    ppriv->state = PP_UNINIT;
    ppriv->sdev = sdev;
    ppriv->ins = ins;


    return ins;

error:
    asgard_dbg("Error in %s", __func__);
    return NULL;
}

/* scheduled_pings index must not be incremented before this function is called */
void add_ts1_to_local_store(struct pingpong_priv *pPriv, int target_lid, uint64_t ts1){
    struct ping_pong_round_trip *cur_target_trip;

    if(target_lid < 0 || target_lid > pPriv->sdev->pminfo.num_of_targets){
        asgard_error("invalid target_lid. could not add ts1 to local store\n");
        return;
    }

    cur_target_trip = &pPriv->targets_rt_meta[target_lid];

    if(!cur_target_trip){
        asgard_error("Could not access rountrip target store in %s\n", __FUNCTION__ );
        return;
    }

    if(cur_target_trip->scheduled_pings >= MAX_PING_PONG_ROUND_TRIPS){
        asgard_error("Maximum number of ping pong rounds reached\n");
        return;
    }

    cur_target_trip->round_trip_data[cur_target_trip->scheduled_pings].ts1 = ts1;

}



int setup_ping_msg(struct pingpong_priv *pPriv, struct asgard_payload *spay, int instance_id, int target_lid) {
    unsigned char *pkt_payload_sub;
    uint64_t ts1;
    struct ping_pong_round_trip *cur_target_trip;

    if(target_lid < 0 || target_lid > pPriv->sdev->pminfo.num_of_targets){
        asgard_error("invalid target_lid. could not add ts1 to local store\n");
        return -1;
    }

    cur_target_trip = &pPriv->targets_rt_meta[target_lid];

    if(!cur_target_trip){
        asgard_error("Could not access rountrip target store\n");
        return -1;
    }

    if (cur_target_trip->scheduled_pings >= MAX_PING_PONG_ROUND_TRIPS) {
        asgard_dbg("num of rounds exceeded maximum. \n");
        return 0;
    }

    pkt_payload_sub = asgard_reserve_proto(instance_id, spay, ASGARD_PROTO_PP_PAYLOAD_SZ);

    if (!pkt_payload_sub) {
        asgard_error("Could not reserve space in payload for ping\n");
        return -1;
    }

    ts1 = ASGARD_TIMESTAMP;
    set_pp_opcode((unsigned char *) pkt_payload_sub, PING, cur_target_trip->scheduled_pings, ts1, 0);

    add_ts1_to_local_store(pPriv, target_lid, ts1);
    asgard_dbg("scheduled ping %d\n", cur_target_trip->scheduled_pings);

    cur_target_trip->scheduled_pings++;



    return 0;
}


