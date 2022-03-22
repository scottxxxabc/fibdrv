#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>

#define FIB_DEV "/dev/fibonacci"

int main()
{
    long long sz;

    char buf[10000];
    // char write_buf[] = "testing writing";
    int offset = 1000; /* TODO: try test something bigger than the limit */
    int fd = open(FIB_DEV, O_RDWR);
    if (fd < 0) {
        perror("Failed to open character device");
        exit(1);
    }
    FILE *fp = fopen("time/time", "w");
    if (!fp) {
        perror("Failed to open file");
        exit(1);
    }

    for (int i = 0; i <= offset; i++) {
        lseek(fd, i, SEEK_SET);
        sz = read(fd, buf, 1);
        printf("Reading from " FIB_DEV
               " at offset %d, returned the sequence "
               "%s.\n",
               i, buf);

        lseek(fd, 1, SEEK_SET);
        long long sz1 = write(fd, buf, 1);
        
        lseek(fd, 2, SEEK_SET);
        long long sz2 = write(fd, buf, 1);
        lseek(fd, 3, SEEK_SET);
        long long sz3 = write(fd, buf, 1);
        printf("%d %lld %lld %lld\n", i, sz1, sz2, sz3);
        fprintf(fp, "%d %lld %lld %lld\n", i, sz1, sz2, sz3);
    }



    close(fd);
    fclose(fp);
    return 0;
}
