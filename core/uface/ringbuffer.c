#include <asguard/ringbuffer.h>
#include <asguard/asguard.h>
#include <linux/uaccess.h>

/*
 * buf must be a pointer to a synbuf memory (that is already allocated)
 */
void setup_asg_ring_buf(struct asg_ring_buf *buf, int max_elements){

    if(max_elements >= ASG_RING_BUF_SIZE_LIMIT){
        asguard_error("size (%d) exceeds the configured size limit for ring buffer of (%d)", max_elements, ASG_RING_BUF_SIZE_LIMIT);
    }

    buf->size = max_elements;

    buf->read_idx = 0;

    buf->write_idx = 0;
    buf->turn = 0;
}


struct data_chunk *get_entry_chunk_ptr(struct asg_ring_buf *buf, int offset) {

    char *chkptr = buf+1;

    return start + offset;
}


/*
 * Appends a data chunk to the ring buffer
 */
int append_rb(struct asg_ring_buf *buf, struct data_chunk *data) {

    if(!buf){
        asguard_error("asg ring buffer is NULL");
        return -1;
    }

    // TODO: if Reader catches up with writer?
    if(buf->write_idx == buf->read_idx && buf->turn) {
        asguard_error("Stopping! Reader can't keep up with Writer");
        return -1;
    }


    if(!&buf->ring[buf->write_idx]) {
        asguard_error("Memory at advertised ringbuffer slot is invalid\n");
        return -1;
    }

    memcpy(&buf->ring[buf->write_idx++], data, sizeof(struct data_chunk));


    /* index starts at 0! */
    if(buf->write_idx == buf->size) {
        asguard_error("ring write turn");
        buf->write_idx = 0;
        buf->turn = 1;
    }


    return 0;
}

int is_rb_empty(struct asg_ring_buf *buf) {
    return (buf->read_idx == buf->write_idx && !buf->turn);
}


int read_rb(struct asg_ring_buf *buf, struct data_chunk *chunk_destination) {

    if(is_rb_empty(buf)) {
        return -1;
    }

    if(!chunk_destination) {
        asguard_dbg("Chunk destination is NULL! Can not copy from rb to NULL.\n");
        return -1;
    }

    if(!&buf->ring[buf->read_idx]) {
        asguard_error("Memory invalid at advertised ring buffer slot!\n");
        return -1;
    }

    asguard_dbg("read_idx: %d", buf->read_idx);

    memcpy(chunk_destination, &buf->ring[buf->read_idx], sizeof(struct data_chunk));
    print_hex_dump(KERN_DEBUG, "first element in ring: ", DUMP_PREFIX_NONE, 16,1,
                   &buf->ring[buf->read_idx], sizeof(struct data_chunk), 0);


    /*
    memcpy(chunk_destination, &buf->ring[buf->read_idx++], sizeof(struct data_chunk));

    print_hex_dump(KERN_DEBUG, "read consensus request: ", DUMP_PREFIX_NONE, 16,1,
                   chunk_destination, sizeof(struct data_chunk), 0);
    print_hex_dump(KERN_DEBUG, "read consensus request: ", DUMP_PREFIX_NONE, 16,1,
                   &buf->ring[buf->read_idx-1], sizeof(struct data_chunk), 0);
    */

    /* index starts at 0! */
    if(buf->read_idx == buf->size) {
        asguard_error("ring read turn");
        buf->read_idx = 0;
        buf->turn = 0;
    }

    return 0;
}


