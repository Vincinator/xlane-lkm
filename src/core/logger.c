#include "logger.h"
#include "libasraft.h"
#include "types.h"


#ifndef ASGARD_KERNEL_MODULE
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#endif

#define SEC2NANOSEC 1000000000L


#ifdef ASGARD_KERNEL_MODULE
#include <linux/kernel.h>
uint64_t asgts(){

    return rdtsc();
}
#else
uint64_t asgts(){

    struct timespec now;

    clock_gettime(CLOCK_MONOTONIC, &now);

    return SEC2NANOSEC * now.tv_sec + now.tv_nsec;
}
#endif

void asg_print_ip(unsigned int ip) {
    unsigned char bytes[4];
    bytes[0] = ip & 0xFF;
    bytes[1] = (ip >> 8) & 0xFF;
    bytes[2] = (ip >> 16) & 0xFF;
    bytes[3] = (ip >> 24) & 0xFF;
    asgard_dbg("%d.%d.%d.%d\n", bytes[3], bytes[2], bytes[1], bytes[0]);
}

void asg_print_mac(unsigned char *sa_data){
    asgard_dbg("MAC Address : %02x:%02x:%02x:%02x:%02x:%02x\n",
           (unsigned char) sa_data[0],
           (unsigned char) sa_data[1],
           (unsigned char) sa_data[2],
           (unsigned char) sa_data[3],
           (unsigned char) sa_data[4],
           (unsigned char) sa_data[5]);
}


void logger_state_transition_to(struct asgard_logger *slog, enum logger_state state)
{
    slog->state = state;
}

static int init_logger_ctrl(struct asgard_logger *slog)
{
    //asgard_dbg("NOT IMPLEMENTED!\n");
    return 0;
}

static int init_logger_out(struct asgard_logger *slog)
{
    // asgard_dbg("NOT IMPLEMENTED!\n");
    return 0;
}

int init_logger(struct asgard_logger *slog, uint16_t instance_id, int ifindex, char name[MAX_LOGGER_NAME], int accept_user_ts)
{
    int err = 0;
    int i;
    
    if (!slog) {
        err = -EINVAL;
        asgard_error("logger device is NULL %s\n", __func__);
        goto error;
    }

    slog->instance_id = instance_id;
    slog->ifindex = ifindex;

    slog->name = AMALLOC(MAX_LOGGER_NAME, 1);
    slog->accept_user_ts = accept_user_ts;
    strncpy(slog->name, name, MAX_LOGGER_NAME);

    // freed by clear_logger
    slog->events = AMALLOC(LOGGER_EVENT_LIMIT * sizeof(struct logger_event*), GFP_KERNEL);

    if (!slog->events) {
        err = -ENOMEM;
        asgard_error("Could not allocate memory for leader election logs items\n");
        goto error;
    }

    /* Pre Allocate */
    for (i = 0; i < LOGGER_EVENT_LIMIT; i++) {
        // freed by clear_logger()
         slog->events[i] = AMALLOC(sizeof(struct logger_event), GFP_KERNEL);
    }

    slog->current_entries = 0;

    init_logger_out(slog);

    init_logger_ctrl(slog);

    logger_state_transition_to(slog, LOGGER_READY);

error:
    return err;
}

void clear_logger(struct asgard_logger *slog)
{
   int i;

    if(slog->events){
        for(i = 0; i < CLUSTER_SIZE; i++)
            AFREE(slog->events[i]);
        AFREE(slog->events);
    }

}

int write_log(struct asgard_logger *slog, enum le_event_type type, uint64_t tcs)
{
    if(!slog) {
        asgard_error("Logger is Null\n");
        return 0;
    }

    if (slog->state != LOGGER_RUNNING)
        return 0;

    if (slog->current_entries > LOGGER_EVENT_LIMIT) {

        asgard_dbg("Logs are full! Stopped event logging. %s\n", __func__);
        // asgard_log_stop(slog);
        logger_state_transition_to(slog, LOGGER_LOG_FULL);
        return -ENOMEM;
    }

    slog->events[slog->current_entries]->timestamp_tcs = tcs;
    slog->events[slog->current_entries]->type = type;

    slog->current_entries += 1;
    return 0;
}

int write_ingress_log(struct asgard_ingress_logger *ailog, enum le_event_type type, uint64_t tcs, int node_id)
{
    struct asgard_logger *slog;

    if(!ailog) {
        asgard_error("Error in %s. ingress logger is null \n", __FUNCTION__);
        return -EINVAL;
    }

    if(node_id < 0 || node_id > CLUSTER_SIZE){
        asgard_dbg("invalid/uninitialized logger for node %d\n", node_id);
        return -EINVAL;
    }

    slog = &ailog->per_node_logger[node_id];

    if(!slog) {
        asgard_error("Error in %s. per node logger for node %d is null \n",  __FUNCTION__, node_id);
        return -EINVAL;
    }
    
    slog->events[slog->current_entries]->node_id = node_id;
    write_log(slog, type, tcs);

    return 0;
}

void calculate_deltas(struct asgard_logger *slog) {
    int i;
    uint64_t prev;

    // init deltas to 0
    for(i = 0; i < slog->current_entries; i++)
        slog->events[i]->delta = 0;

    for(i = 0; i < slog->current_entries; i++) {

        if(slog->events[i]->type != INGRESS_PACKET)
            continue;

        slog->events[i]->delta = slog->events[i]->timestamp_tcs - prev;
        prev = slog->events[i]->timestamp_tcs;
    }
}
#ifndef ASGARD_KERNEL_MODULE
void dump_log_to_file(struct asgard_logger *slog, const char *filename, int node_id, int hbi){
    FILE *fp;
    int i;

    fp = fopen(filename, "a");

    calculate_deltas(slog);
    fprintf(fp, "# stats for node %d\n", node_id);
    fprintf(fp, "# node_id, nanoseconds timestamp, delta to previous timestamp\n");
    asgard_dbg("Dumping %d entries", slog->current_entries);

    for(i = 0; i < slog->current_entries; i++)
        if( slog->events[i].node_id != -1 && slog->events[i].type == INGRESS_PACKET)
            fprintf(fp, "%d, %lu, %lu, %fms, var to exp hb of %f ms: %f ms\n",
                    slog->events[i].node_id,
                    slog->events[i].timestamp_tcs,
                    slog->events[i].delta,
                    0.000001 * slog->events[i].delta,
                    (hbi * 0.000001),
                    (hbi * 0.000001) - (0.000001 * slog->events[i].delta));

    fclose(fp);
}


void dump_ingress_logs_to_file(struct asgard_device *sdev)
{
    int i;
    struct tm *timenow;
    time_t now = time(NULL);
    struct stat st = {0};
    char filename[80];
    char foldername[80];
    char timestring[40];

    timenow = gmtime(&now);

    strftime(timestring, sizeof(timestring), "%Y-%m-%d_%H:%M:%S", timenow);
    sprintf(foldername, "logs/%s", timestring);

    if (stat("logs", &st) == -1) {
        mkdir("logs", 0777);
    }

    if (stat(foldername, &st) == -1) {
        mkdir(foldername, 0777);
    }

    for(i = 0; i < sdev->pminfo.num_of_targets; i++){
        sprintf(filename, "%s/RXTS_from_node_%d", foldername, sdev->pminfo.pm_targets[i].cluster_id);
        asgard_dbg("Writing ingress logs to %s\n",filename);
        dump_log_to_file( &sdev->ingress_logger.per_node_logger[i], filename, sdev->pminfo.pm_targets[i].cluster_id, sdev->pminfo.hbi);
    }


}

#endif

void clear_ingress_logger(struct asgard_ingress_logger *ailog){
    int i;

    if(ailog->per_node_logger) {
        for(i = 0; i < CLUSTER_SIZE; i++)
            AFREE(ailog->per_node_logger[i].events);

        AFREE(ailog->per_node_logger);
    }

}


int init_ingress_logger(struct asgard_ingress_logger *ailog, int instance_id)
{
    int i;

    if (!ailog) {
        asgard_error("logger device is NULL %s\n", __func__);
        return -EINVAL;
    }

    ailog->per_node_logger = AMALLOC(sizeof(struct asgard_logger) * CLUSTER_SIZE, GFP_KERNEL);

    if (!ailog->per_node_logger) {
        asgard_error("Could not allocate memory for logs\n");
        return -ENOMEM;
    }

    // Just init all loggers for all possible nodes.. TODO
    for(i = 0; i < CLUSTER_SIZE; i++){
        init_logger(&ailog->per_node_logger[i], instance_id, -1, "nodelogger", -1);
        logger_state_transition_to(&ailog->per_node_logger[i], LOGGER_RUNNING);
    }


    return 0;
}
