/*
 * CiderPress
 * Copyright (C) 2007 by faddenSoft, LLC.  All Rights Reserved.
 * See the file LICENSE for distribution terms.
 */
/*
 * ASPI I/O functions.
 *
 * Some notes on ASPI stuff:
 *  - The Nero ASPI provides an interface for IDE hard drives.  It also
 *    throws in a couple of mystery devices on a host adapter at the end.
 *    It has "unknown" device type and doesn't respond to SCSI device
 *    inquiries, so it's easy to ignore.
 *  - The Win98 generic ASPI only finds CD-ROM drives on the IDE bus.
 */
#include "StdAfx.h"
#if defined(_WIN32) && defined (WANT_ASPI)

#include "DiskImgPriv.h"
#include "SCSIDefs.h"
#include "CP_wnaspi32.h"
#include "ASPI.h"


/*
 * Initialize ASPI.
 */
DIError ASPI::Init(void)
{
    DWORD aspiStatus;
    static const char* kASPIDllName = "wnaspi32.dll";

    /*
     * Try to load the DLL.
     */
    fhASPI = ::LoadLibrary(kASPIDllName);
    if (fhASPI == NULL) {
        DWORD lastErr = ::GetLastError();
        if (lastErr == ERROR_MOD_NOT_FOUND) {
            LOGI("ASPI DLL '%s' not found", kASPIDllName);
        } else {
            LOGI("ASPI LoadLibrary(%s) failed (err=%ld)",
                kASPIDllName, GetLastError());
        }
        return kDIErrGeneric;
    }

    GetASPI32SupportInfo = (DWORD(*)(void))::GetProcAddress(fhASPI, "GetASPI32SupportInfo");
    SendASPI32Command = (DWORD(*)(LPSRB))::GetProcAddress(fhASPI, "SendASPI32Command");
    GetASPI32DLLVersion = (DWORD(*)(void))::GetProcAddress(fhASPI, "GetASPI32DLLVersion");
    if (GetASPI32SupportInfo == NULL || SendASPI32Command == NULL) {
        LOGI("ASPI functions not found in dll");
        ::FreeLibrary(fhASPI);
        fhASPI = NULL;
        return kDIErrGeneric;
    }

    if (GetASPI32DLLVersion != NULL) {
        fASPIVersion = GetASPI32DLLVersion();
        LOGI(" ASPI version is %d.%d.%d.%d",
            fASPIVersion & 0x0ff,
            (fASPIVersion >> 8) & 0xff,
            (fASPIVersion >> 16) & 0xff,
            (fASPIVersion >> 24) & 0xff);
    } else {
        LOGI("ASPI WARNING: couldn't find GetASPI32DLLVersion interface");
    }

    /*
     * Successfully loaded the library.  Start it up and see if it works.
     */
    aspiStatus = GetASPI32SupportInfo();
    if (HIBYTE(LOWORD(aspiStatus)) != SS_COMP) {
        LOGI("ASPI loaded but not working (status=%d)",
            HIBYTE(LOWORD(aspiStatus)));
        ::FreeLibrary(fhASPI);
        fhASPI = NULL;
        return kDIErrASPIFailure;
    }

    fHostAdapterCount = LOBYTE(LOWORD(aspiStatus));
    LOGI("ASPI loaded successfully, hostAdapterCount=%d",
        fHostAdapterCount);

    return kDIErrNone;
}

/*
 * Destructor.  Unload the ASPI DLL.
 */
ASPI::~ASPI(void)
{
    if (fhASPI != NULL) {
        LOGI("Unloading ASPI DLL");
        ::FreeLibrary(fhASPI);
        fhASPI = NULL;
    }
}


/*
 * Issue an ASPI host adapter inquiry request for the specified adapter.
 *
 * Pass in a pointer to a struct that receives the result.
 */
DIError ASPI::HostAdapterInquiry(unsigned char adapter, AdapterInfo* pAdapterInfo)
{
    SRB_HAInquiry req;
    DWORD result;

    assert(adapter >= 0 && adapter < kMaxAdapters);

    memset(&req, 0, sizeof(req));
    req.SRB_Cmd = SC_HA_INQUIRY;
    req.SRB_HaId = adapter;

    result = SendASPI32Command(&req);
    if (result != SS_COMP) {
        LOGI("ASPI(SC_HA_INQUIRY on %d) failed with result=0x%lx",
            adapter, result);
        return kDIErrASPIFailure;
    }

    pAdapterInfo->adapterScsiID = req.HA_SCSI_ID;
    memcpy(pAdapterInfo->managerID, req.HA_ManagerId,
        sizeof(pAdapterInfo->managerID)-1);
    pAdapterInfo->managerID[sizeof(pAdapterInfo->managerID)-1] = '\0';
    memcpy(pAdapterInfo->identifier, req.HA_Identifier,
        sizeof(pAdapterInfo->identifier)-1);
    pAdapterInfo->identifier[sizeof(pAdapterInfo->identifier)-1] = '\0';
    pAdapterInfo->maxTargets = req.HA_Unique[3];
    pAdapterInfo->bufferAlignment =
        (unsigned short) req.HA_Unique[1] << 8 | req.HA_Unique[0];

    return kDIErrNone;
}

/*
 * Issue an ASPI query on device type.
 */
DIError ASPI::GetDeviceType(unsigned char adapter, unsigned char target,
    unsigned char lun, unsigned char* pType)
{
    SRB_GDEVBlock req;
    DWORD result;

    assert(adapter >= 0 && adapter < kMaxAdapters);
    assert(target >= 0 && target < kMaxTargets);
    assert(lun >= 0 && lun < kMaxLuns);
    assert(pType != NULL);

    memset(&req, 0, sizeof(req));
    req.SRB_Cmd = SC_GET_DEV_TYPE;
    req.SRB_HaId = adapter;
    req.SRB_Target = target;
    req.SRB_Lun = lun;

    result = SendASPI32Command(&req);
    if (result != SS_COMP)
        return kDIErrASPIFailure;

    *pType = req.SRB_DeviceType;

    return kDIErrNone;
}

/*
 * Return a printable string for the given device type.
 */
const char* ASPI::DeviceTypeToString(unsigned char deviceType)
{
    switch (deviceType) {
    case kScsiDevTypeDASD:      return "Disk device";
    case kScsiDevTypeSEQD:      return "Tape device";
    case kScsiDevTypePRNT:      return "Printer";
    case kScsiDevTypePROC:      return "Processor";
    case kScsiDevTypeWORM:      return "Write-once read-multiple";
    case kScsiDevTypeCDROM:     return "CD-ROM device";
    case kScsiDevTypeSCAN:      return "Scanner device";
    case kScsiDevTypeOPTI:      return "Optical memory device";
    case kScsiDevTypeJUKE:      return "Medium changer device";
    case kScsiDevTypeCOMM:      return "Communications device";
    case kScsiDevTypeUNKNOWN:   return "Unknown or no device type";
    default:            return "Invalid type";
    }
}

/*
 * Issue a SCSI device inquiry and return the interesting parts.
 */
DIError ASPI::DeviceInquiry(unsigned char adapter, unsigned char target,
    unsigned char lun, Inquiry* pInquiry)
{
    DIError dierr;
    SRB_ExecSCSICmd srb;
    CDB6Inquiry* pCDB;
    unsigned char buf[96];  // enough to hold everything of interest, and more
    CDB_InquiryData* pInqData = (CDB_InquiryData*) buf;

    assert(sizeof(CDB6Inquiry) == 6);

    memset(&srb, 0, sizeof(srb));
    srb.SRB_Cmd = SC_EXEC_SCSI_CMD;
    srb.SRB_HaId = adapter;
    srb.SRB_Target = target;
    srb.SRB_Lun = lun;
    srb.SRB_Flags = SRB_DIR_IN;
    srb.SRB_BufLen = sizeof(buf);
    srb.SRB_BufPointer = buf;
    srb.SRB_SenseLen = SENSE_LEN;
    srb.SRB_CDBLen = sizeof(*pCDB);

    pCDB = (CDB6Inquiry*) srb.CDBByte;
    pCDB->operationCode = kScsiOpInquiry;
    pCDB->allocationLength = sizeof(buf);

    // Don't set pCDB->logicalUnitNumber.  It's only there for SCSI-1
    //  devices.  SCSI-2 uses an IDENTIFY command; I gather ASPI is doing
    //  this for us.

    dierr = ExecSCSICommand(&srb);
    if (dierr != kDIErrNone)
        return dierr;

    memcpy(pInquiry->vendorID, pInqData->vendorId,
        sizeof(pInquiry->vendorID)-1);
    pInquiry->vendorID[sizeof(pInquiry->vendorID)-1] = '\0';
    memcpy(pInquiry->productID, pInqData->productId,
        sizeof(pInquiry->productID)-1);
    pInquiry->productID[sizeof(pInquiry->productID)-1] = '\0';
    pInquiry->productRevision[0] = pInqData->productRevisionLevel[0];
    pInquiry->productRevision[1] = pInqData->productRevisionLevel[1];
    pInquiry->productRevision[2] = pInqData->productRevisionLevel[2];
    pInquiry->productRevision[3] = pInqData->productRevisionLevel[3];

    return kDIErrNone;
}


/*
 * Get the capacity of a SCSI block device.
 */
DIError ASPI::GetDeviceCapacity(unsigned char adapter, unsigned char target,
    unsigned char lun, unsigned long* pLastBlock, unsigned long* pBlockSize)
{
    DIError dierr;
    SRB_ExecSCSICmd srb;
    CDB10* pCDB;
    CDB_ReadCapacityData dataBuf;

    assert(sizeof(dataBuf) == 8);   // READ CAPACITY returns two longs
    assert(sizeof(CDB10) == 10);

    memset(&srb, 0, sizeof(srb));
    srb.SRB_Cmd = SC_EXEC_SCSI_CMD;
    srb.SRB_HaId = adapter;
    srb.SRB_Target = target;
    srb.SRB_Lun = lun;
    srb.SRB_Flags = SRB_DIR_IN;
    srb.SRB_BufLen = sizeof(dataBuf);
    srb.SRB_BufPointer = (unsigned char*)&dataBuf;
    srb.SRB_SenseLen = SENSE_LEN;
    srb.SRB_CDBLen = sizeof(*pCDB);

    pCDB = (CDB10*) srb.CDBByte;
    pCDB->operationCode = kScsiOpReadCapacity;
    // rest of CDB is zero

    dierr = ExecSCSICommand(&srb);
    if (dierr != kDIErrNone)
        return dierr;

    *pLastBlock =
            (unsigned long) dataBuf.logicalBlockAddr0 << 24 |
            (unsigned long) dataBuf.logicalBlockAddr1 << 16 |
            (unsigned long) dataBuf.logicalBlockAddr2 << 8 |
            (unsigned long) dataBuf.logicalBlockAddr3;
    *pBlockSize =
            (unsigned long) dataBuf.bytesPerBlock0 << 24 |
            (unsigned long) dataBuf.bytesPerBlock1 << 16 |
            (unsigned long) dataBuf.bytesPerBlock2 << 8 |
            (unsigned long) dataBuf.bytesPerBlock3;
    return kDIErrNone;
}

/*
 * Test to see if a device is ready.
 *
 * Returns "true" if the device is ready, "false" if not.
 */
DIError ASPI::TestUnitReady(unsigned char adapter, unsigned char target,
    unsigned char lun, bool* pReady)
{
    DIError dierr;
    SRB_ExecSCSICmd srb;
    CDB6* pCDB;

    assert(sizeof(CDB6) == 6);

    memset(&srb, 0, sizeof(srb));
    srb.SRB_Cmd = SC_EXEC_SCSI_CMD;
    srb.SRB_HaId = adapter;
    srb.SRB_Target = target;
    srb.SRB_Lun = lun;
    srb.SRB_Flags = 0; //SRB_DIR_IN;
    srb.SRB_BufLen = 0;
    srb.SRB_BufPointer = NULL;
    srb.SRB_SenseLen = SENSE_LEN;
    srb.SRB_CDBLen = sizeof(*pCDB);

    pCDB = (CDB6*) srb.CDBByte;
    pCDB->operationCode = kScsiOpTestUnitReady;
    // rest of CDB is zero

    dierr = ExecSCSICommand(&srb);
    if (dierr != kDIErrNone) {
        const CDB_SenseData* pSense = (const CDB_SenseData*) srb.SenseArea;

        if (srb.SRB_TargStat == kScsiStatCheckCondition &&
            pSense->senseKey == kScsiSenseNotReady)
        {
            // expect pSense->additionalSenseCode to be
            //  kScsiAdSenseNoMediaInDevice; no need to check it really.
            LOGI(" ASPI TestUnitReady: drive %d:%d:%d is NOT ready",
                adapter, target, lun);
        } else {
            LOGI(" ASPI TestUnitReady failed, status=0x%02x sense=0x%02x ASC=0x%02x",
                srb.SRB_TargStat, pSense->senseKey,
                pSense->additionalSenseCode);
        }
        *pReady = false;
    } else {
        const CDB_SenseData* pSense = (const CDB_SenseData*) srb.SenseArea;
        LOGI(" ASPI TestUnitReady: drive %d:%d:%d is ready",
            adapter, target, lun);
        //LOGI("   status=0x%02x sense=0x%02x ASC=0x%02x",
        //  srb.SRB_TargStat, pSense->senseKey, pSense->additionalSenseCode);
        *pReady = true;
    }

    return kDIErrNone;
}

/*
 * Read one or more blocks from the device.
 *
 * The block size is going to be whatever the device's native size is
 * (possibly modified by extents, but we'll ignore that).  For a CD-ROM
 * this means 2048-byte blocks.
 */
DIError ASPI::ReadBlocks(unsigned char adapter, unsigned char target,
    unsigned char lun, long startBlock, short numBlocks, long blockSize,
    void* buf)
{
    SRB_ExecSCSICmd srb;
    CDB10* pCDB;

    //LOGI(" ASPI ReadBlocks start=%ld num=%d (size=%d)",
    //  startBlock, numBlocks, blockSize);

    assert(sizeof(CDB10) == 10);
    assert(startBlock >= 0);
    assert(numBlocks > 0);
    assert(buf != NULL);

    memset(&srb, 0, sizeof(srb));
    srb.SRB_Cmd = SC_EXEC_SCSI_CMD;
    srb.SRB_HaId = adapter;
    srb.SRB_Target = target;
    srb.SRB_Lun = lun;
    srb.SRB_Flags = SRB_DIR_IN;
    srb.SRB_BufLen = numBlocks * blockSize;
    srb.SRB_BufPointer = (unsigned char*)buf;
    srb.SRB_SenseLen = SENSE_LEN;
    srb.SRB_CDBLen = sizeof(*pCDB);

    pCDB = (CDB10*) srb.CDBByte;
    pCDB->operationCode = kScsiOpRead;
    pCDB->logicalBlockAddr0 = (unsigned char) (startBlock >> 24);   // MSB
    pCDB->logicalBlockAddr1 = (unsigned char) (startBlock >> 16);
    pCDB->logicalBlockAddr2 = (unsigned char) (startBlock >> 8);
    pCDB->logicalBlockAddr3 = (unsigned char) startBlock;           // LSB
    pCDB->transferLength0 = (unsigned char) (numBlocks >> 8);       // MSB
    pCDB->transferLength1 = (unsigned char) numBlocks;              // LSB

    return ExecSCSICommand(&srb);
}

/*
 * Write one or more blocks to the device.
 */
DIError ASPI::WriteBlocks(unsigned char adapter, unsigned char target,
    unsigned char lun, long startBlock, short numBlocks, long blockSize,
    const void* buf)
{
    SRB_ExecSCSICmd srb;
    CDB10* pCDB;

    LOGI(" ASPI WriteBlocks start=%ld num=%d (size=%d)",
        startBlock, numBlocks, blockSize);

    assert(sizeof(CDB10) == 10);
    assert(startBlock >= 0);
    assert(numBlocks > 0);
    assert(buf != NULL);

    memset(&srb, 0, sizeof(srb));
    srb.SRB_Cmd = SC_EXEC_SCSI_CMD;
    srb.SRB_HaId = adapter;
    srb.SRB_Target = target;
    srb.SRB_Lun = lun;
    srb.SRB_Flags = SRB_DIR_IN;
    srb.SRB_BufLen = numBlocks * blockSize;
    srb.SRB_BufPointer = (unsigned char*)buf;
    srb.SRB_SenseLen = SENSE_LEN;
    srb.SRB_CDBLen = sizeof(*pCDB);

    pCDB = (CDB10*) srb.CDBByte;
    pCDB->operationCode = kScsiOpWrite;
    pCDB->logicalBlockAddr0 = (unsigned char) (startBlock >> 24);   // MSB
    pCDB->logicalBlockAddr1 = (unsigned char) (startBlock >> 16);
    pCDB->logicalBlockAddr2 = (unsigned char) (startBlock >> 8);
    pCDB->logicalBlockAddr3 = (unsigned char) startBlock;           // LSB
    pCDB->transferLength0 = (unsigned char) (numBlocks >> 8);       // MSB
    pCDB->transferLength1 = (unsigned char) numBlocks;              // LSB

    return ExecSCSICommand(&srb);
}


/*
 * Execute a SCSI command.
 *
 * Returns an error if ASPI reports an error or the SCSI status isn't
 * kScsiStatGood.
 *
 * The Nero ASPI layer typically returns immediately, and hands back an
 * SS_ERR when something fails.  Win98 ASPI does the SS_PENDING thang.
 */
DIError ASPI::ExecSCSICommand(SRB_ExecSCSICmd* pSRB)
{
    HANDLE completionEvent = NULL;
    DWORD eventStatus;
    DWORD aspiStatus;

    assert(pSRB->SRB_Cmd == SC_EXEC_SCSI_CMD);
    assert(pSRB->SRB_Flags == SRB_DIR_IN ||
           pSRB->SRB_Flags == SRB_DIR_OUT ||
           pSRB->SRB_Flags == 0);

    /*
     * Set up event-waiting stuff, as described in the Adaptec ASPI docs.
     */
    pSRB->SRB_Flags |= SRB_EVENT_NOTIFY;

    completionEvent = ::CreateEvent(NULL, FALSE, FALSE, NULL);
    if (completionEvent == NULL) {
        LOGI("Failed creating a completion event?");
        return kDIErrGeneric;
    }

    pSRB->SRB_PostProc = completionEvent;

    /*
     * Send the request.
     */
    (void)SendASPI32Command((LPSRB) pSRB);
    aspiStatus = pSRB->SRB_Status;
    if (aspiStatus == SS_PENDING) {
        //LOGI("  (waiting for completion)");
        eventStatus = ::WaitForSingleObject(completionEvent, kTimeout * 1000);

        ::CloseHandle(completionEvent);

        if (eventStatus == WAIT_TIMEOUT) {
            LOGI("  ASPI exec timed out!");
            return kDIErrSCSIFailure;
        } else if (eventStatus != WAIT_OBJECT_0) {
            LOGI("  ASPI exec returned weird wait state %ld", eventStatus);
            return kDIErrGeneric;
        }
    }

    /*
     * Check the final status.
     */
    aspiStatus = pSRB->SRB_Status;

    if (aspiStatus == SS_COMP) {
        /* success! */
    } else if (aspiStatus == SS_ERR) {
        const CDB_SenseData* pSense = (const CDB_SenseData*) pSRB->SenseArea;

        LOGI("  ASPI SCSI command 0x%02x failed: scsiStatus=0x%02x"
              " senseKey=0x%02x ASC=0x%02x\n",
            pSRB->CDBByte[0], pSRB->SRB_TargStat,
            pSense->senseKey, pSense->additionalSenseCode);
        return kDIErrSCSIFailure;
    } else {
        // SS_ABORTED, SS_ABORT_FAIL, SS_NO_DEVICE, ...
        LOGI("  ASPI failed on command 0x%02x: aspiStatus=%d scsiStatus=%d",
            pSRB->CDBByte[0], aspiStatus, pSRB->SRB_TargStat);
        return kDIErrASPIFailure;
    }

    return kDIErrNone;
}


/*
 * Return an array of accessible devices we found.
 *
 * Only return the devices matching device types in "deviceMask".
 */
DIError ASPI::GetAccessibleDevices(int deviceMask, ASPIDevice** ppDeviceArray,
    int* pNumDevices)
{
    DIError dierr;
    ASPIDevice* deviceArray = NULL;
    int idx = 0;

    assert(deviceMask != 0);
    assert((deviceMask & ~(kDevMaskCDROM | kDevMaskHardDrive)) == 0);
    assert(ppDeviceArray != NULL);
    assert(pNumDevices != NULL);

    deviceArray = new ASPIDevice[kMaxAccessibleDrives];
    if (deviceArray == NULL)
        return kDIErrMalloc;

    LOGI("ASPI scanning %d host adapters", fHostAdapterCount);

    for (int ha = 0; ha < fHostAdapterCount; ha++) {
        AdapterInfo adi;

        dierr = HostAdapterInquiry(ha, &adi);
        if (dierr != kDIErrNone) {
            LOGI("  ASPI inquiry on %d failed", ha);
            continue;
        }

        LOGI(" ASPI host adapter %d (SCSI ID=%d)", ha, adi.adapterScsiID);
        LOGI("   identifier='%s' managerID='%s'",
            adi.identifier, adi.managerID);
        LOGI("   maxTargets=%d bufferAlignment=%d",
            adi.maxTargets, adi.bufferAlignment);

        int maxTargets = adi.maxTargets;
        if (!maxTargets) {
            /* Win98 ASPI reports zero here for ATAPI */
            maxTargets = 8;
        }
        if (maxTargets > kMaxTargets)
            maxTargets = kMaxTargets;
        for (int targ = 0; targ < maxTargets; targ++) {
            for (int lun = 0; lun < kMaxLuns; lun++) {
                Inquiry inq;
                unsigned char deviceType;
                char addrString[48];
                bool deviceReady;

                dierr = GetDeviceType(ha, targ, lun, &deviceType);
                if (dierr != kDIErrNone)
                    continue;

                sprintf(addrString, "%d:%d:%d", ha, targ, lun);

                dierr = DeviceInquiry(ha, targ, lun, &inq);
                if (dierr != kDIErrNone) {
                    LOGI(" ASPI DeviceInquiry for '%s' (type=%d) failed",
                        addrString, deviceType);
                    continue;
                }

                LOGI("  Device %s is %s '%s' '%s'",
                    addrString, DeviceTypeToString(deviceType),
                    inq.vendorID, inq.productID);

                if ((deviceMask & kDevMaskCDROM) != 0 &&
                    deviceType == kScsiDevTypeCDROM)
                {
                    /* found CD-ROM */
                } else if ((deviceMask & kDevMaskHardDrive) != 0 &&
                    deviceType == kScsiDevTypeDASD)
                {
                    /* found hard drive */
                } else
                    continue;

                if (idx >= kMaxAccessibleDrives) {
                    LOGI("GLITCH: ran out of places to stuff CD-ROM drives");
                    assert(false);
                    goto done;
                }

                dierr = TestUnitReady(ha, targ, lun, &deviceReady);
                if (dierr != kDIErrNone) {
                    LOGI(" ASPI TestUnitReady for '%s' failed", addrString);
                    continue;
                }

                deviceArray[idx].Init(ha, targ, lun, inq.vendorID,
                    inq.productID, deviceType, deviceReady);
                idx++;
            }
        }
    }

done:
    *ppDeviceArray = deviceArray;
    *pNumDevices = idx;
    return kDIErrNone;
}

#endif /*_WIN32*/
