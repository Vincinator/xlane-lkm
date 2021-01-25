



#ifndef ASGARD_KERNEL_MODULE
#include "list.h"
#include <pthread.h>
#include <stdlib.h>
#else
#include <linux/slab.h>

#include "../lkm/asgard-net.h"


#endif
#include "replication.h"


void ring_aa_doorbell(struct asgard_async_queue_priv *aqueue)
{
    aqueue->doorbell++;
}

int enqueue_async_pkt(struct asgard_async_queue_priv *aqueue, struct asgard_async_pkt *apkt)
{
    if(!apkt || !aqueue)
        return -1;

    asg_rwlock_lock(&aqueue->queue_rwlock, ASG_RW_WRITE);
    list_add_tail(&apkt->async_pkts_head, &aqueue->head_of_async_pkt_queue);
    asg_rwlock_unlock(&aqueue->queue_rwlock, ASG_RW_WRITE);

    return 0;
}


/* Adds the pkt to the front of the queue */
int push_front_async_pkt(struct asgard_async_queue_priv *aqueue, struct asgard_async_pkt *apkt)
{
    asg_rwlock_lock(&aqueue->queue_rwlock, ASG_RW_WRITE);
    list_add(&apkt->async_pkts_head, &aqueue->head_of_async_pkt_queue);
    asg_rwlock_unlock(&aqueue->queue_rwlock, ASG_RW_WRITE);

    return 0;
}


/* Dequeue and return tail of async pkt queue */
struct asgard_async_pkt *dequeue_async_pkt(struct asgard_async_queue_priv *aqueue)
{
    struct asgard_async_pkt *queued_apkt;

    asg_rwlock_lock(&aqueue->queue_rwlock, ASG_RW_WRITE);

    queued_apkt = list_first_entry_or_null(&aqueue->head_of_async_pkt_queue, struct asgard_async_pkt, async_pkts_head);

    if(queued_apkt != NULL) {
        list_del(&queued_apkt->async_pkts_head);
    }else {
        asgard_dbg("apkt is NULL. \n");
    }

    asg_rwlock_unlock(&aqueue->queue_rwlock, ASG_RW_WRITE);

    return queued_apkt;
}



struct asgard_async_pkt *create_async_pkt(struct asgard_device *sdev, struct node_addr target_addr)
{
#ifndef ASGARD_KERNEL_MODULE
    struct asgard_async_pkt *apkt = NULL;

    apkt = calloc(1, sizeof(struct asgard_async_pkt));
    apkt->pkt_data.payload = calloc(1, sizeof(struct asgard_payload));

    apkt->pkt_data.naddr.port = target_addr.port;
    apkt->pkt_data.naddr.dst_ip = target_addr.dst_ip;

#else
    struct asgard_async_pkt *apkt = NULL;
    // freed by _emit_async_pkts
    apkt = kzalloc(sizeof(struct asgard_async_pkt), GFP_KERNEL);

    apkt->skb = asgard_reserve_skb(sdev->ndev, target_addr.dst_ip, target_addr.dst_mac, NULL);

#endif
    return apkt;
}


int init_asgard_async_queue(struct asgard_async_queue_priv *new_queue)
{
    // freed in async_clear_queues (via clean_asgard_async_list_of_queues)

    if(!new_queue) {
        asgard_error("Could not allocate mem for async queue\n");
        return -1;
    }

    new_queue->doorbell = 0;

    asg_rwlock_init(&(new_queue->queue_rwlock));

    INIT_LIST_HEAD(&(new_queue->head_of_async_pkt_queue));

    asgard_dbg("Async Queue initialized - updated version\n");
    asgard_dbg("size of asgard_payload struct=%ld\n", sizeof(struct asgard_payload));

    return 0;

}