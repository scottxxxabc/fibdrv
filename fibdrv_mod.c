#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/kdev_t.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/uaccess.h>

#include "bn.h"
#define USE_FD 1

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
static ktime_t kt;

static int fib_sequence(long long k, char __user *buf)
{
    /*FIXME: use clz/ctz and fast algorithms to speed up */

    bn *a = NULL, *b = NULL, *c = NULL;
    a = kmalloc(sizeof(bn), GFP_KERNEL);
    b = kmalloc(sizeof(bn), GFP_KERNEL);
    c = kmalloc(sizeof(bn), GFP_KERNEL);
    bn_init(a);
    bn_init(b);
    bn_init(c);
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

    kfree(a);
    kfree(b);
    kfree(c);
    kfree(str);
    return n;
}

static int fib_sequence_fd(long long k, char __user *buf)
{
    bn *a = kmalloc(sizeof(bn), GFP_KERNEL),
       *b = kmalloc(sizeof(bn), GFP_KERNEL);
    bn_init(a);
    bn_init(b);
    a->num[0] = 0;
    b->num[0] = 1;
    b->length = 1;

    int h = 64 - (__builtin_clzll(k));
    for (int mask = 1 << (h - 1); mask; mask >>= 1) {
        bn tmp1, tmp2, tmp3;
        bn_ls(b, 1, &tmp1);
        bn_sub(&tmp1, a, &tmp2);
        bn_mul(&tmp2, a, &tmp1);

        bn_mul(a, a, &tmp2);
        *a = tmp1;
        bn_mul(b, b, &tmp3);
        bn_add(&tmp2, &tmp3, &tmp1);
        *b = tmp1;
        if (mask & k) {
            bn_add(a, b, &tmp1);
            *a = *b;
            *b = tmp1;
        }
    }

    char *str = bn_tostr(a);
    int n = strlen(str);
    if (copy_to_user(buf, str, n + 1))
        return -EFAULT;
    kfree(a);
    kfree(b);
    kfree(str);
    return n;
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
    kt = ktime_get();
    if (USE_FD) {
        kt = ktime_get();
        result = fib_sequence_fd(*offset, buf);
        kt = ktime_sub(ktime_get(), kt);
    }

    else {
        kt = ktime_get();
        result = fib_sequence(*offset, buf);
        kt = ktime_sub(ktime_get(), kt);
    }

    return (ssize_t) result;
}

/* write operation is skipped */
static ssize_t fib_write(struct file *file,
                         const char *buf,
                         size_t size,
                         loff_t *offset)
{
    return ktime_to_ns(kt);
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