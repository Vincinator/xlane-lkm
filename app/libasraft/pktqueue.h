#pragma once

#include "replication.h"

int init_asgard_async_queue(struct asgard_async_queue_priv *new_queue);
void ring_aa_doorbell(struct asgard_async_queue_priv *aqueue);
int enqueue_async_pkt(struct asgard_async_queue_priv *aqueue, struct asgard_async_pkt *apkt);
int push_front_async_pkt(struct asgard_async_queue_priv *aqueue, struct asgard_async_pkt *apkt);
struct asgard_async_pkt *dequeue_async_pkt(struct asgard_async_queue_priv *aqueue);
struct asgard_async_pkt *create_async_pkt(struct sockaddr_in target_addr);