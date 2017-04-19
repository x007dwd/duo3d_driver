////////////////////////////////////////////////////////////////////////////////////////////////////////
// This file is part of DUO SDK that allows the use of DUO devices in your own applications
// For updates and file downloads go to: http://duo3d.com/...
// Copyright 2014-2016 (c) Code Laboratories, Inc. All rights reserved.
////////////////////////////////////////////////////////////////////////////////////////////////////////
#ifndef DUO_DRIVER_H
#define DUO_DRIVER_H

#include <linux/module.h>
#include <linux/init.h>
#include <linux/usb.h>			/* USB stuff */
#include <linux/dma-mapping.h>
#include <linux/kfifo.h>

#include "duo_driver_api.h"

#if defined(ML_DEBUG)
    // Debugging levels
    #define DEBUG_LEVEL_DEBUG		0x1F
    #define DEBUG_LEVEL_INFO		0x0F
    #define DEBUG_LEVEL_WARN		0x07
    #define DEBUG_LEVEL_ERROR		0x03
    #define DEBUG_LEVEL_CRITICAL	0x01

    #define DBG_DEBUG(fmt, args...) \
    if ((debug_level & DEBUG_LEVEL_DEBUG) == DEBUG_LEVEL_DEBUG) \
        printk( KERN_DEBUG "[debug] DUO:%s(%d): " fmt "\n", \
                __FUNCTION__, __LINE__, ## args)
    #define DBG_INFO(fmt, args...) \
    if ((debug_level & DEBUG_LEVEL_INFO) == DEBUG_LEVEL_INFO) \
        printk( KERN_DEBUG "[info]  DUO:%s(%d): " fmt "\n", \
                __FUNCTION__, __LINE__, ## args)
    #define DBG_WARN(fmt, args...) \
    if ((debug_level & DEBUG_LEVEL_WARN) == DEBUG_LEVEL_WARN) \
        printk( KERN_DEBUG "[warn]  DUO:%s(%d): " fmt "\n", \
                __FUNCTION__, __LINE__, ## args)
    #define DBG_ERR(fmt, args...) \
    if ((debug_level & DEBUG_LEVEL_ERROR) == DEBUG_LEVEL_ERROR) \
        printk( KERN_DEBUG "[err]   DUO:%s(%d): " fmt "\n", \
                __FUNCTION__, __LINE__, ## args)
    #define DBG_CRIT(fmt, args...) \
    if ((debug_level & DEBUG_LEVEL_CRITICAL) == DEBUG_LEVEL_CRITICAL) \
        printk( KERN_DEBUG "[crit]  DUO:%s(%d): " fmt "\n", \
                __FUNCTION__, __LINE__, ## args)

    static int debug_level = DEBUG_LEVEL_DEBUG; //DEBUG_LEVEL_INFO;
    module_param(debug_level, int, S_IRUGO | S_IWUSR);
    MODULE_PARM_DESC(debug_level, "debug level (bitmask)");
#else
    #define DBG_DEBUG(fmt, args...)
    #define DBG_INFO(fmt, args...)
    #define DBG_WARN(fmt, args...)
    #define DBG_ERR(fmt, args...)
    #define DBG_CRIT(fmt, args...)
#endif

#define BULK_IN_BLOCK_SIZE      (FRAME_HEADER_SIZE)    // default bulk in block size

// Bitmask
#define DUO_BULK_STREAMING      0

struct usb_duo
{
    char                    name[64];
    char                    serial[256];
    struct usb_device       *udev;
    int                     minor;
    unsigned long           flags;

    struct usb_anchor       bulk_anchor;    // usb anchor
    struct duo_streaming    st_params;      // USB streaming parameters

    wait_queue_head_t       frame_event;    // wait queue for DUO_GET_FRAME
    struct kfifo            frame_fifo;
    unsigned char           *frame_buffer;  // page aligned kmalloc buffer for all frames
    unsigned long           frame_buflen;   // buffer length
    void                    *pCurrFrame;    // currently worked on frame
    unsigned int            data_offset;    // frame data offset in current frame
    struct semaphore        sem;            // Locks this structure
};
static struct usb_driver duo_driver;

static struct usb_device_id duo_table [] =
{
    { USB_DEVICE(0x04b4, 0xa1e0) },
    { USB_DEVICE(0xc0de, 0xa1e0) },
    {}
};
MODULE_DEVICE_TABLE (usb, duo_table);

static int duo_open(struct inode *inode, struct file *file);
static int duo_release(struct inode *inode, struct file *file);
static long duo_ioctl(struct file *file, unsigned int cmd, unsigned long arg);
static int duo_mmap(struct file *filp, struct vm_area_struct *vma);

static struct file_operations duo_fops =
{
    .owner = THIS_MODULE,
    .open =           duo_open,
    .release =        duo_release,
    .unlocked_ioctl = duo_ioctl,
    .mmap =           duo_mmap,
};

static char *duo_devnode(struct device *dev, umode_t *mode);

static struct usb_class_driver duo_class =
{
    .name = "duo%d",
    .fops = &duo_fops,
    .devnode = duo_devnode,
    .minor_base = 0,
};

static inline void duo_delete(struct usb_duo *dev);
static int duo_probe(struct usb_interface *interface, const struct usb_device_id *id);
static void duo_disconnect(struct usb_interface *interface);

static struct usb_driver duo_driver =
{
    .name = "DUO Camera",
    .id_table =     duo_table,
    .probe =        duo_probe,
    .disconnect =   duo_disconnect,
};

static int __init duo_init(void);
static void __exit duo_exit(void);

#endif // DUO_DRIVER_H
