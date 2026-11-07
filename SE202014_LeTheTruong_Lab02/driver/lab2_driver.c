#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/mutex.h>
#include <linux/device.h>
#include <linux/init.h>
#include <linux/proc_fs.h>

#define DEVICE_NAME "lab2"
#define CLASS_NAME  "lab2_class"
#define BUFFER_SIZE 1024
#define DRIVER_MAJOR 240

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Embedded Linux Lab");
MODULE_DESCRIPTION("LAB-02: Simple Character Device Driver");
MODULE_VERSION("1.0");

static int major_number = DRIVER_MAJOR;
static char device_buffer[BUFFER_SIZE];
static int buffer_len;
static int open_count;

static DEFINE_MUTEX(lab2_mutex);

/* Sysfs: show buffer length */
static ssize_t buffer_len_show(struct device *dev,
                               struct device_attribute *attr,
                               char *buf)
{
    ssize_t ret;

    mutex_lock(&lab2_mutex);

    ret = scnprintf(buf, PAGE_SIZE, "%d\n", buffer_len);

    mutex_unlock(&lab2_mutex);

    return ret;
}

static DEVICE_ATTR_RO(buffer_len);

/* Sysfs: show device open count */
static ssize_t open_count_show(struct device *dev,
                               struct device_attribute *attr,
                               char *buf)
{
    ssize_t ret;

    mutex_lock(&lab2_mutex);

    ret = scnprintf(buf, PAGE_SIZE, "%d\n", open_count);

    mutex_unlock(&lab2_mutex);

    return ret;
}

static DEVICE_ATTR_RO(open_count);

/* Sysfs: show last written data */
static ssize_t last_data_show(struct device *dev,
                              struct device_attribute *attr,
                              char *buf)
{
    ssize_t ret;

    mutex_lock(&lab2_mutex);

    ret = scnprintf(buf, PAGE_SIZE, "%s\n", device_buffer);

    mutex_unlock(&lab2_mutex);

    return ret;
}

static DEVICE_ATTR_RO(last_data);

static struct class *lab2_class;
static struct device *lab2_device;

#define PROC_NAME "lab2_info"

static struct proc_dir_entry *lab2_proc_entry;

static ssize_t lab2_proc_read(struct file *file,
                              char __user *buf,
                              size_t count,
                              loff_t *offset)
{
    char info[256];
    int len;

    if (*offset > 0)
        return 0;

    mutex_lock(&lab2_mutex);

    len = snprintf(info, sizeof(info),
                   "LAB-02 Character Device Driver\n"
                   "Major number: %d\n"
                   "Buffer length: %d\n"
                   "Open count: %d\n"
                   "Device name: %s\n",
                   major_number,
                   buffer_len,
                   open_count,
                   DEVICE_NAME);

    mutex_unlock(&lab2_mutex);

    if (copy_to_user(buf, info, len))
        return -EFAULT;

    *offset = len;

    return len;
}

static const struct proc_ops lab2_proc_ops = {
    .proc_read = lab2_proc_read,
};

static int lab2_open(struct inode *inode, struct file *file)
{
    mutex_lock(&lab2_mutex);
    open_count++;
    mutex_unlock(&lab2_mutex);

    pr_info("lab2_driver: device opened (count=%d)\n",
            open_count);

    return 0;
}

static int lab2_release(struct inode *inode, struct file *file)
{
    pr_info("lab2_driver: device closed\n");
    return 0;
}

static ssize_t lab2_read(struct file *file,
                         char __user *buf,
                         size_t len,
                         loff_t *offset)
{
    ssize_t bytes_read;

    mutex_lock(&lab2_mutex);

    if (*offset >= buffer_len) {
        mutex_unlock(&lab2_mutex);
        return 0;
    }

    if (len > buffer_len - *offset)
        len = buffer_len - *offset;

    if (copy_to_user(buf, device_buffer + *offset, len)) {
        mutex_unlock(&lab2_mutex);
        pr_err("lab2_driver: copy_to_user failed\n");
        return -EFAULT;
    }

    *offset += len;
    bytes_read = len;

    mutex_unlock(&lab2_mutex);

    pr_info("lab2_driver: sent %zd bytes to user\n",
            bytes_read);

    return bytes_read;
}

static ssize_t lab2_write(struct file *file,
                          const char __user *buf,
                          size_t len,
                          loff_t *offset)
{
    if (len > BUFFER_SIZE - 1)
        len = BUFFER_SIZE - 1;

    mutex_lock(&lab2_mutex);

    if (copy_from_user(device_buffer, buf, len)) {
        mutex_unlock(&lab2_mutex);
        pr_err("lab2_driver: copy_from_user failed\n");
        return -EFAULT;
    }

    buffer_len = len;
    device_buffer[buffer_len] = '\0';

    mutex_unlock(&lab2_mutex);

    pr_info("lab2_driver: received %zu bytes\n", len);

    return len;
}

static const struct file_operations lab2_fops = {
    .owner   = THIS_MODULE,
    .open    = lab2_open,
    .release = lab2_release,
    .read    = lab2_read,
    .write   = lab2_write,
};

static int __init lab2_init(void)
{
    int ret;

    pr_info("lab2_driver: initializing module\n");

    ret = register_chrdev(major_number, DEVICE_NAME, &lab2_fops);
    if (ret < 0) {
        pr_err("lab2_driver: register_chrdev failed: %d\n", ret);
        return ret;
    }

    lab2_class = class_create(THIS_MODULE, CLASS_NAME);
    if (IS_ERR(lab2_class)) {
        ret = PTR_ERR(lab2_class);
        unregister_chrdev(major_number, DEVICE_NAME);
        return ret;
    }

    lab2_device = device_create(
        lab2_class,
        NULL,
        MKDEV(major_number, 0),
        NULL,
        DEVICE_NAME
    );

    if (IS_ERR(lab2_device)) {
        ret = PTR_ERR(lab2_device);
        class_destroy(lab2_class);
        unregister_chrdev(major_number, DEVICE_NAME);
        return ret;
    }

        lab2_proc_entry = proc_create(
        PROC_NAME,
        0444,
        NULL,
        &lab2_proc_ops
    );

    if (!lab2_proc_entry) {
        device_destroy(lab2_class, MKDEV(major_number, 0));
        class_destroy(lab2_class);
        unregister_chrdev(major_number, DEVICE_NAME);
        return -ENOMEM;
    }

    ret = device_create_file(lab2_device, &dev_attr_buffer_len);
    if (ret) {
        proc_remove(lab2_proc_entry);
        device_destroy(lab2_class, MKDEV(major_number, 0));
        class_destroy(lab2_class);
        unregister_chrdev(major_number, DEVICE_NAME);
        return ret;
    }

    ret = device_create_file(lab2_device, &dev_attr_open_count);
    if (ret) {
        device_remove_file(lab2_device, &dev_attr_buffer_len);
        proc_remove(lab2_proc_entry);
        device_destroy(lab2_class, MKDEV(major_number, 0));
        class_destroy(lab2_class);
        unregister_chrdev(major_number, DEVICE_NAME);
        return ret;
    }

    ret = device_create_file(lab2_device, &dev_attr_last_data);
    if (ret) {
        device_remove_file(lab2_device, &dev_attr_open_count);
        device_remove_file(lab2_device, &dev_attr_buffer_len);
        proc_remove(lab2_proc_entry);
        device_destroy(lab2_class, MKDEV(major_number, 0));
        class_destroy(lab2_class);
        unregister_chrdev(major_number, DEVICE_NAME);
        return ret;
    }

    pr_info("lab2_driver: Procfs created /proc/%s\n", PROC_NAME);
    pr_info("lab2_driver: loaded, major=%d\n", major_number);

    return 0;
}

static void __exit lab2_exit(void)
{
    device_remove_file(lab2_device, &dev_attr_last_data);
    device_remove_file(lab2_device, &dev_attr_open_count);
    device_remove_file(lab2_device, &dev_attr_buffer_len);

    proc_remove(lab2_proc_entry);

    device_destroy(lab2_class, MKDEV(major_number, 0));
    class_destroy(lab2_class);
    unregister_chrdev(major_number, DEVICE_NAME);

    pr_info("lab2_driver: Sysfs removed\n");
    pr_info("lab2_driver: Procfs removed\n");
    pr_info("lab2_driver: module unloaded\n");
}

module_init(lab2_init);
module_exit(lab2_exit);
