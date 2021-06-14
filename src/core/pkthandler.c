//
// Created by Riesop, Vincent on 10.12.20.
//


#ifdef ASGARD_KERNEL_MODULE

#else
#include <string.h>
#include <stdio.h>

#endif


#include "pkthandler.h"
#include "membership.h"



#ifndef ASGARD_KERNEL_MODULE

void DumpHex(const void* data, size_t size) {
    char ascii[17];
    size_t i, j;
    ascii[16] = '\0';

    asgard_dbg("Hex Dump of %zu bytes starting from address: %p\n",size, data);

    for (i = 0; i < size; ++i) {
        printf("%02X ", ((unsigned char*)data)[i]);
        if (((unsigned char*)data)[i] >= ' ' && ((unsigned char*)data)[i] <= '~') {
            ascii[i % 16] = ((unsigned char*)data)[i];
        } else {
            ascii[i % 16] = '.';
        }
        if ((i+1) % 8 == 0 || i+1 == size) {
            printf(" ");
            if ((i+1) % 16 == 0) {
                printf("|  %s \n", ascii);
            } else if (i+1 == size) {
                ascii[(i+1) % 16] = '\0';
                if ((i+1) % 16 <= 8) {
                    printf(" ");
                }
                for (j = (i+1) % 16; j < 16; ++j) {
                    printf("   ");
                }
                printf("|  %s \n", ascii);
            }
        }
    }
}

#endif

struct proto_instance *get_proto_instance(struct asgard_device *sdev, uint16_t proto_id)
{
    int idx;

    if (proto_id < 0 || proto_id > MAX_PROTO_INSTANCES){
        asgard_error("proto_id is invalid: %hu\n", proto_id);
        return NULL;
    }

    idx = sdev->instance_id_mapping[proto_id];

    if (idx < 0 || idx >= MAX_PROTO_INSTANCES){
        if(sdev->verbose >= 5)
            asgard_error("idx is invalid: %d\n", idx);

        return NULL;
    }

    return sdev->protos[idx];
}

void handle_sub_payloads(struct asgard_device *sdev, int remote_lid, int cluster_id, char *payload, int instances, uint32_t bcnt, uint64_t ots)
{
    uint16_t cur_proto_id;
    uint16_t cur_offset;
    struct proto_instance *cur_ins;
    int i;
    uint32_t cur_bcnt = bcnt;

    char *cur_payload_ptr = payload;

    if (instances > 4) {
        if(sdev->verbose >= 5)
            asgard_error("BUG!? - Received packet that claimed to include %d instances\n", instances);
        return;
    }

    for(i = 0; i < instances; i++) {

        if (cur_bcnt <= 0)
            return;

        cur_proto_id = GET_PROTO_TYPE_VAL(cur_payload_ptr);
        cur_offset = GET_PROTO_OFFSET_VAL(cur_payload_ptr);

        cur_ins = get_proto_instance(sdev, cur_proto_id);

        if (!cur_ins) {
            if(sdev->verbose >= 4)
                asgard_dbg("No instance for protocol id %d were found. instances=%d", cur_proto_id, instances);

        } else {
            //asgard_dbg("(i: %d, offset: %d, proto_id: %d, cur_ins id: %d, node id:  %d, instances total %d, ots: %lu )\n", i, cur_offset, cur_proto_id, cur_ins->instance_id, remote_lid, instances, ots);
            cur_ins->ctrl_ops.post_payload(cur_ins, remote_lid, cluster_id, payload, ots);
        }
        cur_bcnt = cur_bcnt - cur_offset;
        cur_payload_ptr = cur_payload_ptr + cur_offset;

    }

}


int extract_cluster_id_from_ad(const char *payload) {

    uint32_t ad_cluster_id;
    enum le_opcode opcode;

    // check if opcode is ADVERTISE
    opcode = GET_CON_PROTO_OPCODE_VAL(payload);

    if(opcode != ADVERTISE) {
        return -1;
    }

    ad_cluster_id = GET_CON_PROTO_PARAM1_VAL(payload);
    return ad_cluster_id;
}

uint32_t extract_cluster_ip_from_ad(const char *payload) {

    uint32_t ad_cluster_ip;
    enum le_opcode opcode;

    // check if opcode is ADVERTISE
    opcode = GET_CON_PROTO_OPCODE_VAL(payload);

    if(opcode != ADVERTISE) {
        return 0;
    }

    ad_cluster_ip = GET_CON_PROTO_PARAM2_VAL(payload);
    return ad_cluster_ip;
}


int compare_mac(const unsigned  char *m1, const unsigned char *m2)
{
    int i;

    for (i = 5; i >= 0; i--)
        if (m1[i] != m2[i])
            return -1;

    return 0;
}

void get_cluster_ids_by_ip(struct asgard_device *sdev, uint32_t remote_ip, int *lid, int *cid)
{
    int i;
    struct pminfo *spminfo = &sdev->pminfo;

    *lid = -1;
    *cid = -1;


    for (i = 0; i < spminfo->num_of_targets; i++) {
        if (spminfo->pm_targets[i].pkt_data.naddr.dst_ip == remote_ip) {
            *cid = spminfo->pm_targets[i].cluster_id;
            *lid = i;
            return;
        }
    }
    asgard_error("Did not find ids for remote_ip");
    asg_print_ip(remote_ip);
}


void get_cluster_ids_by_mac(struct asgard_device *sdev, unsigned char *remote_mac, int *lid, int *cid)
{
    int i;
    struct pminfo *spminfo = &sdev->pminfo;

    *lid = -1;
    *cid = -1;

    for (i = 0; i < spminfo->num_of_targets; i++) {
        if(!spminfo->pm_targets[i].pkt_data.naddr.dst_mac){
            asgard_error("Uninitialized destination MAC for cluster id: %d (total members=%d), i=%d\n", spminfo->pm_targets[i].cluster_id, spminfo->num_of_targets, i);
            continue;
        }
        if (compare_mac(spminfo->pm_targets[i].pkt_data.naddr.dst_mac, remote_mac) == 0) {
            *cid = spminfo->pm_targets[i].cluster_id;
            *lid = i;
            if(sdev->verbose >= 10)
            asgard_dbg("Received Packet from cluster id %d and local id %d\n", *cid, *lid);

            return;
        }
    }
    asgard_error("Received Packet with unidentified sender\n");
}


#ifdef ASGARD_KERNEL_MODULE
// Note: this function will not explicitly run on the same isolated cpu
//		.. for consecutive packets (even from the same host)
void pkt_process_handler(struct work_struct *w)
{
    struct asgard_pkt_work_data *aw = NULL;
    char *user_data;

    aw = container_of(w, struct asgard_pkt_work_data, work);

    if (aw->sdev->asgard_wq_lock) {
        asgard_dbg("drop handling of received packet - asgard is shutting down \n");
        goto exit;
    }

    user_data = ((char *)aw->payload) + aw->headroom + ETH_HLEN +
                sizeof(struct iphdr) + sizeof(struct udphdr);

    handle_sub_payloads(aw->sdev, aw->remote_lid, aw->rcluster_id,
                        GET_PROTO_START_SUBS_PTR(user_data),
                        aw->received_proto_instances, aw->cqe_bcnt, aw->ots);

    exit:

    kfree(aw->payload);

    if (aw)
        kfree(aw);
}
#else
void *pkt_process_handler(void *data)
{
    struct pkt_work_data *wd = (struct pkt_work_data*) data;
    //asgard_dbg("processing pkt\n");
    //asgard_dbg("\t  wd->remote_lid = %d\n",  wd->remote_lid);
    //asgard_dbg("\t  wd->rcluster_id = %d\n",  wd->rcluster_id);
    //asgard_dbg("\t  wd->received_proto_instances = %d\n",  wd->received_proto_instances);
    //asgard_dbg("\t  wd->bcnt = %d\n",  wd->bcnt);


    handle_sub_payloads(wd->sdev, wd->remote_lid, wd->rcluster_id,
                         GET_PROTO_START_SUBS_PTR(wd->payload),
                         wd->received_proto_instances, wd->bcnt, wd->ots);
    AFREE(wd->payload);
    AFREE(wd);

    return NULL;
}

#endif

void do_post_payload(struct asgard_device *sdev, int remote_lid, int rcluster_id, char *payload, uint32_t cqe_bcnt, uint64_t ots) {
    struct pminfo *spminfo = &sdev->pminfo;
    int cluster_id_ad;
    uint32_t cluster_ip_ad;
    char *cluster_mac_ad;
    struct pkt_work_data *wd;
    uint16_t received_proto_instances;

    if(sdev->verbose >= 10)
        asgard_dbg("Packet from remote_lid: %d \n", remote_lid);

    /* Remote IP is not registered as peer yet! */
    if (remote_lid == -1) {

        if (GET_CON_PROTO_OPCODE_VAL(payload) == ADVERTISE) {

            cluster_id_ad = extract_cluster_id_from_ad(GET_PROTO_START_SUBS_PTR(payload));

            if (peer_is_registered(&sdev->pminfo, cluster_id_ad)) {
                asgard_error("peer cluster id is already registered under a different ip!\n");
                return; /* ignore advertisement for already registered peer */
            }
            cluster_ip_ad = extract_cluster_ip_from_ad(GET_PROTO_START_SUBS_PTR(payload));

            asgard_dbg("\tID: %d", cluster_id_ad);
            asgard_dbg("\tIP: %pI4", (void *) &cluster_ip_ad);

            if (cluster_id_ad < 0 || cluster_ip_ad == 0 || !cluster_mac_ad) {
                asgard_error("included ip, id or mac is wrong \n");
                AFREE(payload);
                return;
            }
            // use num of targets as local id, since id will be the index of the array
            register_peer_by_ip(sdev, sdev->pminfo.num_of_targets, cluster_ip_ad, cluster_id_ad);
        } 
        return;
    }
    
    // Update aliveness state and timestamps
    spminfo->pm_targets[remote_lid].chb_ts = ASGARD_TIMESTAMP;
    spminfo->pm_targets[remote_lid].alive = 1;
    update_cluster_member(sdev->ci, remote_lid, 1);
    write_ingress_log(&sdev->ingress_logger, INGRESS_PACKET, ASGARD_TIMESTAMP, remote_lid);

    if (check_warmup_state(sdev, spminfo)) {
        AFREE(payload);
        return;
    }

    received_proto_instances = GET_PROTO_AMOUNT_VAL(payload);

    wd = AMALLOC(sizeof(struct pkt_work_data), GFP_KERNEL);
    wd->payload = (struct asgard_payload *) payload;
    wd->rcluster_id = rcluster_id;
    wd->sdev = sdev;
    wd->received_proto_instances = received_proto_instances;
    wd->remote_lid = remote_lid;
    wd->bcnt = cqe_bcnt;
    wd->ots = ots;

#ifdef ASGARD_KERNEL_MODULE
    if (sdev->asgard_wq_lock) {
        asgard_dbg("Asgard is shutting down, ignoring packet\n");
        kfree(wd);
        return;
    }

    INIT_WORK(&wd->work, pkt_process_handler);

    if (!queue_work(sdev->asgard_wq, &wd->work)) {
        asgard_dbg("Work item not put in query..");

        if (payload)
            kfree(payload);
        if (wd)
            kfree(wd);
    }
#else
    pthread_t pkt_handler_thread;
    pthread_create(&pkt_handler_thread, NULL, pkt_process_handler, wd);

#endif

}



#ifdef ASGARD_KERNEL_MODULE

void asgard_post_payload(int asgard_id, void *payload_in, uint16_t headroom, uint32_t cqe_bcnt, uint64_t ots){
    struct asgard_device *sdev = get_sdev(asgard_id);
    int remote_lid = -2, rcluster_id = -2;
    //uint64_t ts2, ts3;
    char *payload;
    char *remote_mac;
    char *user_data;


    if (unlikely(!sdev)) {
        asgard_error("sdev is NULL\n");
        return;
    }
    // freed by pkt_process_handler
    payload = kzalloc(cqe_bcnt, GFP_KERNEL);
    memcpy(payload, payload_in, cqe_bcnt);

    // asgard_write_timestamp(sdev, 1, RDTSC_ASGARD, asgard_id);

    remote_mac = ((char *)payload) + headroom + 6;
    user_data = ((char *)payload) + headroom + ETH_HLEN +
                sizeof(struct iphdr) + sizeof(struct udphdr);

    //ts2 = RDTSC_ASGARD;

    if (unlikely(sdev->pminfo.state != ASGARD_PM_EMITTING))
        return;


    get_cluster_ids_by_mac(sdev, remote_mac, &remote_lid, &rcluster_id);
    // asgard_dbg("remote_lid=%d, rcluster_id=%d\n", remote_lid, rcluster_id);

    do_post_payload(sdev, remote_lid, rcluster_id, payload, cqe_bcnt, ots);


}


#else


void asgard_post_payload(struct asgard_device *sdev, uint32_t remote_ip, void *payload_in, uint32_t payload_len, uint64_t ots)
{
    struct pminfo *spminfo = &sdev->pminfo;
    int remote_lid, rcluster_id, cluster_id_ad, i;
    uint16_t received_proto_instances;
    //uint64_t ts2, ts3;
    char *payload;
    uint32_t *dst_ip;
    uint32_t cluster_ip_ad;
    char *cluster_mac_ad;

    payload =AMALLOC(payload_len, GFP_KERNEL);
    memcpy(payload, payload_in, payload_len);
    // asgard_write_timestamp(sdev, 1, RDTSC_ASGARD, asgard_id);


    if (sdev->pminfo.state != ASGARD_PM_EMITTING) {
        asgard_error("pacemaker is not emitting!\n");
        AFREE(payload);
        return;
    }


    // TODO!!!
    get_cluster_ids_by_ip(sdev, htonl(remote_ip), &remote_lid, &rcluster_id);

    if(remote_lid == -1){
        asgard_error("Could not find local id\n");
    }



    spminfo->pm_targets[remote_lid].pkt_rx_counter++;


    do_post_payload(sdev, remote_lid, rcluster_id, payload, payload_len, ots);



}

#endif


