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
#include <linux/fs.h> // file_operations
#include "aesdchar.h"
int aesd_major =   0; // use dynamic major
int aesd_minor =   0;

MODULE_AUTHOR("Your Name Here"); /** TODO: fill in your name **/
MODULE_LICENSE("Dual BSD/GPL");

struct aesd_dev aesd_device;

int aesd_open(struct inode *inode, struct file *filp)
{
    PDEBUG("open");
    /**
     * TODO: handle open
     */
	struct aesd_dev *dev = container_of(inode->i_cdev, struct aesd_dev, cdev);
	filp->private_data = dev;
	
    return 0;
}

int aesd_release(struct inode *inode, struct file *filp)
{
    PDEBUG("release");
    /**
     * TODO: handle release
     */
    return 0;
}

ssize_t aesd_read(struct file *filp, char __user *buf, size_t count,
                loff_t *f_pos)
{
    PDEBUG("read %zu bytes with offset %lld",count,*f_pos);
    /**
     * TODO: handle read
     */
	struct aesd_dev *dev = filp->private_data;
    ssize_t total_copied = 0;
	
	/* Lock the device to protect circular buffer during read */
	if (mutex_lock_interruptible(&dev->lock)) {
		return -ERESTARTSYS;
	}
	
	/*
     * Read loop:
     * Keep copying data from the circular buffer into user space
     * until either:
     *  - requested byte count is satisfied, or
     *  - no more data exists in the buffer
     */
	while (count > 0) {
        size_t entry_offset = 0;

		/*
         * Find which circular buffer entry corresponds to the current file position (*f_pos).
         *
         * This helper function:
         *  - walks through the circular buffer in order
         *  - sums entry sizes
         *  - determines:
         *      1. which entry contains the requested offset
         *      2. the byte offset inside that entry
         *
         * Result:
         *  - entry: pointer to the correct buffer entry
         *  - entry_offset: offset inside that entry where reading should begin
         */
		struct aesd_buffer_entry *entry =
					aesd_circular_buffer_find_entry_offset_for_fpos(
						&dev->circular_buffer,
						*f_pos,
						&entry_offset);
		
		/* If no valid entry exists, we've reached end of available data */		
        if (!entry || !entry->buffptr)
            break;

        /*
         * Determine how many bytes we can copy from this entry:
         * - limited by remaining data in this entry
         * - limited by remaining requested count
         */
        size_t entry_remaining = entry->size - entry_offset;
        size_t to_copy = (count < entry_remaining) ? count : entry_remaining;

        /* Copy data from kernel space to user space buffer */
        if (copy_to_user(buf + total_copied, entry->buffptr + entry_offset, to_copy)) {	
            if (total_copied == 0)
				total_copied = -EFAULT;
			break;
		}
		
		/* Advance counters after successful copy */
        total_copied += to_copy;
        *f_pos += to_copy;
        count -= to_copy;
    }
	
	/* Release device lock */
	mutex_unlock(dev->lock);
	
	/* Return number of bytes successfully copied (or error) */
    return total_copied;
}

ssize_t aesd_write(struct file *filp, const char __user *buf, size_t count,
                loff_t *f_pos)
{
    ssize_t retval = count;
    PDEBUG("write %zu bytes with offset %lld",count,*f_pos);
    /**
     * TODO: handle write
     */
	struct aesd_dev *dev = filp->private_data;
	
	/* Temporary kernel buffer for incoming data */
    char *kbuf = kmalloc(count, GFP_KERNEL);
    if (!kbuf)
        return -ENOMEM;
	
	/* Copy data from user space into temporary kernel buffer */
    if (copy_from_user(kbuf, buf, count)) {
        kfree(kbuf);
        return -EFAULT;
    }
	
	/* Lock device while modifying buffer state */
    if (mutex_lock_interruptible(&dev->lock)) {
        kfree(kbuf);
        return -ERESTARTSYS;
    }
	
	/* Process input byte-by-byte, building pending buffer and committing on newline */
    for (size_t i = 0; i < count; i++) {

        char *new_buf;

        /* grow pending buffer */
        new_buf = krealloc(dev->pending.buffptr,
                           dev->pending.size + 1,
                           GFP_KERNEL);

        if (!new_buf) {
            retval = -ENOMEM;
            break;
        }

        dev->pending.buffptr = new_buf;
        dev->pending.buffptr[dev->pending.size] = kbuf[i];
        dev->pending.size++;

        /* if newline, commit entry to circular buffer */
        if (kbuf[i] == '\n') {

            struct aesd_buffer_entry entry;

            entry.buffptr = dev->pending.buffptr;
            entry.size = dev->pending.size;

            aesd_circular_buffer_add_entry(&dev->circular_buffer, &entry);

            /* reset pending */
            dev->pending.buffptr = NULL;
            dev->pending.size = 0;
        }
    }
	
	mutex_unlock(&dev->lock);
    kfree(kbuf);
	
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

    /**
     * TODO: initialize the AESD specific portion of the device
     */
	aesd_circular_buffer_init(&aesd_device.circular_buffer);
	aesd_device.pending.buffptr = NULL;
	aesd_device.pending.size = 0;
	mutex_init(&aesd_device.lock);

    result = aesd_setup_cdev(&aesd_device);

    if( result ) {
        unregister_chrdev_region(dev, 1);
    }
    return result;

}

void aesd_cleanup_module(void)
{
    dev_t devno = MKDEV(aesd_major, aesd_minor);

    cdev_del(&aesd_device.cdev);

    /**
     * TODO: cleanup AESD specific poritions here as necessary
     */
	struct aesd_buffer_entry *entry;
	uint8_t i;
	
	for (i = 0; i < AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED; i++) {
		entry = &aesd_device.circular_buffer.entry[i];
		if (entry->buffptr) {
			kfree(entry->buffptr);
			entry->buffptr = NULL;
			entry->size = 0;
		}
	}
	
	if (aesd_device.pending.buffptr) {
		kfree(aesd_device.pending.buffptr);
		aesd_device.pending.buffptr = NULL;
		aesd_device.pending.size = 0;
	}
	
	// No need for mutex_destroy in kernel code 

    unregister_chrdev_region(devno, 1);
}



module_init(aesd_init_module);
module_exit(aesd_cleanup_module);
