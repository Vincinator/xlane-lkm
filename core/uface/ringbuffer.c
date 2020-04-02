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

    /* Pointer stores address to memory placed directly after the asg_ring_buf */
    buf->ring = (struct data_chunk*)  (buf + 1);

}



/*
 * Appends a data chunk to the ring buffer
 */
int append_rb(struct asg_ring_buf *buf, struct data_chunk *data) {

    int write_idx, size, turn;


    if(!buf){
        asguard_error("asg ring buffer is NULL");
        return -1;
    }

    // TODO: if Reader catches up with writer?
    if(buf->write_idx == buf->read_idx && buf->turn) {
        asguard_error("Stopping! Reader can't keep up with Writer");
        return -1;
    }

    get_user(write_idx, &buf->write_idx);


    if(!&buf->ring[write_idx]) {
        asguard_error("Memory at advertised ringbuffer slot is invalid\n");
        return -1;
    }

    copy_to_user(&buf->ring[write_idx], data, sizeof(struct data_chunk));

    write_idx++;

    /* index starts at 0! */
    if(write_idx == size) {
        asguard_error("ring write turn");
        write_idx = 0;
        turn = 1;
        put_user(turn, &buf->turn);
    }

    put_user(write_idx, &buf->write_idx);


    return 0;
}

int is_rb_empty(struct asg_ring_buf *buf) {
    return (buf->read_idx == buf->write_idx && !buf->turn);
}


int read_rb(struct asg_ring_buf *buf, struct data_chunk *chunk_destination) {

    int read_idx, size, turn;

    if(is_rb_empty(buf)) {
        return -1;
    }

    if(!chunk_destination) {
        asguard_dbg("Chunk destination is NULL! Can not copy from rb to NULL.\n");
        return -1;
    }

    /* Read Buffer Idx from user */
    get_user(read_idx, &buf->read_idx);

    if(!&buf->ring[read_idx]) {
        asguard_error("Memory invalid at advertised ring buffer slot!\n");
        return -1;
    }

    asguard_dbg("read_idx: %d  direct read_idx: %d\n", read_idx, buf->read_idx);

    // copy_to_user idx!

    copy_from_user(chunk_destination, &buf->ring[read_idx], sizeof(struct data_chunk));
    read_idx++;

    get_user(size, &buf->size);

    /* index starts at 0! */
    if(read_idx == size) {
        asguard_error("ring read turn");
        read_idx = 0;
        turn = 0;
        put_user(turn, &buf->turn);
    }

    put_user(read_idx, &buf->read_idx);

    return 0;
}


