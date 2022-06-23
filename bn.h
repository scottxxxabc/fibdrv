#include <linux/device.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/string.h>
#define MAX_LENGTH_BN 500


typedef struct bignum {
    uint32_t num[MAX_LENGTH_BN];
    unsigned int length;
} bn;

void bn_init(bn *a);
void bn_sub(bn *a, bn *b, bn *result);
void bn_add(bn *a, bn *b, bn *result);
void bn_grow(bn *a, const unsigned int n);
void bn_mul_single(bn *a, const uint32_t b, bn *result);
void bn_mul(bn *a, bn *b, bn *result);
char *bn_tostr(bn *a);
void bn_ls(bn *a, unsigned n, bn *result);
