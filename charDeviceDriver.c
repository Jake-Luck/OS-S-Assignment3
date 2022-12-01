#include <linux/kernel.h> // kernel stuff
#include <linux/module.h> // module stuff
#include <slab.h> // kmalloc
#include <linux/fs.h> // file stuff
#include <linux/types.h> // ssize etc
#include <linux/uaccess.h> //copy from user

#include "charDeviceDriver.h"

MODULE_LICENSE("GPL");

DEFINE_MUTEX (devlock);
static int counter = 0;