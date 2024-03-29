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
#include <linux/fs.h>   // file_operations
#include <linux/slab.h>
#include <linux/string.h>
#include "aesdchar.h"
#include "aesd_ioctl.h"
int aesd_major = 0; // use dynamic major
int aesd_minor = 0;

MODULE_AUTHOR("Shashank Chandrasekaran"); /** TODO: fill in your name **/
MODULE_LICENSE("Dual BSD/GPL");

struct aesd_dev aesd_device;

int aesd_open(struct inode *inode, struct file *filp)
{
    struct aesd_dev *dev;
    PDEBUG("open");
    /**
     * TODO: handle open
     */
    dev = container_of(inode->i_cdev, struct aesd_dev, cdev);
    filp->private_data = dev;
    return 0;
}

int aesd_release(struct inode *inode, struct file *filp)
{
    PDEBUG("release");
    /**
     * TODO: handle release
     */
    filp->private_data = NULL;
    return 0;
}

ssize_t aesd_read(struct file *filp, char __user *buf, size_t count,
                  loff_t *f_pos)
{
    ssize_t retval = 0;
    int tmp_buffer_count = 0;
    size_t offset_byte;
    struct aesd_buffer_entry *tmp_buffer;
    struct aesd_dev *dev;

    PDEBUG("read %zu bytes with offset %lld", count, *f_pos);

    /**
     * TODO: handle read
     */

    dev = (struct aesd_dev*)filp->private_data;

    if (mutex_lock_interruptible(&dev->lock)!=0)
	{
		PDEBUG(KERN_ERR "Couldn't acquire Mutex\n");
		goto handle_error;
	}

    tmp_buffer = aesd_circular_buffer_find_entry_offset_for_fpos(&dev->circular_buffer, *f_pos, &offset_byte);

    if(tmp_buffer==NULL)
        goto handle_error;

    if ((tmp_buffer->size - offset_byte) < count) 
    {
        *f_pos = *f_pos + (tmp_buffer->size - offset_byte);
        tmp_buffer_count = tmp_buffer->size - offset_byte;
    } 
    else 
    {
        *f_pos = *f_pos + count;
        tmp_buffer_count = count;
    }

    if (copy_to_user(buf, tmp_buffer->buffptr+offset_byte, tmp_buffer_count)) 
    {
		retval = -EFAULT;
		goto handle_error;
	}

    retval = tmp_buffer_count;

    handle_error:
            mutex_unlock(&dev->lock);

    return retval;
}

ssize_t aesd_write(struct file *filp, const char __user *buf, size_t count,
                   loff_t *f_pos)
{
    ssize_t retval = 0;
    char *tmp_buffer;
    const char *replaced_buffer;
    int i, packet_send = 0, tmp_store = 0, tmp_total_size = 0; 
    struct aesd_buffer_entry write_buffer;
    struct aesd_dev *dev = filp->private_data;

    PDEBUG("write %zu bytes with offset %lld", count, *f_pos);
    /**
     * TODO: handle write
     */
    if (mutex_lock_interruptible(&dev->lock)!=0)
	{
		PDEBUG(KERN_ERR "Couldn't acquire Mutex\n");
		return -EFAULT;
	}

    tmp_buffer = (char *)kmalloc(count, GFP_KERNEL);
    if (tmp_buffer == NULL)
    {
        retval = -ENOMEM;
        goto error_handler;
    }
    
    if (copy_from_user(tmp_buffer, buf, count)) 
    {
        retval = -EFAULT;
		goto error_handler;
	}

    for (i = 0; i < count; i++) 
    {
        if (tmp_buffer[i] == '\n') 
        {
            packet_send = 1; 
            tmp_store = i+1; 
            break;
        }
    }

    if (dev->buffer_length == 0) 
    {
        dev->store_buffer = (char *)kmalloc(count, GFP_KERNEL);
        if (dev->store_buffer == NULL) 
        {
            retval = -ENOMEM;
            goto free_memory;
        }
        memcpy(dev->store_buffer, tmp_buffer, count);
        dev->buffer_length += count;
    } 
    else 
    {
        if (packet_send)
            tmp_total_size = tmp_store;
        else
            tmp_total_size = count;

        dev->store_buffer = (char *)krealloc(dev->store_buffer, dev->buffer_length + tmp_total_size, GFP_KERNEL);
        if (dev->store_buffer == NULL) 
        {
            retval = -ENOMEM;
            goto free_memory;
        }
      
        memcpy(dev->store_buffer + dev->buffer_length, tmp_buffer, tmp_total_size);
        dev->buffer_length += tmp_total_size;        
    }
 
    if (packet_send) 
    {
        write_buffer.buffptr = dev->store_buffer;
        write_buffer.size = dev->buffer_length;
        replaced_buffer = aesd_circular_buffer_add_entry(&dev->circular_buffer, &write_buffer);
    
        if (replaced_buffer != NULL)
            kfree(replaced_buffer);
        
        dev->buffer_length = 0;
    } 

    retval = count;

    free_memory: 
            kfree(tmp_buffer);
    error_handler: 
            mutex_unlock(&dev->lock);
  
    return retval;
}

static long aesd_adjust_file_offset(struct file *filp, unsigned int write_cmd, unsigned int write_cmd_offset)
{
    int fpos=0,i;
    long return_val;
    struct aesd_dev *dev=filp->private_data;

    if (mutex_lock_interruptible(&dev->lock)!=0)
	{
		PDEBUG(KERN_ERR "Couldn't acquire Mutex\n");
		return -EFAULT;
	}
    if ((write_cmd>=AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED)|| (write_cmd_offset>=dev->circular_buffer.entry[write_cmd].size))
    {
        PDEBUG(KERN_ERR "Write Command or/and Write Command Offset is inavlid\n");
        return_val = -EINVAL;
    }
    else
    {
        for (i=0;i<write_cmd;i++)
            fpos=fpos + dev->circular_buffer.entry[i].size;
        
        fpos=fpos+write_cmd_offset;
        filp->f_pos=fpos;
    }
    mutex_unlock(&dev->lock);
    return return_val;
}

loff_t aesd_llseek(struct file *filp, loff_t off, int whence)
{
    loff_t return_val, total_buff_size=0;
    int index;
    struct aesd_dev *dev=filp->private_data;
    struct aesd_buffer_entry *buffer_entry;

    if (mutex_lock_interruptible(&dev->lock)!=0)
	{
		PDEBUG(KERN_ERR "Couldn't acquire Mutex\n");
		return -EFAULT;
	}

    AESD_CIRCULAR_BUFFER_FOREACH(buffer_entry, &dev->circular_buffer, index)
    {
        total_buff_size = total_buff_size + buffer_entry->size;
    }
    return_val = fixed_size_llseek(filp, off, whence, total_buff_size);
    mutex_unlock(&dev->lock);
    return return_val;
}

long aesd_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
    struct aesd_seekto seekto;
    long retval=0;
    /*
	 * extract the type and number bitfields, and don't decode
	 * wrong cmds: return ENOTTY (inappropriate ioctl) before access_ok()
	 */
	if (_IOC_TYPE(cmd) != AESD_IOC_MAGIC) return -ENOTTY;
	if (_IOC_NR(cmd) > AESDCHAR_IOC_MAXNR) return -ENOTTY;

    switch(cmd)
    {
        case AESDCHAR_IOCSEEKTO:
            if (copy_from_user(&seekto, (const void __user *)arg, sizeof(seekto)) != 0) {
			    retval = -EFAULT;
            } else {
			    retval = aesd_adjust_file_offset(filp,seekto.write_cmd,seekto.write_cmd_offset);
            }
		    break;

        default:
		    return -ENOTTY;
    }
    return retval;
}


struct file_operations aesd_fops = {
    .owner = THIS_MODULE,
    .read = aesd_read,
    .write = aesd_write,
    .open = aesd_open,
    .release = aesd_release,
    .llseek =   aesd_llseek,
	.unlocked_ioctl = aesd_ioctl
};

static int aesd_setup_cdev(struct aesd_dev *dev)
{
    int err, devno = MKDEV(aesd_major, aesd_minor);

    cdev_init(&dev->cdev, &aesd_fops);
    dev->cdev.owner = THIS_MODULE;
    dev->cdev.ops = &aesd_fops;
    err = cdev_add(&dev->cdev, devno, 1);
    if (err)
    {
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
    if (result < 0)
    {
        printk(KERN_WARNING "Can't get major %d\n", aesd_major);
        return result;
    }
    memset(&aesd_device, 0, sizeof(struct aesd_dev));

    /**
     * TODO: initialize the AESD specific portion of the device
     */
    mutex_init(&aesd_device.lock);

    result = aesd_setup_cdev(&aesd_device);

    if (result)
    {
        unregister_chrdev_region(dev, 1);
    }
    return result;
}

void aesd_cleanup_module(void)
{
    int count = 0;
    struct aesd_buffer_entry *buffer_element;
    dev_t devno = MKDEV(aesd_major, aesd_minor);

    cdev_del(&aesd_device.cdev);

    /**
     * TODO: cleanup AESD specific poritions here as necessary
     */

    AESD_CIRCULAR_BUFFER_FOREACH(buffer_element, &aesd_device.circular_buffer, count)
    {
        if (buffer_element->buffptr != NULL)
        {
            kfree(buffer_element->buffptr);
            buffer_element->size = 0;
        }
    }

    mutex_destroy(&aesd_device.lock);

    unregister_chrdev_region(devno, 1);
}

module_init(aesd_init_module);
module_exit(aesd_cleanup_module);
