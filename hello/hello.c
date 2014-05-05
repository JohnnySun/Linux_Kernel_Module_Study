#include <linux/cdev.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/types.h>
#include <linux/kdev_t.h>
#include <linux/fs.h>
MODULE_LICENSE("Dual BSD/GPL");

dev_t dev;

struct scull_dev {
		struct scull_qset *data; /* Pointer to first quantum set */
		int quantum; /* the current quantum size */
		int qset; /* the current array size */
		unsigned long size; /* amount of data stored here */
		unsigned int access_key; /* used by sculluid and scullpriv */
		struct semaphore sem; /* mutual exclusion semaphore */
		struct cdev cdev; /* Char device structure */
};

struct file_operations scull_fops = {
		.owner = THIS_MODULE,
		.llseek = scull_llseek,
		.read = scull_read,
		.write = scull_write,
		.ioctl = scull_ioctl,
		.open = scull_open,
		.release = scull_release,
};




static void scull_setup_cdev(struct scull_dev *dev, int index)
{
		int err, devno = MKDEV(scull_major, scull_minor + index);
		cdev_init(&dev->cdev, &scull_fops);
		dev->cdev.owner = THIS_MODULE;
		dev->cdev.ops = &scull_fops;
		err = cdev_add (&dev->cdev, devno, 1);
		/* Fail gracefully if need be */
		if (err)
				printk(KERN_NOTICE "Error %d adding scull%d", err, index);
}

int scull_open(struct inode *inode, struct file *filp)
{
		struct scull_dev *dev; /* device information */
		dev = container_of(inode->i_cdev, struct scull_dev, cdev);
		filp->private_data = dev; /* for other methods */
		/* now trim to 0 the length of the device if open was write-only */
		if ( (filp->f_flags & O_ACCMODE) == O_WRONLY)
		{
				scull_trim(dev); /* ignore errors */
		}
		return 0; /* success */
}

int scull_release(struct inode *inode, struct file *filp)
{
		return 0;
}


static int __init hello_init(void)
{
		printk(KERN_ALERT "Hello, world\n");
		if(alloc_chrdev_region(&dev, 0, 10, "scull") != 0) {
				printk(KERN_ALERT "Could not Alloc Major!!!!\n");
		} else {
				printk(KERN_ALERT "Alloc Major Successed, This Devices Major is %d\n", MAJOR(dev));
		}
		//struct cdev *my_cdev = cdev_alloc();
		return 0;
}
static void __exit hello_exit(void)
{
		printk(KERN_ALERT "Goodbye, cruel world\n");
		unregister_chrdev_region(dev, 10);
}
module_init(hello_init);
module_exit(hello_exit);

