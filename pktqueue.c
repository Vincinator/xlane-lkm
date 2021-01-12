

#include <pthread.h>
#include <stdlib.h>
#include "list.h"
#include "replication.h"


void ring_aa_doorbell(struct asgard_async_queue_priv *aqueue)
{
    aqueue->doorbell++;
}

int enqueue_async_pkt(struct asgard_async_queue_priv *aqueue, struct asgard_async_pkt *apkt)
{
    if(!apkt || !aqueue)
        return -1;

    pthread_rwlock_wrlock(&aqueue->queue_rwlock);
    list_add_tail(&apkt->async_pkts_head, &aqueue->head_of_async_pkt_queue);
    pthread_rwlock_unlock(&aqueue->queue_rwlock);

    return 0;
}


/* Adds the pkt to the front of the queue */
int push_front_async_pkt(struct asgard_async_queue_priv *aqueue, struct asgard_async_pkt *apkt)
{
    pthread_rwlock_wrlock(&aqueue->queue_rwlock);
    list_add(&apkt->async_pkts_head, &aqueue->head_of_async_pkt_queue);
    pthread_rwlock_unlock(&aqueue->queue_rwlock);

    return 0;
}


/* Dequeue and return tail of async pkt queue */
struct asgard_async_pkt *dequeue_async_pkt(struct asgard_async_queue_priv *aqueue)
{
    struct asgard_async_pkt *queued_apkt;

    pthread_rwlock_wrlock(&aqueue->queue_rwlock);

    queued_apkt = list_first_entry_or_null(&aqueue->head_of_async_pkt_queue, struct asgard_async_pkt, async_pkts_head);

    if(queued_apkt != NULL) {
        list_del(&queued_apkt->async_pkts_head);
    }else {
        asgard_dbg("apkt is NULL. \n");
    }

    pthread_rwlock_unlock(&aqueue->queue_rwlock);

    return queued_apkt;
}



struct asgard_async_pkt *create_async_pkt(struct sockaddr_in target_addr)
{
    struct asgard_async_pkt *apkt = NULL;

    apkt = calloc(1, sizeof(struct asgard_async_pkt));
    apkt->pkt_data.payload = calloc(1, sizeof(struct asgard_payload));

    apkt->pkt_data.sockaddr.sin_port = target_addr.sin_port;
    apkt->pkt_data.sockaddr.sin_family = target_addr.sin_family;
    apkt->pkt_data.sockaddr.sin_addr = target_addr.sin_addr;

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

    pthread_rwlock_init(&(new_queue->queue_rwlock), NULL);

    INIT_LIST_HEAD(&(new_queue->head_of_async_pkt_queue));

    asgard_dbg("Async Queue initialized - updated version\n");
    asgard_dbg("size of asgard_payload struct=%ld\n", sizeof(struct asgard_payload));

    return 0;

}