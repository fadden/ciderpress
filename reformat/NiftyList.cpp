/*
 * CiderPress
 * Copyright (C) 2007 by faddenSoft, LLC.  All Rights Reserved.
 * See the file LICENSE for distribution terms.
 */
/*
 * NiftyList database functions.
 *
 * The NList.Data file is divided into several sections, separated by lines
 * that start with a '*'.  They are:
 *
 *  ProDOS 8 MLI calls (prefaced with some comments)
 *  ProDOS 16 / GS/OS
 *  System tools (incl. GSBug goodies)
 *  User tools
 *  E1xxxx vectors
 *  E0xxxx vectors
 *  Softswitches and F8 ROM routines (plus Davex stuff at Bxxx)
 *  01xxxx vectors
 *  Nifty List service calls
 *  Resource type names
 *  Error codes (GS/OS and toolbox)
 *  HyperCardIIgs callbacks
 *  Request codes (for Finder extensions?)
 *
 * All lines have the general form:
 *  NNNN Text ...
 * where "NNNN" is a 4-digit hexadecimal value.
 *
 * All sections are in some sort of sort order, though it differs for toolbox
 * calls which are byte-reversed.  Generally speaking, do not expect the input
 * to be sorted already.  The original EOL char is 0x0d, but the file may be
 * converted for convenience in Windows, so supporting arbitrary EOLs is
 * useful.
 */
#include "StdAfx.h"
#include "Reformat.h"

/*static*/ NiftyList::DataSet NiftyList::fP8MLI = { 0 };
/*static*/ NiftyList::DataSet NiftyList::fGSOS = { 0 };
/*static*/ NiftyList::DataSet NiftyList::fSystemTools = { 0 };
/*static*/ NiftyList::DataSet NiftyList::fE1Vectors = { 0 };
/*static*/ NiftyList::DataSet NiftyList::fE0Vectors = { 0 };
/*static*/ NiftyList::DataSet NiftyList::f00Addrs = { 0 };
/*static*/ NiftyList::DataSet NiftyList::f01Vectors = { 0 };

/*static*/ char* NiftyList::fFileData = NULL;
/*static*/ bool NiftyList::fDataReady = false;

/*
 * Load the NiftyList data file.
 */
/*static*/ bool NiftyList::AppInit(const WCHAR* fileName)
{
    FILE* fp = NULL;
    long fileLen;
    char* pData;
    bool result = false;

    /*
     * Open the NList.Data file and load the contents into memory.
     */
    fp = _wfopen(fileName, L"rb");
    if (fp == NULL) {
        LOGI("NL Unable to open '%ls'", fileName);
        goto bail;
    } else {
        LOGI("NL Reading '%ls'", fileName);
    }

    if (fseek(fp, 0, SEEK_END) < 0) {
        LOGI("Seek failed");
        goto bail;
    }
    fileLen = ftell(fp);
    rewind(fp);

    fFileData = new char[fileLen];
    if (fFileData == NULL) {
        LOGI("Failed allocating %d bytes", fileLen);
        goto bail;
    }

    if (fread(fFileData, fileLen, 1, fp) != 1) {
        LOGI("Failed reading NList.Data (%d bytes)", fileLen);
        goto bail;
    }

    /*
     * Scan the data.
     */
    pData = fFileData;
    if (!ReadSection(&pData, &fileLen, &fP8MLI, kModeNormal))
        goto bail;
    if (!ReadSection(&pData, &fileLen, &fGSOS, kModeNormal))
        goto bail;
    if (!ReadSection(&pData, &fileLen, &fSystemTools, kModeNormal))
        goto bail;
    if (!ReadSection(&pData, &fileLen, NULL, kModeSkip)) // user tools
        goto bail;
    if (!ReadSection(&pData, &fileLen, &fE1Vectors, kModeNormal))
        goto bail;
    if (!ReadSection(&pData, &fileLen, &fE0Vectors, kModeNormal))
        goto bail;
    if (!ReadSection(&pData, &fileLen, &f00Addrs, kModeNormal))
        goto bail;
    if (!ReadSection(&pData, &fileLen, &f01Vectors, kModeNormal))
        goto bail;

    //DumpSection(fP8MLI);
    //DumpSection(fGSOS);

    fDataReady = true;
    result = true;
    LOGI("NL NiftyList data loaded");

bail:
    if (fp != NULL)
        fclose(fp);
    return result;
}

/*
 * Discard all allocated storage.
 */
/*static*/ bool NiftyList::AppCleanup(void)
{
    delete[] fP8MLI.pEntries;
    delete[] fGSOS.pEntries;
    delete[] fSystemTools.pEntries;
    delete[] fE1Vectors.pEntries;
    delete[] fE0Vectors.pEntries;
    delete[] f00Addrs.pEntries;
    delete[] f01Vectors.pEntries;

    delete[] fFileData;

    fDataReady = false;
    return true;
}


/*
 * Slurp one section from the data file.  Stomps EOL markers.
 *
 * Leaves "*ppData" pointing at the start of the next section.  "*pRemLen"
 * is updated appropriately.
 */
/*static*/ bool NiftyList::ReadSection(char** ppData, long* pRemLen,
    DataSet* pSet, LoadMode mode)
{
    assert(ppData != NULL);
    assert(pRemLen != NULL);
    assert(*ppData != NULL);
    assert(*pRemLen > 0);
    assert(pSet != NULL || mode == kModeSkip);
    assert(mode == kModeNormal || mode == kModeSkip);

    char* pData = *ppData;
    long remLen = *pRemLen;
    int lineLen, numLines, entry;

    /*
     * Count up the #of entries in this section.
     */
    numLines = 0;
    while (1) {
        lineLen = ScanLine(pData, remLen);
        if (lineLen == 0) {
            LOGI("Failed scanning line, remLen=%ld", remLen);
            return false;
        }
        if (*pData == '*') {
            pData += lineLen;
            remLen -= lineLen;
            break;      // end of section reached
        }

        pData += lineLen;
        remLen -= lineLen;
        numLines++;
    }

    if (mode == kModeSkip) {
        LOGI(" NL Skipping %d entries in section", numLines);
        *ppData = pData;
        *pRemLen = remLen;
        return true;
    } else {
        LOGI(" NL Found %d entries in section", numLines);
    }

    /*
     * Allocate storage for the lookup array.
     */
    assert(pSet->numEntries == 0 && pSet->pEntries == NULL);

    pSet->pEntries = new NameValue[numLines];
    if (pSet->pEntries == NULL) {
        LOGI("NameValue alloc failed");
        return false;
    }
    pSet->numEntries = numLines;

    /*
     * Add the entries to the list.
     */
    pData = *ppData;
    remLen = *pRemLen;
    entry = 0;
    while (1) {
        lineLen = ScanLine(pData, remLen);
        if (lineLen == 0) {
            LOGI("Failed scanning line(2), remLen=%ld", remLen);
            return false;
        }

        if (*pData == '*') {
            pData += lineLen;
            remLen -= lineLen;
            break;      // end of section reached
        }

        if (lineLen < 6 || pData[4] != ' ') {
            LOGI("Found garbled line '%.80hs'", pData);
            return false;
        }
        if (pData[lineLen-2] == '\r' || pData[lineLen-2] == '\n')
            pData[lineLen-2] = '\0';
        else if (pData[lineLen-1] == '\r' || pData[lineLen-1] == '\n')
            pData[lineLen-1] = '\0';
        else {
            LOGI("No EOL found on '%.80hs' (%d)", pData, lineLen);
        }

        assert(entry < numLines);
        pSet->pEntries[entry].name = pData +5;
        pSet->pEntries[entry].value = ConvHexFour(pData);
        entry++;

        pData += lineLen;
        remLen -= lineLen;
    }
    assert(entry == numLines);

    *ppData = pData;
    *pRemLen = remLen;

    /*
     * Sort the array.  In most cases it will already be in sorted order,
     * which is a worst-case for qsort, but the only really big section
     * (toolbox calls) is byte-swapped and sorts just fine with qsort.
     */
    qsort(pSet->pEntries, numLines, sizeof(NameValue), SortNameValue);

    return true;
}

/*
 * Scan a line of text.  Return the #of chars, including the EOL bytes.
 *
 * Returns 0 if there's no data, which can only happen if "remLen" is zero
 * (i.e. we hit EOF).
 */
/*static*/ int NiftyList::ScanLine(const char* pData, long remLen)
{
    bool atEOL = false;
    int lineLen = 0;

    while (remLen--) {
        if (*pData == '\r' || *pData == '\n')
            atEOL = true;
        else if (atEOL)
            break;
        pData++;
        lineLen++;
    }

    return lineLen;
}

/*
 * qsort() sort function.
 */
/*static*/ int NiftyList::SortNameValue(const void* v1, const void* v2)
{
    const NameValue* pnv1 = (const NameValue*) v1;
    const NameValue* pnv2 = (const NameValue*) v2;

    if (pnv1->value > pnv2->value)
        return 1;
    else if (pnv1->value < pnv2->value)
        return -1;
    else {
        DebugBreak();   // should never be equal in well-formed file
        return 0;
    }
}

/*
 * Hex digit converter.
 */
static inline int HexValue(char ch)
{
    if (ch >= '0' && ch <= '9')
        return ch - '0';
    if (ch >= 'A' && ch <= 'F')
        return ch - 'A' +10;
    if (ch >= 'a' && ch <= 'f')
        return ch - 'a' +10;
    DebugBreak();       // shouldn't happen on well-formed file
    return -1;
}

/*
 * Convert a 4-char hexadecimal value to an unsigned 16-bit value.
 */
/*static*/ uint16_t NiftyList::ConvHexFour(const char* data)
{
    return HexValue(data[0]) << 12 |
          HexValue(data[1]) << 8 |
          HexValue(data[2]) << 4 |
          HexValue(data[3]);
}

/*
 * Dump the contents of a section.
 */
/*static*/ void NiftyList::DumpSection(const DataSet& dataSet)
{
    long ent;

    LOGD("Dumping section (count=%ld)", dataSet.numEntries);

    for (ent = 0; ent < dataSet.numEntries; ent++) {
        LOGD("%4d: %04x '%hs'",
            ent, dataSet.pEntries[ent].value, dataSet.pEntries[ent].name);
    }
}

/*
 * Look up a value in the specified table.  Returns the name, or NULL if
 * not found.
 *
 * This uses a binary search, so the entries must be in sorted order.
 */
/*static*/ const char* NiftyList::Lookup(const DataSet& dataSet, uint16_t key)
{
    if (!fDataReady) {
        return NULL;
    }
    assert(dataSet.numEntries > 0);

    int high = dataSet.numEntries-1;
    int low = 0;
    int mid;

    while (low <= high) {
        mid = (high + low)/2;
        uint16_t midVal = dataSet.pEntries[mid].value;

        if (key > midVal)
            low = mid +1;
        else if (key < midVal)
            high = mid -1;
        else
            return dataSet.pEntries[mid].name;
    }

    return NULL;
}
