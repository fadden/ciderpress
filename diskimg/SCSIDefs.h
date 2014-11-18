/*
 * CiderPress
 * Copyright (C) 2007 by faddenSoft, LLC.  All Rights Reserved.
 * See the file LICENSE for distribution terms.
 */
/*
 * Definitions for SCSI (Small Computer System Interface).
 *
 * These structures and defines are passed to the SCSI driver, so they work
 * equally well for ASPI and SPTI
 *
 * Consult the SCSI-2 and MMC-2 specifications for details.
 */
#ifndef DISKIMG_SCSIDEFS_H
#define DISKIMG_SCSIDEFS_H

/*
 * SCSI-2 operation codes.
 */
typedef enum {
    kScsiOpTestUnitReady            = 0x00,
    kScsiOpRezeroUnit               = 0x01,
    kScsiOpRewind                   = 0x01,
    kScsiOpRequestBlockAddr         = 0x02,
    kScsiOpRequestSense             = 0x03,
    kScsiOpFormatUnit               = 0x04,
    kScsiOpReadBlockLimits          = 0x05,
    kScsiOpReassignBlocks           = 0x07,
    kScsiOpRead6                    = 0x08,
    kScsiOpReceive                  = 0x08,
    kScsiOpWrite6                   = 0x0a,
    kScsiOpPrint                    = 0x0a,
    kScsiOpSend                     = 0x0a,
    kScsiOpSeek6                    = 0x0b,
    kScsiOpTrackSelect              = 0x0b,
    kScsiOpSlewPrint                = 0x0b,
    kScsiOpSeekBlock                = 0x0c,
    kScsiOpPartition                = 0x0d,
    kScsiOpReadReverse              = 0x0f,
    kScsiOpWriteFilemarks           = 0x10,
    kScsiOpFlushBuffer              = 0x10,
    kScsiOpSpace                    = 0x11,
    kScsiOpInquiry                  = 0x12,
    kScsiOpVerify6                  = 0x13,
    kScsiOpRecoverBufferedData      = 0x14,
    kScsiOpModeSelect               = 0x15,
    kScsiOpReserveUnit              = 0x16,
    kScsiOpReleaseUnit              = 0x17,
    kScsiOpCopy                     = 0x18,
    kScsiOpErase                    = 0x19,
    kScsiOpModeSense                = 0x1a,
    kScsiOpStartStopUnit            = 0x1b,
    kScsiOpStopPrint                = 0x1b,
    kScsiOpLoadUnload               = 0x1b,
    kScsiOpReceiveDiagnosticResults = 0x1c,
    kScsiOpSendDiagnostic           = 0x1d,
    kScsiOpMediumRemoval            = 0x1e,
    kScsiOpReadFormattedCapacity    = 0x23,
    kScsiOpReadCapacity             = 0x25,
    kScsiOpRead                     = 0x28, // READ(10)
    kScsiOpWrite                    = 0x2a, // WRITE(10)
    kScsiOpSeek                     = 0x2b,
    kScsiOpLocate                   = 0x2b,
    kScsiOpPositionToElement        = 0x2b,
    kScsiOpWriteVerify              = 0x2e,
    kScsiOpVerify                   = 0x2f, // VERIFY(10)
    kScsiOpSearchDataHigh           = 0x30,
    kScsiOpSearchDataEqual          = 0x31,
    kScsiOpSearchDataLow            = 0x32,
    kScsiOpSetLimits                = 0x33,
    kScsiOpReadPosition             = 0x34,
    kScsiOpSynchronizeCache         = 0x35,
    kScsiOpCompare                  = 0x39,
    kScsiOpCopyAndVerify            = 0x3a,
    kScsiOpWriteBuffer              = 0x3b,
    kScsiOpReadBuffer               = 0x3c,
    kScsiOpChangeDefinition         = 0x40,
    kScsiOpReadSubChannel           = 0x42,
    kScsiOpReadTOC                  = 0x43, // READ TOC/PMA/ATIP
    kScsiOpReadHeader               = 0x44,
    kScsiOpPlayAudio                = 0x45,
    kScsiOpPlayAudioMSF             = 0x47,
    kScsiOpPlayTrackIndex           = 0x48,
    kScsiOpPlayTrackRelative        = 0x49,
    kScsiOpPauseResume              = 0x4b,
    kScsiOpLogSelect                = 0x4c,
    kScsiOpLogSense                 = 0x4c,
    kScsiOpStopPlayScan             = 0x4e,
    kScsiOpReadDiscInformation      = 0x51,
    kScsiOpReadTrackInformation     = 0x52,
    kScsiOpSendOPCInformation       = 0x54,
    kScsiOpModeSelect10             = 0x55,
    kScsiOpRepairTrack              = 0x58,
    kScsiOpModeSense10              = 0x5a,
    kScsiOpReportLuns               = 0xa0,
    kScsiOpVerify12                 = 0xa2,
    kScsiOpSendKey                  = 0xa3,
    kScsiOpReportKey                = 0xa4,
    kScsiOpMoveMedium               = 0xa5,
    kScsiOpLoadUnloadSlot           = 0xa6,
    kScsiOpExchangeMedium           = 0xa6,
    kScsiOpSetReadAhead             = 0xa7,
    kScsiOpReadDVDStructure         = 0xad,
    kScsiOpWriteAndVerify           = 0xae,
    kScsiOpRequestVolElement        = 0xb5,
    kScsiOpSendVolumeTag            = 0xb6,
    kScsiOpReadElementStatus        = 0xb8,
    kScsiOpReadCDMSF                = 0xb9,
    kScsiOpScanCD                   = 0xba,
    kScsiOpSetCDSpeed               = 0xbb,
    kScsiOpPlayCD                   = 0xbc,
    kScsiOpMechanismStatus          = 0xbd,
    kScsiOpReadCD                   = 0xbe,
    kScsiOpInitElementRange         = 0xe7,
} SCSIOperationCode;


/*
 * SCSI status codes.
 */
typedef enum {
    kScsiStatGood                   = 0x00,
    kScsiStatCheckCondition         = 0x02,
    kScsiStatConditionMet           = 0x04,
    kScsiStatBusy                   = 0x08,
    kScsiStatIntermediate           = 0x10,
    kScsiStatIntermediateCondMet    = 0x14,
    kScsiStatReservationConflict    = 0x18,
    kScsiStatCommandTerminated      = 0x22,
    kScsiStatQueueFull              = 0x28,
} SCSIStatus;

/*
 * SCSI sense codes.
 */
typedef enum {
    kScsiSenseNoSense               = 0x00,
    kScsiSenseRecoveredError        = 0x01,
    kScsiSenseNotReady              = 0x02,
    kScsiSenseMediumError           = 0x03,
    kScsiSenseHardwareError         = 0x04,
    kScsiSenseIllegalRequest        = 0x05,
    kScsiSenseUnitAttention         = 0x06,
    kScsiSenseDataProtect           = 0x07,
    kScsiSenseBlankCheck            = 0x08,
    kScsiSenseUnqiue                = 0x09,
    kScsiSenseCopyAborted           = 0x0a,
    kScsiSenseAbortedCommand        = 0x0b,
    kScsiSenseEqual                 = 0x0c,
    kScsiSenseVolOverflow           = 0x0d,
    kScsiSenseMiscompare            = 0x0e,
    kScsiSenseReserved              = 0x0f,
} SCSISenseCode;


/*
 * SCSI additional sense codes.
 */
typedef enum {
    kScsiAdSenseNoSense             = 0x00,
    kScsiAdSenseInvalidMedia        = 0x30,
    kScsiAdSenseNoMediaInDevice     = 0x3a,
} SCSIAdSenseCode;

/*
 * SCSI device types.
 */
typedef enum {
    kScsiDevTypeDASD                = 0x00,     // Disk Device
    kScsiDevTypeSEQD                = 0x01,     // Tape Device
    kScsiDevTypePRNT                = 0x02,     // Printer
    kScsiDevTypePROC                = 0x03,     // Processor
    kScsiDevTypeWORM                = 0x04,     // Write-once read-multiple
    kScsiDevTypeCDROM               = 0x05,     // CD-ROM device
    kScsiDevTypeSCAN                = 0x06,     // Scanner device
    kScsiDevTypeOPTI                = 0x07,     // Optical memory device
    kScsiDevTypeJUKE                = 0x08,     // Medium Changer device
    kScsiDevTypeCOMM                = 0x09,     // Communications device
    kScsiDevTypeRESL                = 0x0a,     // Reserved (low)
    kScsiDevTypeRESH                = 0x1e,     // Reserved (high)
    kScsiDevTypeUNKNOWN             = 0x1f,     // Unknown or no device type
} SCSIDeviceType;

/*
 * Generic 6-byte request block.
 */
typedef struct CDB6 {
    unsigned char   operationCode;
    unsigned char   immediate : 1;
    unsigned char   commandUniqueBits : 4;
    unsigned char   logicalUnitNumber : 3;
    unsigned char   commandUniqueBytes[3];
    unsigned char   link : 1;
    unsigned char   flag : 1;
    unsigned char   reserved : 4;
    unsigned char   vendorUnique : 2;
} CDB6;

/*
 * Generic 10-byte request block.
 *
 * Use for READ(10), READ CAPACITY.
 */
typedef struct CDB10 {
    unsigned char   operationCode;
    unsigned char   relativeAddress : 1;
    unsigned char   reserved1 : 2;
    unsigned char   forceUnitAccess : 1;
    unsigned char   disablePageOut : 1;
    unsigned char   logicalUnitNumber : 3;
    unsigned char   logicalBlockAddr0;  // MSB
    unsigned char   logicalBlockAddr1;
    unsigned char   logicalBlockAddr2;
    unsigned char   logicalBlockAddr3;  // LSB
    unsigned char   reserved2;
    unsigned char   transferLength0;    // MSB
    unsigned char   transferLength1;    // LSB
    unsigned char   control;
} CDB10;

/*
 * INQUIRY request block.
 */
typedef struct CDB6Inquiry {
    unsigned char   operationCode;
    unsigned char   EVPD : 1;
    unsigned char   reserved1 : 4;
    unsigned char   logicalUnitNumber : 3;
    unsigned char   pageCode;
    unsigned char   reserved2;
    unsigned char   allocationLength;
    unsigned char   control;
} CDB6Inquiry;

/*
 * Sense data (ASPI SenseArea).
 */
typedef struct CDB_SenseData {
    unsigned char   errorCode:7;
    unsigned char   valid:1;
    unsigned char   segmentNumber;
    unsigned char   senseKey:4;
    unsigned char   reserved:1;
    unsigned char   incorrectLength:1;
    unsigned char   endOfMedia:1;
    unsigned char   fileMark:1;
    unsigned char   information[4];
    unsigned char   additionalSenseLength;
    unsigned char   commandSpecificInformation[4];
    unsigned char   additionalSenseCode;            // ASC
    unsigned char   additionalSenseCodeQualifier;   // ASCQ
    unsigned char   fieldReplaceableUnitCode;
    unsigned char   senseKeySpecific[3];
} CDB_SenseData;

/*
 * Default sense buffer size.
 */
#define kSenseBufferSize 18


//#define INQUIRYDATABUFFERSIZE 36

/*
 * Result from INQUIRY.
 */
typedef struct CDB_InquiryData {
    unsigned char   deviceType : 5;
    unsigned char   deviceTypeQualifier : 3;
    unsigned char   deviceTypeModifier : 7;
    unsigned char   removableMedia : 1;
    unsigned char   versions;
    unsigned char   responseDataFormat : 4;
    unsigned char   reserved1 : 2;
    unsigned char   trmIOP : 1;
    unsigned char   AENC : 1;
    unsigned char   additionalLength;
    unsigned char   reserved2[2];
    unsigned char   softReset : 1;
    unsigned char   commandQueue : 1;
    unsigned char   reserved3 : 1;
    unsigned char   linkedCommands : 1;
    unsigned char   synchronous : 1;
    unsigned char   wide16Bit : 1;
    unsigned char   wide32Bit : 1;
    unsigned char   relativeAddressing : 1;
    unsigned char   vendorId[8];
    unsigned char   productId[16];
    unsigned char   productRevisionLevel[4];
    unsigned char   vendorSpecific[20];
    unsigned char   reserved4[40];
} CDB_InquiryData;

/*
 * Result from READ CAPACITY.
 */
typedef struct CDB_ReadCapacityData {
    unsigned char   logicalBlockAddr0;  // MSB
    unsigned char   logicalBlockAddr1;
    unsigned char   logicalBlockAddr2;
    unsigned char   logicalBlockAddr3;  // LSB
    unsigned char   bytesPerBlock0;     // MSB
    unsigned char   bytesPerBlock1;
    unsigned char   bytesPerBlock2;
    unsigned char   bytesPerBlock3;     // LSB
} CDB_ReadCapacityData;

#endif /*DISKIMG_SCSIDEFS_H*/
