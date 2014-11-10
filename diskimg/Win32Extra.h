/*
 * CiderPress
 * Copyright (C) 2007 by faddenSoft, LLC.  All Rights Reserved.
 * See the file LICENSE for distribution terms.
 */
/*
 * Visual C++ 6.0 doesn't have this definition, because it wasn't added until
 * WinXP.
 *
 * (Do we want IOCTL_DISK_GET_DRIVE_LAYOUT_EX too?)
 */
#ifndef DISKIMG_WIN32EXTRA_H
#define DISKIMG_WIN32EXTRA_H

#include <winioctl.h>   // base definitions

#ifndef IOCTL_DISK_GET_DRIVE_GEOMETRY_EX

/*
BOOL DeviceIoControl(
  (HANDLE) hDevice,                 // handle to volume
  IOCTL_DISK_GET_DRIVE_GEOMETRY_EX, // dwIoControlCode
  NULL,                             // lpInBuffer
  0,                                // nInBufferSize
  (LPVOID) lpOutBuffer,             // output buffer
  (DWORD) nOutBufferSize,           // size of output buffer
  (LPDWORD) lpBytesReturned,        // number of bytes returned
  (LPOVERLAPPED) lpOverlapped       // OVERLAPPED structure
);
*/

#define IOCTL_DISK_GET_DRIVE_GEOMETRY_EX    \
  CTL_CODE(IOCTL_DISK_BASE, 0x0028, METHOD_BUFFERED, FILE_ANY_ACCESS)

typedef struct _DISK_GEOMETRY_EX {
        DISK_GEOMETRY Geometry;
        LARGE_INTEGER DiskSize;
        BYTE Data[1];
} DISK_GEOMETRY_EX, *PDISK_GEOMETRY_EX;

#if 0
typedef struct _DISK_DETECTION_INFO {
        DWORD SizeOfDetectInfo;
        DETECTION_TYPE DetectionType;
        union {
            struct {
                DISK_INT13_INFO Int13;
                DISK_EX_INT13_INFO ExInt13;
            };
        };
} DISK_DETECTION_INFO, *PDISK_DETECTION_INFO;

PDISK_DETECTION_INFO DiskGeometryGetDetect(PDISK_GEOMETRY_EX Geometry);
PDISK_PARTITION_INFO DiskGeometryGetPartition(PDISK_GEOMETRY_EX Geometry);
#endif


#endif /*IOCTL_DISK_GET_DRIVE_GEOMETRY_EX*/

#endif /*DISKIMG_WIN32EXTRA_H*/
