/*
 * CiderPress
 * Copyright (C) 2007, 2008 by faddenSoft, LLC.  All Rights Reserved.
 * See the file LICENSE for distribution terms.
 */
/*
 * Save and restore preferences from the config file.
 */
#include "stdafx.h"
#include "Preferences.h"
#include "NufxArchive.h"
#include "MyApp.h"
#include "../util/UtilLib.h"

static const WCHAR kDefaultTempPath[] = L".";

/* registry section for columns */
static const WCHAR kColumnSect[] = L"columns";
/* registry section for file add options */
static const WCHAR kAddSect[] = L"add";
/* registry section for extraction options */
static const WCHAR kExtractSect[] = L"extract";
/* registry section for view options */
static const WCHAR kViewSect[] = L"view";
/* registry section for logical/physical volume operations */
static const WCHAR kVolumeSect[] = L"volume";
/* registry section for file-to-disk options */
//static const WCHAR kConvDiskSect[] = L"conv-disk";
/* registry section for disk-to-file options */
static const WCHAR kConvFileSect[] = L"conv-file";
/* registry section for folders */
static const WCHAR kFolderSect[] = L"folders";
/* registry section for preferences on property pages */
static const WCHAR kPrefsSect[] = L"prefs";
/* registry section for miscellaneous settings */
static const WCHAR kMiscSect[] = L"misc";


/*
 * Map PrefNum to type and registry string.
 *
 * To make life easier, we require that the PrefNum (first entry) match the
 * offset in the table.  That way instead of searching for a match we can just
 * index into the table.
 */
const Preferences::PrefMap Preferences::fPrefMaps[kPrefNumLastEntry] = {
    /**/ { kPrefNumUnknown,             kPTNone, NULL,          NULL },

    { kPrAddIncludeSubFolders,          kBool,  kAddSect,       L"include-sub-folders" },
    { kPrAddStripFolderNames,           kBool,  kAddSect,       L"strip-folder-names" },
    { kPrAddOverwriteExisting,          kBool,  kAddSect,       L"overwrite-existing" },
    { kPrAddTypePreservation,           kLong,  kAddSect,       L"type-preservation" },
    { kPrAddConvEOL,                    kLong,  kAddSect,       L"conv-eol" },

//  { kPrExtractPath,                   kString, kExtractSect,  L"path" },
    { kPrExtractConvEOL,                kLong,  kExtractSect,   L"conv-eol" },
    { kPrExtractConvHighASCII,          kBool,  kExtractSect,   L"conv-high-ascii" },
    { kPrExtractIncludeData,            kBool,  kExtractSect,   L"include-data" },
    { kPrExtractIncludeRsrc,            kBool,  kExtractSect,   L"include-rsrc" },
    { kPrExtractIncludeDisk,            kBool,  kExtractSect,   L"include-disk" },
    { kPrExtractEnableReformat,         kBool,  kExtractSect,   L"enable-reformat" },
    { kPrExtractDiskTo2MG,              kBool,  kExtractSect,   L"disk-to-2mg" },
    { kPrExtractAddTypePreservation,    kBool,  kExtractSect,   L"add-type-preservation" },
    { kPrExtractAddExtension,           kBool,  kExtractSect,   L"add-extension" },
    { kPrExtractStripFolderNames,       kBool,  kExtractSect,   L"strip-folder-names" },
    { kPrExtractOverwriteExisting,      kBool,  kExtractSect,   L"overwrite-existing" },

//  { kPrViewIncludeDataForks,          kBool,  kViewSect,      L"include-data-forks" },
//  { kPrViewIncludeRsrcForks,          kBool,  kViewSect,      L"include-rsrc-forks" },
//  { kPrViewIncludeDiskImages,         kBool,  kViewSect,      L"include-disk-images" },
//  { kPrViewIncludeComments,           kBool,  kViewSect,      L"include-comments" },

    { kPrConvFileEmptyFolders,          kBool,  kConvFileSect,  L"preserve-empty-folders" },

    { kPrOpenArchiveFolder,             kString, kFolderSect,   L"open-archive" },
    { kPrConvertArchiveFolder,          kString, kFolderSect,   L"convert-archive" },
    { kPrAddFileFolder,                 kString, kFolderSect,   L"add-file" },
    { kPrExtractFileFolder,             kString, kFolderSect,   L"extract-file" },

    { kPrVolumeFilter,                  kLong,  kVolumeSect,    L"open-filter" },
    //{ kPrVolumeReadOnly,              kBool,  kVolumeSect,    L"read-only" },

    { kPrCassetteAlgorithm,             kLong,  kVolumeSect,    L"cassette-algorithm" },
    { kPrOpenWAVFolder,                 kString, kFolderSect,   L"open-wav" },

    { kPrMimicShrinkIt,                 kBool,  kPrefsSect,     L"mimic-shrinkit" },
    { kPrBadMacSHK,                     kBool,  kPrefsSect,     L"bad-mac-shk" },
    { kPrReduceSHKErrorChecks,          kBool,  kPrefsSect,     L"reduce-shk-error-checks" },
    { kPrCoerceDOSFilenames,            kBool,  kPrefsSect,     L"coerce-dos-filenames" },
    { kPrSpacesToUnder,                 kBool,  kPrefsSect,     L"spaces-to-under" },
    { kPrPasteJunkPaths,                kBool,  kPrefsSect,     L"paste-junk-paths" },
    { kPrBeepOnSuccess,                 kBool,  kPrefsSect,     L"beep-on-success" },

    { kPrQueryImageFormat,              kBool,  kPrefsSect,     L"query-image-format" },
    { kPrOpenVolumeRO,                  kBool,  kPrefsSect,     L"open-volume-ro" },
    { kPrOpenVolumePhys0,               kBool,  kPrefsSect,     L"open-volume-phys0" },
    { kPrProDOSAllowLower,              kBool,  kPrefsSect,     L"prodos-allow-lower" },
    { kPrProDOSUseSparse,               kBool,  kPrefsSect,     L"prodos-use-sparse" },

    { kPrCompressionType,               kLong,  kPrefsSect,     L"compression-type" },

    { kPrMaxViewFileSize,               kLong,  kPrefsSect,     L"max-view-file-size" },
    { kPrNoWrapText,                    kBool,  kPrefsSect,     L"no-wrap-text" },

    { kPrHighlightHexDump,              kBool,  kPrefsSect,     L"highlight-hex-dump" },
    { kPrHighlightBASIC,                kBool,  kPrefsSect,     L"highlight-basic" },
    { kPrConvHiResBlackWhite,           kBool,  kPrefsSect,     L"conv-hi-res-black-white" },
    { kPrConvDHRAlgorithm,              kLong,  kPrefsSect,     L"dhr-algorithm" },
    { kPrRelaxGfxTypeCheck,             kBool,  kPrefsSect,     L"relax-gfx-type-check" },
    { kPrDisasmOneByteBrkCop,           kBool,  kPrefsSect,     L"disasm-onebytebrkcop" },
    { kPrConvMouseTextToASCII,          kBool,  kPrefsSect,     L"conv-mouse-text-to-ascii" },
    //{ kPrEOLConvRaw,                  kBool,  kPrefsSect,     L"eol-conv-raw" },
    { kPrConvTextEOL_HA,                kBool,  kPrefsSect,     L"conv-eol-ha" },
    { kPrConvPascalText,                kBool,  kPrefsSect,     L"conv-pascal-text" },
    { kPrConvPascalCode,                kBool,  kPrefsSect,     L"conv-pascal-code" },
    { kPrConvCPMText,                   kBool,  kPrefsSect,     L"conv-cpm-text" },
    { kPrConvApplesoft,                 kBool,  kPrefsSect,     L"conv-applesoft" },
    { kPrConvInteger,                   kBool,  kPrefsSect,     L"conv-integer" },
    { kPrConvBusiness,                  kBool,  kPrefsSect,     L"conv-business" },
    { kPrConvGWP,                       kBool,  kPrefsSect,     L"conv-gwp" },
    { kPrConvText8,                     kBool,  kPrefsSect,     L"conv-text8" },
    { kPrConvGutenberg,                 kBool,  kPrefsSect,     L"conv-gutenberg" },
    { kPrConvAWP,                       kBool,  kPrefsSect,     L"conv-awp" },
    { kPrConvADB,                       kBool,  kPrefsSect,     L"conv-adb" },
    { kPrConvASP,                       kBool,  kPrefsSect,     L"conv-asp" },
    { kPrConvSCAssem,                   kBool,  kPrefsSect,     L"conv-scassem" },
    { kPrConvDisasm,                    kBool,  kPrefsSect,     L"conv-disasm" },
    { kPrConvHiRes,                     kBool,  kPrefsSect,     L"conv-hi-res" },
    { kPrConvDHR,                       kBool,  kPrefsSect,     L"conv-dhr" },
    { kPrConvSHR,                       kBool,  kPrefsSect,     L"conv-shr" },
    { kPrConvPrintShop,                 kBool,  kPrefsSect,     L"conv-print-shop" },
    { kPrConvMacPaint,                  kBool,  kPrefsSect,     L"conv-mac-paint" },
    { kPrConvProDOSFolder,              kBool,  kPrefsSect,     L"conv-prodos-folder" },
    { kPrConvResources,                 kBool,  kPrefsSect,     L"conv-resources" },

    { kPrTempPath,                      kString, kPrefsSect,    L"temp-path" },
    { kPrExtViewerExts,                 kString, kPrefsSect,    L"extviewer-exts" },

    { kPrLastOpenFilterIndex,           kLong,  kMiscSect,      L"open-filter-index" },

    /**/ { kPrefNumLastRegistry,        kPTNone, NULL,          NULL },

    { kPrViewTextTypeFace,              kString, NULL,          NULL },
    { kPrViewTextPointSize,             kLong,  NULL,           NULL },
    { kPrFileViewerWidth,               kLong,  NULL,           NULL },
    { kPrFileViewerHeight,              kLong,  NULL,           NULL },
    { kPrDiskImageCreateFormat,         kLong,  NULL,           NULL },
};

Preferences::Preferences(void)
{
    LOGI("Initializing Preferences");

    ScanPrefMaps();     // sanity-check the table
    memset(fValues, 0, sizeof(fValues));

    SetPrefBool(kPrAddIncludeSubFolders, true);
    SetPrefBool(kPrAddStripFolderNames, false);
    SetPrefBool(kPrAddOverwriteExisting, false);
    SetPrefLong(kPrAddTypePreservation, 1);     // kPreserveTypes
    SetPrefLong(kPrAddConvEOL, 1);              // kConvEOLType

    InitFolders();  // set default add/extract folders; overriden by reg
    SetPrefLong(kPrExtractConvEOL, 0);          // kConvEOLNone
    SetPrefBool(kPrExtractConvHighASCII, true);
    SetPrefBool(kPrExtractIncludeData, true);
    SetPrefBool(kPrExtractIncludeRsrc, false);
    SetPrefBool(kPrExtractIncludeDisk, true);
    SetPrefBool(kPrExtractEnableReformat, false);
    SetPrefBool(kPrExtractDiskTo2MG, false);
    SetPrefBool(kPrExtractAddTypePreservation, true);
    SetPrefBool(kPrExtractAddExtension, false);
    SetPrefBool(kPrExtractStripFolderNames, false);
    SetPrefBool(kPrExtractOverwriteExisting, false);

//  SetPrefBool(kPrViewIncludeDataForks, true);
//  SetPrefBool(kPrViewIncludeRsrcForks, false);
//  SetPrefBool(kPrViewIncludeDiskImages, false);
//  SetPrefBool(kPrViewIncludeComments, false);

    SetPrefBool(kPrConvFileEmptyFolders, true);

    // string  kPrOpenArchiveFolder
    // string  kPrAddFileFolder
    // string  kPrExtractFileFolder

    SetPrefLong(kPrVolumeFilter, 0);
    //SetPrefBool(kPrVolumeReadOnly, true);

    SetPrefLong(kPrCassetteAlgorithm, 0);
    // string  kPrOpenWAVFolder

    SetPrefBool(kPrMimicShrinkIt, false);
    SetPrefBool(kPrBadMacSHK, false);
    SetPrefBool(kPrReduceSHKErrorChecks, false);
    SetPrefBool(kPrCoerceDOSFilenames, false);
    SetPrefBool(kPrSpacesToUnder, false);
    SetPrefBool(kPrPasteJunkPaths, true);
    SetPrefBool(kPrBeepOnSuccess, true);

    SetPrefBool(kPrQueryImageFormat, false);
    SetPrefBool(kPrOpenVolumeRO, true);
    SetPrefBool(kPrOpenVolumePhys0, false);
    SetPrefBool(kPrProDOSAllowLower, false);
    SetPrefBool(kPrProDOSUseSparse, true);

    SetPrefLong(kPrCompressionType, DefaultCompressionType());

    SetPrefLong(kPrMaxViewFileSize, 1024*1024); // 1MB
    SetPrefBool(kPrNoWrapText, false);

    SetPrefBool(kPrHighlightHexDump, false);
    SetPrefBool(kPrHighlightBASIC, false);
    SetPrefBool(kPrConvHiResBlackWhite, false);
    SetPrefLong(kPrConvDHRAlgorithm, 1);        // latched
    SetPrefBool(kPrRelaxGfxTypeCheck, true);
    SetPrefBool(kPrDisasmOneByteBrkCop, false);
    SetPrefBool(kPrConvMouseTextToASCII, false);
    //SetPrefBool(kPrEOLConvRaw, true);
    SetPrefBool(kPrConvTextEOL_HA, true);
    SetPrefBool(kPrConvPascalText, true);
    SetPrefBool(kPrConvPascalCode, true);
    SetPrefBool(kPrConvCPMText, true);
    SetPrefBool(kPrConvApplesoft, true);
    SetPrefBool(kPrConvInteger, true);
    SetPrefBool(kPrConvBusiness, true);
    SetPrefBool(kPrConvGWP, true);
    SetPrefBool(kPrConvText8, true);
    SetPrefBool(kPrConvGutenberg, true);
    SetPrefBool(kPrConvAWP, true);
    SetPrefBool(kPrConvADB, true);
    SetPrefBool(kPrConvASP, true);
    SetPrefBool(kPrConvSCAssem, true);
    SetPrefBool(kPrConvDisasm, true);
    SetPrefBool(kPrConvHiRes, true);
    SetPrefBool(kPrConvDHR, true);
    SetPrefBool(kPrConvSHR, true);
    SetPrefBool(kPrConvPrintShop, true);
    SetPrefBool(kPrConvMacPaint, true);
    SetPrefBool(kPrConvProDOSFolder, true);
    SetPrefBool(kPrConvResources, true);

    InitTempPath();     // set default for kPrTempPath
    SetPrefString(kPrExtViewerExts, L"gif; jpg; jpeg");

    SetPrefLong(kPrLastOpenFilterIndex, 0);

    SetPrefString(kPrViewTextTypeFace, L"Courier New");
    SetPrefLong(kPrViewTextPointSize, 10);
    long width = 680 +  /* exact width for 80-column text */
            ::GetSystemMetrics(SM_CXVSCROLL);
    long height = 516;  /* exact height for file viewer to show IIgs graphic */
    if (GetSystemMetrics(SM_CXSCREEN) < width)
        width = GetSystemMetrics(SM_CXSCREEN);
    if (GetSystemMetrics(SM_CYSCREEN) < height)
        height = GetSystemMetrics(SM_CYSCREEN); // may overlap system bar
    //width = 640; height = 480;
    SetPrefLong(kPrFileViewerWidth, width);
    SetPrefLong(kPrFileViewerHeight, height);
    SetPrefLong(kPrDiskImageCreateFormat, -1);
}


/*
 * ==========================================================================
 *      ColumnLayout
 * ==========================================================================
 */

void ColumnLayout::LoadFromRegistry(const WCHAR* section)
{
    WCHAR numBuf[8];
    int i;

    for (i = 0; i < kNumVisibleColumns; i++) {
        wsprintf(numBuf, L"%d", i);

        fColumnWidth[i] = gMyApp.GetProfileInt(section, numBuf,
                            fColumnWidth[i]);
        fColumnWidth[i] = gMyApp.GetProfileInt(section, numBuf,
                            fColumnWidth[i]);
    }
    fSortColumn = gMyApp.GetProfileInt(section, L"sort-column", fSortColumn);
    fAscending = (gMyApp.GetProfileInt(section, L"ascending", fAscending) != 0);
}

void ColumnLayout::SaveToRegistry(const WCHAR* section)
{
    WCHAR numBuf[8];
    int i;

    for (i = 0; i < kNumVisibleColumns; i++) {
        wsprintf(numBuf, L"%d", i);

        gMyApp.WriteProfileInt(section, numBuf, fColumnWidth[i]);
    }
    gMyApp.WriteProfileInt(section, L"sort-column", fSortColumn);
    gMyApp.WriteProfileInt(section, L"ascending", fAscending);
}


/*
 * ==========================================================================
 *      Preferences
 * ==========================================================================
 */

void Preferences::InitTempPath(void)
{
    WCHAR buf[MAX_PATH];
    DWORD len;
    CString tempPath;

    len = ::GetTempPath(NELEM(buf), buf);
    if (len == 0) {
        DWORD err = ::GetLastError();
        LOGI("GetTempPath failed, err=%d", err);
        tempPath = kDefaultTempPath;
    } else if (len >= NELEM(buf)) {
        /* sheesh! */
        LOGI("GetTempPath wants a %d-unit buffer", len);
        tempPath = kDefaultTempPath;
    } else {
        tempPath = buf;
    }

    PathName path(tempPath);
    LOGD("Temp path is '%ls'", (LPCWSTR) tempPath);
    path.SFNToLFN();
    tempPath = path.GetPathName();

    LOGD("Temp path (long form) is '%ls'", (LPCWSTR) tempPath);

    SetPrefString(kPrTempPath, tempPath);

//  ::GetFullPathName(fTempPath, sizeof(buf), buf, &foo);
//  ::SetCurrentDirectory(buf);
//  ::GetCurrentDirectory(sizeof(buf2), buf2);
}

void Preferences::InitFolders(void)
{
    CString path;

    if (GetMyDocuments(&path)) {
        SetPrefString(kPrOpenArchiveFolder, path);
        SetPrefString(kPrConvertArchiveFolder, path);
        SetPrefString(kPrAddFileFolder, path);
        SetPrefString(kPrExtractFileFolder, path);
        SetPrefString(kPrOpenWAVFolder, path);
    } else {
        WCHAR buf[MAX_PATH];
        ::GetCurrentDirectory(NELEM(buf), buf);
        SetPrefString(kPrOpenArchiveFolder, buf);
        SetPrefString(kPrConvertArchiveFolder, buf);
        SetPrefString(kPrAddFileFolder, buf);
        SetPrefString(kPrExtractFileFolder, buf);
        SetPrefString(kPrOpenWAVFolder, buf);
    }

    LOGD("Default folder is '%ls'", GetPrefString(kPrExtractFileFolder));
}

bool Preferences::GetMyDocuments(CString* pPath)
{
    LPITEMIDLIST pidl = NULL;
    LPMALLOC lpMalloc = NULL;
    HRESULT hr;
    bool result = false;

    hr = ::SHGetMalloc(&lpMalloc);
    if (FAILED(hr))
       return NULL;

    hr = SHGetSpecialFolderLocation(NULL, CSIDL_PERSONAL, &pidl);
    if (FAILED(hr)) {
        LOGW("WARNING: unable to get CSIDL_PERSONAL");
        goto bail;
    }

    result = (Pidl::GetPath(pidl, pPath) != FALSE);
    if (!result) {
        LOGW("WARNING: unable to convert CSIDL_PERSONAL to path");
        /* fall through with "result" */
    }

bail:
    lpMalloc->Free(pidl);
    lpMalloc->Release();
    return result;
}

int Preferences::DefaultCompressionType(void)
{
    if (NufxArchive::IsCompressionSupported(kNuThreadFormatLZW2))
        return kNuThreadFormatLZW2;
    else
        return kNuThreadFormatUncompressed;
}

bool Preferences::GetPrefBool(PrefNum num) const
{
    if (!ValidateEntry(num, kBool))
        return false;
    //return (bool) (fValues[num]);
    return (bool) ((long) (fValues[num]) != 0);
}

void Preferences::SetPrefBool(PrefNum num, bool val)
{
    if (!ValidateEntry(num, kBool))
        return;
    fValues[num] = (void*) val;
}

long Preferences::GetPrefLong(PrefNum num) const
{
    if (!ValidateEntry(num, kLong))
        return -1;
    return (long) fValues[num];
}

void Preferences::SetPrefLong(PrefNum num, long val)
{
    if (!ValidateEntry(num, kLong))
        return;
    fValues[num] = (void*) val;
}

const WCHAR* Preferences::GetPrefString(PrefNum num) const
{
    if (!ValidateEntry(num, kString))
        return NULL;
    return (const WCHAR*) fValues[num];
}

void Preferences::SetPrefString(PrefNum num, const WCHAR* str)
{
    if (!ValidateEntry(num, kString))
        return;
    free(fValues[num]);
    if (str == NULL) {
        fValues[num] = NULL;
    } else {
        fValues[num] = wcsdup(str);
    }
}

void Preferences::FreeStringValues(void)
{
    for (int i = 0; i < kPrefNumLastEntry; i++) {
        if (fPrefMaps[i].type == kString) {
            delete[] fValues[i];
        }
    }
}

void Preferences::ScanPrefMaps(void)
{
    int i, j;

    /* scan PrefNum */
    for (i = 0; i < kPrefNumLastEntry; i++) {
        if (fPrefMaps[i].num != i) {
            LOGE("HEY: PrefMaps[%d] has num=%d", i, fPrefMaps[i].num);
            ASSERT(false);
            break;
        }
    }

    /* look for duplicate strings */
    for (i = 0; i < kPrefNumLastEntry; i++) {
        for (j = i+1; j < kPrefNumLastEntry; j++) {
            if (fPrefMaps[i].registryKey == NULL ||
                fPrefMaps[j].registryKey == NULL)
            {
                continue;
            }
            if (wcsicmp(fPrefMaps[i].registryKey,
                        fPrefMaps[j].registryKey) == 0 &&
                wcsicmp(fPrefMaps[i].registrySection,
                        fPrefMaps[j].registrySection) == 0)
            {
                LOGE("HEY: PrefMaps[%d] and [%d] both have '%ls'/'%ls'",
                    i, j, fPrefMaps[i].registrySection,
                    fPrefMaps[i].registryKey);
                ASSERT(false);
                break;
            }
        }
    }
}

int Preferences::LoadFromRegistry(void)
{
    CString sval;
    bool bval;
    long lval;

    LOGI("Loading preferences from registry");

    fColumnLayout.LoadFromRegistry(kColumnSect);

    int i;
    for (i = 0; i < kPrefNumLastRegistry; i++) {
        if (fPrefMaps[i].registryKey == NULL)
            continue;

        switch (fPrefMaps[i].type) {
        case kBool:
            bval = GetPrefBool(fPrefMaps[i].num);
            SetPrefBool(fPrefMaps[i].num,
                GetBool(fPrefMaps[i].registrySection, fPrefMaps[i].registryKey, bval));
            break;
        case kLong:
            lval = GetPrefLong(fPrefMaps[i].num);
            SetPrefLong(fPrefMaps[i].num,
                GetInt(fPrefMaps[i].registrySection, fPrefMaps[i].registryKey, lval));
            break;
        case kString:
            sval = GetPrefString(fPrefMaps[i].num);
            SetPrefString(fPrefMaps[i].num,
                GetString(fPrefMaps[i].registrySection, fPrefMaps[i].registryKey, sval));
            break;
        default:
            LOGW("Invalid type %d on num=%d", fPrefMaps[i].type, i);
            ASSERT(false);
            break;
        }
    }

    return 0;
}

int Preferences::SaveToRegistry(void)
{
    LOGI("Saving preferences to registry");

    fColumnLayout.SaveToRegistry(kColumnSect);

    int i;
    for (i = 0; i < kPrefNumLastRegistry; i++) {
        if (fPrefMaps[i].registryKey == NULL)
            continue;

        switch (fPrefMaps[i].type) {
        case kBool:
            WriteBool(fPrefMaps[i].registrySection, fPrefMaps[i].registryKey,
                GetPrefBool(fPrefMaps[i].num));
            break;
        case kLong:
            WriteInt(fPrefMaps[i].registrySection, fPrefMaps[i].registryKey,
                GetPrefLong(fPrefMaps[i].num));
            break;
        case kString:
            WriteString(fPrefMaps[i].registrySection, fPrefMaps[i].registryKey,
                GetPrefString(fPrefMaps[i].num));
            break;
        default:
            LOGW("Invalid type %d on num=%d", fPrefMaps[i].type, i);
            ASSERT(false);
            break;
        }
    }

    return 0;
}
