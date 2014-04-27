#include <linux/init.h>
#include <linux/module.h>
#include <linux/types.h>
#include <linux/kdev_t.h>
#include <linux/fs.h>

MODULE_LICENSE("Dual BSD/GPL");

dev_t dev;

static int __init hello_init(void)
{
		printk(KERN_ALERT "Hello, world\n");
		if(alloc_chrdev_region(&dev, 0, 10, "scull") != 0) {
				printk(KERN_ALERT "Could not Alloc Major!!!!\n");
		} else {
				printk(KERN_ALERT "Alloc Major Successed, This Devices Major is %d\n", MAJOR(dev));
		}
		return 0;
}
static void __exit hello_exit(void)
{
		printk(KERN_ALERT "Goodbye, cruel world\n");
		unregister_chrdev_region(dev, 10);
}
module_init(hello_init);
module_exit(hello_exit);

