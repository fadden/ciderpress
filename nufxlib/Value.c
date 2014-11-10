/*
 * NuFX archive manipulation library
 * Copyright (C) 2000-2007 by Andy McFadden, All Rights Reserved.
 * This is free software; you can redistribute it and/or modify it under the
 * terms of the BSD License, see the file COPYING-LIB.
 *
 * Get/set certain values and attributes.
 */
#include "NufxLibPriv.h"

#define kMaxJunkSkipMax 8192


/*
 * Get a configurable parameter.
 */
NuError
Nu_GetValue(NuArchive* pArchive, NuValueID ident, NuValue* pValue)
{
    NuError err = kNuErrNone;

    if (pValue == nil)
        return kNuErrInvalidArg;

    switch (ident) {
    case kNuValueAllowDuplicates:
        *pValue = pArchive->valAllowDuplicates;
        break;
    case kNuValueConvertExtractedEOL:
        *pValue = pArchive->valConvertExtractedEOL;
        break;
    case kNuValueDataCompression:
        *pValue = pArchive->valDataCompression;
        break;
    case kNuValueDiscardWrapper:
        *pValue = pArchive->valDiscardWrapper;
        break;
    case kNuValueEOL:
        *pValue = pArchive->valEOL;
        break;
    case kNuValueHandleExisting:
        *pValue = pArchive->valHandleExisting;
        break;
    case kNuValueIgnoreCRC:
        *pValue = pArchive->valIgnoreCRC;
        break;
    case kNuValueMaskDataless:
        *pValue = pArchive->valMaskDataless;
        break;
    case kNuValueMimicSHK:
        *pValue = pArchive->valMimicSHK;
        break;
    case kNuValueModifyOrig:
        *pValue = pArchive->valModifyOrig;
        break;
    case kNuValueOnlyUpdateOlder:
        *pValue = pArchive->valOnlyUpdateOlder;
        break;
    case kNuValueStripHighASCII:
        *pValue = pArchive->valStripHighASCII;
        break;
    case kNuValueJunkSkipMax:
        *pValue = pArchive->valJunkSkipMax;
        break;
    case kNuValueIgnoreLZW2Len:
        *pValue = pArchive->valIgnoreLZW2Len;
        break;
    case kNuValueHandleBadMac:
        *pValue = pArchive->valHandleBadMac;
        break;
    default:
        err = kNuErrInvalidArg;
        Nu_ReportError(NU_BLOB, err, "Unknown ValueID %d requested", ident);
        goto bail;
    }

bail:
    return err;
}


/*
 * Set a configurable parameter.
 */
NuError
Nu_SetValue(NuArchive* pArchive, NuValueID ident, NuValue value)
{
    NuError err = kNuErrInvalidArg;

    switch (ident) {
    case kNuValueAllowDuplicates:
        if (value != true && value != false) {
            Nu_ReportError(NU_BLOB, err,
                "Invalid kNuValueAllowDuplicates value %ld", value);
            goto bail;
        }
        pArchive->valAllowDuplicates = value;
        break;
    case kNuValueConvertExtractedEOL:
        if (value < kNuConvertOff || value > kNuConvertAuto) {
            Nu_ReportError(NU_BLOB, err,
                "Invalid kNuValueConvertExtractedEOL value %ld", value);
            goto bail;
        }
        pArchive->valConvertExtractedEOL = value;
        break;
    case kNuValueDataCompression:
        if (value < kNuCompressNone || value > kNuCompressBzip2) {
            Nu_ReportError(NU_BLOB, err,
                "Invalid kNuValueDataCompression value %ld", value);
            goto bail;
        }
        pArchive->valDataCompression = value;
        break;
    case kNuValueDiscardWrapper:
        if (value != true && value != false) {
            Nu_ReportError(NU_BLOB, err,
                "Invalid kNuValueDiscardWrapper value %ld", value);
            goto bail;
        }
        pArchive->valDiscardWrapper = value;
        break;
    case kNuValueEOL:
        if (value < kNuEOLUnknown || value > kNuEOLCRLF) {
            Nu_ReportError(NU_BLOB, err,
                "Invalid kNuValueEOL value %ld", value);
            goto bail;
        }
        pArchive->valEOL = value;
        break;
    case kNuValueHandleExisting:
        if (value < kNuMaybeOverwrite || value > kNuMustOverwrite) {
            Nu_ReportError(NU_BLOB, err,
                "Invalid kNuValueHandleExisting value %ld", value);
            goto bail;
        }
        pArchive->valHandleExisting = value;
        break;
    case kNuValueIgnoreCRC:
        if (value != true && value != false) {
            Nu_ReportError(NU_BLOB, err,
                "Invalid kNuValueIgnoreCRC value %ld", value);
            goto bail;
        }
        pArchive->valIgnoreCRC = value;
        break;
    case kNuValueMaskDataless:
        if (value != true && value != false) {
            Nu_ReportError(NU_BLOB, err,
                "Invalid kNuValueMaskDataless value %ld", value);
            goto bail;
        }
        pArchive->valMaskDataless = value;
        break;
    case kNuValueMimicSHK:
        if (value != true && value != false) {
            Nu_ReportError(NU_BLOB, err,
                "Invalid kNuValueMimicSHK value %ld", value);
            goto bail;
        }
        pArchive->valMimicSHK = value;
        break;
    case kNuValueModifyOrig:
        if (value != true && value != false) {
            Nu_ReportError(NU_BLOB, err,
                "Invalid kNuValueModifyOrig value %ld", value);
            goto bail;
        }
        pArchive->valModifyOrig = value;
        break;
    case kNuValueOnlyUpdateOlder:
        if (value != true && value != false) {
            Nu_ReportError(NU_BLOB, err,
                "Invalid kNuValueOnlyUpdateOlder value %ld", value);
            goto bail;
        }
        pArchive->valOnlyUpdateOlder = value;
        break;
    case kNuValueStripHighASCII:
        if (value != true && value != false) {
            Nu_ReportError(NU_BLOB, err,
                "Invalid kNuValueStripHighASCII value %ld", value);
            goto bail;
        }
        pArchive->valStripHighASCII = value;
        break;
    case kNuValueJunkSkipMax:
        if (value > kMaxJunkSkipMax) {
            Nu_ReportError(NU_BLOB, err,
                "Invalid kNuValueJunkSkipMax value %ld", value);
            goto bail;
        }
        pArchive->valJunkSkipMax = value;
        break;
    case kNuValueIgnoreLZW2Len:
        if (value != true && value != false) {
            Nu_ReportError(NU_BLOB, err,
                "Invalid kNuValueIgnoreLZW2Len value %ld", value);
            goto bail;
        }
        pArchive->valIgnoreLZW2Len = value;
        break;
    case kNuValueHandleBadMac:
        if (value != true && value != false) {
            Nu_ReportError(NU_BLOB, err,
                "Invalid kNuValueHandleBadMac value %ld", value);
            goto bail;
        }
        pArchive->valHandleBadMac = value;
        break;
    default:
        Nu_ReportError(NU_BLOB, err, "Unknown ValueID %d requested", ident);
        goto bail;
    }

    err = kNuErrNone;

bail:
    return err;
}


/*
 * Get an archive attribute.  These are things that you would have to
 * pry into pArchive to get at (like the archive type) or get the master
 * header (like the number of records).
 */
NuError
Nu_GetAttr(NuArchive* pArchive, NuAttrID ident, NuAttr* pAttr)
{
    NuError err = kNuErrNone;
    if (pAttr == nil)
        return kNuErrInvalidArg;

    switch (ident) {
    case kNuAttrArchiveType:
        *pAttr = pArchive->archiveType;
        break;
    case kNuAttrNumRecords:
        *pAttr = pArchive->masterHeader.mhTotalRecords;
        break;
    case kNuAttrHeaderOffset:
        *pAttr = pArchive->headerOffset;
        break;
    case kNuAttrJunkOffset:
        *pAttr = pArchive->junkOffset;
        break;
    default:
        err = kNuErrInvalidArg;
        Nu_ReportError(NU_BLOB, err, "Unknown AttrID %d requested", ident);
        goto bail;
    }

bail:
    return err;
}

/*
 * Convert a NuValue compression type to a "phyiscal" ThreadFormat.
 *
 * Unsupported compression types cause a warning to be flagged.
 */
NuThreadFormat
Nu_ConvertCompressValToFormat(NuArchive* pArchive, NuValue compValue)
{
    NuThreadFormat threadFormat;
    Boolean unsup = false;

    switch (compValue) {
    case kNuCompressNone:   threadFormat = kNuThreadFormatUncompressed; break;

    #ifdef ENABLE_SQ
    case kNuCompressSQ:     threadFormat = kNuThreadFormatHuffmanSQ;    break;
    #else
    case kNuCompressSQ:     threadFormat = kNuThreadFormatHuffmanSQ;
                            unsup = true;                               break;
    #endif

    #ifdef ENABLE_LZW
    case kNuCompressLZW1:   threadFormat = kNuThreadFormatLZW1;         break;
    case kNuCompressLZW2:   threadFormat = kNuThreadFormatLZW2;         break;
    #else
    case kNuCompressLZW1:   threadFormat = kNuThreadFormatLZW1;
                            unsup = true;                               break;
    case kNuCompressLZW2:   threadFormat = kNuThreadFormatLZW2;
                            unsup = true;                               break;
    #endif

    #ifdef ENABLE_LZC
    case kNuCompressLZC12:  threadFormat = kNuThreadFormatLZC12;        break;
    case kNuCompressLZC16:  threadFormat = kNuThreadFormatLZC16;        break;
    #else
    case kNuCompressLZC12:  threadFormat = kNuThreadFormatLZC12;
                            unsup = true;                               break;
    case kNuCompressLZC16:  threadFormat = kNuThreadFormatLZC16;
                            unsup = true;                               break;
    #endif

    #ifdef ENABLE_DEFLATE
    case kNuCompressDeflate: threadFormat = kNuThreadFormatDeflate;     break;
    #else
    case kNuCompressDeflate: threadFormat = kNuThreadFormatDeflate;
                            unsup = true;                               break;
    #endif

    #ifdef ENABLE_BZIP2
    case kNuCompressBzip2:  threadFormat = kNuThreadFormatBzip2;        break;
    #else
    case kNuCompressBzip2:  threadFormat = kNuThreadFormatBzip2;
                            unsup = true;                               break;
    #endif

    default:
        Nu_ReportError(NU_BLOB, kNuErrInvalidArg,
            "Unknown compress value %ld", compValue);
        Assert(false);
        return kNuThreadFormatUncompressed;
    }

    if (unsup) {
        Nu_ReportError(NU_BLOB, kNuErrNone,
            "Unsupported compression 0x%04x requested (%ld), storing",
            threadFormat, compValue);
        return kNuThreadFormatUncompressed;
    }

    return threadFormat;
}

