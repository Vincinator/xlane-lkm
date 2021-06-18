
#include "ringbuffer.h"

const char validation_key[] = { 0x39, 0x3e, 0x52, 0x00, 0x00, 0x00, 0x00 };
#define VALIDATION_KEY_SIZE ASG_CHUNK_SIZE - 1

struct asg_ring_buf * get_tx_buffer(struct consensus_priv *priv){

#if ASGARD_KERNEL_MODULE

    if(!priv || !priv->synbuf_tx || !priv->synbuf_tx->ubuf){
        asgard_error("Uninitialized private data when trying to access synbuf's ubuf\n");
        return NULL;
    }

    return (struct asg_ring_buf *)priv->synbuf_tx->ubuf;
#else
    if(!priv || !priv->txbuf ){
        asgard_error("Uninitialized private data when trying to access rx_buf\n");
        return NULL;
    }
    return (struct asg_ring_buf *)priv->txbuf;
#endif
}
struct asg_ring_buf * get_rx_buffer(struct consensus_priv *priv){

#if ASGARD_KERNEL_MODULE

    if(!priv || !priv->synbuf_rx || !priv->synbuf_rx->ubuf){
        asgard_error("Uninitialized private data when trying to access synbuf's ubuf\n");
        return NULL;
    }


    return (struct asg_ring_buf *)priv->synbuf_rx->ubuf;
#else
    if(!priv || !priv->rxbuf ){
        asgard_error("Uninitialized private data when trying to access rx_buf\n");
        return NULL;
    }

    return (struct asg_ring_buf *)priv->rxbuf;
#endif
}


char* get_start_of_obj_key(char *noc, int *size) {

    uint8_t *ptr;

    ptr = ((uint8_t *) noc) + 1;

    (*size) = sizeof(struct data_chunk) - sizeof(uint8_t);

    return ((char*) ptr);
}

int validate_header_key(char *noc) {
    int size;
    char *key_in_header;

    key_in_header = get_start_of_obj_key(noc, &size);

    return memcmp(key_in_header, validation_key, VALIDATION_KEY_SIZE) == 0;
}

/*
 * buf must be a pointer to a synbuf memory (that is already allocated)
 */
void setup_asg_ring_buf(struct asg_ring_buf *buf, int max_elements){

    buf->size = max_elements;

    buf->read_idx = 0;

    buf->write_idx = 0;
    buf->turn = 0;
}


/*
 * Appends a data chunk to the ring buffer
 */
int append_rb(struct asg_ring_buf *buf, struct data_chunk *data) {


    if(!buf){
        asgard_error("asg ring buffer is NULL\n");
        return -1;
    }

    // TODO: if Reader catches up with writer?
    /*if(buf->write_idx == buf->read_idx && buf->turn) {
        asgard_error("Stopping! Reader can't keep up with Writer");
        return -1;
    }*/

    if(buf->write_idx < 0 || buf->write_idx >= ASG_RING_BUF_SIZE_LIMIT){
        asgard_error("Write Index (%d) is invalid. ASG_RING_BUF_SIZE_LIMIT=%d \n", buf->write_idx, ASG_RING_BUF_SIZE_LIMIT);
        return -1;
    }

    memcpy(&buf->ring[buf->write_idx], data, sizeof(struct data_chunk));

    if(sdev->verbose >= 5) {
        asgard_dbg("write_idx: %d, read_idx: %d, turn: %d \n", buf->write_idx, buf->read_idx, buf->turn);
        print_hex_dump(KERN_DEBUG, "rx consensus hexdump (at write idx): ", DUMP_PREFIX_NONE, 16,1,
                 &buf->ring[buf->write_idx], sizeof(struct data_chunk), 0);
    }

    buf->write_idx++;

    /* index starts at 0! */
    if(buf->write_idx == buf->size) {
        asgard_error("ring write turn\n");
        buf->write_idx = 0;
        buf->turn = 1;
    }

    return 0;
}

uint8_t get_num_of_chunks(char *noc) {

    uint8_t *ptr;

    ptr = (uint8_t *) noc;

    return (*ptr);
}

int check_entry(struct asg_ring_buf *buf) {

    int num_of_chunks;


    if(is_rb_empty(buf)) {
        //log_info("Ring Buf is empty, nothing to read");
        return -1;
    }

    /* Check if read_idx points to a header */
    if(validate_header_key((char*) &buf->ring[buf->read_idx].data) == 0 ){
        asgard_error("first item is not a header! Read Idx = %d, write IDX = %d", buf->read_idx, buf->write_idx);
        //print_hex_dump(KERN_DEBUG, "This should be the header: ", DUMP_PREFIX_OFFSET, 64, 1,
        //               &buf->ring[buf->read_idx].data, sizeof(struct data_chunk), 0);
        return 1;
    }

    num_of_chunks = get_num_of_chunks((char*) &buf->ring[buf->read_idx].data);

    /* Check if enough Chunks are in the buffer */
    if(buf->turn) {

        if(num_of_chunks + buf->read_idx >= buf->write_idx + buf->size) {
            asgard_dbg("asgObj not complete in ringBuffer (TURN)!");
            asgard_dbg("\tNumber of Chunks: %d", num_of_chunks);
            asgard_dbg("\tBuffer Read Index: %d", buf->read_idx);
            asgard_dbg("\tBuffer Write Index: %d", buf->write_idx);
            return 1;
        }
    } else {
        if(num_of_chunks + buf->read_idx >= buf->write_idx) {
            asgard_dbg("asgObj not complete in ringBuffer!");
            asgard_dbg("\tNumber of Chunks: %d", num_of_chunks);
            asgard_dbg("\tBuffer Read Index: %d", buf->read_idx);
            asgard_dbg("\tBuffer Write Index: %d", buf->write_idx);
            //print_hex_dump(KERN_DEBUG, "Ring Buffer Header Dump: ", DUMP_PREFIX_OFFSET, 64, 1,
            //               &buf->ring[buf->read_idx].data, sizeof(struct data_chunk), 0);
            return 1;
        }
    }

    return 0;
}

int is_rb_empty(struct asg_ring_buf *buf)
{

    if(!buf){
        asgard_error("Ring Buffer is NULL! But was checked if it is empty \n");
        return 1;
    }

    return (buf->read_idx == buf->write_idx);
}

int read_rb(struct asg_ring_buf *buf, struct data_chunk *chunk_destination)
{

    if(is_rb_empty(buf)) {
        return -1;
    }

    if(!chunk_destination) {
        asgard_dbg("Chunk destination is NULL! Can not copy from rb to NULL.\n");
        return -1;
    }

    if(!&buf->ring[buf->read_idx]) {
        asgard_error("Memory invalid at advertised ring buffer slot!\n");
        return -1;
    }

    memcpy(chunk_destination, &buf->ring[buf->read_idx], sizeof(struct data_chunk));
    //print_hex_dump(KERN_DEBUG, "consensus request hexdump: ", DUMP_PREFIX_NONE, 16,1,
    //              &buf->ring[buf->read_idx], sizeof(struct data_chunk), 0);

    buf->read_idx++;

    /* index starts at 0! */
    if(buf->read_idx == buf->size) {
        asgard_error("ring read turn");
        buf->read_idx = 0;
        buf->turn = 0;
    }

    return 0;
}


