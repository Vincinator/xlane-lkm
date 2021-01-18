#include "logger.h"
#include "libasraft.h"

#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define SEC2NANOSEC 1000000000L

uint64_t __asgts(){

    struct timespec now;

    clock_gettime(CLOCK_MONOTONIC, &now);

    return SEC2NANOSEC * now.tv_sec + now.tv_nsec;
}

void asg_print_ip(unsigned int ip) {
    unsigned char bytes[4];
    bytes[0] = ip & 0xFF;
    bytes[1] = (ip >> 8) & 0xFF;
    bytes[2] = (ip >> 16) & 0xFF;
    bytes[3] = (ip >> 24) & 0xFF;
    printf("%d.%d.%d.%d\n", bytes[3], bytes[2], bytes[1], bytes[0]);
}

void asg_print_mac(unsigned char *sa_data){
    printf("MAC Address : %02x:%02x:%02x:%02x:%02x:%02x\n",
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

    if (!slog) {
        err = -EINVAL;
        asgard_error("logger device is NULL %s\n", __func__);
        goto error;
    }

    slog->instance_id = instance_id;
    slog->ifindex = ifindex;

    slog->name = malloc(MAX_LOGGER_NAME);
    slog->accept_user_ts = accept_user_ts;
    strncpy(slog->name, name, MAX_LOGGER_NAME);

    // freed by clear_logger
    slog->events = malloc(LOGGER_EVENT_LIMIT * sizeof(struct logger_event));

    if (!slog->events) {
        err = -ENOMEM;
        asgard_error("Could not allocate memory for leader election logs items\n");
        goto error;
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
    if(slog->events)
        free(slog->events);
}

int write_log(struct asgard_logger *slog, int type, uint64_t tcs)
{
    if (slog->state != LOGGER_RUNNING)
        return 0;

    if (slog->current_entries > LOGGER_EVENT_LIMIT) {

        asgard_dbg("Logs are full! Stopped event logging. %s\n", __func__);
        // asgard_log_stop(slog);
        logger_state_transition_to(slog, LOGGER_LOG_FULL);
        return -ENOMEM;
    }

    slog->events[slog->current_entries].timestamp_tcs = tcs;
    slog->events[slog->current_entries].type = type;
    slog->events[slog->current_entries].node_id = -1; // invalidate node_id

    slog->current_entries += 1;

    return 0;
}


int write_ingress_log(struct asgard_ingress_logger *ailog, int type, uint64_t tcs, int node_id)
{

    struct asgard_logger *slog;

    if(node_id < 0 || node_id > MAX_NODE_ID){
        asgard_dbg("invalid/uninitialized logger for node %d\n", node_id);
        return -EINVAL;
    }

    slog = &ailog->per_node_logger[node_id];

    if (slog->current_entries > LOGGER_EVENT_LIMIT) {

        asgard_dbg("Logs are full! Stopped event logging. %s\n", __func__);
        // asgard_log_stop(slog);
        logger_state_transition_to(slog, LOGGER_LOG_FULL);
        return -ENOMEM;
    }

    slog->events[slog->current_entries].timestamp_tcs = tcs;
    slog->events[slog->current_entries].type = type;
    slog->events[slog->current_entries].node_id = node_id;

    slog->current_entries += 1;

    return 0;
}


void calculate_deltas(struct asgard_logger *slog) {
    int i, id ;
    uint64_t prev;

    // init deltas to 0
    for(i = 0; i < slog->current_entries; i++)
        slog->events[i].delta = 0;

    for(i = 0; i < slog->current_entries; i++) {

        if(slog->events[i].type != INGRESS_PACKET)
            continue;

        slog->events[i].delta = slog->events[i].timestamp_tcs - prev;
        prev = slog->events[i].timestamp_tcs;
    }
}

void dump_ingress_log(struct asgard_logger *slog, int node_id, uint64_t hb_ns) {
    FILE *fp;
    int i;
    struct tm *timenow;
    time_t now = time(NULL);
    char timestring[40];
    char filename[80];

    timenow = gmtime(&now);

    strftime(timestring, sizeof(timestring), "%Y-%m-%d_%H:%M:%S", timenow);

    sprintf(filename, "ingress_timestamp_%f_ms_hb_from_%d_at_%s", hb_ns * 0.000001, node_id, timestring);

    asgard_dbg("\n\nDumping logs to %s\n\n",filename);

    fp = fopen(filename, "a");

    calculate_deltas(slog);
    fprintf(fp, "# stats for node %d\n", node_id);
    fprintf(fp, "# node_id, nanoseconds timestamp, delta to previous timestamp in nanoseconds\n");
    asgard_dbg("\n\nDumping %d entries\n\n", slog->current_entries);

    for(i = 0; i < slog->current_entries; i++)
        if( slog->events[i].node_id != -1 && slog->events[i].type == INGRESS_PACKET)
            fprintf(fp, "%d, %lu, %lu, %fms, var to exp hb of %f ms: %f ms\n",
                    slog->events[i].node_id,
                    slog->events[i].timestamp_tcs,
                    slog->events[i].delta,
                    0.000001 * slog->events[i].delta,
                    (hb_ns * 0.000001),
                    (hb_ns * 0.000001) - (0.000001 * slog->events[i].delta));

    fclose(fp);
}


void clear_ingress_logger(struct asgard_ingress_logger *ailog){
    int i;

    for(i = 0; i < ailog->num_of_nodes; i++)
        clear_logger(&ailog->per_node_logger[i]);


    free(ailog->per_node_logger);

}


int init_ingress_logger(struct asgard_ingress_logger *ailog, int instance_id)
{
    int i;

    if (!ailog) {
        asgard_error("logger device is NULL %s\n", __func__);
        return -EINVAL;
    }

    ailog->per_node_logger = malloc(sizeof(struct asgard_logger) * MAX_NODE_ID);

    if (!ailog->per_node_logger) {
        asgard_error("Could not allocate memory for logs\n");
        return -ENOMEM;
    }

    // Just init all loggers for all possible nodes.. TODO
    for(i = 0; i < MAX_NODE_ID; i++)
        init_logger(&ailog->per_node_logger[i], instance_id, -1, "nodelogger", -1);

    return 0;
}
