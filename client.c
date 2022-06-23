#define _GNU_SOURCE
#include <fcntl.h>
#include <sched.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>


#define FIB_DEV "/dev/fibonacci"
#define MAX_LENGTH_BN 500
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
    // char write_buf[] = "testing writing";
    int offset = 100; /* TODO: try test something bigger than the limit */
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

    for (int i = 0; i <= offset; i++) {
        lseek(fd, i, SEEK_SET);
        sz = read(fd, buf, 1);
        
        bn a;
        bn_init(&a);

        size_t j = 0;
        int idx = 0;
        for (; j < sz - (sz & 0x3); j += sizeof(int32_t))
            a.num[idx++] = *((int32_t *) (buf + j));

        for (int k = 0; k < (sz & 0x3); k++)
            a.num[idx] |= (*(buf + sz - (sz & 0x3))) << k * 8;

        
        a.length = idx + 1;
        char *str = bn_tostr_client(&a);

        printf("Reading from " FIB_DEV
               " at offset %d, returned the sequence "
               "%s.\n",
               i, str);
        free(str);

        /*
        printf("Reading from " FIB_DEV
               " at offset %d, returned the sequence "
               "%s.\n",
               i, buf);
        
        
        lseek(fd, 1, SEEK_SET);
        long long sz1 = write(fd, buf, 0);

        lseek(fd, 2, SEEK_SET);
        long long sz2 = write(fd, buf, 0);
        lseek(fd, 3, SEEK_SET);
        long long sz3 = write(fd, buf, 0);
        printf("%d %lld %lld %lld\n", i, sz1, sz2, sz3);
        fprintf(fp, "%d %lld %lld %lld\n", i, sz1, sz2, sz3);
        */
    }



    close(fd);
    fclose(fp);
    return 0;
}
