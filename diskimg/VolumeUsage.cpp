/*
 * CiderPress
 * Copyright (C) 2007 by faddenSoft, LLC.  All Rights Reserved.
 * See the file LICENSE for distribution terms.
 */
/*
 * Support for the VolumeUsage sub-class in DiskFS.
 */
#include "StdAfx.h"
#include "DiskImgPriv.h"


/*
 * Initialize structures for a block-structured disk.
 */
DIError DiskFS::VolumeUsage::Create(long numBlocks)
{
    if (numBlocks <= 0 || numBlocks > 32*1024*1024)     // 16GB
        return kDIErrInvalidArg;

    fByBlocks = true;
    fNumSectors = -1;
    fTotalChunks = numBlocks;
    fListSize = numBlocks;
    fList = new unsigned char[fListSize];
    if (fList == NULL)
        return kDIErrMalloc;

    memset(fList, 0, fListSize);

    return kDIErrNone;
}

/*
 * Initialize structures for a track/sector-structured disk.
 */
DIError DiskFS::VolumeUsage::Create(long numTracks, long numSectors)
{
    long count = numTracks * numSectors;
    if (numTracks <= 0 || count <= 0 || count > 32*1024*1024)
        return kDIErrInvalidArg;

    fByBlocks = false;
    fNumSectors = numSectors;
    fTotalChunks = count;
    fListSize = count;
    fList = new unsigned char[fListSize];
    if (fList == NULL)
        return kDIErrMalloc;

    memset(fList, 0, fListSize);

    return kDIErrNone;
}

/*
 * Return the state of a particular chunk.
 */
DIError DiskFS::VolumeUsage::GetChunkState(long block, ChunkState* pState) const
{
    if (!fByBlocks)
        return kDIErrInvalidArg;
    return GetChunkStateIdx(block, pState);
}

DIError DiskFS::VolumeUsage::GetChunkState(long track, long sector,
    ChunkState* pState) const
{
    if (fByBlocks)
        return kDIErrInvalidArg;
    if (track < 0 || sector < 0 || sector >= fNumSectors)
        return kDIErrInvalidArg;
    return GetChunkStateIdx(track * fNumSectors + sector, pState);
}

DIError DiskFS::VolumeUsage::GetChunkStateIdx(int idx, ChunkState* pState) const
{
    if (fList == NULL || idx < 0 || idx >= fListSize) {
        assert(false);
        return kDIErrInvalidArg;
    }

    unsigned char val = fList[idx];
    pState->isUsed = (val & kChunkUsedFlag) != 0;
    pState->isMarkedUsed = (val & kChunkMarkedUsedFlag) != 0;
    pState->purpose = (ChunkPurpose)(val & kChunkPurposeMask);

    return kDIErrNone;
}

/*
 * Set the state of a particular chunk.
 */
DIError DiskFS::VolumeUsage::SetChunkState(long block, const ChunkState* pState)
{
    if (!fByBlocks)
        return kDIErrInvalidArg;
    return SetChunkStateIdx(block, pState);
}

DIError DiskFS::VolumeUsage::SetChunkState(long track, long sector,
    const ChunkState* pState)
{
    if (fByBlocks)
        return kDIErrInvalidArg;
    if (track < 0 || sector < 0 || sector >= fNumSectors)
        return kDIErrInvalidArg;
    return SetChunkStateIdx(track * fNumSectors + sector, pState);
}

DIError DiskFS::VolumeUsage::SetChunkStateIdx(int idx, const ChunkState* pState)
{
    if (fList == NULL || idx < 0 || idx >= fListSize) {
        assert(false);
        return kDIErrInvalidArg;
    }

    unsigned char val = 0;
    if (pState->isUsed) {
        if ((pState->purpose & ~kChunkPurposeMask) != 0) {
            assert(false);
            return kDIErrInvalidArg;
        }
        val |= kChunkUsedFlag;
        val |= (int)pState->purpose;
    }
    if (pState->isMarkedUsed)
        val |= kChunkMarkedUsedFlag;

    fList[idx] = val;

    return kDIErrNone;
}

/*
 * Count up the #of free chunks.
 */
long DiskFS::VolumeUsage::GetActualFreeChunks(void) const
{
    ChunkState cstate;  // could probably do this bitwise...
    int freeCount = 0;
    int funkyCount = 0;

    for (int i = 0; i < fTotalChunks; i++) {
        if (GetChunkStateIdx(i, &cstate) != kDIErrNone) {
            assert(false);
            return -1;
        }

        if (!cstate.isUsed && !cstate.isMarkedUsed)
            freeCount++;

        if ((!cstate.isUsed && cstate.isMarkedUsed) ||
            (cstate.isUsed && !cstate.isMarkedUsed) ||
            (cstate.isUsed && cstate.purpose == kChunkPurposeConflict))
        {
            funkyCount++;
        }
    }

    LOGI(" VU total=%ld free=%d funky=%d",
        fTotalChunks, freeCount, funkyCount);

    return freeCount;
}

/*
 * Convert a ChunkState into a single, hopefully meaningful, character.
 *
 * Possible states:
 *  '.' - !inuse, !marked (free space)
 *  'X' - !inuse, marked (could be embedded volume)
 *  '!' - inuse, !marked (danger!)
 *  '#' - inuse, marked, used by more than one thing
 *  'S' - inuse, marked, used by system (directories, volume bit map)
 *  'I' - inuse, marked, used by file structure (index block)
 *  'F' - inuse, marked, used by file
 */
char DiskFS::VolumeUsage::StateToChar(ChunkState* pState) const
{
    if (!pState->isUsed && !pState->isMarkedUsed)
        return '.';
    if (!pState->isUsed && pState->isMarkedUsed)
        return 'X';
    if (pState->isUsed && !pState->isMarkedUsed)
        return '!';
    assert(pState->isUsed && pState->isMarkedUsed);
    if (pState->purpose == kChunkPurposeUnknown)
        return '?';
    if (pState->purpose == kChunkPurposeConflict)
        return '#';
    if (pState->purpose == kChunkPurposeSystem)
        return 'S';
    if (pState->purpose == kChunkPurposeVolumeDir)
        return 'V';
    if (pState->purpose == kChunkPurposeSubdir)
        return 'D';
    if (pState->purpose == kChunkPurposeUserData)
        return 'F';
    if (pState->purpose == kChunkPurposeFileStruct)
        return 'I';
    if (pState->purpose == kChunkPurposeEmbedded)
        return 'E';

    assert(false);
    return '?';
}

/*
 * Dump the list.
 */
void DiskFS::VolumeUsage::Dump(void) const
{
#define kMapInit "--------------------------------"
    if (fList == NULL) {
        LOGI(" VU asked to dump empty list?");
        return;
    }

    LOGI(" VU VolumeUsage dump (%ld free chunks):",
        GetActualFreeChunks());

    if (fByBlocks) {
        ChunkState cstate;
        char freemap[32+1] = kMapInit;
        int block;
        const int kEntriesPerLine = 32;     // use 20 to match Copy][+

        for (block = 0; block < fTotalChunks; block++) {
            if (GetChunkState(block, &cstate) != kDIErrNone) {
                assert(false);
                return;
            }

            freemap[block % kEntriesPerLine] = StateToChar(&cstate);
            if ((block % kEntriesPerLine) == kEntriesPerLine-1) {
                LOGI("   0x%04x: %s", block-(kEntriesPerLine-1), freemap);
            }
        }
        if ((block % kEntriesPerLine) != 0) {
            memset(freemap + (block % kEntriesPerLine), '-',
                kEntriesPerLine - (block % kEntriesPerLine));
            LOGI("   0x%04x: %s", block-(kEntriesPerLine-1), freemap);
        }
    } else {
        ChunkState cstate;
        char freemap[32+1] = kMapInit;
        long numTracks = fTotalChunks / fNumSectors;
        int track, sector;

        if (fNumSectors > 32) {
            LOGI(" VU too many sectors (%ld)", fNumSectors);
            return;
        }

        LOGI("   map 0123456789abcdef");

        for (track = 0; track < numTracks; track++) {
            for (sector = 0; sector < fNumSectors; sector++) {
                if (GetChunkState(track, sector, &cstate) != kDIErrNone) {
                    assert(false);
                    return;
                }
                freemap[sector] = StateToChar(&cstate);
            }
            LOGI("   %2d: %s", track, freemap);
        }
    }
}
