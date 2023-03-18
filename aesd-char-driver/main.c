/**
 * @file aesdchar.c
 * @brief Functions and data related to the AESD char driver implementation
 *
 * Based on the implementation of the "scull" device driver, found in
 * Linux Device Drivers example code.
 *
 * @author Dan Walkes
 * @date 2019-10-22
 * @copyright Copyright (c) 2019
 *
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/printk.h>
#include <linux/types.h>
#include <linux/cdev.h>
#include <linux/slab.h>
#include <linux/fs.h> // file_operations
#include "aesdchar.h"
int aesd_major =   0; // use dynamic major
int aesd_minor =   0;

MODULE_AUTHOR("Kshitija Dhondage");
MODULE_LICENSE("Dual BSD/GPL");

struct aesd_dev aesd_device;

int aesd_open(struct inode *inode, struct file *filp)
{
	struct aesd_dev *dev;
	
	PDEBUG("open");
	
	dev = container_of(inode->i_cdev, struct aesd_dev, cdev);
	
	filp->private_data = dev;
	
	return 0;
}

int aesd_release(struct inode *inode, struct file *filp)
{
	PDEBUG("release");
	
	return 0;
}

ssize_t aesd_read(struct file *filp, char __user *buf, size_t count,
                loff_t *f_pos)
{
	ssize_t retval = 0;
	ssize_t offset_read = 0;
	ssize_t data_unread = 0;
	
	struct aesd_dev *dev;
	struct aesd_buffer_entry *read_entry = NULL;
	

	
	PDEBUG("read %zu bytes with offset %lld",count,*f_pos);
	
	printk(KERN_DEBUG "read %zu bytes with offset %lld",count,*f_pos);
	
	dev = (struct aesd_dev*) filp->private_data;
	
	if(filp == NULL || buf == NULL || f_pos == NULL){
		return -EFAULT;
	}
	
	// Check for NULL parameters
	if (count == 0){
		return 0;
	}
	
	//Mutex lock
	if(mutex_lock_interruptible(&dev->lock)){
		PDEBUG(KERN_ERR "Mutex Lock Failed");
		return -ERESTARTSYS;
	}
	
	//find the read entry, and offset for given f_pos
	read_entry = aesd_circular_buffer_find_entry_offset_for_fpos(&(dev->cir_buff), *f_pos, &offset_read); 
	if(read_entry == NULL){
		goto error_exit;
	}
	else{
		// Check for max read limits
		if(count > (read_entry->size - offset_read)) {
			count = read_entry->size - offset_read;
			}
	}
	
	//copy to user for read
	data_unread = copy_to_user(buf, (read_entry->buffptr + offset_read), count);
	
	// update fpos and return value (retval)
	retval = count - data_unread;
	
	*f_pos += retval;

error_exit:
	mutex_unlock(&(dev->lock));
	return retval;
}

ssize_t aesd_write(struct file *filp, const char __user *buf, size_t count,
                loff_t *f_pos)
{
	ssize_t retval = -ENOMEM;
	const char *new_entry = NULL;
	ssize_t data_write = 0;
	
	struct aesd_dev *dev;
	
	PDEBUG("write %zu bytes with offset %lld",count,*f_pos);
	
	dev = (struct aesd_dev*) filp->private_data;
	
	if(filp == NULL || buf == NULL || f_pos == NULL){
		return -EFAULT;
	}
	
	// Check for NULL parameters
	if (count == 0){
		return 0;
	}
	
	//Mutex lock
	if(mutex_lock_interruptible(&dev->lock)){
		PDEBUG(KERN_ERR "Mutex Lock Failed");
		return -ERESTARTSYS;
	}
	
	//allocate new buffer size
	if(dev->buff_entry.size == 0){
		
		dev->buff_entry.buffptr = kmalloc(count*sizeof(char), GFP_KERNEL);

		if(dev->buff_entry.buffptr == NULL){
			PDEBUG("Memory allocation error");
			goto exit_error;
		}
	}
	//if memory already exist
	else{
		dev->buff_entry.buffptr = krealloc(dev->buff_entry.buffptr, (dev->buff_entry.size + count)*sizeof(char), GFP_KERNEL);
		if(dev->buff_entry.buffptr == NULL){
			PDEBUG("Memory realloc failed");
			goto exit_error;
		}
	}
	
	//copy data from user space buffer to current command
	data_write = copy_from_user((void *)(dev->buff_entry.buffptr + dev->buff_entry.size), buf, count);
	retval = count - data_write; //Write bytes
	dev->buff_entry.size += retval;
	
	//add \n in circular buffer
	if(memchr(dev->buff_entry.buffptr, '\n', dev->buff_entry.size)){

		new_entry = aesd_circular_buffer_add_entry(&dev->cir_buff, &dev->buff_entry); 
		
		if(new_entry){
			kfree(new_entry);// !doubt about this
		}

		//clear entry parameters
		dev->buff_entry.buffptr = NULL;
		dev->buff_entry.size = 0;

	}


	//handle errors
exit_error:
	mutex_unlock(&dev->lock);
	return retval;

}
struct file_operations aesd_fops = {
	.owner =    THIS_MODULE,
	.read =     aesd_read,
	.write =    aesd_write,
	.open =     aesd_open,
	.release =  aesd_release,
};

static int aesd_setup_cdev(struct aesd_dev *dev)
{
	int err, devno = MKDEV(aesd_major, aesd_minor);

	cdev_init(&dev->cdev, &aesd_fops);
	dev->cdev.owner = THIS_MODULE;
	dev->cdev.ops = &aesd_fops;
	err = cdev_add (&dev->cdev, devno, 1);
	if (err) {
		printk(KERN_ERR "Error %d adding aesd cdev", err);
	}
	return err;
}



int aesd_init_module(void)
{
	dev_t dev = 0;
	int result;
	result = alloc_chrdev_region(&dev, aesd_minor, 1,
			"aesdchar");
	aesd_major = MAJOR(dev);
	if (result < 0) {
		printk(KERN_WARNING "Can't get major %d\n", aesd_major);
		return result;
	}
	memset(&aesd_device,0,sizeof(struct aesd_dev));


	mutex_init(&aesd_device.lock);
	aesd_circular_buffer_init(&aesd_device.cir_buff);

	result = aesd_setup_cdev(&aesd_device);

	if( result ) {
		unregister_chrdev_region(dev, 1);
	}
	return result;

}

void aesd_cleanup_module(void)
{
	uint8_t current_idx = 0; 
	struct aesd_buffer_entry *entry = NULL;
	
	dev_t devno = MKDEV(aesd_major, aesd_minor);

	cdev_del(&aesd_device.cdev);

	kfree(aesd_device.buff_entry.buffptr);
	
	AESD_CIRCULAR_BUFFER_FOREACH(entry, &aesd_device.cir_buff, current_idx){
		if(entry->buffptr != NULL){
			kfree(entry->buffptr);
		}
	}
	unregister_chrdev_region(devno, 1);
}



module_init(aesd_init_module);
module_exit(aesd_cleanup_module);