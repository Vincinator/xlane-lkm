#include <asguard/asguard_async.h>
#include <linux/slab.h>
#include <asguard/asguard.h>
#include <linux/ip.h>
#include <linux/rwlock_types.h>

#define LOG_PREFIX "[ASGUARD][ASYNC QUEUE]"

#define NIPQUAD(addr) \
    ((unsigned char *)&addr)[0], \
    ((unsigned char *)&addr)[1], \
    ((unsigned char *)&addr)[2], \
    ((unsigned char *)&addr)[3]


void async_clear_queue( struct asguard_async_queue_priv *queue)
{
    struct asguard_async_pkt *entry, *tmp_entry;

    if(!queue)
        return;

    write_lock(&queue->queue_rwlock);

    if(list_empty(&queue->head_of_async_pkt_queue))
        goto unlock;


    if(queue->doorbell <= 0)
        goto unlock;

    list_for_each_entry_safe(entry, tmp_entry, &queue->head_of_async_pkt_queue, async_pkts_head)
    {
        if(entry) {
            list_del(&entry->async_pkts_head);
            kfree(entry);
            asguard_dbg("freed apkt entry\n");
        }
    }


unlock:
    write_unlock(&queue->queue_rwlock);
}

void async_clear_queues(struct asguard_async_head_of_queues_priv *aapriv)
{
    struct asguard_async_queue_priv *entry, *tmp_entry;

    if(!aapriv)
        return;

    write_lock(&aapriv->top_list_rwlock);

    if(list_empty(&aapriv->head_of_aa_queues))
        goto unlock;

    list_for_each_entry_safe(entry, tmp_entry, &aapriv->head_of_aa_queues, aa_queues_head)
    {
        if(entry) {
            async_clear_queue(entry);
            list_del(&entry->aa_queues_head);
            kfree(entry);
        }
    }

unlock:
   write_unlock(&aapriv->top_list_rwlock);

}

int clean_asguard_async_list_of_queues(struct asguard_async_head_of_queues_priv *aapriv)
{

    if(!aapriv)
        return;

    async_clear_queues(aapriv);

    kfree(aapriv);

    asguard_dbg("Async Queues List cleaned\n");

}

int init_asguard_async_list_of_queues(struct asguard_async_head_of_queues_priv **aapriv)
{
    // freed by clean_asguard_async_list_of_queues
    *aapriv = kmalloc(sizeof(struct asguard_async_head_of_queues_priv), GFP_KERNEL);

    if(!*aapriv){
        asguard_error("Could not allocate mem for async queue\n");
        return -1;
    }

    rwlock_init(&(*aapriv)->top_list_rwlock);

    INIT_LIST_HEAD(&((*aapriv)->head_of_aa_queues));

    asguard_dbg("Async Queues List initialized\n");

    return 0;
}

int init_asguard_async_queue(struct asguard_async_head_of_queues_priv *aapriv, struct asguard_async_queue_priv **new_queue)
{
    // freed in async_clear_queues (via clean_asguard_async_list_of_queues)
    *new_queue = kmalloc(sizeof(struct asguard_async_head_of_queues_priv), GFP_KERNEL);

    if(!(*new_queue)) {
        asguard_error("Could not allocate mem for async queue\n");
        return -1;
    }

    (*new_queue)->doorbell = 0;

    rwlock_init(&(*new_queue)->queue_rwlock);

    INIT_LIST_HEAD(&(*new_queue)->head_of_async_pkt_queue);

    write_lock(&aapriv->top_list_rwlock);

    list_add_tail(&(*new_queue)->head_of_async_pkt_queue, &aapriv->head_of_aa_queues);

    write_unlock(&aapriv->top_list_rwlock);

    asguard_dbg("Async Queue initialized\n");
    asguard_dbg("ASGUARD_PAYLOAD_BYTES=%d\n", ASGUARD_PAYLOAD_BYTES);
    asguard_dbg("size of asguard_payload struct=%d\n", sizeof(struct asguard_payload));

    return 0;

}
EXPORT_SYMBOL(init_asguard_async_queue);

int enqueue_async_pkt(struct asguard_async_queue_priv *aqueue, struct asguard_async_pkt *apkt)
{   
    if(!apkt || !aqueue)
        return -1;

    write_lock(&aqueue->queue_rwlock);

    list_add_tail(&apkt->async_pkts_head, &aqueue->head_of_async_pkt_queue);

    write_unlock(&aqueue->queue_rwlock);


    asguard_dbg("Packet enqueued\n");

    return 0;
}
EXPORT_SYMBOL(enqueue_async_pkt);

/* Adds the pkt to the front of the queue */
int push_front_async_pkt(struct asguard_async_queue_priv *aqueue, struct asguard_async_pkt *apkt)
{

    write_lock(&aqueue->queue_rwlock);
    list_add(&apkt->async_pkts_head, &aqueue->head_of_async_pkt_queue);
    write_unlock(&aqueue->queue_rwlock);

    asguard_dbg("Packet pushed to front of queue\n");

    return 0;

}
EXPORT_SYMBOL(push_front_async_pkt);

/* Dequeue and return tail of async pkt queue */
 struct asguard_async_pkt * dequeue_async_pkt(struct asguard_async_queue_priv *aqueue)
 {
    struct asguard_async_pkt *queued_apkt;

     write_lock(&aqueue->queue_rwlock);

     queued_apkt = list_first_entry_or_null(&aqueue->head_of_async_pkt_queue, struct asguard_async_pkt, async_pkts_head);

    if(queued_apkt != NULL) {
        list_del(&queued_apkt->async_pkts_head);
        asguard_dbg("Packet dequeued\n");
    }else {
        asguard_dbg("apkt is NULL. \n");
    }

    write_unlock(&aqueue->queue_rwlock);

    return queued_apkt;
}
EXPORT_SYMBOL(dequeue_async_pkt);


struct asguard_async_pkt *create_async_pkt(struct net_device *ndev, u32 dst_ip, unsigned char dst_mac[6])
{
    struct asguard_async_pkt *apkt = NULL;
    // freed by _emit_async_pkts
    apkt = kmalloc(sizeof(struct asguard_async_pkt), GFP_KERNEL);

    //apkt->skb = asguard_reserve_skb(ndev, dst_ip, dst_mac, NULL);

    // freed by _emit_async_pkts
    apkt->payload = kmalloc(sizeof(struct asguard_payload), GFP_KERNEL);


    if(!apkt->payload) {
        asguard_error("Could not allocate SKB!\n");
    } else {
        asguard_dbg("Packet reserved\n");
    }

    memset(apkt->payload, 0, sizeof(struct asguard_payload));

    asguard_dbg("Created async packet! \n");

    return apkt;
}
EXPORT_SYMBOL(create_async_pkt);

void ring_aa_doorbell(struct asguard_async_queue_priv *aqueue) 
{
    aqueue->doorbell++;
    asguard_dbg("Doorbell rung\n");

}
EXPORT_SYMBOL(ring_aa_doorbell);


void async_pkt_dump(struct asguard_async_pkt *apkt)
{
    struct iphdr *iph;

    if(!apkt){
        asguard_dbg("pkt is NULL\n");
        return;
    }


    if(!apkt->skb) {
        asguard_dbg("skb is NULL\n");
        return;
    }

    skb_get(apkt->skb);

    if (apkt->skb->protocol != htons(ETH_P_IP)){
        asguard_dbg("SKB is not IP protocol \n");
        return;
    }

    iph = ip_hdr(apkt->skb);

    if(!iph){
        asguard_dbg("IP Header is NULL \n");
        return;
    }

    asguard_dbg("FROM: %d.%d.%d.%d TO:%d.%d.%d.%d\n",
           NIPQUAD(iph->saddr), NIPQUAD(iph->daddr));


}
EXPORT_SYMBOL(async_pkt_dump);