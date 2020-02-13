#include <asguard/asguard_async.h>
#include <asguard/payload_helper.h>
#include <linux/slab.h>
#include <asguard/asguard.h>


int init_asguard_async_list_of_queues(struct asguard_async_head_of_queues_priv **aapriv)
{

    *aapriv = kmalloc(sizeof(struct asguard_async_head_of_queues_priv), GFP_KERNEL);

    if(!*aapriv){
        asguard_error("Could not allocate mem for async queue\n");
        return -1;
    }

    INIT_LIST_HEAD(&((*aapriv)->head_of_aa_queues));

    return 0;
}

int init_asguard_async_queue(struct asguard_async_head_of_queues_priv *aapriv, struct asguard_async_queue_priv **new_queue)
{

    *new_queue = kmalloc(sizeof(struct asguard_async_head_of_queues_priv), GFP_KERNEL);

    if(!(*new_queue)) {
        asguard_error("Could not allocate mem for async queue\n");
        return -1;
    }

    INIT_LIST_HEAD(&(*new_queue)->head_of_async_pkt_queue);

    list_add_tail(&(*new_queue)->head_of_async_pkt_queue, &aapriv->head_of_aa_queues);
    
    return 0;

}
EXPORT_SYMBOL(init_asguard_async_queue);

int enqueue_async_pkt(struct asguard_async_queue_priv *aqueue, struct asguard_async_pkt *apkt)
{   

    list_add_tail(&apkt->async_pkts_head, &aqueue->head_of_async_pkt_queue);

    return 0;
}
EXPORT_SYMBOL(enqueue_async_pkt);

/* Adds the pkt to the front of the queue */
int push_front_async_pkt(struct asguard_async_queue_priv *aqueue, struct asguard_async_pkt *apkt)
{

    list_add(&apkt->async_pkts_head, &aqueue->head_of_async_pkt_queue);

    return 0;

}
EXPORT_SYMBOL(push_front_async_pkt);

/* Dequeue and return tail of async pkt queue */
 struct asguard_async_pkt * dequeue_async_pkt(struct asguard_async_queue_priv *aqueue)
 {
    struct asguard_async_pkt *queued_apkt;

    queued_apkt = list_first_entry_or_null(&aqueue->head_of_async_pkt_queue, struct asguard_async_pkt, async_pkts_head);

    if(queued_apkt != NULL)
        list_del(&queued_apkt->async_pkts_head);

    return queued_apkt;
}
EXPORT_SYMBOL(dequeue_async_pkt);


struct asguard_async_pkt *create_async_pkt(struct net_device *ndev, u32 dst_ip, unsigned char dst_mac[6])
{
    struct asguard_async_pkt *apkt = NULL;

    apkt = kmalloc(sizeof(struct asguard_async_pkt), GFP_KERNEL);

    apkt->skb = reserve_skb(ndev, dst_ip, dst_mac, &apkt->payload_ptr);

    return apkt;
}
EXPORT_SYMBOL(create_async_pkt);

void ring_aa_doorbell(struct asguard_async_queue_priv *aqueue) 
{
    aqueue->doorbell++;
}
EXPORT_SYMBOL(ring_aa_doorbell);