
#include "types.h"




#ifdef ASGARD_KERNEL_MODULE

void asg_mutex_lock(asg_mutex_t *mutex){
    mutex_lock(mutex);

}
void asg_mutex_unlock(asg_mutex_t *mutex){
    mutex_unlock(mutex);
}



void asg_rwlock_init(asg_rwlock_t *lock) {
    rwlock_init(lock);
}


void asg_rwlock_lock(asg_rwlock_t *lock, asg_rw_locktype_t type){
    if(type == ASG_RW_WRITE){
        write_lock(lock);
    }else if(type == ASG_RW_READ){
        read_lock(lock);
    }
}


void asg_rwlock_unlock(asg_rwlock_t *lock, asg_rw_locktype_t type){
    if(type == ASG_RW_WRITE){
        write_unlock(lock);
    }else if(type == ASG_RW_READ){
        read_unlock(lock);
    }
}


void asg_mutex_init(asg_mutex_t *lock){
    mutex_init(lock);
}


#else
#include <pthread.h>
void asg_mutex_lock(asg_mutex_t *mutex){
    pthread_mutex_lock(mutex);
}
void asg_mutex_unlock(asg_mutex_t *mutex){
    pthread_mutex_unlock(mutex);
}

void asg_rwlock_init(asg_rwlock_t *lock) {
    pthread_rwlock_init(lock, NULL);
}


void asg_rwlock_lock(asg_rwlock_t *lock, asg_rw_locktype_t type){
    if(type == ASG_RW_WRITE){
        pthread_rwlock_wrlock(lock);
    }else if(type == ASG_RW_READ){
        pthread_rwlock_rdlock(lock);
    }
}

void asg_rwlock_unlock(asg_rwlock_t *lock, asg_rw_locktype_t type){
        pthread_rwlock_unlock(lock);
}



void asg_mutex_init(asg_mutex_t *lock){
    pthread_mutex_init(lock, NULL);

}



#endif