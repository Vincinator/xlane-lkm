#include <stdio.h>
#include <stdint.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <ctype.h>
#include <stdint.h>
#include <unistd.h>

#include "../../src/core/membership.h"


#define DEVNAME "synbufclustermem"
#define MAX_NAME_SIZE 64
#define RX_OFFSET_BYTES 8


#define CYCLES_PER_1MS 2400000
#define CYCLES_PER_1US 2400




char *shared_mem_page;

const char *nstate_string(enum node_state state)
{
    switch (state) {
        case FOLLOWER:
            return "Follower";
        case CANDIDATE:
            return "Candidate";
        case LEADER:
            return "Leader";
        default:
            return "Unknown State ";
    }
}

int asgard_ulib_setup(int ifindex) {
        int fd;
        size_t pagesize = getpagesize();
        char name_buf[MAX_NAME_SIZE];
        struct stat s;
        int status;

        snprintf(name_buf,  sizeof(name_buf), "/dev/%s", DEVNAME);
        printf("[Setup] Trying to open %s\n", name_buf);

        fd = open(name_buf, O_RDWR);
        if (fd < 0) {
                printf("[Setup] failed to open %s\n", name_buf);
                return -1;
        }

        status = fstat(fd, &s);

        if (status < 0) {
                printf("[setup] fstat failed (%d) for device %s\n", status, name_buf);
                return -1;
        } else {
                printf("[setup] successfully opened device. Status %d, size %lu\n", status, pagesize);
        }

        shared_mem_page = (char*) mmap(0, pagesize, PROT_READ | PROT_WRITE , MAP_SHARED, fd, 0);
        if (shared_mem_page == MAP_FAILED) {
                printf("[setup] mmap failed for %s\n", name_buf);
                return -1;
        } else {
                printf("[setup] mmap success for %s\n", name_buf);
        }

        return 0;
}

// Very unsecure to let the proc decide where to write - not for production!
// NOTE: memory protection mechanism - process can only write to its assigned memory
//			... however, this is just a prototype to show the mechanics.
int asgard_update_status(int procid, uint64_t status) {

        if (!shared_mem_page) {
                printf(" Update status failed, shared mem page not available\n");
                return -EFAULT;
        }

        shared_mem_page[procid] = status;
        return 0;
}

// Returns number of lines printed
int print_cluster_info(){

        struct cluster_info *ci;
        int i;

        ci = (struct cluster_info *) shared_mem_page;
        int64_t us_convertion = CYCLES_PER_1US;
        int64_t ms_convertion = CYCLES_PER_1MS;

        printf("Cluster Self ID: %d\n", ci->cluster_self_id);
        printf("HBI: %lu\n", ci->hbi);
        printf("Node State: %s\n", nstate_string(ci->node_state));
        printf("Last update timestamp: %lu\n", ci->last_update_timestamp);
        printf("Overall cluster member: %d\n", ci->overall_cluster_member);
        printf("Active remote cluster member: %d\n", ci->active_cluster_member);
        printf("Dead remote cluster member: %d\n", ci->dead_cluster_member);
        printf("Cluster joins: %d\n", ci->cluster_joins);
        printf("Cluster dropouts: %d\n", ci->cluster_dropouts);
        printf("\nCluster member Info\n"); //note the double new line!
        for(i=0; i < ci->overall_cluster_member - 1; i++){
                printf("\t Cluster Node %d\n", ci->member_info[i].global_cluster_id);
                printf("\t state:  %d\n", ci->member_info[i].state);
                printf("\t HB Counter: %lu\n", ci->member_info[i].hb_metrics.hb_counter);
                printf("\t Delta to last HB: %ld\n", ci->member_info[i].hb_metrics.last_delta);
                printf("\t Latency\n");
                printf("\t\t Avg Latency: %ld ms\n", ci->member_info[i].hb_metrics.avg_latency / ms_convertion);
                printf("\t\t Max Latency: %ld ms\n", ci->member_info[i].hb_metrics.max_latency / ms_convertion);
                printf("\t\t Min Latency: %ld ms\n", ci->member_info[i].hb_metrics.min_latency / ms_convertion);
                printf("\t Jitter\n");
                printf("\t\t Avg Jitter: %ld us\n", ci->member_info[i].hb_metrics.avg_jitter / us_convertion);
                printf("\t\t Max Jitter: %ld us\n", ci->member_info[i].hb_metrics.max_jitter / us_convertion);
                printf("\t\t Min Jitter: %ld us\n", ci->member_info[i].hb_metrics.min_jitter / us_convertion);

        }
        // Move up X lines so we overwrite the printf output in the next loop 
        return (ci->overall_cluster_member - 1) * 12 + 11;
}



int running = 1;


int main(int argc, char **argv)
{
        int procid, devid, counter, lines;

        if (argc != 3) {
                printf("sudo %s <procid> <devid>\n", argv[0]);
                return -1;
        }

        if (sscanf (argv[1], "%i", &procid) != 1) {
                fprintf(stderr, "error - procid not an integer");
                printf("sudo %s <procid> <devid>\n", argv[0]);
                return -1;
        }
        if (sscanf (argv[2], "%i", &devid) != 1) {
                fprintf(stderr, "error - devid not an integer");
                printf("sudo %s <procid> <devid>\n", argv[0]);
                return -1;
	}

    // Get aliveness counter;
    asgard_ulib_setup(devid);

    printf("\n");
    printf("\n");

    while(running) {

        if (counter >= 255)
	    	counter = 0;

        //if(asgard_update_status(procid, counter))
        //     break;

        lines = print_cluster_info();
        printf("\033[%dA\r",lines ); 


        sleep(1);

        running++;
        counter++;
    }

    printf("Stopped %s \n", argv[0]);

    return 0;
}
