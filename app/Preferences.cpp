/*
 * CiderPress
 * Copyright (C) 2007 by faddenSoft, LLC.  All Rights Reserved.
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

static const char* kDefaultTempPath = ".";

/* registry section for columns */
static const char* kColumnSect = _T("columns");
/* registry section for file add options */
static const char* kAddSect = _T("add");
/* registry section for extraction options */
static const char* kExtractSect = _T("extract");
/* registry section for view options */
static const char* kViewSect = _T("view");
/* registry section for logical/physical volume operations */
static const char* kVolumeSect = _T("volume");
/* registry section for file-to-disk options */
//static const char* kConvDiskSect = _T("conv-disk");
/* registry section for disk-to-file options */
static const char* kConvFileSect = _T("conv-file");
/* registry section for folders */
static const char* kFolderSect = _T("folders");
/* registry section for preferences on property pages */
static const char* kPrefsSect = _T("prefs");
/* registry section for miscellaneous settings */
static const char* kMiscSect = _T("misc");


/*
 * Map PrefNum to type and registry string.
 *
 * To make life easier, we require that the PrefNum (first entry) match the
 * offset in the table.  That way instead of searching for a match we can just
 * index into the table.
 */
const Preferences::PrefMap Preferences::fPrefMaps[kPrefNumLastEntry] = {
	/**/ { kPrefNumUnknown,				kPTNone, nil,			nil },

	{ kPrAddIncludeSubFolders,			kBool,	kAddSect,		_T("include-sub-folders") },
	{ kPrAddStripFolderNames,			kBool,	kAddSect,		_T("strip-folder-names") },
	{ kPrAddOverwriteExisting,			kBool,	kAddSect,		_T("overwrite-existing") },
	{ kPrAddTypePreservation,			kLong,	kAddSect,		_T("type-preservation") },
	{ kPrAddConvEOL,					kLong,	kAddSect,		_T("conv-eol") },

//	{ kPrExtractPath,					kString, kExtractSect,	_T("path") },
	{ kPrExtractConvEOL,				kLong,	kExtractSect,	_T("conv-eol") },
	{ kPrExtractConvHighASCII,			kBool,	kExtractSect,	_T("conv-high-ascii") },
	{ kPrExtractIncludeData,			kBool,	kExtractSect,	_T("include-data") },
	{ kPrExtractIncludeRsrc,			kBool,	kExtractSect,	_T("include-rsrc") },
	{ kPrExtractIncludeDisk,			kBool,	kExtractSect,	_T("include-disk") },
	{ kPrExtractEnableReformat,			kBool,	kExtractSect,	_T("enable-reformat") },
	{ kPrExtractDiskTo2MG,				kBool,	kExtractSect,	_T("disk-to-2mg") },
	{ kPrExtractAddTypePreservation,	kBool,	kExtractSect,	_T("add-type-preservation") },
	{ kPrExtractAddExtension,			kBool,	kExtractSect,	_T("add-extension") },
	{ kPrExtractStripFolderNames,		kBool,	kExtractSect,	_T("strip-folder-names") },
	{ kPrExtractOverwriteExisting,		kBool,	kExtractSect,	_T("overwrite-existing") },

//	{ kPrViewIncludeDataForks,			kBool,	kViewSect,		_T("include-data-forks") },
//	{ kPrViewIncludeRsrcForks,			kBool,	kViewSect,		_T("include-rsrc-forks") },
//	{ kPrViewIncludeDiskImages,			kBool,	kViewSect,		_T("include-disk-images") },
//	{ kPrViewIncludeComments,			kBool,	kViewSect,		_T("include-comments") },

	{ kPrConvFileEmptyFolders,			kBool,	kConvFileSect,	_T("preserve-empty-folders") },

	{ kPrOpenArchiveFolder,				kString, kFolderSect,	_T("open-archive") },
	{ kPrConvertArchiveFolder,			kString, kFolderSect,	_T("convert-archive") },
	{ kPrAddFileFolder,					kString, kFolderSect,	_T("add-file") },
	{ kPrExtractFileFolder,				kString, kFolderSect,	_T("extract-file") },

	{ kPrVolumeFilter,					kLong,	kVolumeSect,	_T("open-filter") },
	//{ kPrVolumeReadOnly,				kBool,	kVolumeSect,	_T("read-only") },

	{ kPrCassetteAlgorithm,				kLong,	kVolumeSect,	_T("cassette-algorithm") },
	{ kPrOpenWAVFolder,					kString, kFolderSect,	_T("open-wav") },

	{ kPrMimicShrinkIt,					kBool,	kPrefsSect,		_T("mimic-shrinkit") },
	{ kPrBadMacSHK,						kBool,	kPrefsSect,		_T("bad-mac-shk") },
	{ kPrReduceSHKErrorChecks,			kBool,	kPrefsSect,		_T("reduce-shk-error-checks") },
	{ kPrCoerceDOSFilenames,			kBool,	kPrefsSect,		_T("coerce-dos-filenames") },
	{ kPrSpacesToUnder,					kBool,	kPrefsSect,		_T("spaces-to-under") },
	{ kPrPasteJunkPaths,				kBool,	kPrefsSect,		_T("paste-junk-paths") },
	{ kPrBeepOnSuccess,					kBool,	kPrefsSect,		_T("beep-on-success") },

	{ kPrQueryImageFormat,				kBool,	kPrefsSect,		_T("query-image-format") },
	{ kPrOpenVolumeRO,					kBool,	kPrefsSect,		_T("open-volume-ro") },
	{ kPrOpenVolumePhys0,				kBool,	kPrefsSect,		_T("open-volume-phys0") },
	{ kPrProDOSAllowLower,				kBool,	kPrefsSect,		_T("prodos-allow-lower") },
	{ kPrProDOSUseSparse,				kBool,	kPrefsSect,		_T("prodos-use-sparse") },

	{ kPrCompressionType,				kLong,	kPrefsSect,		_T("compression-type") },

	{ kPrMaxViewFileSize,				kLong,	kPrefsSect,		_T("max-view-file-size") },
	{ kPrNoWrapText,					kBool,	kPrefsSect,		_T("no-wrap-text") },

	{ kPrHighlightHexDump,				kBool,	kPrefsSect,		_T("highlight-hex-dump") },
	{ kPrHighlightBASIC,				kBool,	kPrefsSect,		_T("highlight-basic") },
	{ kPrConvHiResBlackWhite,			kBool,	kPrefsSect,		_T("conv-hi-res-black-white") },
	{ kPrConvDHRAlgorithm,				kLong,	kPrefsSect,		_T("dhr-algorithm") },
	{ kPrRelaxGfxTypeCheck,				kBool,	kPrefsSect,		_T("relax-gfx-type-check") },
	{ kPrDisasmOneByteBrkCop,			kBool,	kPrefsSect,		_T("disasm-onebytebrkcop") },
	//{ kPrEOLConvRaw,					kBool,	kPrefsSect,		_T("eol-conv-raw") },
	{ kPrConvTextEOL_HA,				kBool,	kPrefsSect,		_T("conv-eol-ha") },
	{ kPrConvPascalText,				kBool,	kPrefsSect,		_T("conv-pascal-text") },
	{ kPrConvPascalCode,				kBool,	kPrefsSect,		_T("conv-pascal-code") },
	{ kPrConvCPMText,					kBool,	kPrefsSect,		_T("conv-cpm-text") },
	{ kPrConvApplesoft,					kBool,	kPrefsSect,		_T("conv-applesoft") },
	{ kPrConvInteger,					kBool,	kPrefsSect,		_T("conv-integer") },
	{ kPrConvGWP,						kBool,	kPrefsSect,		_T("conv-gwp") },
	{ kPrConvText8,						kBool,	kPrefsSect,		_T("conv-text8") },
	{ kPrConvAWP,						kBool,	kPrefsSect,		_T("conv-awp") },
	{ kPrConvADB,						kBool,	kPrefsSect,		_T("conv-adb") },
	{ kPrConvASP,						kBool,	kPrefsSect,		_T("conv-asp") },
	{ kPrConvSCAssem,					kBool,	kPrefsSect,		_T("conv-scassem") },
	{ kPrConvDisasm,					kBool,	kPrefsSect,		_T("conv-disasm") },
	{ kPrConvHiRes,						kBool,	kPrefsSect,		_T("conv-hi-res") },
	{ kPrConvDHR,						kBool,	kPrefsSect,		_T("conv-dhr") },
	{ kPrConvSHR,						kBool,	kPrefsSect,		_T("conv-shr") },
	{ kPrConvPrintShop,					kBool,	kPrefsSect,		_T("conv-print-shop") },
	{ kPrConvMacPaint,					kBool,	kPrefsSect,		_T("conv-mac-paint") },
	{ kPrConvProDOSFolder,				kBool,	kPrefsSect,		_T("conv-prodos-folder") },
	{ kPrConvResources,					kBool,	kPrefsSect,		_T("conv-resources") },

	{ kPrTempPath,						kString, kPrefsSect,	_T("temp-path") },
	{ kPrExtViewerExts,					kString, kPrefsSect,	_T("extviewer-exts") },

	{ kPrLastOpenFilterIndex,			kLong,	kMiscSect,		_T("open-filter-index") },

	/**/ { kPrefNumLastRegistry,		kPTNone, nil,			nil },

	{ kPrViewTextTypeFace,				kString, nil,			nil },
	{ kPrViewTextPointSize,				kLong,	nil,			nil },
	{ kPrFileViewerWidth,				kLong,	nil,			nil },
	{ kPrFileViewerHeight,				kLong,	nil,			nil },
	{ kPrDiskImageCreateFormat,			kLong,	nil,			nil },
};

/*
 * Constructor.  There should be only one Preferences object in the
 * application, so this should only be run once.
 */
Preferences::Preferences(void)
{
	WMSG0("Initializing Preferences\n");

	ScanPrefMaps();		// sanity-check the table
	memset(fValues, 0, sizeof(fValues));

	SetPrefBool(kPrAddIncludeSubFolders, true);
	SetPrefBool(kPrAddStripFolderNames, false);
	SetPrefBool(kPrAddOverwriteExisting, false);
	SetPrefLong(kPrAddTypePreservation, 1);		// kPreserveTypes
	SetPrefLong(kPrAddConvEOL, 1);				// kConvEOLType

	InitFolders();	// set default add/extract folders; overriden by reg
	SetPrefLong(kPrExtractConvEOL, 0);			// kConvEOLNone
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

//	SetPrefBool(kPrViewIncludeDataForks, true);
//	SetPrefBool(kPrViewIncludeRsrcForks, false);
//	SetPrefBool(kPrViewIncludeDiskImages, false);
//	SetPrefBool(kPrViewIncludeComments, false);

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

	SetPrefLong(kPrMaxViewFileSize, 1024*1024);	// 1MB
	SetPrefBool(kPrNoWrapText, false);

	SetPrefBool(kPrHighlightHexDump, false);
	SetPrefBool(kPrHighlightBASIC, false);
	SetPrefBool(kPrConvHiResBlackWhite, false);
	SetPrefLong(kPrConvDHRAlgorithm, 1);		// latched
	SetPrefBool(kPrRelaxGfxTypeCheck, true);
	SetPrefBool(kPrDisasmOneByteBrkCop, false);
	//SetPrefBool(kPrEOLConvRaw, true);
	SetPrefBool(kPrConvTextEOL_HA, true);
	SetPrefBool(kPrConvPascalText, true);
	SetPrefBool(kPrConvPascalCode, true);
	SetPrefBool(kPrConvCPMText, true);
	SetPrefBool(kPrConvApplesoft, true);
	SetPrefBool(kPrConvInteger, true);
	SetPrefBool(kPrConvGWP, true);
	SetPrefBool(kPrConvText8, true);
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

	InitTempPath();		// set default for kPrTempPath
	SetPrefString(kPrExtViewerExts, "gif; jpg; jpeg");

	SetPrefLong(kPrLastOpenFilterIndex, 0);

	SetPrefString(kPrViewTextTypeFace, "Courier New");
	SetPrefLong(kPrViewTextPointSize, 10);
	long width = 680;	/* exact width for 80-column text */
	long height = 510;	/* exact height for file viewer to show IIgs graphic */
	if (GetSystemMetrics(SM_CXSCREEN) < width)
		width = GetSystemMetrics(SM_CXSCREEN);
	if (GetSystemMetrics(SM_CYSCREEN) < height)
		height = GetSystemMetrics(SM_CYSCREEN);	// may overlap system bar
	//width = 640; height = 480;
	SetPrefLong(kPrFileViewerWidth, width);
	SetPrefLong(kPrFileViewerHeight, height);
	SetPrefLong(kPrDiskImageCreateFormat, -1);
}


/*
 * ==========================================================================
 *		ColumnLayout
 * ==========================================================================
 */

/*
 * Restore column widths.
 */
void
ColumnLayout::LoadFromRegistry(const char* section)
{
	char numBuf[8];
	int i;

	for (i = 0; i < kNumVisibleColumns; i++) {
		sprintf(numBuf, "%d", i);

		fColumnWidth[i] = gMyApp.GetProfileInt(section, numBuf,
							fColumnWidth[i]);
		fColumnWidth[i] = gMyApp.GetProfileInt(section, numBuf,
							fColumnWidth[i]);
	}
	fSortColumn = gMyApp.GetProfileInt(section, _T("sort-column"), fSortColumn);
	fAscending = (gMyApp.GetProfileInt(section, _T("ascending"), fAscending) != 0);
}

/*
 * Store column widths.
 */
void
ColumnLayout::SaveToRegistry(const char* section)
{
	char numBuf[8];
	int i;

	for (i = 0; i < kNumVisibleColumns; i++) {
		sprintf(numBuf, "%d", i);

		gMyApp.WriteProfileInt(section, numBuf, fColumnWidth[i]);
	}
	gMyApp.WriteProfileInt(section, _T("sort-column"), fSortColumn);
	gMyApp.WriteProfileInt(section, _T("ascending"), fAscending);
}


/*
 * ==========================================================================
 *		Preferences
 * ==========================================================================
 */

/*
 * Get a default value for the temp path.
 */
void
Preferences::InitTempPath(void)
{
	char buf[MAX_PATH];
	DWORD len;
	CString tempPath;

	len = ::GetTempPath(sizeof(buf), buf);
	if (len == 0) {
		DWORD err = ::GetLastError();
		WMSG1("GetTempPath failed, err=%d\n", err);
		tempPath = kDefaultTempPath;
	} else if (len >= sizeof(buf)) {
		/* sheesh! */
		WMSG1("GetTempPath wants a %d-byte buffer\n", len);
		tempPath = kDefaultTempPath;
	} else {
		tempPath = buf;
	}

	PathName path(tempPath);
	WMSG1("Temp path is '%s'\n", tempPath);
	path.SFNToLFN();
	tempPath = path.GetPathName();

	WMSG1("Temp path (long form) is '%s'\n", tempPath);

	SetPrefString(kPrTempPath, tempPath);

//	::GetFullPathName(fTempPath, sizeof(buf), buf, &foo);
//	::SetCurrentDirectory(buf);
//	::GetCurrentDirectory(sizeof(buf2), buf2);
}

/*
 * Set default values for the various folders.
 */
void
Preferences::InitFolders(void)
{
	CString path;

	if (GetMyDocuments(&path)) {
		SetPrefString(kPrOpenArchiveFolder, path);
		SetPrefString(kPrConvertArchiveFolder, path);
		SetPrefString(kPrAddFileFolder, path);
		SetPrefString(kPrExtractFileFolder, path);
		SetPrefString(kPrOpenWAVFolder, path);
	} else {
		char buf[MAX_PATH];
		::GetCurrentDirectory(sizeof(buf), buf);
		SetPrefString(kPrOpenArchiveFolder, buf);
		SetPrefString(kPrConvertArchiveFolder, buf);
		SetPrefString(kPrAddFileFolder, buf);
		SetPrefString(kPrExtractFileFolder, buf);
		SetPrefString(kPrOpenWAVFolder, buf);
	}

	WMSG1("Default folder is '%s'\n", GetPrefString(kPrExtractFileFolder));
}

/*
 * Get the path to the "My Documents" folder.
 */
bool
Preferences::GetMyDocuments(CString* pPath)
{
	LPITEMIDLIST pidl = nil;
	LPMALLOC lpMalloc = nil;
	HRESULT hr;
	bool result = false;

	hr = ::SHGetMalloc(&lpMalloc);
	if (FAILED(hr))
	   return nil;

	hr = SHGetSpecialFolderLocation(nil, CSIDL_PERSONAL, &pidl);
	if (FAILED(hr)) {
		WMSG0("WARNING: unable to get CSIDL_PERSONAL\n");
		goto bail;
	}

	result = (Pidl::GetPath(pidl, pPath) != FALSE);
	if (!result) {
		WMSG0("WARNING: unable to convert CSIDL_PERSONAL to path\n");
		/* fall through with "result" */
	}

bail:
	lpMalloc->Free(pidl);
	lpMalloc->Release();
	return result;
}

/*
 * Determine the type of compression to use as a default, based on what this
 * version of NufxLib supports.
 *
 * Note this happens *before* the AppInit call, so we should restrict this to
 * things that are version-safe for all of NufxLib v2.x.
 */
int
Preferences::DefaultCompressionType(void)
{
	if (NufxArchive::IsCompressionSupported(kNuThreadFormatLZW2))
		return kNuThreadFormatLZW2;
	else
		return kNuThreadFormatUncompressed;
}

/*
 * Preference getters and setters.
 */
bool
Preferences::GetPrefBool(PrefNum num) const
{
	if (!ValidateEntry(num, kBool))
		return false;
	//return (bool) (fValues[num]);
	return (bool) ((long) (fValues[num]) != 0);
}
void
Preferences::SetPrefBool(PrefNum num, bool val)
{
	if (!ValidateEntry(num, kBool))
		return;
	fValues[num] = (void*) val;
}
long
Preferences::GetPrefLong(PrefNum num) const
{
	if (!ValidateEntry(num, kLong))
		return -1;
	return (long) fValues[num];
}
void
Preferences::SetPrefLong(PrefNum num, long val)
{
	if (!ValidateEntry(num, kLong))
		return;
	fValues[num] = (void*) val;
}
const char*
Preferences::GetPrefString(PrefNum num) const
{
	if (!ValidateEntry(num, kString))
		return nil;
	return (const char*) fValues[num];
}
void
Preferences::SetPrefString(PrefNum num, const char* str)
{
	if (!ValidateEntry(num, kString))
		return;
	free(fValues[num]);
	if (str == nil)
		fValues[num] = nil;
	else {
		fValues[num] = new char[strlen(str) +1];
		if (fValues[num] != nil)
			strcpy((char*)fValues[num], str);
	}
}

/*
 * Free storage for any string entries.
 */
void
Preferences::FreeStringValues(void)
{
	int i;

	for (i = 0; i < kPrefNumLastEntry; i++) {
		if (fPrefMaps[i].type == kString) {
			delete[] fValues[i];
		}
	}
}


/*
 * Do a quick scan of the PrefMaps to identify duplicate, misplaced, and
 * missing entries.
 */
void
Preferences::ScanPrefMaps(void)
{
	int i, j;

	/* scan PrefNum */
	for (i = 0; i < kPrefNumLastEntry; i++) {
		if (fPrefMaps[i].num != i) {
			WMSG2("HEY: PrefMaps[%d] has num=%d\n", i, fPrefMaps[i].num);
			ASSERT(false);
			break;
		}
	}

	/* look for duplicate strings */
	for (i = 0; i < kPrefNumLastEntry; i++) {
		for (j = i+1; j < kPrefNumLastEntry; j++) {
			if (fPrefMaps[i].registryKey == nil ||
				fPrefMaps[j].registryKey == nil)
			{
				continue;
			}
			if (strcasecmp(fPrefMaps[i].registryKey,
						   fPrefMaps[j].registryKey) == 0 &&
				strcasecmp(fPrefMaps[i].registrySection,
						   fPrefMaps[j].registrySection) == 0)
			{
				WMSG4("HEY: PrefMaps[%d] and [%d] both have '%s'/'%s'\n",
					i, j, fPrefMaps[i].registrySection,
					fPrefMaps[i].registryKey);
				ASSERT(false);
				break;
			}
		}
	}
}

/*
 * Load preferences from the registry.
 */
int
Preferences::LoadFromRegistry(void)
{
	CString sval;
	bool bval;
	long lval;

	WMSG0("Loading preferences from registry\n");

	fColumnLayout.LoadFromRegistry(kColumnSect);

	int i;
	for (i = 0; i < kPrefNumLastRegistry; i++) {
		if (fPrefMaps[i].registryKey == nil)
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
			WMSG2("Invalid type %d on num=%d\n", fPrefMaps[i].type, i);
			ASSERT(false);
			break;
		}
	}

	return 0;
}

/*
 * Save preferences to the registry.
 */
int
Preferences::SaveToRegistry(void)
{
	WMSG0("Saving preferences to registry\n");

	fColumnLayout.SaveToRegistry(kColumnSect);

	int i;
	for (i = 0; i < kPrefNumLastRegistry; i++) {
		if (fPrefMaps[i].registryKey == nil)
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
			WMSG2("Invalid type %d on num=%d\n", fPrefMaps[i].type, i);
			ASSERT(false);
			break;
		}
	}

	return 0;
}
