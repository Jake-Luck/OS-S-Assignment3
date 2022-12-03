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
static char* messageQueue;
static int head = 0;
static int queueLength = 0;

int init_module(void) {
    Major = register_chrdev(0, DEVICE_NAME, &fops);
	if (Major < 0) {
	  printk(KERN_ALERT "Registering char device failed with %d\n", Major);
	  return Major;
	}
    if ((messageQueue = kmalloc(MAX_MESSAGES * sizeof(char), GFP_KERNEL)) == NULL) {
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
    for (head = 0; head < MAX_MESSAGES; head++) {
        kfree(&messageQueue[head]);
    }
    kfree(messageQueue);
}

static int device_open(struct inode *inode, struct file *file) {
    printk(KERN_INFO "Device has been opened\n");
    try_module_get(THIS_MODULE);
    return 0;
}

static int device_release(struct inode *inode, struct file *file) {
    module_put(THIS_MODULE);
    return 0;
}

static ssize_t device_read(struct file *filp, char *buffer, size_t bufferLength, loff_t *offset) {
    mutex_lock(&lock);
    if(queueLength == 0) {
        mutex_unlock(&lock);
        printk(KERN_INFO "No messages to read\n");
        return -EAGAIN;
    }
    if (copy_to_user(buffer, messageQueue + head, bufferLength) > 0) {
        mutex_unlock(&lock);
        printk(KERN_INFO "Copy went wrong\n");
        return -EFAULT;
    }
    kfree(messageQueue + head);
    head = (head + 1) % MAX_MESSAGES;
    queueLength--;
    mutex_unlock(&lock);
    return bufferLength;
}

static ssize_t device_write(struct file *filp, const char *buffer, size_t bufferLength, loff_t *offset) {
    char *messageWritten;
    int messageLength;
    printk(KERN_INFO "Test: initialised messageWritten\n");
    messageLength = bufferLength + sizeof(char);
    printk(KERN_INFO "Test: initialised messageLength\n");
    if (messageLength > MAX_MESSAGE_BYTES) {
        printk(KERN_INFO "Message too long\n");
        return -EINVAL;
    }
    printk(KERN_INFO "Test: Message not too long\n");
    if((messageWritten = kmalloc(messageLength, GFP_KERNEL)) == NULL) {
        printk(KERN_INFO "Memory allocation failed\n");
        return -EFAULT;
    }
    printk(KERN_INFO "Test: Memory allocated\n");
    mutex_lock(&lock);
    printk(KERN_INFO "Test: Mutex locked\n");
    if (queueLength >= 1000) {
        mutex_unlock(&lock);
        printk(KERN_INFO "Message queue full\n");
        return -EBUSY;
    }
    printk(KERN_INFO "Test: Queue not full\n");
    if(copy_from_user(messageWritten, buffer, bufferLength) > 0) {
        mutex_unlock(&lock);
        printk(KERN_INFO "Copy went wrong\n");
        return -EFAULT;
    }
    printk(KERN_INFO "Test: Copy successful\n");
    *(messageQueue + ((head + queueLength) % MAX_MESSAGES)) = *messageWritten;
    printk(KERN_INFO "Test: Message placed in queue\n");
    queueLength++;
    printk(KERN_INFO "Test: Queue length incremented\n");
    mutex_unlock(&lock);
    printk(KERN_INFO "Test: Mutex unlocked\n");
    return bufferLength;
}