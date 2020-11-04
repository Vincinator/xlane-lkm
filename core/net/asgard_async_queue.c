#include <asgard/asgard_async.h>
#include <linux/slab.h>
#include <asgard/asgard.h>
#include <linux/ip.h>
#include <linux/rwlock_types.h>



#ifdef LOG_PREFIX
#undef LOG_PREFIX
#endif

#define LOG_PREFIX "[ASGARD][ASYNC QUEUE]"

#define NIPQUAD(addr) \
    ((unsigned char *)&addr)[0], \
    ((unsigned char *)&addr)[1], \
    ((unsigned char *)&addr)[2], \
    ((unsigned char *)&addr)[3]





void async_clear_queue( struct asgard_async_queue_priv *queue)
{
    struct asgard_async_pkt *entry, *tmp_entry;

    if(!queue) {
        asgard_error("BUG - tried to clear uninitialized async queue\n");
        return;
    }

    if(queue->doorbell <= 0)
        return;

    write_lock(&(queue->queue_rwlock));

    if(list_empty(&(queue->head_of_async_pkt_queue)))
        goto unlock;

    list_for_each_entry_safe(entry, tmp_entry, &(queue->head_of_async_pkt_queue), async_pkts_head)
    {
        if(entry) {
            list_del(&(entry->async_pkts_head));
            kfree(entry);
            asgard_dbg("freed apkt entry\n");
        }
    }


unlock:
    write_unlock(&(queue->queue_rwlock));
}


int init_asgard_async_queue(struct asgard_async_queue_priv *new_queue)
{
    // freed in async_clear_queues (via clean_asgard_async_list_of_queues)

    if(!new_queue) {
        asgard_error("Could not allocate mem for async queue\n");
        return -1;
    }

    new_queue->doorbell = 0;

    rwlock_init(&(new_queue->queue_rwlock));

    INIT_LIST_HEAD(&(new_queue->head_of_async_pkt_queue));

    asgard_dbg("Async Queue initialized - updated version\n");
    asgard_dbg("ASGARD_PAYLOAD_BYTES=%d\n", ASGARD_PAYLOAD_BYTES);
    asgard_dbg("size of asgard_payload struct=%ld\n", sizeof(struct asgard_payload));

    return 0;

}
EXPORT_SYMBOL(init_asgard_async_queue);

int enqueue_async_pkt(struct asgard_async_queue_priv *aqueue, struct asgard_async_pkt *apkt)
{   
    if(!apkt || !aqueue)
        return -1;

    write_lock(&aqueue->queue_rwlock);

    list_add_tail(&apkt->async_pkts_head, &aqueue->head_of_async_pkt_queue);


    write_unlock(&aqueue->queue_rwlock);

    return 0;
}
EXPORT_SYMBOL(enqueue_async_pkt);

/* Adds the pkt to the front of the queue */
int push_front_async_pkt(struct asgard_async_queue_priv *aqueue, struct asgard_async_pkt *apkt)
{

    write_lock(&aqueue->queue_rwlock);
    list_add(&apkt->async_pkts_head, &aqueue->head_of_async_pkt_queue);
    write_unlock(&aqueue->queue_rwlock);

    return 0;

}
EXPORT_SYMBOL(push_front_async_pkt);

/* Dequeue and return tail of async pkt queue */
 struct asgard_async_pkt *dequeue_async_pkt(struct asgard_async_queue_priv *aqueue)
 {
    struct asgard_async_pkt *queued_apkt;

     write_lock(&aqueue->queue_rwlock);

     queued_apkt = list_first_entry_or_null(&aqueue->head_of_async_pkt_queue, struct asgard_async_pkt, async_pkts_head);

    if(queued_apkt != NULL) {
        list_del(&queued_apkt->async_pkts_head);
    }else {
        asgard_dbg("apkt is NULL. \n");
    }

    write_unlock(&aqueue->queue_rwlock);

    return queued_apkt;
}
EXPORT_SYMBOL(dequeue_async_pkt);


struct asgard_async_pkt *create_async_pkt(struct net_device *ndev, u32 dst_ip, unsigned char *dst_mac)
{
    struct asgard_async_pkt *apkt = NULL;
    // freed by _emit_async_pkts
    apkt = kzalloc(sizeof(struct asgard_async_pkt), GFP_KERNEL);

    apkt->skb = asgard_reserve_skb(ndev, dst_ip, dst_mac, NULL);
/*
    // freed by _emit_async_pkts
    apkt->payload = kzalloc(sizeof(struct asgard_payload), GFP_KERNEL);

  if(!apkt->payload) {
        asgard_error("Could not allocate SKB!\n");
        return NULL;
    }

    memset(apkt->payload, 0, sizeof(struct asgard_payload));
*/
    return apkt;
}
EXPORT_SYMBOL(create_async_pkt);

void ring_aa_doorbell(struct asgard_async_queue_priv *aqueue) 
{
    aqueue->doorbell++;
}
EXPORT_SYMBOL(ring_aa_doorbell);


void async_pkt_dump(struct asgard_async_pkt *apkt)
{
    struct iphdr *iph;

    if(!apkt){
        asgard_dbg("pkt is NULL\n");
        return;
    }


    if(!apkt->skb) {
        asgard_dbg("skb is NULL\n");
        return;
    }

    skb_get(apkt->skb);

    if (apkt->skb->protocol != htons(ETH_P_IP)){
        asgard_dbg("SKB is not IP protocol \n");
        return;
    }

    iph = ip_hdr(apkt->skb);

    if(!iph){
        asgard_dbg("IP Header is NULL \n");
        return;
    }

    asgard_dbg("FROM: %d.%d.%d.%d TO:%d.%d.%d.%d\n",
           NIPQUAD(iph->saddr), NIPQUAD(iph->daddr));


}
EXPORT_SYMBOL(async_pkt_dump);