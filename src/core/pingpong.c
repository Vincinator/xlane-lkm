
#include "pingpong.h"
#include "libasraft.h"



/* Initializes data and user space interfaces */
int pingpong_init(struct proto_instance *ins){
    struct pingpong_priv *priv = (struct consensus_priv *)ins->proto_data;

    asgard_error("NOT IMPLEMENTED %s\n", __FUNCTION__ );
    return 0;
}

int pingpong_init_payload(void *payload){

    asgard_error("NOT IMPLEMENTED %s\n", __FUNCTION__ );
    return 0;
}



int pingpong_start(struct proto_instance *ins){


    asgard_error("NOT IMPLEMENTED %s\n", __FUNCTION__ );
    return 0;
}

int pingpong_stop(struct proto_instance *ins){

    asgard_error("NOT IMPLEMENTED %s\n", __FUNCTION__ );
    return 0;
}

int pingpong_us_update(struct proto_instance *ins, void *payload){

    asgard_error("NOT IMPLEMENTED %s\n", __FUNCTION__ );
    return 0;
}

/* free memory of app and remove user space interfaces */
int pingpong_clean(struct proto_instance *ins){



    asgard_error("NOT IMPLEMENTED %s\n", __FUNCTION__ );
    return 0;
}

int pingpong_post_payload(struct proto_instance *ins, int remote_lid, int cluster_id, void *payload){

    asgard_error("NOT IMPLEMENTED %s\n", __FUNCTION__ );
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
