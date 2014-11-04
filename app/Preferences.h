/*
 * CiderPress
 * Copyright (C) 2007, 2008 by faddenSoft, LLC.  All Rights Reserved.
 * See the file LICENSE for distribution terms.
 */
/*
 * Keep track of user preferences.
 *
 * How to add a new preference item:
 *  - Add an entry to the PrefNum enum, below.
 *  - Add a corresponding entry to Preferences::fPrefMaps, adding a new
 *    section to the registry if appropriate.
 *  - Add a default value to Preferences::Preferences.  If not specified,
 *    strings will be nil and numeric values will be zero.
 */
#ifndef __PREFERENCES__
#define __PREFERENCES__

#include "MyApp.h"

class ContentList;

/*
 * Number of visible columns.  (We no longer have "invisible" columns, so the
 * name is somewhat misleading.)
 *
 * This is used widely.  Update with care.
 */
const int kNumVisibleColumns = 9;

/*
 * Used to save & restore column layout and sorting preferences for
 * the ContentList class.
 */
class ColumnLayout {
public:
    ColumnLayout(void) {
        for (int i = 0; i < kNumVisibleColumns; i++)
            fColumnWidth[i] = kWidthDefaulted;
        fSortColumn = kNumVisibleColumns;   // means "use original order"
        fAscending = true;
    }
    ~ColumnLayout(void) {}

    void LoadFromRegistry(const char* section);
    void SaveToRegistry(const char* section);

    int GetColumnWidth(int col) const {
        ASSERT(col >= 0 && col < kNumVisibleColumns);
        return fColumnWidth[col];
    }
    void SetColumnWidth(int col, int width) {
        ASSERT(col >= 0 && col < kNumVisibleColumns);
        ASSERT(width >= 0 || width == kWidthDefaulted);
        fColumnWidth[col] = width;
    }

    int GetSortColumn(void) const { return fSortColumn; }
    void SetSortColumn(int col) {
        ASSERT(col >= 0 && col <= kNumVisibleColumns);
        fSortColumn = col;
    }
    bool GetAscending(void) const { return fAscending; }
    void SetAscending(bool val) { fAscending = val; }

    /* column width value used to flag "defaulted" status */
    enum { kWidthDefaulted = -1 };
    /* minimium width of column 0 (pathname) */
    enum { kMinCol0Width = 50 };

private:
    // Create a dummy list control to get default column widths.
    void DetermineDefaultWidths(ContentList* pList);

    int     fColumnWidth[kNumVisibleColumns];
    int     fSortColumn;
    bool    fAscending;
};


/*
 * Preferences type enumeration.
 *
 * This is logically part of the Preferences object, but it's annoying to
 * have to specify the scope resolution operator everywhere.
 */
typedef enum {
    /**/ kPrefNumUnknown = 0,

/* these are saved in the registry */

    // sticky settings for add file options
    kPrAddIncludeSubFolders,        // bool
    kPrAddStripFolderNames,         // bool
    kPrAddOverwriteExisting,        // bool
    kPrAddTypePreservation,         // long
    kPrAddConvEOL,                  // long

    // sticky settings for file extraction
    //kPrExtractPath,                   // string
    kPrExtractConvEOL,              // long
    kPrExtractConvHighASCII,        // bool
    kPrExtractIncludeData,          // bool
    kPrExtractIncludeRsrc,          // bool
    kPrExtractIncludeDisk,          // bool
    kPrExtractEnableReformat,       // bool
    kPrExtractDiskTo2MG,            // bool
    kPrExtractAddTypePreservation,  // bool
    kPrExtractAddExtension,         // bool
    kPrExtractStripFolderNames,     // bool
    kPrExtractOverwriteExisting,    // bool

//  // view file options
//  kPrViewIncludeDataForks,        // bool
//  kPrViewIncludeRsrcForks,        // bool
//  kPrViewIncludeDiskImages,       // bool
//  kPrViewIncludeComments,         // bool

    // convert disk image to file archive
    //kPrConvFileConvDOSText,           // bool
    //kPrConvFileConvPascalText,        // bool
    kPrConvFileEmptyFolders,        // bool

    // folders for CFileDialog initialization
    kPrOpenArchiveFolder,           // string
    kPrConvertArchiveFolder,        // string
    kPrAddFileFolder,               // string
    kPrExtractFileFolder,           // string

    // logical/physical volume prefs
    kPrVolumeFilter,                // long
    //kPrVolumeReadOnly,                // bool

    // cassette import/export prefs
    kPrCassetteAlgorithm,           // long
    kPrOpenWAVFolder,               // string

    // items from the Preferences propertypages (must be saved/restored)
    kPrMimicShrinkIt,               // bool
    kPrBadMacSHK,                   // bool
    kPrReduceSHKErrorChecks,        // bool
    kPrCoerceDOSFilenames,          // bool
    kPrSpacesToUnder,               // bool
    kPrPasteJunkPaths,              // bool
    kPrBeepOnSuccess,               // bool

    kPrQueryImageFormat,            // bool
    kPrOpenVolumeRO,                // bool
    kPrOpenVolumePhys0,             // bool
    kPrProDOSAllowLower,            // bool
    kPrProDOSUseSparse,             // bool

    kPrCompressionType,             // long

    kPrMaxViewFileSize,             // long
    kPrNoWrapText,                  // bool

    kPrHighlightHexDump,            // bool
    kPrHighlightBASIC,              // bool
    kPrConvHiResBlackWhite,         // bool
    kPrConvDHRAlgorithm,            // long
    kPrRelaxGfxTypeCheck,           // bool
    kPrDisasmOneByteBrkCop,         // bool
    //kPrEOLConvRaw,                    // bool
    kPrConvTextEOL_HA,              // bool
    kPrConvPascalText,              // bool
    kPrConvPascalCode,              // bool
    kPrConvCPMText,                 // bool
    kPrConvApplesoft,               // bool
    kPrConvInteger,                 // bool
    kPrConvBusiness,                // bool
    kPrConvGWP,                     // bool
    kPrConvText8,                   // bool
    kPrConvGutenberg,               // bool
    kPrConvAWP,                     // bool
    kPrConvADB,                     // bool
    kPrConvASP,                     // bool
    kPrConvSCAssem,                 // bool
    kPrConvDisasm,                  // bool
    kPrConvHiRes,                   // bool
    kPrConvDHR,                     // bool
    kPrConvSHR,                     // bool
    kPrConvPrintShop,               // bool
    kPrConvMacPaint,                // bool
    kPrConvProDOSFolder,            // bool
    kPrConvResources,               // bool

    kPrTempPath,                    // string
    kPrExtViewerExts,               // string

    // open file dialog
    kPrLastOpenFilterIndex,         // long

    /**/ kPrefNumLastRegistry,
/* these are temporary settings, not saved in the registry */

    // sticky settings for internal file viewer (ViewFilesDialog)
    kPrViewTextTypeFace,            // string
    kPrViewTextPointSize,           // long
    kPrFileViewerWidth,             // long
    kPrFileViewerHeight,            // long

    // sticky setting for disk image creator
    kPrDiskImageCreateFormat,       // long

    /**/ kPrefNumLastEntry
} PrefNum;


/*
 * Container for preferences.
 */
class Preferences {
public:
    Preferences(void);
    ~Preferences(void) {
        FreeStringValues();
    }

    // Load/save preferences from/to registry.
    int LoadFromRegistry(void);
    int SaveToRegistry(void);

    ColumnLayout* GetColumnLayout(void) { return &fColumnLayout; }
    //bool GetShowToolbarText(void) const { return fShowToolbarText; }
    //void SetShowToolbarText(bool val) { fShowToolbarText = val; }

    bool GetPrefBool(PrefNum num) const;
    void SetPrefBool(PrefNum num, bool val);
    long GetPrefLong(PrefNum num) const;
    void SetPrefLong(PrefNum num, long val);
    const char* GetPrefString(PrefNum num) const;
    void SetPrefString(PrefNum num, const char* str);


private:
    void InitTempPath(void);
    void InitFolders(void);
    bool GetMyDocuments(CString* pPath);
    int DefaultCompressionType(void);
    void FreeStringValues(void);

    /*
     * Internal data structure used to manage preferences.
     */
    typedef enum { kPTNone, kBool, kLong, kString } PrefType;
    typedef struct PrefMap {
        PrefNum     num;
        PrefType    type;
        const char* registrySection;
        const char* registryKey;
    } PrefMap;
    static const PrefMap fPrefMaps[kPrefNumLastEntry];
    void ScanPrefMaps(void);

    // this holds the actual values
    void*   fValues[kPrefNumLastEntry];

    // verify that the entry exists and has the expected type
    bool ValidateEntry(PrefNum num, PrefType type) const {
        if (num <= kPrefNumUnknown || num >= kPrefNumLastEntry) {
            ASSERT(false);
            return false;
        }
        if (fPrefMaps[num].type != type) {
            ASSERT(false);
            return false;
        }
        return true;
    }

    // column widths for ContentList
    ColumnLayout    fColumnLayout;

    /*
     * Registry helpers.
     */
    UINT GetInt(const char* section, const char* key, int dflt) {
        return gMyApp.GetProfileInt(section, key, dflt);
    }
    bool GetBool(const char* section, const char* key, bool dflt) {
        return (gMyApp.GetProfileInt(section, key, dflt) != 0);
    }
    CString GetString(const char* section, const char* key,
        const char* dflt)
    {
        return gMyApp.GetProfileString(section, key, dflt);
    }
    BOOL WriteInt(const char* section, const char* key, int value) {
        return gMyApp.WriteProfileInt(section, key, value);
    }
    BOOL WriteBool(const char* section, const char* key, bool value) {
        return gMyApp.WriteProfileInt(section, key, value);
    }
    BOOL WriteString(const char* section, const char* key, const char* value) {
        return gMyApp.WriteProfileString(section, key, value);
    }
};

#endif /*__PREFERENCES__*/