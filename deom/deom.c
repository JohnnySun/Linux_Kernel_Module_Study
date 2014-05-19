#include <linux/module.h>
#include <linux/types.h>
#include <linux/fs.h>
#include <linux/errno.h>
#include <linux/mm.h>
#include <linux/sched.h>
#include <linux/init.h>
#include <linux/cdev.h>
#include <asm/io.h>
#include <asm/system.h>
#include <asm/uaccess.h>
#include <linux/slab.h>

#define GLOBALMEM_SIZE 0x1000 /*Global memory size:4kb*/
#define MEM_CLEAR 0x1 /*Global memory clear 0*/
#define GLOBALMEM_MAJOR 250 /*Suggested globalmem major Number*/

#define _user 
static int globalmem_major = GLOBALMEM_MAJOR;

/*cdev_init(),the globalmem's cdev linked file_operations struct*/
/*struct of device globalmem_dev*/
struct globalmem_dev
{
		struct cdev cdev; /*cdev struct */
		unsigned char mem[GLOBALMEM_SIZE]; /*global memory */
};

struct globalmem_dev *globalmem_devp; /*the pointer of device struct*/

/* the file open function */
int globalmem_open(struct inode *inode,struct file *filp)
{
		/*put the struct of device date to file private data pointer */
		filp->private_data= globalmem_devp;
		return 0;
}
/* the function of release file */
int globalmem_release(struct inode *inode,struct file *filp)
{
		return 0;
}


static ssize_t globalmem_read(struct file *filp,char _user *buf,size_t size,loff_t *ppos)
{
		unsigned long p = *ppos;
		unsigned int count = size;
		int ret = 0;

		struct globalmem_dev *dev = filp->private_data;/*get the device pointer*/
		/*analyse and get usefull read length*/
		if(p >= GLOBALMEM_SIZE) //offset position over boundary*/
		{
				return count ? - ENXIO:0;
		}
		if(count >GLOBALMEM_SIZE -p) //to read word number too long
		{
				count =GLOBALMEM_SIZE -p;
		}
		/*kernel space -> user space*/
		if(copy_to_user(buf,(void*)(dev->mem +p),count))
		{
				ret = -EFAULT;
		}
		else
		{
				*ppos += count;
				ret = count;

				printk(KERN_INFO"read %u bytes(s) from %lu \n",count,p);
		}
		return ret;
}

static ssize_t globalmem_write(struct file *filp,const char _user *buf,size_t size, loff_t *ppos)
{
		unsigned long p = *ppos;
		unsigned int count = size;
		int ret =0;

		struct globalmem_dev *dev = filp->private_data;/*get device pointer */
		/*analyse and get valid writing length */
		if(p >= GLOBALMEM_SIZE) // to write offset position over boundry*/
		{
				return count? -ENXIO:0;
		}

		/*user space -> kernel space */
		if(copy_from_user(dev->mem +p,buf,count))
		{
				ret = -EFAULT;
		}
		else
		{
				*ppos += count;
				ret = count;

				printk(KERN_INFO "written %u bytes(s) from %lu \n",count,p);
		}
		return ret;
}

static loff_t globalmem_llseek(struct file *filp,loff_t offset, int orig)
{
		loff_t ret;
		switch (orig)
		{
				case 0: /*offset from the file head*/
						if(offset < 0)
						{
								ret = - EINVAL;
								break;
						}

						if((unsigned int)offset>GLOBALMEM_SIZE) // offset overstep the boundary
						{
								ret = -EINVAL;
								break;
						}
						filp->f_pos = (unsigned int)offset;
						ret =filp->f_pos;
						break;

				case 1: /* offset from the current position */
						if((filp->f_pos +offset) >GLOBALMEM_SIZE) //offset overstep the boundary
						{
								ret = -EINVAL;
								break;
						}
						if((filp->f_pos +offset)<0)
						{
								ret = -EINVAL;
								break;
						}
						filp->f_pos += offset;
						ret = filp->f_pos;
						break;
				default:
						ret = -EINVAL;
		}
		return ret;
}

static long globalmem_ioctl(struct file *filp,unsigned int cmd,unsigned long arg)
{
		struct globalmem_dev *dev = filp->private_data;/* get the struct of device pointer */
		switch(cmd)
		{
				case MEM_CLEAR: //clear global meme
						memset(dev->mem,0,GLOBALMEM_SIZE);
						printk(KERN_INFO "globalmem is set to zero \n");
						break;
				default:
						return - EINVAL; //other order not support
		}
		return 0;
}
static const struct file_operations globalmem_fops=
{
		.owner = THIS_MODULE,
		.llseek = globalmem_llseek,
		.read = globalmem_read,
		.write = globalmem_write,
		.unlocked_ioctl = globalmem_ioctl,
		.open = globalmem_open,
		.release= globalmem_release,
};

/*init and add cdev struct*/
static void globalmem_setup_cdev(struct globalmem_dev *dev,int index)
{
		int err,devno = MKDEV(globalmem_major,0);
		cdev_init(&dev->cdev,&globalmem_fops);
		dev->cdev.owner = THIS_MODULE;
		dev->cdev.ops = &globalmem_fops;
		err = cdev_add(&dev->cdev,devno,1);
		if(err)
		{
				printk(KERN_NOTICE"Error %d adding globalmem",err);
		}
}


/* the function of globalmem driver module insmod */
int globalmem_init(void)
{
		int result;
		dev_t devno = MKDEV(globalmem_major,0);

		/*apply for char device driver region */
		if(globalmem_major)
		{
				result = register_chrdev_region(devno,1,"globalmem");
		}else{ /*dynamic get major device number*/
				result = alloc_chrdev_region(&devno,0,1,"globalmem");
				globalmem_major = MAJOR(devno);
		}

		if (result < 0)
		{
				return result;
		}

		/*dynamic apply for memery of the device struct */
		globalmem_devp = kmalloc(sizeof (struct globalmem_dev),GFP_KERNEL);
		if(!globalmem_devp)/* apply failed*/
		{
				result = -ENOMEM;
				goto fail_malloc;
		}
		memset(globalmem_devp,0,sizeof(struct globalmem_dev));
		globalmem_setup_cdev(globalmem_devp,0);
		return 0;
fail_malloc:unregister_chrdev_region(devno,1);
			return result;
}

/*rsmod module function*/
void globalmem_exit(void)
{
		cdev_del(&globalmem_devp->cdev); /* unregister cdev*/
		kfree(globalmem_devp); /* release device struct memery*/
		unregister_chrdev_region(MKDEV(globalmem_major,0),1);/* release device number*/
}
MODULE_AUTHOR("JohnnySun");
MODULE_LICENSE("Dual BSD/GPL");

module_param(globalmem_major,int,S_IRUGO);
module_init(globalmem_init);
module_exit(globalmem_exit);
