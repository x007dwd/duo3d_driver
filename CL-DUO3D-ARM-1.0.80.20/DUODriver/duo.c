////////////////////////////////////////////////////////////////////////////////////////////////////////
// This file is part of DUO SDK that allows the use of DUO devices in your own applications
// For updates and file downloads go to: http://duo3d.com/...
// Copyright 2014-2016 (c) Code Laboratories, Inc. All rights reserved.
////////////////////////////////////////////////////////////////////////////////////////////////////////
#include "duo.h"
#include <linux/slab.h>
#include <asm/uaccess.h>

// DO NOT MODIFY!!!
#define MAX_FRAME_COUNT         (3)
#define FRAME_HEADER_SIZE       (512*2)
#define FRAME_DATA_SIZE         (752*480)
#define FRAME_BUFFER_SIZE       ((FRAME_HEADER_SIZE+FRAME_DATA_SIZE*2)*MAX_FRAME_COUNT)

#define BULK_IN_EP              (USB_DIR_IN + 0x02)

// Current version
#define FILE_VERSION            1,0,80,20
#define PRODUCT_VERSION         "1.0.80.20"

// Prevent races between open() and disconnect()
static DEFINE_MUTEX(disconnect_mutex);

static void duo_bulk_complete(struct urb *urb)
{
    struct usb_duo *dev = urb->context;
    int err, i;
    void *usbBufPtr;

    if (!test_bit(DUO_BULK_STREAMING, &dev->flags))
        return;

    if (urb->status == 0)
    {
        usbBufPtr = urb->transfer_buffer;
        // process data here
        for(i = 0; i < dev->st_params.block_cnt; i++)
        {
            // Beginning of the frame - header signature
            if(!memcmp(usbBufPtr, "ALEXP", 5))
            {
                if(dev->data_offset != dev->st_params.frame_size2)
                {
                    DBG_ERR("Got 'ALEXP' (%d:%d)[%p]", dev->data_offset,
                                                       dev->st_params.frame_size2,
                                                       usbBufPtr);
                }
                dev->pCurrFrame = usbBufPtr;
                dev->data_offset = 0;
            }
            else if(dev->pCurrFrame)
            {
                // move data offset
                dev->data_offset += BULK_IN_BLOCK_SIZE;
                // Is this a complete frame?
                if(dev->data_offset >= dev->st_params.frame_size2)
                {
                    // signal frame complete using dev->frame_event
                    uint32_t offset = (uintptr_t)dev->pCurrFrame - (uintptr_t)dev->frame_buffer;
                    kfifo_in(&dev->frame_fifo, &offset, sizeof(uint32_t));
                    dev->pCurrFrame = NULL;
                    wake_up_interruptible(&dev->frame_event);
                }
            }
            usbBufPtr += BULK_IN_BLOCK_SIZE;
        }
    }
    else
    {
        DBG_ERR("%s urb 0x%p status %d count %d", dev->name, urb, urb->status, urb->actual_length);
    }

    if (!test_bit(DUO_BULK_STREAMING, &dev->flags))
        return;

    usb_anchor_urb(urb, &dev->bulk_anchor);
    err = usb_submit_urb(urb, GFP_ATOMIC);
    if (err < 0)
    {
        DBG_ERR("%s urb %p failed to resubmit (%d)", dev->name, urb, -err);
        usb_unanchor_urb(urb);
    }
}

static void _cleanup_anchored_urbs(struct usb_duo *dev)
{
    struct urb *urb;
    DBG_INFO("_cleanup_anchored_urbs");
    while ((urb = usb_get_from_anchor(&dev->bulk_anchor)))
    {
        DBG_INFO("%s usb_kill_urb(%p)", dev->name, urb);
        usb_kill_urb(urb);
        // Remove streaming DMA mapping
        DBG_INFO("%s dma_unmap_single(%p)", dev->name, (void*)urb->transfer_dma);
        dma_unmap_single(urb->dev->bus->controller, urb->transfer_dma,
                         urb->transfer_buffer_length, DMA_FROM_DEVICE);
        usb_free_urb(urb);
    }
}

static int _duo_submit_bulk_urbs(struct usb_duo *dev)
{
    int ret = 0, i;
    struct urb *urb;
    DBG_INFO("alloc, fill & submit urbs (%d)...", dev->st_params.xfer_cnt);
    for(i = 0; i < dev->st_params.xfer_cnt; i++)
    {
        urb = usb_alloc_urb(0, GFP_KERNEL);
        if (urb == NULL)
        {
            DBG_ERR("%s allocating urb failed", dev->name);
            ret = -ENOMEM;
            goto error;
        }
        usb_fill_bulk_urb(urb, dev->udev, usb_rcvbulkpipe(dev->udev, BULK_IN_EP),
                          dev->frame_buffer + (i * dev->st_params.xfer_size),
                          dev->st_params.xfer_size, duo_bulk_complete, dev);
        // Setup streaming DMA mappings
        urb->transfer_dma = dma_map_single(urb->dev->bus->controller,
                                           urb->transfer_buffer,
                                           urb->transfer_buffer_length, DMA_FROM_DEVICE);
        urb->transfer_flags |= URB_NO_TRANSFER_DMA_MAP;

        usb_anchor_urb(urb, &dev->bulk_anchor);
        ret = usb_submit_urb(urb, GFP_KERNEL);
        if (ret < 0)
        {
            DBG_ERR("%s urb %p submission failed (%d)", dev->name, urb, -ret);
            goto error;
        }
        usb_free_urb(urb);
    }
    DBG_INFO("done");
    return 0;

error:
    _cleanup_anchored_urbs(dev);
    return ret;
}

static int duo_open(struct inode *inode, struct file *file)
{
    struct usb_duo *dev = NULL;
    struct usb_interface *interface;
    int subminor = iminor(inode);
    int retval = 0;

    DBG_INFO("DUO Open");

    mutex_lock(&disconnect_mutex);

    interface = usb_find_interface(&duo_driver, subminor);
    if (!interface)
    {
        DBG_ERR("can't find device for minor %d", subminor);
        retval = -ENODEV;
        goto exit;
    }
    dev = usb_get_intfdata(interface);
    if (!dev)
    {
        retval = -ENODEV;
        goto exit;
    }
    // lock this device
    if (down_interruptible(&dev->sem))
    {
        DBG_ERR("sem down failed");
        retval = -ERESTARTSYS;
        goto exit;
    }
    // Save our object in the file's private structure
    file->private_data = dev;
    up(&dev->sem);

exit:
    mutex_unlock(&disconnect_mutex);
    return retval;
}

static int duo_release(struct inode *inode, struct file *file)
{
    int retval = 0;
    struct usb_duo *dev = file->private_data;

    DBG_INFO("DUO Release");

    if (!dev)
    {
        DBG_ERR("dev is NULL");
        retval =  -ENODEV;
        goto exit;
    }
    // Lock our device
    if (down_interruptible(&dev->sem))
    {
        retval = -ERESTARTSYS;
        goto exit;
    }
    if (!dev->udev)
    {
        DBG_DEBUG("device unplugged before the file was released");
        up(&dev->sem);     // Unlock here as duo_delete frees dev
        duo_delete(dev);
        goto exit;
    }
    if(test_and_clear_bit(DUO_BULK_STREAMING, &dev->flags))
    {
        _cleanup_anchored_urbs(dev);
    }
    up(&dev->sem);

exit:
    return retval;
}

static long duo_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
    int ret = 0;
    struct usb_duo *dev = file->private_data;
    unsigned char *tbuf = NULL;

    switch(cmd)
    {
        case DUO_CONTROL:
        {
            struct duo_ctrltransfer ctrl;
            if (copy_from_user(&ctrl, (const void __user *)arg, sizeof(ctrl)))
                return -EFAULT;
            DBG_INFO("DUO_CONTROL (0x%02X)", ctrl.bRequest);

            if(ctrl.wLength > PAGE_SIZE)
                return -EINVAL;

            tbuf = (unsigned char *)__get_free_page(GFP_KERNEL);
            if (!tbuf)
            {
                ret = -ENOMEM;
                goto done;
            }
            if(ctrl.bRequestType & USB_DIR_IN) // is it read?
            {
                ret = usb_control_msg(dev->udev, usb_rcvctrlpipe(dev->udev, 0),
                                       ctrl.bRequest, ctrl.bRequestType,
                                       ctrl.wValue, ctrl.wIndex,
                                       tbuf, ctrl.wLength, ctrl.timeout);
                if ((ret > 0) && ctrl.wLength)
                    if(copy_to_user(ctrl.data, tbuf, ret))
                    {
                        ret = -EFAULT;
                        goto done;
                    }
            }
            else
            {
                if (ctrl.wLength)
                    if (copy_from_user(tbuf, ctrl.data, ctrl.wLength))
                    {
                        ret = -EFAULT;
                        goto done;
                    }

                ret = usb_control_msg(dev->udev, usb_sndctrlpipe(dev->udev, 0),
                                      ctrl.bRequest, ctrl.bRequestType,
                                      ctrl.wValue, ctrl.wIndex,
                                      tbuf, ctrl.wLength, ctrl.timeout);
            }
        }
        break;
        case DUO_START_STREAMING:
        {
            if (copy_from_user(&dev->st_params, (const void __user *)arg, sizeof(dev->st_params)))
            {
                ret = -EFAULT;
                goto done;
            }
            DBG_INFO("DUO_START_STREAMING (%d bytes)", dev->st_params.frame_size);

            if (!test_and_set_bit(DUO_BULK_STREAMING, &dev->flags))
            {
                DBG_INFO("xfer_size: %d, block_cnt: %d",
                         dev->st_params.xfer_size, dev->st_params.block_cnt);

                DBG_INFO("usb_clear_halt");
                ret = usb_clear_halt(dev->udev, usb_rcvbulkpipe(dev->udev, BULK_IN_EP));
                if (ret < 0)
                {
                    DBG_ERR("%s usb_clear_halt failed (%d)", dev->name, -ret);
                    goto done;
                }
                kfifo_reset(&dev->frame_fifo);
                // submit urbs
                ret = _duo_submit_bulk_urbs(dev);
                if(ret < 0)
                    clear_bit(DUO_BULK_STREAMING, &dev->flags);
            }
        }
        break;
        case DUO_STOP_STREAMING:
        {
            DBG_INFO("DUO_STOP_STREAMING");
            if(test_and_clear_bit(DUO_BULK_STREAMING, &dev->flags))
                _cleanup_anchored_urbs(dev);
        }
        break;
        case DUO_GET_FRAME:
        {
            uint32_t offset;
            DBG_INFO("DUO_GET_FRAME");
            ret = wait_event_interruptible_timeout(dev->frame_event, (kfifo_len(&dev->frame_fifo) >= sizeof(uint32_t)), 200);
            if(ret <= 0)
            {
                ret = -EFAULT;
                goto done;
            }
            ret = kfifo_out(&dev->frame_fifo, &offset, sizeof(uint32_t));
            ret = put_user(offset, (uint32_t *)arg);
            if(ret < 0)
            {
                ret = -EFAULT;
                goto done;
            }
        }
        break;
        case DUO_GET_INFO:
        {
            struct duo_info info = { { FILE_VERSION } };
            info.frame_header_size = FRAME_HEADER_SIZE;
            info.frame_buffer_size = PAGE_ALIGN((unsigned long)FRAME_BUFFER_SIZE);
            DBG_INFO("DUO_GET_INFO");
            ret = copy_to_user((void __user *)arg, &info, sizeof(info));
            if(ret < 0)
            {
                ret = -EFAULT;
                goto done;
            }
        }
        break;
        default:
            ret = -EINVAL;
        break;
    }
done:
    if(tbuf) free_page((unsigned long)tbuf);
    return ret;
}

static int duo_mmap(struct file *file, struct vm_area_struct *vma)
{
    struct usb_duo *dev = file->private_data;
    unsigned long length = vma->vm_end - vma->vm_start;
    DBG_INFO("duo_mmap: length: %ld", length);
    if(dev->frame_buffer == NULL)
    {
        DBG_ERR("dev->frame_buffer == NULL");
        return -EFAULT;
    }
    // check length - do not allow larger mappings than the number of pages allocated
    if (length > dev->frame_buflen)
    {
        DBG_ERR("length > dev->frame_buflen");
        return -EIO;
    }
    if (remap_pfn_range(vma, vma->vm_start,
                        virt_to_phys((void*)dev->frame_buffer) >> PAGE_SHIFT,
                        length, vma->vm_page_prot) < 0)
    {
        DBG_ERR("remap_pfn_range failed");
        return -EAGAIN;
    }
    return 0;
}

static inline void duo_delete(struct usb_duo *dev)
{
    unsigned long i;
    _cleanup_anchored_urbs(dev);
    DBG_INFO("duo_delete");
    if(dev->frame_buffer)
    {
        for (i = 0; i < dev->frame_buflen; i += PAGE_SIZE)
        {
            struct page *page = virt_to_page((unsigned long)dev->frame_buffer + i);
            ClearPageReserved(page);
        }
        DBG_INFO("free_pages");
        free_pages((unsigned long)dev->frame_buffer, get_order(dev->frame_buflen));
        dev->frame_buffer = NULL;
    }
    kfifo_free(&dev->frame_fifo);
    kfree(dev);
}

static char *duo_devnode(struct device *dev, umode_t *mode)
{
    if (mode) *mode = 0666;
    DBG_INFO("duo_devnode: %s", dev_name(dev));
    return kasprintf(GFP_KERNEL, "%s", dev_name(dev));
}

static int duo_probe(struct usb_interface *interface, const struct usb_device_id *id)
{
    struct usb_device *udev = interface_to_usbdev(interface);
    struct usb_duo *dev = NULL;
    unsigned long i;
	int retval = -ENODEV;

    if (!udev)
    {
		DBG_ERR("udev is NULL");
		goto exit;
	}

    DBG_INFO("kzalloc(%ld)", sizeof(struct usb_duo));
    dev = kzalloc(sizeof(struct usb_duo), GFP_KERNEL);
    if (!dev)
    {
        DBG_ERR("cannot allocate memory for struct usb_duo");
		retval = -ENOMEM;
		goto exit;
	}
    dev->udev = udev;
    sema_init(&dev->sem, 1);

    init_usb_anchor(&dev->bulk_anchor);
    init_waitqueue_head(&dev->frame_event);

    DBG_INFO("kfifo_alloc...");
    retval = kfifo_alloc(&dev->frame_fifo, sizeof(uint32_t) * 4, GFP_KERNEL);
    if (retval)
    {
        DBG_ERR("could not allocate memory for frame fifo");
        retval = -ENOMEM;
        goto error;
    }

    // round the frame buffer size to next page size
    dev->frame_buflen = PAGE_ALIGN((unsigned long)FRAME_BUFFER_SIZE);

    // allocate DMA buffer for all frames
    DBG_INFO("__get_dma_pages: %ld bytes", dev->frame_buflen);
    dev->frame_buffer = (__u8*)__get_dma_pages(GFP_KERNEL, get_order(dev->frame_buflen));
    if(dev->frame_buffer == NULL)
    {
        DBG_ERR("could not allocate dma memory for frame buffer");
        retval = -ENOMEM;
        goto error;
    }
    DBG_INFO("__get_dma_pages: 0x%p, size: %ld bytes", dev->frame_buffer, dev->frame_buflen);

    // Mark the pages reserved...
    DBG_INFO("Mark the pages reserved...");
    for (i = 0; i < dev->frame_buflen; i += PAGE_SIZE)
    {
        struct page *page = virt_to_page((unsigned long)dev->frame_buffer + i);
        SetPageReserved(page);
    }

    // Save our data pointer in this interface device
    usb_set_intfdata(interface, dev);

    // We can register the device now, as it is ready
    retval = usb_register_dev(interface, &duo_class);
    if (retval)
    {
		DBG_ERR("not able to get a minor for this device.");
		usb_set_intfdata(interface, NULL);
        goto error;
	}
	dev->minor = interface->minor;
    sprintf(dev->name, "/dev/duo%d", dev->minor);
    DBG_INFO("dev->name = '%s'", dev->name);

    if (usb_set_interface(udev, 0, (FRAME_HEADER_SIZE == 512) ? 0 : 1))
    {
        DBG_ERR("could not set i:a[0,0]");
        usb_set_intfdata(interface, NULL);
        retval = -ENODEV;
        goto error;
    }

    // Retrieve DUO device serial number
    if (!usb_string(udev, udev->descriptor.iSerialNumber, dev->serial, 256))
    {
        DBG_ERR("could not retrieve serial number");
        retval = -ENODEV;
        goto error;
    }
    DBG_INFO("DUO Serial number: '%s'", dev->serial);
    DBG_INFO("DUO Camera is now attached to /dev/duo%d", interface->minor);
exit:
	return retval;

error:
    duo_delete(dev);
	return retval;
}

static void duo_disconnect(struct usb_interface *interface)
{
    struct usb_duo *dev;
    int minor;

    mutex_lock(&disconnect_mutex);	// Not interruptible
	dev = usb_get_intfdata(interface);
	usb_set_intfdata(interface, NULL);
    down(&dev->sem); // Not interruptible
    minor = dev->minor;
    // Give back our minor
    usb_deregister_dev(interface, &duo_class);
    dev->udev = NULL;
    up(&dev->sem);
    duo_delete(dev);
	mutex_unlock(&disconnect_mutex);
    DBG_INFO("DUO Camera /dev/duo%d is now disconnected", minor);
}

static int __init duo_init(void)
{
    int result = usb_register(&duo_driver);
    if (result)
    {
        DBG_ERR("Registering DUO driver failed");
    }
    else
    {
        DBG_INFO("DUO driver registered successfully");
	}
	return result;
}

static void __exit duo_exit(void)
{
    usb_deregister(&duo_driver);
    DBG_INFO("DUO driver de-registered");
}

module_init(duo_init);
module_exit(duo_exit);

MODULE_AUTHOR("Alexander Popovich");
MODULE_VERSION(PRODUCT_VERSION);
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Kernel module for DUO device");
