/*
 * CiderPress
 * Copyright (C) 2007 by faddenSoft, LLC.  All Rights Reserved.
 * See the file LICENSE for distribution terms.
 */
/*
 * ASPI (Advanced SCSI Programming Interface) definitions.
 *
 * This may be included directly by an application.  It must not be necessary
 * to include the lower-level headers, e.g. wnaspi32.h.
 *
 * TODO: this was only necessary for older versions of Windows, e.g. Win98,
 * as a way to access SCSI drives.  It's no longer needed.
 */
#ifndef __ASPI__
#define __ASPI__

#if !defined(_WIN32) || !defined(WANT_ASPI)
/*
 * Placeholder definition to keep Linux build happy.
 */
namespace DiskImgLib {
    class DISKIMG_API ASPI {
    public:
        ASPI(void) {}
        virtual ~ASPI(void) {}
    };
};

#else



#ifndef __WNASPI32_H__
struct SRB_ExecSCSICmd;     // fwd
#endif

namespace DiskImgLib {

/*
 * Descriptor for one SCSI device.
 */
class DISKIMG_API ASPIDevice {
public:
    ASPIDevice(void) : fVendorID(NULL), fProductID(NULL),
        fAdapter(0xff), fTarget(0xff), fLun(0xff), fDeviceReady(false)
        {}
    virtual ~ASPIDevice(void) {
        delete[] fVendorID;
        delete[] fProductID;
    }

    void Init(unsigned char adapter, unsigned char target, unsigned char lun,
        const unsigned char* vendor, unsigned const char* product,
        int deviceType, bool ready)
    {
        fAdapter = adapter;
        fTarget = target;
        fLun = lun;
        assert(fVendorID == NULL);
        fVendorID = new char[strlen((const char*)vendor)+1];
        strcpy(fVendorID, (const char*)vendor);
        assert(fProductID == NULL);
        fProductID = new char[strlen((const char*)product)+1];
        strcpy(fProductID, (const char*)product);
        fDeviceReady = ready;
        fDeviceType = deviceType;
    }

    enum {
        kTypeDASD = 0,      // kScsiDevTypeDASD
        kTypeCDROM = 5,     // kScsiDevTypeCDROM
    };

    unsigned char GetAdapter(void) const { return fAdapter; }
    unsigned char GetTarget(void) const { return fTarget; }
    unsigned char GetLun(void) const { return fLun; }
    const char* GetVendorID(void) const { return fVendorID; }
    const char* GetProductID(void) const { return fProductID; }
    bool GetDeviceReady(void) const { return fDeviceReady; }
    int GetDeviceType(void) const { return fDeviceType; }

private:
    // strings from SCSI inquiry, padded with spaces at end
    char*       fVendorID;
    char*       fProductID;

    unsigned char   fAdapter;   // physical or logical host adapter (0-15)
    unsigned char   fTarget;    // SCSI ID on adapter (0-15)
    unsigned char   fLun;       // logical unit (0-7)

    int         fDeviceType;    // e.g. kScsiDevTypeCDROM
    bool        fDeviceReady;
};

/*
 * There should be only one instance of this in the library (part of the
 * DiskImgLib Globals).  It wouldn't actually break anything to have more than
 * one, but there's no need for it.
 */
class DISKIMG_API ASPI {
public:
    ASPI(void) :
        fhASPI(NULL),
        GetASPI32SupportInfo(NULL),
        SendASPI32Command(NULL),
        GetASPI32DLLVersion(NULL),
        fASPIVersion(0),
        fHostAdapterCount(-1)
        {}
    virtual ~ASPI(void);

    // load ASPI DLL if it exists
    DIError Init(void);

    // return the version returned by the loaded DLL
    DWORD GetVersion(void) const {
        assert(fhASPI != NULL);
        return fASPIVersion;
    }

    // Return an *array* of ASPIDevice structures for drives in system;
    //  the caller is expected to delete[] it when done.
    enum { kDevMaskCDROM = 0x01, kDevMaskHardDrive = 0x02 };
    DIError GetAccessibleDevices(int deviceMask, ASPIDevice** ppDeviceArray,
        int* pNumDevices);

    // Get the type of the device (DTYPE_*, 0x00-0x1f) using ASPI query
    DIError GetDeviceType(unsigned char adapter, unsigned char target,
        unsigned char lun, unsigned char* pType);
    // convert DTYPE_* to a string
    const char* DeviceTypeToString(unsigned char deviceType);

    // Get the capacity, expressed as the highest-available LBA and the device
    //  block size.
    DIError GetDeviceCapacity(unsigned char adapter, unsigned char target,
        unsigned char lun, unsigned long* pLastBlock, unsigned long* pBlockSize);

    // Read blocks from the device.
    DIError ReadBlocks(unsigned char adapter, unsigned char target,
        unsigned char lun, long startBlock, short numBlocks, long blockSize,
        void* buf);

    // Write blocks to the device.
    DIError WriteBlocks(unsigned char adapter, unsigned char target,
        unsigned char lun, long startBlock, short numBlocks, long blockSize,
        const void* buf);

private:
    /*
     * The interesting bits that come out of an SC_HA_INQUIRY request.
     */
    typedef struct AdapterInfo {
        unsigned char   adapterScsiID;      // SCSI ID of the adapter itself
        unsigned char   managerID[16+1];    // string describing manager
        unsigned char   identifier[16+1];   // string describing host adapter
        unsigned char   maxTargets;         // max #of targets on this adapter
        unsigned short  bufferAlignment;    // buffer alignment requirement
    } AdapterInfo;
    /*
     * The interesting bits from a SCSI device INQUIRY command.
     */
    typedef struct Inquiry {
        unsigned char   vendorID[8+1];      // vendor ID string
        unsigned char   productID[16+1];    // product ID string
        unsigned char   productRevision[4]; // product revision bytes
    } Inquiry;

    // Issue an ASPI adapter inquiry request.
    DIError HostAdapterInquiry(unsigned char adapter,
        AdapterInfo* pAdapterInfo);
    // Issue a SCSI device inquiry request.
    DIError DeviceInquiry(unsigned char adapter, unsigned char target,
        unsigned char lun, Inquiry* pInquiry);
    // Issue a SCSI test unit ready request.
    DIError TestUnitReady(unsigned char adapter, unsigned char target,
        unsigned char lun, bool* pReady);
    // execute a SCSI command
    DIError ExecSCSICommand(SRB_ExecSCSICmd* pSRB);


    enum {
        kMaxAdapters = 16,
        kMaxTargets = 16,
        kMaxLuns = 8,
        kTimeout = 30,          // timeout, in seconds
        kMaxAccessibleDrives = 16,
    };

    HMODULE fhASPI;
    DWORD   (*GetASPI32SupportInfo)(void);
    DWORD   (*SendASPI32Command)(void* lpsrb);
    DWORD   (*GetASPI32DLLVersion)(void);
    DWORD   fASPIVersion;
    int     fHostAdapterCount;
};

}; // namespace DiskImgLib

#endif /*__ASPI__*/

#endif /*_WIN32*/
