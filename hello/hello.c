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

struct scull_qset {
		void **data;
		struct scull_qset *next;
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


int scull_trim(struct scull_dev *dev)
{
		struct scull_qset *next, *dptr;
		int qset = dev->qset; /* "dev" is not-null */
		int i;
		for (dptr = dev->data; dptr; dptr = next)
		{ /* all the list items */
				if (dptr->data) {
						for (i = 0; i < qset; i++)
								kfree(dptr->data[i]);
						kfree(dptr->data);
						dptr->data = NULL;
				}
				next = dptr->next;
				kfree(dptr);
		}
		dev->size = 0;
		dev->quantum = scull_quantum;
		dev->qset = scull_qset;
		dev->data = NULL;
		return 0;
}


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

ssize_t scull_read(struct file *filp, char __user *buf, size_t count, loff_t *f_pos)
{
		struct scull_dev *dev = filp->private_data;
		struct scull_qset *dptr; /* the first listitem */
		int quantum = dev->quantum, qset = dev->qset;
		int itemsize = quantum * qset; /* how many bytes in the listitem */
		int item, s_pos, q_pos, rest;
		ssize_t retval = 0;
		if (down_interruptible(&dev->sem))
				return -ERESTARTSYS;
		if (*f_pos >= dev->size)
				goto out;
		if (*f_pos + count > dev->size)
				count = dev->size - *f_pos;
		/* find listitem, qset index, and offset in the quantum */
		item = (long)*f_pos / itemsize;
		rest = (long)*f_pos % itemsize;
		s_pos = rest / quantum;
		q_pos = rest % quantum;
		/* follow the list up to the right position (defined elsewhere) */
		dptr = scull_follow(dev, item);
		if (dptr == NULL || !dptr->data || ! dptr->data[s_pos])
				goto out; /* don't fill holes */
		/* read only up to the end of this quantum */
		if (count > quantum - q_pos)
				count = quantum - q_pos;
		if (copy_to_user(buf, dptr->data[s_pos] + q_pos, count))
		{
				retval = -EFAULT;
				goto out;
		}
		/* copy data to user space */
		*f_pos += count;
		retval = count;
out:
		up(&dev->sem);
		return retval;
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

