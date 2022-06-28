#define _GNU_SOURCE
#include <fcntl.h>
#include <sched.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <unistd.h>


#define FIB_DEV "/dev/fibonacci"
#define MAX_LENGTH_BN 1000
typedef struct bignum {
    uint32_t num[MAX_LENGTH_BN];
    unsigned int length;
} bn;

void bn_init(bn *a)
{
    if (!a) {
        a = calloc(1, sizeof(bn));
        a->num[0] = 0;
        a->length = 1;
    } else {
        for (int i = 0; i < MAX_LENGTH_BN; i++)
            a->num[i] = 0;
        a->length = 1;
    }
}

char *bn_tostr_client(bn *a)
{
    char buf[8 * sizeof(uint32_t) * MAX_LENGTH_BN / 3 + 2] = {0};
    int str_size = 8 * sizeof(uint32_t) * MAX_LENGTH_BN / 3 + 2;
    buf[str_size - 1] = '\0';
    int i = 32 * MAX_LENGTH_BN - 1;
    // i/32 = i >> 5, i%32 = i & 0x1F
    while (i >= 0) {
        int carry;
        if (i >> 5 > a->length)
            carry = 0;
        else
            carry = (a->num[i >> 5] & ((unsigned int) 1 << (i & 0x1F))) >>
                    (i & 0x1F);

        for (int j = str_size - 2; j >= 0; j--) {
            buf[j] += buf[j] - '0' + carry;
            carry = (buf[j] > '9');
            if (carry)
                buf[j] -= 10;
        }
        i--;
    }
    i = 0;
    while (i < str_size - 2 && (buf[i] == '0' || buf[i] == '\0'))
        i++;
    char *tmp = malloc((str_size - i) * sizeof(char));
    memcpy(tmp, buf + i, (str_size - i) * sizeof(char));

    return tmp;
}
int main()
{
    size_t sz;

    char buf[10000];
    int offset = 1000;
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



    lseek(fd, 0, SEEK_SET);
    long long pid = write(fd, buf, 0);
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(0, &cpuset);
    if (sched_setaffinity(pid, sizeof(cpuset), &cpuset)) {
        exit(1);
    }

    if (mlockall(MCL_CURRENT | MCL_FUTURE) < 0) {
        perror("mlockall");
        return 1;
    }

    for (int i = 0; i <= offset; i++) {
        lseek(fd, i, SEEK_SET);
        sz = read(fd, buf, 1);
    }

    for (int i = 0; i <= offset; i++) {
        lseek(fd, i, SEEK_SET);
        sz = read(fd, buf, 1);

        bn a;
        bn_init(&a);

        memcpy(a.num, buf, sz);
        int j = MAX_LENGTH_BN;
        for (; a.num[j] == 0; j--)
            ;
        a.length = j + 1;
        char *str = bn_tostr_client(&a);

        printf("Reading from " FIB_DEV
               " at offset %d, returned the sequence "
               "%s.\n",
               i, str);
        free(str);



        lseek(fd, 2, SEEK_SET);
        long long sz2 = write(fd, buf, 0);
        lseek(fd, 3, SEEK_SET);
        long long sz3 = write(fd, buf, 0);
        fprintf(fp, "%d %lld %lld\n", i, sz2, sz3);
    }


    munlockall();
    close(fd);
    fclose(fp);
    return 0;
}
