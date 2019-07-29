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

#define DEVNAME "sassy_fd_tx_mem"
#define MAX_NAME_SIZE 64
#define RX_OFFSET_BYTES 8

char *shared_mem_page;

int sassy_ulib_setup(int ifindex) {
        int fd;
        size_t pagesize = getpagesize();
        char name_buf[MAX_NAME_SIZE];
        struct stat s;
        int status;
        size_t size;

        printf(" System page size: %zu bytes\n", pagesize);

        snprintf(name_buf,  sizeof name_buf, "/dev/%s%d", DEVNAME, ifindex);

        fd = open(name_buf, O_RDWR);
        if (fd < 0) {
                printf(" failed to open %s\n", name_buf);
                return -1;
        }

        status = fstat(fd, &s);
        size = 512;
        printf(" status %d, size %d\n", status, pagesize);

        if(status < 0){
                printf(" fstat failed for file %s", name_buf);
                return -1;
        }

        shared_mem_page = mmap(0, pagesize, PROT_READ | PROT_WRITE , MAP_SHARED, fd, 0);
        if(shared_mem_page == MAP_FAILED) {
                printf(" mmap failed for %s", name_buf);
                return -1;
        }

        printf(" Page Mapped successfully.\n");
        return 0;
}

// Very unsecure to let the proc decide where to write - not for production! 
// TODO: memory protection mechanism - process can only write to its assigned memory
//			... however, this is just a prototype to show the mechanics. 
int syncbeat_update_status(int procid, uint64_t status) {
        if (!shared_mem_page){
                printf(" Update status failed, shared mem page not available\n");
                return -EFAULT;
        }

        shared_mem_page[procid] = status;
        return 0;
}


int running = 1;


int main(int argc, char **argv)
{
	int procid, devid;
    
    printf("Started Demo Application. \n");

    if(argc != 3){
    	printf("sudo ./fd_app <procid> <devid>\n");
    	return -1;
    }

    if (sscanf (argv[1], "%i", &procid) != 1) {
    	fprintf(stderr, "error - procid not an integer");
    	printf("sudo ./fd_app <procid> <devid>\n");
    	return -1;
	}
    if (sscanf (argv[2], "%i", &devid) != 1) {
    	fprintf(stderr, "error - devid not an integer");
    	printf("sudo ./fd_app <procid> <devid>\n");
    	return -1;
	}

    // Get aliveness counter;
    sassy_ulib_setup(devid);

    while(running < 1000){
	    
	    syncbeat_update_status(procid, running);
	    
	    running++;
    }
  
    printf("Stopped Demo Application. \n");
  
    return 0;
}
