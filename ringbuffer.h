#pragma once

#if ASGARD_KERNEL_MODULE == 0
#include <stdlib.h>
#endif

#include "types.h"

#ifndef ASG_CHUNK_SIZE
/* Use 64 Bits (8 Bytes) as default chunk size for asgard
 *
 * Must be longer than 8 bits (1byte)!
 */
#define ASG_CHUNK_SIZE 8
#endif

#ifndef ASG_RING_BUF_SIZE_LIMIT
/* Use 8 Bytes (64 Bits) as default chunk size for asgard */
#define ASG_RING_BUF_SIZE_LIMIT 100000000
#endif

struct data_chunk {
    char data[ASG_CHUNK_SIZE];
};


struct asg_ring_buf {
    int read_idx;
    int write_idx;

    /* if true, the write_idx turned over, but the read_idx didn't turn yet
     *
     * CAUTION: the read_idx does not block the write_idx from over turning the read_idx!
     *          This means, that if the reader cant catch up with the write speed,
     *          the ring buffer will be broken.
     *
     *          TODO: prevent by blocking writes? Large enough Buffer, that this is not likely?
     * */
    int turn;

    /* Size of this Ring Buffer*/
    int size;

    /* Pointer to the data of the ring buffer where each slot
     * has the size of sizeof(struct data_chunk)
     *
     * Data will be located directly after this struct
     */
    struct data_chunk ring[ASG_RING_BUF_SIZE_LIMIT];
};


int read_rb(struct asg_ring_buf *buf, struct data_chunk *chunk_destination);
int is_rb_empty(struct asg_ring_buf *buf);
int check_entry(struct asg_ring_buf *buf);
uint8_t get_num_of_chunks(char *noc);
int append_rb(struct asg_ring_buf *buf, struct data_chunk *data);
void setup_asg_ring_buf(struct asg_ring_buf *buf, int max_elements);
int validate_header_key(char *noc);
char* get_start_of_obj_key(char *noc, int *size);
