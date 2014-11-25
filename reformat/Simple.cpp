/*
 * CiderPress
 * Copyright (C) 2007 by faddenSoft, LLC.  All Rights Reserved.
 * See the file LICENSE for distribution terms.
 */
/*
 * Some very simple, generic reformatters.
 */
#include "StdAfx.h"
#include "Simple.h"


/*
 * Indicate that we handle all parts of all files.
 */
void ReformatRaw::Examine(ReformatHolder* pHolder)
{
    pHolder->SetApplic(ReformatHolder::kReformatRaw,
        ReformatHolder::kApplicAlways,
        ReformatHolder::kApplicAlways, ReformatHolder::kApplicAlways);

    if (pHolder->GetSourceLen(ReformatHolder::kPartData) == 0)
        pHolder->SetApplicPreferred(ReformatHolder::kReformatRaw,
                                    ReformatHolder::kPartData);
    if (pHolder->GetSourceLen(ReformatHolder::kPartRsrc) == 0)
        pHolder->SetApplicPreferred(ReformatHolder::kReformatRaw,
                                    ReformatHolder::kPartRsrc);
    if (pHolder->GetSourceLen(ReformatHolder::kPartCmmt) == 0)
        pHolder->SetApplicPreferred(ReformatHolder::kReformatRaw,
                                    ReformatHolder::kPartCmmt);
}

/*
 * Reformat a file by not reformatting it.
 *
 * This should inspire whoever is calling us to present the data without
 * reformatting it first.
 */
int ReformatRaw::Process(const ReformatHolder* pHolder,
    ReformatHolder::ReformatID id, ReformatHolder::ReformatPart part,
    ReformatOutput* pOutput)
{
    return -1;
}

/*
 * Indicate that we handle all parts of all files.
 */
void ReformatHexDump::Examine(ReformatHolder* pHolder)
{
    pHolder->SetApplic(ReformatHolder::kReformatHexDump,
        ReformatHolder::kApplicAlways,
        ReformatHolder::kApplicAlways, ReformatHolder::kApplicAlways);
}

/*
 * Convert a file to a hex dump.
 */
int ReformatHexDump::Process(const ReformatHolder* pHolder,
    ReformatHolder::ReformatID id, ReformatHolder::ReformatPart part,
    ReformatOutput* pOutput)
{
    const uint8_t* srcBuf = pHolder->GetSourceBuf(part);
    long srcLen = pHolder->GetSourceLen(part);

    /*
     * The RichEdit control gets all wonky on large RTF files, but doesn't
     * seem to have trouble with large text files.  So, for large files,
     * turn off the RTF formatting.  We also allow the user the option of
     * turning the formatting off.
     */
    if (srcLen > 65536)
        fUseRTF = false;
    if (fUseRTF) {
        if (!pHolder->GetOption(ReformatHolder::kOptHiliteHexDump))
            fUseRTF = false;
    }

    RTFBegin();

    BufHexDump(srcBuf, srcLen);
    
    RTFEnd();

    SetResultBuffer(pOutput);
    return 0;
}

/*
 * Indicate that we handle all parts of all files.
 */
void ReformatEOL_HA::Examine(ReformatHolder* pHolder)
{
    pHolder->SetApplic(ReformatHolder::kReformatTextEOL_HA,
        ReformatHolder::kApplicAlways,
        ReformatHolder::kApplicAlways, ReformatHolder::kApplicAlways);
}

/*
 * Convert the EOL markers in a text file to Windows' idiotic CRLF, and
 * strip off all of the high bits.
 *
 * Sadly, this most likely requires expanding the original.
 */
int ReformatEOL_HA::Process(const ReformatHolder* pHolder,
    ReformatHolder::ReformatID id, ReformatHolder::ReformatPart part,
    ReformatOutput* pOutput)
{
    fUseRTF = false;

    //LOGI("Reformatting EOL (testing for high-ASCII too)");

    //bool isHighASCII = false;

    if (pHolder->GetSourceLen(part) == 0)
        return -1;

    //isHighASCII = GenericEntry::CheckHighASCII(
    //  (const uint8_t*) pHolder->fSourceBuf, pHolder->fSourceLen);

    ConvertEOL(pHolder->GetSourceBuf(part), pHolder->GetSourceLen(part), true);

    SetResultBuffer(pOutput);
    return 0;
}
