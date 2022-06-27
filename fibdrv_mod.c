#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/kdev_t.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/uaccess.h>

#include "bn_d.h"
#define USE_FD 1

#define SWAP(a, b, type) \
    do {                 \
        type tmp = a;    \
        a = b;           \
        b = tmp;         \
    } while (0)

MODULE_LICENSE("Dual MIT/GPL");
MODULE_AUTHOR("National Cheng Kung University, Taiwan");
MODULE_DESCRIPTION("Fibonacci engine driver");
MODULE_VERSION("0.1");

#define DEV_FIBONACCI_NAME "fibonacci"

/* MAX_LENGTH is set to 92 because
 * ssize_t can't fit the number > 92
 */
#define MAX_LENGTH 10000

static dev_t fib_dev = 0;
static struct cdev *fib_cdev;
static struct class *fib_class;
static DEFINE_MUTEX(fib_mutex);
static ssize_t time1 = 0, time2 = 0, time3 = 0;

static long long fib_sequence_ori(long long k)
{
    /* FIXME: C99 variable-length array (VLA) is not allowed in Linux kernel. */
    long long f[120];

    f[0] = 0;
    f[1] = 1;

    for (int i = 2; i <= k; i++) {
        f[i] = f[i - 1] + f[i - 2];
    }

    return f[k];
}

static int my_copy_to_user(const bn *bignum, char __user *buf)
{
    int i = bignum->max_length - 1;
    for (; bignum->num[i] == 0; i--)
        ;
    int lzbyte = __builtin_clz(bignum->num[i]) >> 3;

    size_t size = sizeof(int32_t) * (i + 1) - lzbyte;
    copy_to_user(buf, bignum->num, size);

    return size;
}

static unsigned long long fib_size(long long n)
{
    if (n <= 47)
        return 1;
    long long tmp = (-3628013 + n * 2169507) / 100000000 + 1;
    return tmp;
}

static int fib_sequence(long long k, char __user *buf)
{
    /*FIXME: use clz/ctz and fast algorithms to speed up */
    unsigned int esz = fib_size(k);
    bn *a = NULL, *b = NULL, *c = NULL;
    a = kmalloc(sizeof(bn), GFP_KERNEL);
    b = kmalloc(sizeof(bn), GFP_KERNEL);
    c = kmalloc(sizeof(bn), GFP_KERNEL);
    bn_init(a, esz);
    bn_init(b, esz);
    bn_init(c, esz);
    b->num[0] = 1;

    for (int i = 2; i <= k; i++) {
        bn_add(a, b, c);

        bn *tmp = c;
        c = a;
        a = b;
        b = tmp;
    }

    char *str = bn_tostr(b);
    int n = strlen(str);
    if (copy_to_user(buf, str, strlen(str) + 1))
        return -EFAULT;
    kfree(str);

    // size_t sz = my_copy_to_user(a, buf);

    bn_free(a);
    bn_free(b);
    bn_free(c);

    return 1;
}


static int fib_sequence_fd(long long k, char __user *buf)
{
    unsigned int esz = fib_size(k + 1);

    bn *a = kmalloc(sizeof(bn), GFP_KERNEL),
       *b = kmalloc(sizeof(bn), GFP_KERNEL);
    bn_init(a, esz);
    bn_init(b, esz);
    a->num[0] = 0;
    b->num[0] = 1;
    b->length = 1;
    bn *tmp1 = kmalloc(sizeof(bn), GFP_KERNEL),
       *tmp2 = kmalloc(sizeof(bn), GFP_KERNEL),
       *tmp3 = kmalloc(sizeof(bn), GFP_KERNEL);
    bn_init(tmp1, esz);
    bn_init(tmp2, esz);
    bn_init(tmp3, esz);

    if (!a || !b || !tmp1 || !tmp2 || !tmp3)
        return 0;


    int h = 64 - (__builtin_clzll(k));
    for (int mask = 1 << (h - 1); mask; mask >>= 1) {
        bn_init(tmp1, esz);
        bn_init(tmp2, esz);
        bn_init(tmp3, esz);
        bn_ls(b, 1, tmp1);
        bn_sub(tmp1, a, tmp2);
        bn_mul(tmp2, a, tmp1);

        bn_mul(a, a, tmp2);
        SWAP(a, tmp1, bn *);
        bn_mul(b, b, tmp3);
        bn_add(tmp2, tmp3, tmp1);
        SWAP(b, tmp1, bn *);
        if (mask & k) {
            bn_add(a, b, tmp1);
            SWAP(a, b, bn *);
            SWAP(b, tmp1, bn *);
        }
    }

    char *str = bn_tostr(a);
    int sz = strlen(str);
    if (copy_to_user(buf, str, strlen(str) + 1))
        return -EFAULT;
    kfree(str);


    // size_t sz = my_copy_to_user(a, buf);

    bn_free(a);
    bn_free(b);
    bn_free(tmp1);
    bn_free(tmp2);
    bn_free(tmp3);

    return sz;
}

static int fib_open(struct inode *inode, struct file *file)
{
    if (!mutex_trylock(&fib_mutex)) {
        printk(KERN_ALERT "fibdrv is in use");
        return -EBUSY;
    }
    return 0;
}

static int fib_release(struct inode *inode, struct file *file)
{
    mutex_unlock(&fib_mutex);
    return 0;
}

/* calculate the fibonacci number at given offset */
static ssize_t fib_read(struct file *file,
                        char __user *buf,
                        size_t size,
                        loff_t *offset)
{
    int result;
    /*
    ktime_t kt1 = ktime_get();
    result = fib_sequence_ori(*offset);
    kt1 = ktime_sub(ktime_get(), kt1);

    ktime_t kt2 = ktime_get();
    result = fib_sequence(*offset, buf);
    kt2 = ktime_sub(ktime_get(), kt2);
    */
    ktime_t kt3 = ktime_get();
    result = fib_sequence_fd(*offset, buf);
    kt3 = ktime_sub(ktime_get(), kt3);

    // time1 = ktime_to_ns(kt1);
    // time2 = ktime_to_ns(kt2);
    // time3 = ktime_to_ns(kt3);
    return result;
}

/* write operation is skipped */
static ssize_t fib_write(struct file *file,
                         const char *buf,
                         size_t size,
                         loff_t *offset)
{
    switch (*offset) {
    case 1:
        return time1;
    case 2:
        return time2;
    case 3:
        return time3;
    default:
        return current->pid;
    }
}

static loff_t fib_device_lseek(struct file *file, loff_t offset, int orig)
{
    loff_t new_pos = 0;
    switch (orig) {
    case 0: /* SEEK_SET: */
        new_pos = offset;
        break;
    case 1: /* SEEK_CUR: */
        new_pos = file->f_pos + offset;
        break;
    case 2: /* SEEK_END: */
        new_pos = MAX_LENGTH - offset;
        break;
    }

    if (new_pos > MAX_LENGTH)
        new_pos = MAX_LENGTH;  // max case
    if (new_pos < 0)
        new_pos = 0;        // min case
    file->f_pos = new_pos;  // This is what we'll use now
    return new_pos;
}

const struct file_operations fib_fops = {
    .owner = THIS_MODULE,
    .read = fib_read,
    .write = fib_write,
    .open = fib_open,
    .release = fib_release,
    .llseek = fib_device_lseek,
};

static int __init init_fib_dev(void)
{
    int rc = 0;

    mutex_init(&fib_mutex);

    // Let's register the device
    // This will dynamically allocate the major number
    rc = alloc_chrdev_region(&fib_dev, 0, 1, DEV_FIBONACCI_NAME);

    if (rc < 0) {
        printk(KERN_ALERT
               "Failed to register the fibonacci char device. rc = %i",
               rc);
        return rc;
    }

    fib_cdev = cdev_alloc();
    if (fib_cdev == NULL) {
        printk(KERN_ALERT "Failed to alloc cdev");
        rc = -1;
        goto failed_cdev;
    }
    fib_cdev->ops = &fib_fops;
    rc = cdev_add(fib_cdev, fib_dev, 1);

    if (rc < 0) {
        printk(KERN_ALERT "Failed to add cdev");
        rc = -2;
        goto failed_cdev;
    }

    fib_class = class_create(THIS_MODULE, DEV_FIBONACCI_NAME);

    if (!fib_class) {
        printk(KERN_ALERT "Failed to create device class");
        rc = -3;
        goto failed_class_create;
    }

    if (!device_create(fib_class, NULL, fib_dev, NULL, DEV_FIBONACCI_NAME)) {
        printk(KERN_ALERT "Failed to create device");
        rc = -4;
        goto failed_device_create;
    }
    return rc;
failed_device_create:
    class_destroy(fib_class);
failed_class_create:
    cdev_del(fib_cdev);
failed_cdev:
    unregister_chrdev_region(fib_dev, 1);
    return rc;
}

static void __exit exit_fib_dev(void)
{
    mutex_destroy(&fib_mutex);
    device_destroy(fib_class, fib_dev);
    class_destroy(fib_class);
    cdev_del(fib_cdev);
    unregister_chrdev_region(fib_dev, 1);
}

module_init(init_fib_dev);
module_exit(exit_fib_dev);
