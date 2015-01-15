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
void ReformatGWP::Examine(ReformatHolder* pHolder)
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
int ReformatGWP::Process(const ReformatHolder* pHolder,
    ReformatHolder::ReformatID id, ReformatHolder::ReformatPart part,
    ReformatOutput* pOutput)
{
    const uint8_t* srcBuf = pHolder->GetSourceBuf(part);
    long srcLen = pHolder->GetSourceLen(part);
    fUseRTF = false;

    Charset::CheckGSCharConv();
    RTFBegin();

    /* convert EOL markers and IIgs characters */
    uint8_t ch;
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
            BufPrintf("%c", Charset::ConvertMacRomanTo1252(ch));
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
void ReformatTeach::Examine(ReformatHolder* pHolder)
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
int ReformatTeach::Process(const ReformatHolder* pHolder,
    ReformatHolder::ReformatID id, ReformatHolder::ReformatPart part,
    ReformatOutput* pOutput)
{
    const uint8_t* dataBuf;
    const uint8_t* rsrcBuf;
    long dataLen, rsrcLen;
    const uint8_t* styleBlock;
    long styleLen;

    if (part != ReformatHolder::kPartData)
        return -1;

    dataBuf = pHolder->GetSourceBuf(ReformatHolder::kPartData);
    dataLen = pHolder->GetSourceLen(ReformatHolder::kPartData);
    rsrcBuf = pHolder->GetSourceBuf(ReformatHolder::kPartRsrc);
    rsrcLen = pHolder->GetSourceLen(ReformatHolder::kPartRsrc);
    if (dataBuf == NULL || rsrcBuf == NULL || dataLen <= 0 || rsrcLen <= 0) {
        LOGI("Teach reformatter missing one fork of the file");
        return -1;
    }
    Charset::CheckGSCharConv();

    /* find the rStyleBlock */
    if (!ReformatResourceFork::GetResource(rsrcBuf, rsrcLen, 0x8012, 0x0001,
        &styleBlock, &styleLen))
    {
        LOGI("Resource fork of Teach Text file not found");
        return -1;
    }

    RStyleBlock rStyleBlock;
    if (!rStyleBlock.Create(styleBlock, styleLen)) {
        LOGI("Unable to unpack rStyleBlock");
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
        LOGI("WARNING: not using left justified Teach text");
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
        uint8_t uch;
        while (numBytes--) {
            if (!dataLen) {
                LOGI("WARNING: Teach underrun (%ld wanted)", numBytes);
                break;
            }
            uch = *dataBuf;
            if (uch == '\r') {
                RTFNewPara();
            } else if (uch == '\t') {
                RTFTab();
            } else {
                RTFPrintUTF16Char(Charset::ConvertMacRomanToUTF16(uch));
            }
            dataBuf++;
            dataLen--;
        }
    }
    if (dataLen) {
        LOGI("WARNING: Teach overrun (%ld remain)", dataLen);
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
bool RStyleBlock::Create(const uint8_t* buf, long len)
{
    uint16_t version;
    uint32_t partLen;
    int i;

    assert(buf != NULL);
    if (len < kMinLen) {
        LOGI("Too short to be rStyleBlock (%d)", len);
        return false;
    }

    version = Reformat::Read16(&buf, &len);
    if (version != kExpectedVersion) {
        LOGI("Bad rStyleBlock version (%d)", version);
        return false;
    }

    /* extract ruler(s) */
    partLen = Reformat::Read32(&buf, &len);
    if (partLen > (uint32_t) (len+8)) {
        /* not enough to satisfy data + two more counts */
        LOGI("Invalid part1 length (%d vs %d)", partLen, len);
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
    if (partLen > (uint32_t) (len + 4)) {
        LOGI("Invalid part2 length (%d vs %d)", partLen, len);
        return false;
    }
    if ((partLen % TEStyle::kDataLen) != 0) {
        LOGI("Invalid part2 length (%d mod %d)",
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
    partLen = fNumStyleItems * StyleItem::kItemDataLen;
    if (partLen > (uint32_t) len) {
        LOGI("Invalid part3 length (%d vs %d)", partLen, len);
        return false;
    }

    fpStyleItems = new StyleItem[fNumStyleItems];
    if (fpStyleItems == NULL)
        return false;
    for (i = 0; i < fNumStyleItems; i++) {
        fpStyleItems[i].Create(buf);
        if ((fpStyleItems[i].GetOffset() % TEStyle::kDataLen) != 0) {
            LOGI("Invalid offset %d (mod %d)",
                fpStyleItems[i].GetOffset(), TEStyle::kDataLen);
            return false;
        }
        buf += StyleItem::kItemDataLen;
        len -= StyleItem::kItemDataLen;
    }

    if (len != 0) {
        LOGI("WARNING: at end of rStyleBlock, len is %ld", len);
    }

    return true;
}

/*
 * Construct a TERuler from a chunk of data.
 *
 * Returns the #of bytes consumed, or -1 on failure.
 */
int RStyleBlock::TERuler::Create(const uint8_t* buf, long len)
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
        LOGI("Invalid tab type %d", fTabType);
        return -1;
    }

    LOGI("TERuler consumed %ld bytes (%ld left over)", origLen - len, len);
    return origLen - len;
}

/*
 * Extract a TEStyle object from the buffer.
 */
void RStyleBlock::TEStyle::Create(const uint8_t* buf)
{
    fFontID = Reformat::Get32LE(buf);
    fForeColor = Reformat::Get16LE(buf + 4);
    fBackColor = Reformat::Get16LE(buf + 6);
    fUserData = Reformat::Get32LE(buf + 8);

    LOGI("  TEStyle: font fam=0x%04x size=%-2d style=0x%02x  fore=0x%04x",
        GetFontFamily(), GetFontSize(), GetTextStyle(), fForeColor);
}

/*
 * Extract a StyleItem object from the buffer.
 */
void RStyleBlock::StyleItem::Create(const uint8_t* buf)
{
    fLength = Reformat::Get32LE(buf);
    fOffset = Reformat::Get32LE(buf + 4);

    LOGI("  StyleItem: len=%ld off=%ld", fLength, fOffset);
}
