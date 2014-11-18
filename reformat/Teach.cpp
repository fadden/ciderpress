/*
 * CiderPress
 * Copyright (C) 2007 by faddenSoft, LLC.  All Rights Reserved.
 * See the file LICENSE for distribution terms.
 */
/*
 * Reformat "generic" GWP and Teach files.
 */
#include "StdAfx.h"
#include "Teach.h"
#include "ResourceFork.h"


/*
 * ==========================================================================
 *      ReformatGWP
 * ==========================================================================
 */

/*
 * Decide whether or not we want to treat this file as a "generic" IIgs
 * document.  Possibly useful for converting the special characters.  We'll
 * activate it for any GWP file other than the two known types.
 */
void
ReformatGWP::Examine(ReformatHolder* pHolder)
{
    ReformatHolder::ReformatApplies applies = ReformatHolder::kApplicNot;

    if (pHolder->GetFileType() == kTypeGWP &&
        (pHolder->GetAuxType() != 0x5445 && pHolder->GetAuxType() != 0x8010))
    {
        applies = ReformatHolder::kApplicProbably;
    }

    pHolder->SetApplic(ReformatHolder::kReformatGWP, applies,
        ReformatHolder::kApplicNot, ReformatHolder::kApplicNot);
}

/*
 * Convert GWP into formatted text.
 */
int
ReformatGWP::Process(const ReformatHolder* pHolder,
    ReformatHolder::ReformatID id, ReformatHolder::ReformatPart part,
    ReformatOutput* pOutput)
{
    const unsigned char* srcBuf = pHolder->GetSourceBuf(part);
    long srcLen = pHolder->GetSourceLen(part);
    fUseRTF = false;

    CheckGSCharConv();
    RTFBegin();

    /* convert EOL markers and IIgs characters */
    unsigned char ch;
    while (srcLen) {
        ch = *srcBuf++;
        srcLen--;

        if (ch == '\r') {
            /* got CR, check for CRLF -- not really expected on IIgs */
            if (srcLen != 0 && *srcBuf == '\n') {
                srcBuf++;
                srcLen--;
            }
            BufPrintf("\r\n");
        } else if (ch == '\n') {
            BufPrintf("\r\n");
        } else {
            // RTF is always off, so just use BufPrintf
            BufPrintf("%c", ConvertGSChar(ch));
        }
    }

    RTFEnd();

    SetResultBuffer(pOutput);
    return 0;
}


/*
 * ==========================================================================
 *      ReformatTeach
 * ==========================================================================
 */

/*
 * Decide whether or not we want to handle this file.
 */
void
ReformatTeach::Examine(ReformatHolder* pHolder)
{
    ReformatHolder::ReformatApplies applies = ReformatHolder::kApplicNot;

    if (pHolder->GetFileType() == kTypeGWP && pHolder->GetAuxType() == 0x5445)
        applies = ReformatHolder::kApplicYes;

    pHolder->SetApplic(ReformatHolder::kReformatTeach, applies,
        ReformatHolder::kApplicNot, ReformatHolder::kApplicNot);
}

/*
 * Convert Teach Text into formatted text.
 *
 * The text is in the data fork and the formatting is in the resource fork.
 */
int
ReformatTeach::Process(const ReformatHolder* pHolder,
    ReformatHolder::ReformatID id, ReformatHolder::ReformatPart part,
    ReformatOutput* pOutput)
{
    const unsigned char* dataBuf;
    const unsigned char* rsrcBuf;
    long dataLen, rsrcLen;
    const unsigned char* styleBlock;
    long styleLen;

    if (part != ReformatHolder::kPartData)
        return -1;

    dataBuf = pHolder->GetSourceBuf(ReformatHolder::kPartData);
    dataLen = pHolder->GetSourceLen(ReformatHolder::kPartData);
    rsrcBuf = pHolder->GetSourceBuf(ReformatHolder::kPartRsrc);
    rsrcLen = pHolder->GetSourceLen(ReformatHolder::kPartRsrc);
    if (dataBuf == NULL || rsrcBuf == NULL || dataLen <= 0 || rsrcLen <= 0) {
        WMSG0("Teach reformatter missing one fork of the file\n");
        return -1;
    }
    CheckGSCharConv();

    /* find the rStyleBlock */
    if (!ReformatResourceFork::GetResource(rsrcBuf, rsrcLen, 0x8012, 0x0001,
        &styleBlock, &styleLen))
    {
        WMSG0("Resource fork of Teach Text file not found\n");
        return -1;
    }

    RStyleBlock rStyleBlock;
    if (!rStyleBlock.Create(styleBlock, styleLen)) {
        WMSG0("Unable to unpack rStyleBlock\n");
        return -1;
    }

    fUseRTF = true;
    RTFBegin();

    /*
     * Configure ruler options.  The format allows us to have more than one
     * ruler, but doesn't provide a way to select between them, so we have
     * to assume that the first ruler is global.
     *
     * In practice, the Teach application doesn't provide a way to set
     * margins or tab stops, so there's really not much to do.  It seems to
     * create tab stops with 64-pixel widths, which translates to 10 tab
     * stops per page.  WordPad seems to default to 12, so I figure there's
     * not much point in futzing with it since the fonts are all different
     * anyway.
     */
    RStyleBlock::TERuler* pRuler = rStyleBlock.GetRuler(0);
    assert(pRuler != NULL);
    if (pRuler->GetJustification() != RStyleBlock::TERuler::kJustLeft) {
        WMSG0("WARNING: not using left justified Teach text\n");
        /* ignore it */
    }

    for (int style = 0; style < rStyleBlock.GetNumStyleItems(); style++) {
        RStyleBlock::StyleItem* pStyleItem;
        RStyleBlock::TEStyle* pStyle;
        int numBytes;

        /* set up the style */
        pStyleItem = rStyleBlock.GetStyleItem(style);
        assert(pStyleItem != NULL);
        if (pStyleItem->GetLength() == RStyleBlock::StyleItem::kUnusedItem)
            continue;
        numBytes = pStyleItem->GetLength();
        pStyle = rStyleBlock.GetStyle(pStyleItem->GetStyleIndex());

        /*
         * Set the font first; that defines the point size multiplier.  Set
         * the size second, because the point size determines whether we
         * show underline.  Set the style last.
         */
        RTFSetGSFont(pStyle->GetFontFamily());
        RTFSetGSFontSize(pStyle->GetFontSize());
        RTFSetGSFontStyle(pStyle->GetTextStyle());

        /*
         * The Teach app doesn't let you set text colors, but it appears
         * there's a way to do it, since the SpyHunterGS Read.Me file has
         * a bit of red in it.
         */
        if (pStyle->GetTextColor() != 0) {
            // do something with color, someday
        }

        /* output the characters */
        unsigned char uch;
        while (numBytes--) {
            if (!dataLen) {
                WMSG1("WARNING: Teach underrun (%ld wanted)\n", numBytes);
                break;
            }
            uch = *dataBuf;
            if (uch == '\r') {
                RTFNewPara();
            } else if (uch == '\t') {
                RTFTab();
            } else {
                RTFPrintExtChar(ConvertGSChar(uch));
            }
            dataBuf++;
            dataLen--;
        }
    }
    if (dataLen) {
        WMSG1("WARNING: Teach overrun (%ld remain)\n", dataLen);
        /* no big deal */
    }


    RTFEnd();

    SetResultBuffer(pOutput, true);
    return 0;
}


/*
 * ==========================================================================
 *      RStyleBlock functions
 * ==========================================================================
 */

/*
 * Unpack an rStyleBlock resource.
 */
bool
RStyleBlock::Create(const unsigned char* buf, long len)
{
    unsigned short version;
    unsigned long partLen;
    int i;

    assert(buf != NULL);
    if (len < kMinLen) {
        WMSG1("Too short to be rStyleBlock (%d)\n", len);
        return false;
    }

    version = Reformat::Read16(&buf, &len);
    if (version != kExpectedVersion) {
        WMSG1("Bad rStyleBlock version (%d)\n", version);
        return false;
    }

    /* extract ruler(s) */
    partLen = Reformat::Read32(&buf, &len);
    if (partLen > (unsigned long) (len+8)) {
        /* not enough to satisfy data + two more counts */
        WMSG2("Invalid part1 length (%d vs %d)\n", partLen, len);
        return false;
    }

    fNumRulers = 1;
    fpRulers = new TERuler[fNumRulers];
    if (fpRulers == NULL)
        return false;
    if (fpRulers->Create(buf, partLen) <= 0)
        return false;

    buf += partLen;
    len -= partLen;

    /* extract TEStyles */
    partLen = Reformat::Read32(&buf, &len);
    if (partLen > (unsigned long) (len+4)) {
        WMSG2("Invalid part2 length (%d vs %d)\n", partLen, len);
        return false;
    }
    if ((partLen % TEStyle::kDataLen) != 0) {
        WMSG2("Invalid part2 length (%d mod %d)\n",
            partLen, TEStyle::kDataLen);
        return false;
    }

    fNumStyles = partLen / TEStyle::kDataLen;
    fpStyles = new TEStyle[fNumStyles];
    if (fpStyles == NULL)
        return false;
    for (i = 0; i < fNumStyles; i++) {
        fpStyles[i].Create(buf);
        buf += TEStyle::kDataLen;
        len -= TEStyle::kDataLen;
    }

    /* extract StyleItems */
    fNumStyleItems = (int) Reformat::Read32(&buf, &len);
    partLen = fNumStyleItems * StyleItem::kDataLen;
    if (partLen > (unsigned long) len) {
        WMSG2("Invalid part3 length (%d vs %d)\n", partLen, len);
        return false;
    }

    fpStyleItems = new StyleItem[fNumStyleItems];
    if (fpStyleItems == NULL)
        return false;
    for (i = 0; i < fNumStyleItems; i++) {
        fpStyleItems[i].Create(buf);
        if ((fpStyleItems[i].GetOffset() % TEStyle::kDataLen) != 0) {
            WMSG2("Invalid offset %d (mod %d)\n",
                fpStyleItems[i].GetOffset(), TEStyle::kDataLen);
            return false;
        }
        buf += StyleItem::kDataLen;
        len -= StyleItem::kDataLen;
    }

    if (len != 0) {
        WMSG1("WARNING: at end of rStyleBlock, len is %ld\n", len);
    }

    return true;
}


/*
 * Construct a TERuler from a chunk of data.
 *
 * Returns the #of bytes consumed, or -1 on failure.
 */
int
RStyleBlock::TERuler::Create(const unsigned char* buf, long len)
{
    long origLen = len;

    if (len < kMinLen)
        return -1;

    fLeftMargin = Reformat::Get16LE(buf);
    fLeftIndent = Reformat::Get16LE(buf + 2);
    fRightMargin = Reformat::Get16LE(buf + 4);
    fJust = Reformat::Get16LE(buf + 6);
    fExtraLS = Reformat::Get16LE(buf + 8);
    fFlags = Reformat::Get16LE(buf + 10);
    fUserData = Reformat::Get32LE(buf + 12);
    fTabType = Reformat::Get16LE(buf + 16);
    buf += 18;
    len -= 18;

    /*
     * What we do now depends on the value of fTabType:
     *  0: no tabs are set, there's nothing else to read.
     *  1: regularly-spaced tabs; spacing defined by fTabTerminator field.
     *  2: irregular tabs, defined by array terminated by 0xffff in the
     *     fTabTerminator field.
     *
     * At present we're just throwing the tab array out.
     */
    switch (fTabType) {
    case 0:
        fTabTerminator = 0;
        break;
    case 1:
        fTabTerminator = Reformat::Get16LE(buf);
        buf += 2;
        len -= 2;
        break;
    case 2:
        while (len >= 2) {
            TabItem tabItem;
            tabItem.tabKind = Reformat::Get16LE(buf);
            buf += 2;
            len -= 2;

            if (tabItem.tabKind == kTabArrayEnd || len < 2)
                break;
            tabItem.tabData = Reformat::Get16LE(buf);
            buf += 2;
            len -= 2;
        }
        break;
    default:
        WMSG1("Invalid tab type %d\n", fTabType);
        return -1;
    }

    WMSG2("TERuler consumed %ld bytes (%ld left over)\n", origLen - len, len);
    return origLen - len;
}

/*
 * Extract a TEStyle object from the buffer.
 */
void
RStyleBlock::TEStyle::Create(const unsigned char* buf)
{
    fFontID = Reformat::Get32LE(buf);
    fForeColor = Reformat::Get16LE(buf + 4);
    fBackColor = Reformat::Get16LE(buf + 6);
    fUserData = Reformat::Get32LE(buf + 8);

    WMSG4("  TEStyle: font fam=0x%04x size=%-2d style=0x%02x  fore=0x%04x\n",
        GetFontFamily(), GetFontSize(), GetTextStyle(), fForeColor);
}

/*
 * Extract a StyleItem object from the buffer.
 */
void
RStyleBlock::StyleItem::Create(const unsigned char* buf)
{
    fLength = Reformat::Get32LE(buf);
    fOffset = Reformat::Get32LE(buf + 4);

    WMSG2("  StyleItem: len=%ld off=%ld\n", fLength, fOffset);
}
