#include "bn_d.h"
#include <linux/types.h>


#define clz(t) __builtin_clz(t)
#define unlikely(x) __builtin_expect(!!(x), 0)

void bn_init(bn *a, unsigned int size)
{
    if (!a) {
        a = kzalloc(sizeof(bn), GFP_KERNEL);
        a->num = kzalloc(sizeof(uint32_t) * size, GFP_KERNEL);
        if (!a->num) {
            a->length = 0;
            a->max_length = 0;
            return;
        }
    } else {
        if (!a->num) {
            a->num = kzalloc(sizeof(uint32_t) * size, GFP_KERNEL);
            if (!a->num) {
                a->length = 0;
                a->max_length = 0;
                return;
            }
        }
    }
    a->max_length = size;
    for (int i = 0; i < a->max_length; i++)
        a->num[i] = 0;
    a->length = 1;
}

void bn_free(bn *a)
{
    if (a) {
        if (a->num)
            kfree(a->num);
        kfree(a);
    }
}


// nonnull 123
void bn_sub(bn *a, bn *b, bn *result)
{
    int max = a->length >= b->length ? a->length : b->length;
    uint32_t *buf = kzalloc(sizeof(uint32_t) * result->max_length, GFP_KERNEL);
    int i = 0;
    uint32_t carry = 0;
    while (i < max) {
        if (a->num[i] < b->num[i] + carry) {
            buf[i] = UINT_MAX - b->num[i] + a->num[i] + 1 - carry;
            carry = 1;
        } else {
            buf[i] = a->num[i] - b->num[i] - carry;
            carry = 0;
        }
        i++;
    }

    if (carry) {
        bn_sub(b, a, result);
        return;
    }

    result->length = result->max_length;
    for (i = result->max_length - 1; i >= 0; i--)
        result->num[i] = buf[i];
    for (i = result->length - 1; i > 0; i--)
        if (result->num[i] == 0)
            result->length--;
        else
            break;
    kfree(buf);
}


// nonnull 123
void bn_add(bn *a, bn *b, bn *result)
{
    unsigned int max = a->length >= b->length ? a->length : b->length;
    uint32_t *buf = kzalloc(sizeof(uint32_t) * result->max_length, GFP_KERNEL);
    if (!buf)
        return;

    int i = 0;
    uint32_t carry = 0;
    uint64_t sum;
    while (i < max) {
        sum = (uint64_t) a->num[i] + b->num[i] + carry;
        carry = sum >> 32;
        buf[i] = (uint32_t) sum;
        i++;
    }

    if (carry && i < result->max_length)
        buf[i++] = carry;

    result->length = i;
    for (i = 0; i < result->max_length; i++)
        result->num[i] = buf[i];
    kfree(buf);
}

// nonnull 123
void bn_grow(bn *a, const unsigned int n)
{
    if (!a)
        return;
    if (n == 0 || a->length + n > a->max_length)
        return;
    int i = a->max_length - n - 1;
    for (; i >= 0; i--)
        a->num[i + n] = a->num[i];
    for (i = 0; i < n; i++)
        a->num[i] = 0;
    a->length += n;
}

// nonnull 123
void bn_mul_single(bn *a, const uint32_t b, bn *result)
{
    uint32_t *buf = kzalloc(sizeof(uint32_t) * result->max_length, GFP_KERNEL);
    int i = 0;
    uint32_t carry = 0;
    uint64_t sum;
    while (i < a->length) {
        sum = (uint64_t) a->num[i] * (uint64_t) b + carry;
        carry = sum >> 32;
        buf[i] = (uint32_t) sum;
        i++;
    }
    if (carry && (i < result->max_length))
        buf[i++] = carry;

    for (int j = 0; j < result->max_length; j++)
        result->num[j] = buf[j];
    result->length = i;
    kfree(buf);
}

void bn_mul(bn *a, bn *b, bn *result)
{
    int i = 0;
    bn *sum = kzalloc(sizeof(bn), GFP_KERNEL);
    bn_init(sum, result->max_length);
    bn *tmp = kzalloc(sizeof(bn), GFP_KERNEL);
    bn_init(tmp, result->max_length);

    if (!sum || !tmp)
        return;

    while (i < b->length) {
        bn_mul_single(a, b->num[i], tmp);
        bn_grow(tmp, i);
        bn_add(sum, tmp, sum);
        i++;
    }

    for (int j = 0; j < result->max_length; j++)
        result->num[j] = sum->num[j];
    result->length = sum->length;
    bn_free(tmp);
    bn_free(sum);
}


void bn_ls(bn *a, unsigned n, bn *result)
{
    int i = result->max_length - 1;
    while (i > 0) {
        if (i > a->max_length)
            result->num[i] = 0;
        else if (i == a->max_length)
            result->num[i] = a->num[i - 1] >> (32 - n);
        else
            result->num[i] = (a->num[i] << n) | (a->num[i - 1] >> (32 - n));

        if (result->num[i] && result->length < i + 1)
            result->length = i + 1;
        i--;
    }
    result->num[0] = (a->num[0] << n);
    if (result->num[0] && result->length < 1)
        result->length = 1;
}

char *bn_tostr(bn *a)
{
    unsigned long long str_size = (a->max_length * 963296) / 100000 + 1;
    char *buf = kzalloc(sizeof(char) * str_size, GFP_KERNEL);
    buf[str_size - 1] = '\0';
    int i = 33 * a->max_length - 1;
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
    char *tmp = kmalloc((str_size - i) * sizeof(char), GFP_KERNEL);
    memcpy(tmp, buf + i, (str_size - i) * sizeof(char));
    kfree(buf);
    return tmp;
}
