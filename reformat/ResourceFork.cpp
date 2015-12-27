/*
 * CiderPress
 * Copyright (C) 2007 by faddenSoft, LLC.  All Rights Reserved.
 * See the file LICENSE for distribution terms.
 */
/*
 * Generic resource fork handling.
 */
#include "StdAfx.h"
#include "ResourceFork.h"

/*
 * Apple IIgs resource definitions.
 *
 * The most up-to-date list appears to be the System 6.0.1
 * NList.Data from NiftyList, which included some items that were
 * blanks in the Apple docs.
 */
static const char* kUnknownSysRsrc = "(system resource)";
static const char* kRsrc8000[0x30] = {
    // 0x8000 through 0x802f
    kUnknownSysRsrc,            "rIcon",
    "rPicture",                 "rControlList",
    "rControlTemplate",         "rC1InputString",
    "rPString",                 "rStringList",
    "rMenuBar",                 "rMenu",
    "rMenuItem",                "rTextForLETextBox2",
    "rCtlDefProc",              "rCtlColorTbl",
    "rWindParam1",              "rWindParam2",

    "rWindColor",               "rTextBlock",
    "rStyleBlock",              "rToolStartup",
    "rResName",                 "rAlertString",
    "rText",                    "rCodeResource",
    "rCDEVCode",                "rCDEVFlags",
    "rTwoRects",                "rFileType",
    "rListRef",                 "rCString",
    "rXCMD",                    "rXFCN",

    "rErrorString",             "rKTransTable",
    "rWString",                 "rC1OutputString",
    "rSoundSample",             "rTERuler",
    "rFSequence",               "rCursor",
    "rItemStruct",              "rVersion",
    "rComment",                 "rBundle",
    "rFinderPath",              "rPaletteWindow",
    "rTaggedStrings",           "rPatternList",
};
static const char* kRsrcC000[0x04] = {
    // 0xc000 through 0xc003
    kUnknownSysRsrc,            "rRectList",
    "rPrintRecord",             "rFont",
};

/*
 * We handle all files, but only the resource fork.
 */
void ReformatResourceFork::Examine(ReformatHolder* pHolder)
{
    pHolder->SetApplic(ReformatHolder::kReformatResourceFork,
        ReformatHolder::kApplicNot,
        ReformatHolder::kApplicMaybe, ReformatHolder::kApplicNot);
}

/*
 * Split a resource fork into its individual resources, and display them.
 */
int ReformatResourceFork::Process(const ReformatHolder* pHolder,
    ReformatHolder::ReformatID id, ReformatHolder::ReformatPart part,
    ReformatOutput* pOutput)
{
    const uint8_t* srcBuf = pHolder->GetSourceBuf(part);
    long srcLen = pHolder->GetSourceLen(part);
    fUseRTF = false;

    long rFileVersion, rFileToMap, rFileMapSize;
    bool littleEndian;
    bool result;

    result = ReadHeader(srcBuf, srcLen, &rFileVersion, &rFileToMap,
                &rFileMapSize, &littleEndian);
    if (!result) {
        BufPrintf("Does not appear to be a valid resource fork.\r\n");
        goto done;
    }

    BufPrintf("Resource fork header (%s):\r\n",
        littleEndian ? "Apple IIgs little-endian" : "Macintosh big-endian");
    BufPrintf("  rFileVersion = %d\r\n", rFileVersion);
    BufPrintf("  rFileToMap   = 0x%08lx\r\n", rFileToMap);
    BufPrintf("  rFileMapSize = %ld\r\n", rFileMapSize);
    BufPrintf("  rFileMemo:\r\n");
    BufHexDump(srcBuf+12, 128);
    BufPrintf("\r\n");

    if (rFileVersion != 0) {
        BufPrintf("Not an Apple IIgs resource fork (probably Macintosh).\r\n");
        goto done;
    }

    /* move to start of resource map */
    const uint8_t* mapPtr;
    long mapToIndex, mapIndexSize, mapIndexUsed;

    mapPtr = srcBuf + rFileToMap;
    mapToIndex = Get16(mapPtr + 0x0e, littleEndian);
    mapIndexSize = Get32(mapPtr + 0x14, littleEndian);
    mapIndexUsed = Get32(mapPtr + 0x18, littleEndian);

    BufPrintf("Resource map:\r\n");
    BufPrintf("  mapToIndex      = 0x%04x (file offset=0x%08lx)\n",
        mapToIndex, mapToIndex + rFileToMap);
    BufPrintf("  mapIndexSize    = %ld\r\n", mapIndexSize);
    BufPrintf("  mapIndexUsed    = %ld\r\n", mapIndexUsed);
    BufPrintf("  mapFreeListSize = %ld\r\n", Get16(mapPtr + 0x1c, littleEndian));
    BufPrintf("  mapFreeListUsed = %ld\r\n", Get16(mapPtr + 0x1e, littleEndian));

    /* dump contents of resource reference records */
    const uint8_t* indexPtr;

    BufPrintf("\r\nResources:");
    indexPtr = mapPtr + mapToIndex;
    int i;

    for (i = 0; i < mapIndexSize; i++) {
        uint16_t resType = Get16(indexPtr + 0x00, littleEndian);
        if (resType == 0)
            break;      // should happen when i == mapIndexUsed

        const char* typeDescr;
        if (resType >= 0x8000 && resType < 0x8000 + NELEM(kRsrc8000))
            typeDescr = kRsrc8000[resType - 0x8000];
        else if (resType >= 0xc000 && resType < 0xc000 + NELEM(kRsrcC000))
            typeDescr = kRsrcC000[resType - 0xc000];
        else if (resType >= 0x0001 && resType <= 0x7fff)
            typeDescr = "(application-defined resource)";
        else
            typeDescr = kUnknownSysRsrc;

        BufPrintf("\r\n  Entry #%d:\r\n", i);
        BufPrintf("    resType   = 0x%04x - %s\r\n", resType, typeDescr);
        BufPrintf("    resID     = 0x%04x\r\n",
            Get32(indexPtr + 0x02, littleEndian));
        BufPrintf("    resOffset = 0x%04x\r\n",
            Get32(indexPtr + 0x06, littleEndian));
        BufPrintf("    resAttr   = 0x%04x\r\n",
            Get16(indexPtr + 0x0a, littleEndian));
        BufPrintf("    resSize   = 0x%04x\r\n",
            Get32(indexPtr + 0x0c, littleEndian));
        //BufPrintf("    resHandle = 0x%04x\r\n",
        //  Get32(indexPtr + 0x10, littleEndian));

        BufHexDump(srcBuf + Get32(indexPtr + 0x06, littleEndian),
            Get32(indexPtr + 0x0c, littleEndian));

        indexPtr += kRsrcMapEntryLen;
    }

done:
    SetResultBuffer(pOutput);
    return 0;
}

/*
 * Extract and verify the header of a resource fork.
 */
/*static*/ bool ReformatResourceFork::ReadHeader(const uint8_t* srcBuf,
    long srcLen, long* pFileVersion, long* pFileToMap, long* pFileMapSize,
    bool* pLittleEndian)
{
    if (srcLen < 128) {
        LOGD("ReformatResource: invalid len %d", srcLen);
        return false;
    }

    *pFileVersion = Get32LE(srcBuf);
    if (*pFileVersion == 0)
        *pLittleEndian = true;
    else
        *pLittleEndian = false;

    *pFileVersion = Get32(srcBuf, *pLittleEndian);
    *pFileToMap = Get32(srcBuf+4, *pLittleEndian);
    *pFileMapSize = Get32(srcBuf+8, *pLittleEndian);

    if (*pFileVersion != 0)
        return false;
    if (*pFileMapSize <= 0 || *pFileMapSize >= srcLen ||
        *pFileToMap <= 0 || *pFileToMap >= srcLen)
    {
        return false;
    }

    return true;
}

/*
 * For use by other reformatters: find a specific resource.
 *
 * Returns "true" on success, "false" on failure.
 */
/*static*/ bool ReformatResourceFork::GetResource(const uint8_t* srcBuf,
    long srcLen, uint16_t resourceType, uint32_t resourceID,
    const uint8_t** pResource, long* pResourceLen)
{
    /* read the file header */
    long rFileVersion, rFileToMap, rFileMapSize;
    bool littleEndian;
    bool result;

    result = ReadHeader(srcBuf, srcLen, &rFileVersion, &rFileToMap,
                &rFileMapSize, &littleEndian);
    if (!result)
        return false;

    /* move to start of resource map */
    const uint8_t* mapPtr;
    long mapToIndex, mapIndexSize, mapIndexUsed;

    mapPtr = srcBuf + rFileToMap;
    mapToIndex = Get16(mapPtr + 0x0e, littleEndian);
    mapIndexSize = Get32(mapPtr + 0x14, littleEndian);
    mapIndexUsed = Get32(mapPtr + 0x18, littleEndian);

    /* find the appropriate entry */
    const uint8_t* indexPtr = mapPtr + mapToIndex;
    int i;

    for (i = 0; i < mapIndexSize; i++) {
        uint16_t resType;
        uint32_t resID;
        
        resType = Get16(indexPtr + 0x00, littleEndian);
        if (resType == 0)
            break;      // should happen when i == mapIndexUsed
        resID = Get32(indexPtr + 0x02, littleEndian);

        if (resType == resourceType && resID == resourceID) {
            LOGI("Found resource with type=0x%04x id=0x%04x",
                resType, resID);
            *pResource = srcBuf + Get32(indexPtr + 0x06, littleEndian);
            *pResourceLen = Get32(indexPtr + 0x0c, littleEndian);
            if (*pResource + *pResourceLen > srcBuf+srcLen) {
                LOGI(" Bad bounds on resource");
                DebugBreak();
                return false;
            }
            return true;
        }

        indexPtr += kRsrcMapEntryLen;
    }

    LOGI("Resource not found (type=0x%04x id=0x%04x)",
        resourceType, resourceID);
    return false;
}
