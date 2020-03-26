#include <stdio.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

void* synbuf_setup(const char* device_path) {
        int fd;
        size_t pagesize = getpagesize();
        struct stat s;
        int status;
        void *shared_mem_page;

        fd = open(device_path, O_RDWR);
        if (fd < 0) {
                printf(" failed to open %s; did you run as root?\n", device_path);
                return NULL;
        }

        status = fstat(fd, &s);
        //printf(" status %d, size %zu\n", status, pagesize);

        if (status < 0) {
                printf(" fstat failed for file %s", device_path);
                return NULL;
        }

        shared_mem_page = mmap(0, pagesize, PROT_READ | PROT_WRITE , MAP_SHARED, fd, 0);
        if (shared_mem_page == MAP_FAILED) {
                printf(" mmap failed for %s", device_path);
                return NULL;
        }

        return shared_mem_page;
}
