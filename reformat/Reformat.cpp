/*
 * CiderPress
 * Copyright (C) 2007, 2008 by faddenSoft, LLC.  All Rights Reserved.
 * See the file LICENSE for distribution terms.
 */
/*
 * Common code for reformatters.
 */
#include "StdAfx.h"
#include "Reformat.h"

#include "ReformatBase.h"
#include "AppleWorks.h"
#include "Asm.h"
#include "AWGS.h"
#include "Basic.h"
#include "CPMFiles.h"
#include "Directory.h"
#include "Disasm.h"
#include "DoubleHiRes.h"
#include "HiRes.h"
#include "MacPaint.h"
#include "PascalFiles.h"
#include "PrintShop.h"
#include "ResourceFork.h"
#include "Simple.h"
#include "SuperHiRes.h"
#include "Teach.h"
#include "Text8.h"

/*
 * Create an instance of the class identified by "id".
 */
/*static*/ Reformat* ReformatHolder::GetReformatInstance(ReformatID id)
{
    Reformat* pReformat = NULL;

    switch (id) {
    case kReformatTextEOL_HA:       pReformat = new ReformatEOL_HA;             break;
    case kReformatRaw:              pReformat = new ReformatRaw;                break;
    case kReformatHexDump:          pReformat = new ReformatHexDump;            break;
    case kReformatResourceFork:     pReformat = new ReformatResourceFork;       break;

    case kReformatProDOSDirectory:  pReformat = new ReformatDirectory;          break;
    case kReformatPascalText:       pReformat = new ReformatPascalText;         break;
    case kReformatPascalCode:       pReformat = new ReformatPascalCode;         break;
    case kReformatCPMText:          pReformat = new ReformatCPMText;            break;
    case kReformatApplesoft:        pReformat = new ReformatApplesoft;          break;
    case kReformatApplesoft_Hilite: pReformat = new ReformatApplesoft;          break;
    case kReformatInteger:          pReformat = new ReformatInteger;            break;
    case kReformatInteger_Hilite:   pReformat = new ReformatInteger;            break;
    case kReformatBusiness:         pReformat = new ReformatBusiness;           break;
    case kReformatBusiness_Hilite:  pReformat = new ReformatBusiness;           break;
    case kReformatSCAssem:          pReformat = new ReformatSCAssem;            break;
    case kReformatMerlin:           pReformat = new ReformatMerlin;             break;
    case kReformatLISA2:            pReformat = new ReformatLISA2;              break;
    case kReformatLISA3:            pReformat = new ReformatLISA3;              break;
    case kReformatLISA4:            pReformat = new ReformatLISA4;              break;
    case kReformatMonitor8:         pReformat = new ReformatDisasm8;            break;
    case kReformatDisasmMerlin8:    pReformat = new ReformatDisasm8;            break;
    case kReformatMonitor16Long:    pReformat = new ReformatDisasm16;           break;
    case kReformatMonitor16Short:   pReformat = new ReformatDisasm16;           break;
    case kReformatDisasmOrcam16:    pReformat = new ReformatDisasm16;           break;
    case kReformatAWGS_WP:          pReformat = new ReformatAWGS_WP;            break;
    case kReformatTeach:            pReformat = new ReformatTeach;              break;
    case kReformatGWP:              pReformat = new ReformatGWP;                break;
    case kReformatMagicWindow:      pReformat = new ReformatMagicWindow;        break;
    case kReformatAWP:              pReformat = new ReformatAWP;                break;
    case kReformatADB:              pReformat = new ReformatADB;                break;
    case kReformatASP:              pReformat = new ReformatASP;                break;
    case kReformatHiRes:            pReformat = new ReformatHiRes;              break;
    case kReformatHiRes_BW:         pReformat = new ReformatHiRes;              break;
    case kReformatDHR_Latched:      pReformat = new ReformatDHR;                break;
    case kReformatDHR_BW:           pReformat = new ReformatDHR;                break;
    case kReformatDHR_Plain140:     pReformat = new ReformatDHR;                break;
    case kReformatDHR_Window:       pReformat = new ReformatDHR;                break;
    case kReformatSHR_PIC:          pReformat = new ReformatUnpackedSHR;        break;
    case kReformatSHR_JEQ:          pReformat = new ReformatJEQSHR;             break;
    case kReformatSHR_Paintworks:   pReformat = new ReformatPaintworksSHR;      break;
    case kReformatSHR_Packed:       pReformat = new ReformatPackedSHR;          break;
    case kReformatSHR_APF:          pReformat = new ReformatAPFSHR;             break;
    case kReformatSHR_3200:         pReformat = new Reformat3200SHR;            break;
    case kReformatSHR_3201:         pReformat = new Reformat3201SHR;            break;
    case kReformatSHR_DG256:        pReformat = new ReformatDG256SHR;           break;
    case kReformatSHR_DG3200:       pReformat = new ReformatDG3200SHR;          break;
    case kReformatPrintShop:        pReformat = new ReformatPrintShop;          break;
    case kReformatMacPaint:         pReformat = new ReformatMacPaint;           break;
    case kReformatGutenberg:        pReformat = new ReformatGutenberg;          break;
    case kReformatUnknown:
    case kReformatMAX:
    default:                        assert(false);                              break;
    }

    return pReformat;
}

/*
 * Return a string describing the class identified by "id".  We need this for
 * the pop-up menu in the file viewer.
 *
 * Would have been nice to embed these in the individual classes, but that's
 * harder to maintain.
 */
/*static*/ const WCHAR* ReformatHolder::GetReformatName(ReformatID id)
{
    const WCHAR* descr = NULL;

    switch (id) {
    case kReformatTextEOL_HA:
        descr = L"Converted Text";
        break;
    case kReformatRaw:
        descr = L"Raw";
        break;
    case kReformatHexDump:
        descr = L"Hex Dump";
        break;
    case kReformatResourceFork:
        descr = L"Resource Fork";
        break;
    case kReformatProDOSDirectory:
        descr = L"ProDOS Directory";
        break;
    case kReformatPascalText:
        descr = L"Pascal Text";
        break;
    case kReformatPascalCode:
        descr = L"Pascal Code";
        break;
    case kReformatCPMText:
        descr = L"CP/M Text";
        break;
    case kReformatApplesoft:
        descr = L"Applesoft BASIC";
        break;
    case kReformatApplesoft_Hilite:
        descr = L"Applesoft BASIC w/Highlighting";
        break;
    case kReformatInteger:
        descr = L"Integer BASIC";
        break;
    case kReformatInteger_Hilite:
        descr = L"Integer BASIC w/Highlighting";
        break;
    case kReformatBusiness:
        descr = L"Apple /// Business BASIC";
        break;
    case kReformatBusiness_Hilite:
        descr = L"Apple /// Business BASIC w/Highlighting";
        break;
    case kReformatSCAssem:
        descr = L"S-C Assembler";
        break;
    case kReformatMerlin:
        descr = L"Merlin Assembler";
        break;
    case kReformatLISA2:
        descr = L"LISA Assembler (v2)";
        break;
    case kReformatLISA3:
        descr = L"LISA Assembler (v3)";
        break;
    case kReformatLISA4:
        descr = L"LISA Assembler (v4/v5)";
        break;
    case kReformatMonitor8:
        descr = L"//e monitor listing";
        break;
    case kReformatDisasmMerlin8:
        descr = L"8-bit disassembly (Merlin)";
        break;
    case kReformatMonitor16Long:
        descr = L"IIgs monitor listing (long regs)";
        break;
    case kReformatMonitor16Short:
        descr = L"IIgs monitor listing (short regs)";
        break;
    case kReformatDisasmOrcam16:
        descr = L"16-bit disassembly (Orca/M)";
        break;
    case kReformatAWGS_WP:
        descr = L"AppleWorks GS Word Processor";
        break;
    case kReformatTeach:
        descr = L"Teach Text";
        break;
    case kReformatGWP:
        descr = L"Generic IIgs text document";
        break;
    case kReformatMagicWindow:
        descr = L"Magic Window";
        break;
    case kReformatGutenberg:
        descr = L"Gutenberg Word Processor";
        break;
    case kReformatAWP:
        descr = L"AppleWorks Word Processor";
        break;
    case kReformatADB:
        descr = L"AppleWorks Database";
        break;
    case kReformatASP:
        descr = L"AppleWorks Spreadsheet";
        break;
    case kReformatHiRes:
        descr = L"Hi-Res / Color";
        break;
    case kReformatHiRes_BW:
        descr = L"Hi-Res / B&W";
        break;
    case kReformatDHR_Latched:
        descr = L"Double Hi-Res / Latched";
        break;
    case kReformatDHR_BW:
        descr = L"Double Hi-Res / B&W";
        break;
    case kReformatDHR_Plain140:
        descr = L"Double Hi-Res / Plain140";
        break;
    case kReformatDHR_Window:
        descr = L"Double Hi-Res / Windowed";
        break;
    case kReformatSHR_PIC:
        descr = L"Super Hi-Res";
        break;
    case kReformatSHR_JEQ:
        descr = L"JEQ Super Hi-Res";
        break;
    case kReformatSHR_Paintworks:
        descr = L"Paintworks Super Hi-Res";
        break;
    case kReformatSHR_Packed:
        descr = L"Packed Super Hi-Res";
        break;
    case kReformatSHR_APF:
        descr = L"APF Super Hi-Res";
        break;
    case kReformatSHR_3200:
        descr = L"3200-Color Super Hi-Res";
        break;
    case kReformatSHR_3201:
        descr = L"Packed 3200-Color Super Hi-Res";
        break;
    case kReformatSHR_DG256:
        descr = L"DreamGrafix 256-Color Super Hi-Res";
        break;
    case kReformatSHR_DG3200:
        descr = L"DreamGrafix 3200-Color Super Hi-Res";
        break;
    case kReformatPrintShop:
        descr = L"Print Shop Clip Art";
        break;
    case kReformatMacPaint:
        descr = L"MacPaint";
        break;
    case kReformatUnknown:
    case kReformatMAX:
    default:
        assert(false);
        descr = L"UNKNOWN";
        break;
    }

    return descr;
}

/*
 * Set the file attributes.  These are used by TestApplicability tests to
 * decide what it is we're looking at.
 */
void ReformatHolder::SetSourceAttributes(long fileType, long auxType,
    SourceFormat sourceFormat, const char* nameExt)
{
    fFileType = fileType;
    fAuxType = auxType;
    fSourceFormat = sourceFormat;

    assert(fNameExt == NULL);
    if (nameExt == NULL) {
        fNameExt = new char[1];
        fNameExt[0] = '\0';
    } else {
        fNameExt = strdup(nameExt);
    }
}

/*
 * Run through the set of reformatters and figure out which apply.
 *
 * Each reformatter function is handed the full set.  If it can make a
 * determination for more than one entry (e.g. Applesoft and ApplesoftHilite),
 * it can set all that apply.  There must not be any overlap between
 * reformatters -- this is here so that a single reformatter may have more
 * than one entry without having to re-process the data multiple times.
 *
 * Before calling here, the file data and file attributes (e.g. file type)
 * should be stored in "ReformatHolder".
 */
void ReformatHolder::TestApplicability(void)
{
    Reformat* pReformat;
    int i;

    for (i = 0; i < kReformatMAX; i++) {
        if (fApplies[kPartData][i] != kApplicUnknown) {
            assert(fApplies[kPartRsrc][i] != kApplicUnknown);
            assert(fApplies[kPartCmmt][i] != kApplicUnknown);
            continue;   // already set by previous test
        }
        if (!fAllow[i]) {
            if (i != 0) {
                LOGI(" NOTE: Applic %d disallowed", i);
                // did you update ConfigureReformatFromPreferences()?
            }
            fApplies[kPartData][i] = kApplicNot;
            fApplies[kPartRsrc][i] = kApplicNot;
            fApplies[kPartCmmt][i] = kApplicNot;
            continue;
        }

        /*
         * Create an instance of the object, test its applicability, and
         * then destroy the instance.  It's less efficient to do it this
         * way than some other approaches, but it's easier maintenance
         * than creating a separate table of pointers to static functions.
         */
        pReformat = GetReformatInstance((ReformatID) i);
        assert(pReformat != NULL);
        pReformat->Examine(this);
        delete pReformat;
    }

    /* don't mess with the unknown */
    assert(fApplies[kPartData][kReformatUnknown] == kApplicNot);
    assert(fApplies[kPartRsrc][kReformatUnknown] == kApplicNot);
    assert(fApplies[kPartCmmt][kReformatUnknown] == kApplicNot);
}

/*
 * Return the appropriate applicability level.
 */
ReformatHolder::ReformatApplies ReformatHolder::GetApplic(ReformatPart part,
    ReformatID id) const
{
    if (id < kReformatUnknown || id >= kReformatMAX) {
        assert(false);
        return kApplicUnknown;
    }
    if (part <= kPartUnknown || part >= kPartMAX) {
        assert(false);
        return kApplicUnknown;
    }

    return fApplies[part][id];
}

/*
 * Set the appropriate applicability level.
 */
void ReformatHolder::SetApplic(ReformatID id, ReformatApplies applyData,
    ReformatApplies applyRsrc, ReformatApplies applyCmmt)
{
    if (id <= kReformatUnknown || id >= kReformatMAX) {
        assert(false);
        return;
    }

    fApplies[kPartData][id] = applyData;
    fApplies[kPartRsrc][id] = applyRsrc;
    fApplies[kPartCmmt][id] = applyCmmt;
}

/*
 * Set the "preferred" flag on all parts that aren't "unknown" or "not"
 * for the specified id.  If "part" isn't kPartUnknown, then only that
 * part will be altered.
 *
 * The idea is to prefer one variation over another, such as highlighting
 * a BASIC program or choosing a double-hi-res algorithm.  The preference
 * indicates the default choice, but does not exclude others.
 */
void ReformatHolder::SetApplicPreferred(ReformatID id, ReformatPart part)
{
    for (int i = 0; i < kPartMAX; i++) {
        if (i == kPartUnknown)
            continue;

        if (part != kPartUnknown && i != part)
            continue;

        // Don't set "preferred" flag for "not", "probably not", "always".
        if (fApplies[i][id] >= kApplicAlways) {
            fApplies[i][id] =
                (ReformatApplies) (fApplies[i][id] | kApplicPreferred);
        }
    }
}

/*
 * Returns <0, 0, or >0 depending on whether app1 is worse, equal to,
 * or better than app2.
 */
int ReformatHolder::CompareApplies(ReformatApplies app1, ReformatApplies app2)
{
    if ((app1 & kApplicPrefMask) < (app2 & kApplicPrefMask))
        return -1;
    else if ((app1 & kApplicPrefMask) > (app2 & kApplicPrefMask))
        return 1;
    else return (app1 - app2);      // compare with "preferred" bit
}

/*
 * Find the best reformatter for the specified part of the loaded file.
 */
ReformatHolder::ReformatID ReformatHolder::FindBest(ReformatPart part)
{
    ReformatID bestID = kReformatUnknown;
    ReformatApplies bestApply = kApplicNot;
    ReformatApplies apply;
    int i;

    /* if the source couldn't be loaded, just return "raw" */
    if (fErrorBuf[part] != NULL)
        return kReformatRaw;

    /*
     * Use the best option, or an equivalent-valued option that has
     * the "preferred" flag set.
     */
    for (i = 0; i < kReformatMAX; i++) {
        apply = GetApplic(part, (ReformatID) i);
        if (CompareApplies(apply, bestApply) > 0) {
            bestApply = apply;
            bestID = (ReformatID) i;
        }
    }

    if (bestID == kReformatUnknown || bestApply == kApplicNot) {
        LOGW("Did you forget to call TestApplicability?");
        assert(false);
        return kReformatRaw;
    }

    LOGI("Best is %d at lvl=%d", bestID, bestApply);

    return bestID;
}

/*
 * Apply the requested formatter to the specified part.
 */
ReformatOutput* ReformatHolder::Apply(ReformatPart part, ReformatID id)
{
    ReformatOutput* pOutput;
    Reformat* pReformat;
    int result;

    if (id <= kReformatUnknown || id >= kReformatMAX ||
        part <= kPartUnknown || part >= kPartMAX)
    {
        LOGW("Invalid reformat request (part=%d id=%d)", part, id);
        assert(false);
        return NULL;
    }

    /* create a place for the output */
    pOutput = new ReformatOutput;
    if (pOutput == NULL) {
        assert(false);
        return NULL;     // alloc failure
    }

    /*
     * If the caller was unable to fill our source buffer, they will have
     * supplied us with an error message.  Return that instead of the data.
     */
    if (fErrorBuf[part] != NULL) {
        pOutput->SetTextBuf(fErrorBuf[part], strlen(fErrorBuf[part]), false);
        pOutput->SetOutputKind(ReformatOutput::kOutputErrorMsg);
        pOutput->SetFormatDescr(L"Error Message");
        return pOutput;
    }

    /*
     * Set the format description here, based on the id.  If the reformatter
     * fails we will need to change this, but it allows the reformatter to
     * override our label with its own in case it wants to indicate some
     * sort of sub-variant.
     */
    pOutput->SetFormatDescr(GetReformatName(id));

    /* create an instance of a reformatter */
    pReformat = GetReformatInstance(id);
    assert(pReformat != NULL);
    result = pReformat->Process(this, id, part, pOutput);
    delete pReformat;

    /*
     * If it fails, return a pointer to the source buffer.
     *
     * This commonly happens on zero-length files.  The chosen reformatter
     * rejects it, returns -1, and we return the source buffer.  Since even
     * zero-length files are guaranteed to have some sort of allocated
     * buffer, "pOutput" never points at NULL.  Unless, of course, a text
     * reformatter produces no output but still returns "success".
     */
    if (result < 0) {
        pOutput->SetTextBuf((char*)fSourceBuf[part], fSourceLen[part], false);
        pOutput->SetOutputKind(ReformatOutput::kOutputRaw);
        pOutput->SetFormatDescr(GetReformatName(kReformatRaw));
    }

    return pOutput;
}

/*
 * Get the appropriate input buffer.
 */
const uint8_t* ReformatHolder::GetSourceBuf(ReformatPart part) const
{
    if (part <= kPartUnknown || part >= kPartMAX) {
        assert(false);
        return NULL;
    }

    return fSourceBuf[part];
}

/*
 * Get the length of the appropriate input buffer.
 */
long ReformatHolder::GetSourceLen(ReformatPart part) const
{
    if (part <= kPartUnknown || part >= kPartMAX) {
        assert(false);
        return NULL;
    }

    return fSourceLen[part];
}

/*
 * Set the input buffer.
 *
 * A buffer is required, even for empty input.  This makes the overall
 * housekeeping simpler.
 *
 * The ReformatHolder "owns" the buffer afterward, so the caller should
 * discard its pointer.
 */
void ReformatHolder::SetSourceBuf(ReformatPart part, uint8_t* buf, long len)
{
    if (part <= kPartUnknown || part >= kPartMAX) {
        assert(false);
        return;
    }
    assert(buf != NULL);
    assert(len >= 0);

    fSourceBuf[part] = buf;
    fSourceLen[part] = len;
}

/*
 * Specify an error message to return instead of reformatted text for a
 * given part.
 */
void ReformatHolder::SetErrorMsg(ReformatPart part, const char* msg)
{
    assert(msg != NULL && *msg != '\0');
    assert(part > kPartUnknown && part < kPartMAX);
    assert(fErrorBuf[part] == NULL);

    fErrorBuf[part] = strdup(msg);
    LOGI("+++ set error message for part %d to '%hs'", part, msg);
}

void ReformatHolder::SetErrorMsg(ReformatPart part, const CString& str)
{
    CStringA stra(str);
    SetErrorMsg(part, (LPCSTR) stra);
}
