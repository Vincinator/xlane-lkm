#pragma once

#ifndef ASGARD_KERNEL_MODULE

    #include <stdio.h>
    #include <stdlib.h>
    #include <stdint.h>
    #include <pthread.h>
    #define GFP_KERNEL 0
    #define GFP_ATOMIC 0
    #define GFP_KERNEL 0
    #define GFP_KERNEL_ACCOUNT 0
    #define GFP_NOWAIT  0
    #define GFP_NOIO    0
    #define GFP_NOFS    0
    #define GFP_USER	0
    #define GFP_DMA     0
    #define GFP_DMA32   0

#else

    #include <linux/types.h>
    #include <linux/slab.h>
#endif

typedef enum {
    ASG_RW_READ = 0,
    ASG_RW_WRITE = 1,

} asg_rw_locktype_t;



#ifdef ASGARD_KERNEL_MODULE

    #define AFREE(ptr)                              \
    ({                                              \
        if (1)                                      \
            kfree(ptr);                             \
    })
    #define AMALLOC(size, flags)                    \
    ({                                              \
            kmalloc(size, flags);                   \
    })

    #define ACMALLOC(num, size, flags)              \
    ({                                              \
            kcalloc(num, size, flags);              \
    })

#else

    #define AFREE(ptr)                              \
    ({                                              \
        if (1)                                      \
            free(ptr);                              \
    })

    #define AMALLOC(size, flags)                    \
    ({                                              \
            malloc(size);                           \
    })

    #define ACMALLOC(num, size, flags)             \
    ({                                             \
            calloc(num, size);                     \
    })

#endif


#ifdef ASGARD_DPDK

    typedef struct rte_ether_addr * asg_mac_ptr_t;

#else

    typedef char*  asg_mac_ptr_t;

#endif


#ifdef ASGARD_KERNEL_MODULE
#include <linux/mutex.h>

typedef rwlock_t asg_rwlock_t;
typedef spinlock_t asg_spinlock_t;
typedef struct mutex asg_mutex_t;

#else
typedef pthread_rwlock_t asg_rwlock_t;
typedef pthread_mutex_t asg_mutex_t;

#endif


void asg_rwlock_lock(asg_rwlock_t *lock, asg_rw_locktype_t type);
void asg_rwlock_unlock(asg_rwlock_t *lock, asg_rw_locktype_t type);
void asg_rwlock_init(asg_rwlock_t *lock);

void asg_mutex_init(asg_mutex_t *lock);
int asg_mutex_unlock(asg_mutex_t *mutex);
int asg_mutex_lock(asg_mutex_t *mutex);
