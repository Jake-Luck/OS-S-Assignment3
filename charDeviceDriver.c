#include <linux/kernel.h> // kernel stuff
#include <linux/module.h> // module stuff
#include <linux/slab.h> // kmalloc
#include <linux/fs.h> // file stuff
#include <linux/types.h> // ssize etc
#include <linux/uaccess.h> //copy from user
#include "charDeviceDriver.h"

MODULE_LICENSE("GPL");

static struct file_operations fops = {
	.read = device_read,
	.write = device_write,
	.open = device_open,
	.release = device_release
};

#define DEVICE_NAME "chardev"
#define MAX_MESSAGES 1000
#define MAX_MESSAGE_BYTES 4096
DEFINE_MUTEX(lock);

static int Major;
static int Device_Open = 0;
static char **messageQueue;
static int head = 0;
static int queueLength = 0;

int init_module(void) {
    Major = register_chrdev(0, DEVICE_NAME, &fops);
	if (Major < 0) {
	  printk(KERN_ALERT "Registering char device failed with %d\n", Major);
	  return Major;
	}
    if ((messageQueue = (char**) kmalloc(MAX_MESSAGES * sizeof(char*), GFP_KERNEL)) == NULL) {
        printk(KERN_INFO "Memory allocation failed");
        return -EFAULT;
    }

	printk(KERN_INFO "Major number %d. To talk to\n", Major);
	printk(KERN_INFO "the driver, create a dev file with\n");
	printk(KERN_INFO "'mknod /dev/%s c %d 0'.\n", DEVICE_NAME, Major);
	printk(KERN_INFO "Remove the device file and module when done.\n");

	return 0;
}

void cleanup_module(void)
{
	/*  Unregister the device */
	unregister_chrdev(Major, DEVICE_NAME);
    for (queueLength; queueLength >= 0; queueLength--) {
        kfree(*(messageQueue + ((head+queueLength) % MAX_MESSAGES)));
    }
    kfree(messageQueue);
}

static int device_open(struct inode *inode, struct file *file) {
    mutex_lock(&lock);
        if (Device_Open == 1) {
            mutex_unlock(&lock);
            return -EBUSY;
        }
        Device_Open = 1;
    mutex_unlock(&lock);
    
    try_module_get(THIS_MODULE);
    return 0;
}

static int device_release(struct inode *inode, struct file *file) {
    mutex_lock(&lock);
        Device_Open = 0;
    mutex_unlock(&lock);

    module_put(THIS_MODULE);
    return 0;
}

static ssize_t device_read(struct file *filp, char *buffer, size_t bufferLength, loff_t *offset) {
    mutex_lock(&lock);
        if(queueLength == 0) {
            mutex_unlock(&lock);
            printk(KERN_ALERT "No messages to read\n");
            return -EAGAIN;
        }
        if(strlen(*(messageQueue+head)) + 1 < bufferLength) bufferLength = strlen(*(messageQueue+head)) + 1;
        if (copy_to_user(buffer, *(messageQueue + head), bufferLength) > 0) {
            mutex_unlock(&lock);
            printk(KERN_ALERT "Copy went wrong\n");
            return -EFAULT;
        }
        queueLength--;
        kfree(*(messageQueue+head));
        head++;
    mutex_unlock(&lock);

    return bufferLength;
}

static ssize_t device_write(struct file *filp, const char *buffer, size_t bufferLength, loff_t *offset) {
    int messageLength = bufferLength + sizeof(char);
    if (messageLength > MAX_MESSAGE_BYTES) {
        printk(KERN_ALERT "Message too long\n");
        return -EINVAL;
    }
    if((*(messageQueue + ((head + queueLength) % MAX_MESSAGES)) = kmalloc(messageLength, GFP_KERNEL)) == NULL) {
        printk(KERN_ALERT "Memory allocation failed\n");
        return -EFAULT;
    }

    mutex_lock(&lock);
        if (queueLength >= 1000) {
            mutex_unlock(&lock);
            printk(KERN_ALERT "Message queue full\n");
            return -EBUSY;
        }
        if(copy_from_user(*(messageQueue + ((head + queueLength) % MAX_MESSAGES)), buffer, bufferLength) > 0) {
            mutex_unlock(&lock);
            printk(KERN_ALERT "Copy went wrong\n");
            return -EFAULT;
        }
        *(*(messageQueue + ((head + queueLength) % MAX_MESSAGES)) + bufferLength) = '\0';
        queueLength++;
    mutex_unlock(&lock);

    return bufferLength;
}