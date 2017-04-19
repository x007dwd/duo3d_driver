////////////////////////////////////////////////////////////////////////////////////////////////////////
// This file is part of DUO SDK that allows the use of DUO devices in your own applications
// For updates and file downloads go to: http://duo3d.com/...
// Copyright 2014-2016 (c) Code Laboratories, Inc. All rights reserved.
////////////////////////////////////////////////////////////////////////////////////////////////////////
#ifndef DUO_DRIVER_API_H
#define DUO_DRIVER_API_H

#include <linux/ioctl.h>

struct duo_ctrltransfer
{
    uint8_t bRequestType;        // SB_DIR_IN/USB_DIR_OUT | USB_TYPE_VENDOR | USB_RECIP_DEVICE
    uint8_t bRequest;
    uint16_t wValue;
    uint16_t wIndex;
    uint16_t wLength;
    uint32_t timeout;            // milliseconds
    void *data;
};

struct duo_streaming
{
    uint32_t xfer_size;          // single USB xfer size
    uint32_t xfer_cnt;           // number of queued USB transfers
    uint32_t block_cnt;          // number of FRAME_HEADER_SIZE data blocks per single USB xfer
    uint32_t frame_size;         // frame size (w*h)
    uint32_t frame_size2;        // frame size (w*h)*2
};

struct duo_info
{
    uint16_t version[4];          // driver version
    uint32_t frame_header_size;   // frame header size
    uint32_t frame_buffer_size;   // frame buffer size
};

#define DUO_CONTROL             _IOWR('D', 0x00, struct duo_ctrltransfer)
#define DUO_START_STREAMING     _IOW ('D', 0x01, struct duo_streaming)
#define DUO_STOP_STREAMING      _IO  ('D', 0x02)
#define DUO_GET_FRAME           _IOR ('D', 0x03, uint32_t)
#define DUO_GET_INFO            _IOR ('D', 0x04, struct duo_info)

#endif // DUO_DRIVER_API_H
