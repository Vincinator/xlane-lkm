#pragma once

#if ASGARD_KERNEL_MODULE == 0

    #include <stdin>
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

#endif


#if ASGARD_KERNEL_MODULE

    #define AFREE(ptr)                              \
    ({                                              \
        if (1)                                      \
            kfree(ptr);                              \
    })
    #define AMALLOC(size, flags)                    \
    ({                                              \
            kmalloc(size, flags);                   \
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

#endif


#if ASGARD_DPDK

    typedef struct rte_ether_addr * asg_mac_ptr_t;

#else

    typedef char*  asg_mac_ptr_t;

#endif


#if ASGARD_KERNEL_MODULE
#include <linux/mutex.h>

typedef rwlock_t asg_rwlock_t;
typedef spinlock_t asg_spinlock_t;
typedef struct mutex asg_mutex_t;
#else
typedef pthread_rwlock_t asg_rwlock_t;
typedef pthread_mutex_t asg_mutex;

#endif