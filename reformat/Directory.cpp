/*
 * CiderPress
 * Copyright (C) 2007 by faddenSoft, LLC.  All Rights Reserved.
 * See the file LICENSE for distribution terms.
 */
/*
 * Directory dump.
 */
#include "StdAfx.h"
#include "Directory.h"
#include "../diskimg/DiskImgDetail.h"
#include "../app/FileNameConv.h"

using namespace DiskImgLib;

/*
 * Decide whether or not we want to handle this file.
 */
void ReformatDirectory::Examine(ReformatHolder* pHolder)
{
    ReformatHolder::ReformatApplies applies = ReformatHolder::kApplicNot;

    if (pHolder->GetFileType() == kTypeDIR)
        applies = ReformatHolder::kApplicProbably;

    pHolder->SetApplic(ReformatHolder::kReformatProDOSDirectory, applies,
        ReformatHolder::kApplicNot, ReformatHolder::kApplicNot);
}


/*
 * Convert a ProDOS directory into a format resembling BASIC.System's
 * 80-column "catalog" command.
 */
int ReformatDirectory::Process(const ReformatHolder* pHolder,
    ReformatHolder::ReformatID id, ReformatHolder::ReformatPart part,
    ReformatOutput* pOutput)
{
    const uint8_t* srcBuf = pHolder->GetSourceBuf(part);
    long srcLen = pHolder->GetSourceLen(part);
    fUseRTF = false;

    if (srcLen < 512 || (srcLen % 512) != 0) {
        LOGI("ReformatDirectory: invalid len %d", srcLen);
        return -1;
    }

    BufPrintf(" NAME           TYPE  BLOCKS  MODIFIED  "
              "       CREATED          ENDFILE SUBTYPE\r\n\r\n");

    PrintDirEntries(srcBuf, srcLen, false);

    BufPrintf("\r\nDeleted entries:\r\n");
    PrintDirEntries(srcBuf, srcLen, true);

    if ((srcBuf[0x04] & 0xf0) == 0xf0) {
        /* this is the volume directory */
        BufPrintf("\r\nTotal blocks: %d\n", srcBuf[0x29] | srcBuf[0x2a] << 8);
    }

    SetResultBuffer(pOutput);
    return 0;
}

/*
 * Print all of the entries in the directory.
 *
 * The "showDeleted" flag determines whether we show active entries or
 * deleted entries.
 */
void ReformatDirectory::PrintDirEntries(const uint8_t* srcBuf,
    long srcLen, bool showDeleted)
{
    const int kEntriesPerBlock = 0x0d;      // expected value for entries per blk
    const int kEntryLength = 0x27;          // expected value for dir entry len
    const int kMaxFileName = 15;
    const uint8_t* pDirEntry;
    int blockIdx;
    int entryIdx;

    for (blockIdx = 0; blockIdx < srcLen / 512; blockIdx++) {
        pDirEntry = srcBuf + 512*blockIdx + 4;  // skip 4 bytes of prev/next

        for (entryIdx = 0; entryIdx < kEntriesPerBlock;
                                        entryIdx++, pDirEntry += kEntryLength)
        {
            /* skip directory header; should probably check entries_per_block */
            if (blockIdx == 0 && entryIdx == 0)
                continue;

            if ((showDeleted && (pDirEntry[0x00] & 0xf0) == 0) ||
                (!showDeleted && (pDirEntry[0x00] & 0xf0) != 0))
            {
                if (pDirEntry[0x01] == 0)   /* never-used entry */
                    continue;

                int nameLen = pDirEntry[0x00] & 0x0f;
                if (nameLen == 0) {
                    /* scan for it */
                    while (pDirEntry[0x01 + nameLen] != 0 && nameLen < kMaxFileName)
                        nameLen++;
                }

                char fileName[kMaxFileName +1];
                strncpy(fileName, (const char*)&pDirEntry[0x01], kMaxFileName);
                fileName[nameLen] = '\0';

                CString createStrW, modStrW;
                A2FileProDOS::ProDate prodosDateTime;

                prodosDateTime = pDirEntry[0x18] | pDirEntry[0x19] << 8 |
                                pDirEntry[0x1a] << 16 | pDirEntry[0x1b] << 24;
                FormatDate(A2FileProDOS::ConvertProDate(prodosDateTime), &createStrW);
                CStringA createStr(createStrW);
                prodosDateTime = pDirEntry[0x21] | pDirEntry[0x22] << 8 |
                                pDirEntry[0x23] << 16 | pDirEntry[0x24] << 24;
                FormatDate(A2FileProDOS::ConvertProDate(prodosDateTime), &modStrW);
                CStringA modStr(modStrW);

                char lockedFlag = '*';
                if (pDirEntry[0x1e] & 0x80)
                    lockedFlag = ' ';

                CStringA auxTypeStr;
                uint16_t auxType = pDirEntry[0x1f] | pDirEntry[0x20] << 8;
                if (pDirEntry[0x10] == 0x06)        // bin
                    auxTypeStr.Format("A=$%04X", auxType);
                else if (pDirEntry[0x10] == 0x04)   // txt
                    auxTypeStr.Format("R=%5d", auxType);

                BufPrintf("%c%-15s %-3ls  %6d %16s %16s %8d %s\r\n",
                    lockedFlag,
                    fileName,
                    PathProposal::FileTypeString(pDirEntry[0x10]),
                    pDirEntry[0x13] | pDirEntry[0x14] << 8,
                    (LPCSTR) modStr,
                    (LPCSTR) createStr,
                    pDirEntry[0x15] | pDirEntry[0x16] << 8 | pDirEntry[0x17] << 16,
                    (LPCSTR) auxTypeStr);
            }
        }
    }
}
