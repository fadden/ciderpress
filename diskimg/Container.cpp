/*
 * CiderPress
 * Copyright (C) 2007 by faddenSoft, LLC.  All Rights Reserved.
 * See the file LICENSE for distribution terms.
 */
/*
 * Base "container FS" support.
 */
#include "StdAfx.h"
#include "DiskImgPriv.h"


/*
 * Blank out the volume usage map, setting all entries to "embedded".
 */
void DiskFSContainer::SetVolumeUsageMap(void)
{
    VolumeUsage::ChunkState cstate;
    long block;

    fVolumeUsage.Create(fpImg->GetNumBlocks());

    cstate.isUsed = true;
    cstate.isMarkedUsed = true;
    cstate.purpose = VolumeUsage::kChunkPurposeEmbedded;

    for (block = fpImg->GetNumBlocks()-1; block >= 0; block--)
        fVolumeUsage.SetChunkState(block, &cstate);
}


/*
 * Create a "placeholder" sub-volume.  Useful for some of the tools when
 * dealing with unformatted (or unknown-formatted) partitions.
 */
DIError DiskFSContainer::CreatePlaceholder(long startBlock, long numBlocks,
    const char* partName, const char* partType,
    DiskImg** ppNewImg, DiskFS** ppNewFS)
{
    DIError dierr = kDIErrNone;
    DiskFS* pNewFS = NULL;
    DiskImg* pNewImg = NULL;

    LOGI(" %s/CrPl creating placeholder for %ld +%ld", GetDebugName(),
        startBlock, numBlocks);

    if (startBlock > fpImg->GetNumBlocks()) {
        LOGI(" %s/CrPl start block out of range (%ld vs %ld)",
            GetDebugName(), startBlock, fpImg->GetNumBlocks());
        return kDIErrBadPartition;
    }

    pNewImg = new DiskImg;
    if (pNewImg == NULL) {
        dierr = kDIErrMalloc;
        goto bail;
    }

    if (partName != NULL) {
        if (partType != NULL)
            pNewImg->AddNote(DiskImg::kNoteInfo,
                "Partition name='%s' type='%s'.", partName, partType);
        else
            pNewImg->AddNote(DiskImg::kNoteInfo,
                "Partition name='%s'.", partName);
    }

    dierr = pNewImg->OpenImage(fpImg, startBlock, numBlocks);
    if (dierr != kDIErrNone) {
        LOGI(" %s/CrPl: OpenImage(%ld,%ld) failed (err=%d)",
            GetDebugName(), startBlock, numBlocks, dierr);
        goto bail;
    }

    /*
     * If this slot isn't formatted at all, the call will return with
     * "unknown FS".  If it's formatted enough to pass the initial test,
     * but fails during "Initialize", we'll have a non-unknown value
     * for the FS format.  We need to stomp that.
     */
    dierr = pNewImg->AnalyzeImage();
    if (dierr != kDIErrNone) {
        LOGI(" %s/CrPl: analysis failed (err=%d)", GetDebugName(), dierr);
        goto bail;
    }
    if (pNewImg->GetFSFormat() != DiskImg::kFormatUnknown) {
        dierr = pNewImg->OverrideFormat(pNewImg->GetPhysicalFormat(),
                    DiskImg::kFormatUnknown, pNewImg->GetSectorOrder());
        if (dierr != kDIErrNone) {
            LOGI(" %s/CrPl: unable to override to unknown",
                GetDebugName());
            goto bail;
        }
    }

    /* open a DiskFS for the sub-image, allowing "unknown" */
    pNewFS = pNewImg->OpenAppropriateDiskFS(true);
    if (pNewFS == NULL) {
        LOGI(" %s/CrPl: OpenAppropriateDiskFS failed", GetDebugName());
        dierr = kDIErrUnsupportedFSFmt;
        goto bail;
    }

    /* sets the DiskImg ptr (and very little else) */
    dierr = pNewFS->Initialize(pNewImg, kInitFull);
    if (dierr != kDIErrNone) {
        LOGI(" %s/CrPl: init failed (err=%d)", GetDebugName(), dierr);
        goto bail;
    }

bail:
    if (dierr != kDIErrNone) {
        delete pNewFS;
        delete pNewImg;
    } else {
        *ppNewImg = pNewImg;
        *ppNewFS = pNewFS;
    }
    return dierr;
}
