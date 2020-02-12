#ifndef _ASGUARD_ASYNC_H_
#define _ASGUARD_ASYNC_H_

#include <linux/list.h>

#include <asguard/asguard.h>

struct asguard_async_head_of_queues_priv {

    /* List head for asguard_async_queue_list_items */
   struct list_head head_of_aa_queues;

};

struct asguard_async_queue_priv {

    /* parent list head: asguard_async_head_of_queues_priv*/
    struct list_head aa_queues_head;

    /* List head for asguard async pkt*/
    struct list_head head_of_async_pkt_queue;

    int doorbell;

};

struct asguard_async_pkt {

    /* parent list head: asguard_async_queue_priv*/
    struct list_head async_pkts_head;

    struct sk_buff *skb;

    /* Ptr to the payload in the skb */
    char *payload_ptr;

    int target_id;

};

/* Initializes the list of queues*/
int init_asguard_async_list_of_queues(struct asguard_async_head_of_queues_priv *aapriv);


/* Initializes an async asguard queue and registers it int the head_of_aa_queues list */
int init_asguard_async_queue(struct asguard_async_head_of_queues_priv *aapriv, struct asguard_async_queue_priv *new_queue);

/* Enqueues a pkt to the back of the async pkt queue */
int enqueue_async_pkt(struct asguard_async_queue_priv *aqueue, struct asguard_async_pkt *apkt);

/* Adds the pkt to the front of the queue */
int push_front_async_pkt(struct asguard_async_queue_priv *aqueue, struct asguard_async_pkt *apkt);

/* Dequeue and return tail of async pkt queue */
struct asguard_async_pkt * dequeue_async_pkt(struct asguard_async_queue_priv *aqueue);

struct asguard_async_pkt * create_async_pkt(struct net_device *ndev, struct node_addr *naddr);

void ring_aa_doorbell(struct asguard_async_queue_priv *aqueue);


#endif /* _ASGUARD_ASYNC_H_ */
