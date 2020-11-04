#ifndef _ASGARD_ASYNC_H_
#define _ASGARD_ASYNC_H_

#include <linux/list.h>
#include <linux/netdevice.h>
#include <linux/skbuff.h>

struct asgard_async_queue_priv {

    /* List head for asgard async pkt*/
    struct list_head head_of_async_pkt_queue;

    int doorbell;

    rwlock_t queue_rwlock;
};

struct asgard_async_pkt {

    /* parent list head: asgard_async_queue_priv*/
    struct list_head async_pkts_head;

    struct sk_buff *skb;
};


/* Initializes an async asgard queue and registers it int the head_of_aa_queues list */
int init_asgard_async_queue(struct asgard_async_queue_priv *new_queue);

/* Enqueues a pkt to the back of the async pkt queue */
int enqueue_async_pkt(struct asgard_async_queue_priv *aqueue, struct asgard_async_pkt *apkt);

/* Adds the pkt to the front of the queue */
int push_front_async_pkt(struct asgard_async_queue_priv *aqueue, struct asgard_async_pkt *apkt);

/* Dequeue and return tail of async pkt queue */
struct asgard_async_pkt * dequeue_async_pkt(struct asgard_async_queue_priv *aqueue);

struct asgard_async_pkt * create_async_pkt(struct net_device *ndev, u32 dst_ip, unsigned char *dst_mac);

void ring_aa_doorbell(struct asgard_async_queue_priv *aqueue);

void async_pkt_dump(struct asgard_async_pkt *apkt);

void async_clear_queue(struct asgard_async_queue_priv *aqueue);


#endif /* _ASGARD_ASYNC_H_ */
