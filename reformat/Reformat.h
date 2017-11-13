/*
 * CiderPress
 * Copyright (C) 2009 by CiderPress authors.  All Rights Reserved.
 * Copyright (C) 2007, 2008 by faddenSoft, LLC.  All Rights Reserved.
 * See the file LICENSE for distribution terms.
 */
/*
 * File reformatter class.  Used to encapsulate working state and RTF
 * knowledge while rewriting a file.
 *
 * Currently missing: a way to provide progress updates when reformatting
 * a large file.
 */
#ifndef REFORMAT_REFORMAT_H
#define REFORMAT_REFORMAT_H

#include "../util/UtilLib.h"
#include <stdint.h>

class Reformat;
class ReformatOutput;


/*
 * This holds all three file parts (data, resource, comment) for use by the
 * reformatters.
 *
 * The "source" buffers are owned by this class, and will be freed when
 * the object is deleted.
 *
 * Typical calling sequence:
 *  - Prep:
 *   - Allocate object
 *   - Load parts into source buffers
 *   - MainWindow::ConfigureReformatFromPreferences()
 *   - SetSourceAttributes()
 *   - TestApplicability()
 *  - Action:
 *   - id = FindBest(part)
 *   - pOutput = Apply(part, id)
 *  - Cleanup:
 *   - Destroy ReformatOutput (when done with part)
 *   - Destroy ReformatHolder (when done with all parts)
 *
 * When adding new formatters:
 * Any additions here need to be reflected in the switch statements in
 * GetReformatInstance() and GetReformatName(), and added to the set of
 * things allowed by preferences in ConfigureReformatFromPreferences.
 * New classes must also be added to the list of friends, below.
 */
class ReformatHolder {
public:
    /*
     * Reformatters, including minor variants.
     *
     * TextEOL_HA and Raw must be the first two entries.  If you change these,
     * you may also need to adjust the way extraction is handled over in
     * DoBulkExtract.  The extract code depends on getting "raw" data back
     * for files that don't have a better reformatter.
     */
    typedef enum ReformatID {
        kReformatUnknown = 0,

        /* don't change the order of these! */
        kReformatTextEOL_HA,        // default for unknown types (if enabled)
        kReformatRaw,               // backup default
        kReformatHexDump,

        /* from here on, only order within sub-groups matters */
        kReformatResourceFork,

        kReformatPascalText,
        kReformatPascalCode,

        kReformatCPMText,

        kReformatApplesoft,
        kReformatApplesoft_Hilite,
        kReformatInteger,
        kReformatInteger_Hilite,
        kReformatBusiness,
        kReformatBusiness_Hilite,

        kReformatSCAssem,
        kReformatMerlin,
        kReformatLISA2,
        kReformatLISA3,
        kReformatLISA4,

        kReformatMonitor8,
        kReformatDisasmMerlin8,
        kReformatMonitor16Long,
        kReformatMonitor16Short,
        kReformatDisasmOrcam16,

        kReformatAWGS_WP,
        kReformatTeach,
        kReformatGWP,

        kReformatMagicWindow,
        kReformatGutenberg,

        kReformatAWP,
        kReformatADB,
        kReformatASP,

        kReformatHiRes,
        kReformatHiRes_BW,

        kReformatDHR_Latched,
        kReformatDHR_BW,
        kReformatDHR_Window,
        kReformatDHR_Plain140,

        kReformatProDOSDirectory,

        kReformatSHR_PIC,
        kReformatSHR_JEQ,
        kReformatSHR_Paintworks,
        kReformatSHR_Packed,
        kReformatSHR_APF,
        kReformatSHR_3200,
        kReformatSHR_3201,
        kReformatSHR_DG256,
        kReformatSHR_DG3200,

        kReformatPrintShop,

        kReformatMacPaint,

        kReformatMAX        // must be last
    } ReformatID;

    /*
     * Set options.  Each value is a "long".
     */
    typedef enum OptionID {
        kOptUnknown = 0,

        kOptHiliteHexDump,
        kOptHiliteBASIC,
        kOptHiResBW,
        kOptDHRAlgorithm,
        kOptRelaxGfxTypeCheck,
        kOptOneByteBrkCop,
        kOptMouseTextToASCII,

        kOptMAX             // must be last
    } OptionID;

    /*
     * Each reformatter examines the input and determines its applicability.
     * This ranges from "impossible" to being the preferred sub-variant of
     * a verified converter.
     *
     * Thought: could set kApplicPreferred to 0x01, use even values for
     * the enumeration, and then just do a numeric sort.
     */
    typedef enum ReformatApplies {
        kApplicUnknown      = 0,    // not set
        kApplicNot,                 // does not apply
        kApplicProbablyNot,         // allow, but don't use as default, ever
        kApplicAlways,              // generic, always-applicable (e.g. hex)
        kApplicMaybe,               // might work
        kApplicProbably,            // should work, based on size and type
        kApplicYes,                 // contents look right
        kApplicMAX,                 // must be at end, not including bits

        kApplicPreferred    = 0x80, // tag preferred variation
        kApplicPrefMask     = 0x7f, // mask off "preferred" flag
    } ReformatApplies;

    /* which part of the file are we targetting? */
    typedef enum ReformatPart {
        kPartUnknown = -1,
        kPartData = 0,
        kPartRsrc,
        kPartCmmt,
        kPartMAX        // must be last
    } ReformatPart;

    /*
     * This is more or less a restatement of DiskImg::FSFormat.  We do this so
     * we don't force everybody to include DiskImg.h, especially since there's
     * only a couple of interesting cases.
     *
     * We want to know if it's DOS so we can relax some file-type checking,
     * and we want to know if it's CP/M so we can adjust the way we think
     * about text files.  We want to know if it's Gutenberg because they only
     * have one type of file, and it's indistinguishable from any other text file!
     */
    typedef enum SourceFormat {
        kSourceFormatGeneric = 0,
        kSourceFormatDOS,
        kSourceFormatCPM,
        kSourceFormatGutenberg,
    } SourceFormat;


    /*
     * Construct/destruct our object.
     */
    ReformatHolder(void) {
        int i;
        for (int part = 0; part < kPartMAX; part++) {
            if (part == kPartUnknown)
                continue;
            for (i = 0; i < kReformatMAX; i++)
                fApplies[part][i] = kApplicUnknown;
            fSourceBuf[part] = NULL;
            fSourceLen[part] = NULL;
            fErrorBuf[part] = NULL;
        }
        for (i = 0; i < kReformatMAX; i++) {
            fAllow[i] = false;
        }
        for (i = 0; i < kOptMAX; i++)
            fOption[i] = 0;

        fFileType = fAuxType = 0;
        fSourceFormat = kSourceFormatGeneric;
        fNameExt = NULL;
    }
    ~ReformatHolder(void) {
        LOGI("In ~ReformatHolder");
        for (int i = 0; i < kPartMAX; i++) {
            if (i == kPartUnknown)
                continue;
            delete[] fSourceBuf[i];
            delete[] fErrorBuf[i];
        }
        delete[] fNameExt;
    }

    /**
     * Set attributes before calling TestApplicability.  We want the
     * Apple II filename, so the name extension is passed as a narrow string.
     */
    void SetSourceAttributes(long fileType, long auxType,
        SourceFormat sourceFormat, const char* nameExt);
    /* run through the list of reformatters, testing each against the data */
    void TestApplicability(void);
    /* get a ReformatApplies value */
    ReformatApplies GetApplic(ReformatPart part, ReformatID id) const;

    /* compare two ReformatApplies values */
    int CompareApplies(ReformatApplies app1, ReformatApplies app2);

    /* find the best reformatter for this part */
    ReformatID FindBest(ReformatPart part);
    /* apply the chosen reformatter */
    ReformatOutput* Apply(ReformatPart part, ReformatID id);


    /*
     * Getters & setters.
     */
    bool GetReformatAllowed(ReformatID id) const { return fAllow[id]; }
    void SetReformatAllowed(ReformatID id, bool val) { fAllow[id] = val; }
    long GetOption(OptionID id) const { return fOption[id]; }
    void SetOption(OptionID id, long val) { fOption[id] = val; }

    /* use this to force "reformatted" output to show an error instead */
    void SetErrorMsg(ReformatPart part, const char* msg);
    void SetErrorMsg(ReformatPart part, const CString& str);

    /* give a pointer (allocated with new[]) for one of the inputs */
    void SetSourceBuf(ReformatPart part, uint8_t* buf, long len);

    static const WCHAR* GetReformatName(ReformatID id);


    /* make these friends so they can call the "protected" stuff below */
    friend class ReformatText;
    friend class ReformatGraphics;
    friend class ReformatAWGS_WP;
    friend class ReformatTeach;
    friend class ReformatGWP;
    friend class ReformatMagicWindow;
    friend class ReformatGutenberg;
    friend class ReformatAWP;
    friend class ReformatADB;
    friend class ReformatASP;
    friend class ReformatSCAssem;
    friend class ReformatMerlin;
    friend class ReformatLISA2;
    friend class ReformatLISA3;
    friend class ReformatLISA4;
    friend class ReformatDisasm8;
    friend class ReformatDisasm16;
    friend class ReformatApplesoft;
    friend class ReformatInteger;
    friend class ReformatBusiness;
    friend class ReformatCPMText;
    friend class ReformatDirectory;
    friend class ReformatDHR;
    friend class ReformatHiRes;
    friend class ReformatMacPaint;
    friend class ReformatPascalCode;
    friend class ReformatPascalText;
    friend class ReformatResourceFork;
    friend class ReformatRaw;
    friend class ReformatHexDump;
    friend class ReformatEOL_HA;
    friend class ReformatUnpackedSHR;
    friend class ReformatJEQSHR;
    friend class ReformatPaintworksSHR;
    friend class ReformatPackedSHR;
    friend class ReformatAPFSHR;
    friend class Reformat3200SHR;
    friend class Reformat3201SHR;
    friend class DreamGrafix;
    friend class ReformatDG256SHR;
    friend class ReformatDG3200SHR;
    friend class ReformatPrintShop;

protected:
    /*
     * Functions for the use of reformatters.
     */
    /* set the applicability level */
    void SetApplic(ReformatID id, ReformatApplies applyData,
        ReformatApplies applyRsrc, ReformatApplies applyCmmt);
    /* set the "preferred" flag on all non-"not" entries */
    void SetApplicPreferred(ReformatID id, ReformatPart part = kPartUnknown);

    const uint8_t* GetSourceBuf(ReformatPart part) const;
    long GetSourceLen(ReformatPart part) const;

    long GetFileType(void) const { return fFileType; }
    long GetAuxType(void) const { return fAuxType; }
    SourceFormat GetSourceFormat(void) const { return fSourceFormat; }
    const char* GetNameExt(void) const { return fNameExt; }

private:
    DECLARE_COPY_AND_OPEQ(ReformatHolder)

    /*
     * Utility functions.
     */
    static Reformat* GetReformatInstance(ReformatID id);

    /* set by app: which reformatters are allowed? */
    bool            fAllow[kReformatMAX];

    /* set by app: various options */
    long            fOption[kOptMAX];

    /* set by TestApplicability: which tests work with this data? */
    ReformatApplies fApplies[kPartMAX][kReformatMAX];

    /* file attributes, used to determine applicability */
    long            fFileType;
    long            fAuxType;
    SourceFormat    fSourceFormat;
    char*           fNameExt;       // guaranteed non-NULL

    /* input goes here */
    uint8_t*         fSourceBuf[kPartMAX];
    long            fSourceLen[kPartMAX];

    char*           fErrorBuf[kPartMAX];
};

/*
 * This holds reformatted (or raw) output.
 */
class ReformatOutput {
public:
    /* what form does the reformatted data take */
    typedef enum OutputKind {
        kOutputUnknown = 0,
        kOutputRaw,         // reformatting not applied
        kOutputErrorMsg,    // text is an error message
        kOutputText,
        kOutputRTF,
        kOutputCSV,
        kOutputBitmap,
    } OutputKind;

    ReformatOutput(void) :
        fOutputKind(kOutputUnknown),
        //fOutputID
        fOutputFormatDescr(L"(none)"),
        fMultipleFonts(false),
        fTextBuf(NULL),
        fTextLen(-1),
        fDoDeleteText(true),
        fpDIB(NULL)
        {}
    virtual ~ReformatOutput(void) {
        if (fDoDeleteText)
            delete[] fTextBuf;
        delete fpDIB;
    }

    /*
     * Getters, used by all.
     */
    OutputKind GetOutputKind(void) const { return fOutputKind; }
    const char* GetTextBuf(void) const { return fTextBuf; }
    long GetTextLen(void) const { return fTextLen; }
    const MyDIBitmap* GetDIB(void) const { return fpDIB; }
    const WCHAR* GetFormatDescr(void) const { return fOutputFormatDescr; }
    // multiple-font flag currently not used
    bool GetMultipleFontsFlag(void) const { return fMultipleFonts; }

    /*
     * Setters, used by reformatters.
     */
    /* set the format description; string must be persistent (static) */
    void SetFormatDescr(const WCHAR* str) { fOutputFormatDescr = str; }
    /* set the kind of output we're providing */
    void SetOutputKind(OutputKind kind) { fOutputKind = kind; }
    void SetMultipleFontsFlag(bool val) { fMultipleFonts = val; }

    /* set the output */
    // TODO: split into two different functions, one takes const char* and
    //  doesn't delete, the other char* and does delete
    void SetTextBuf(char* buf, long len, bool doDelete) {
        assert(fTextBuf == NULL);
        fTextBuf = buf;
        fTextLen = len;
        fDoDeleteText = doDelete;
    }

    void SetDIB(MyDIBitmap* pDIB) {
        ASSERT(fpDIB == NULL);
        fpDIB = pDIB;
    }

private:
    DECLARE_COPY_AND_OPEQ(ReformatOutput)

    /* what we're holding */
    OutputKind      fOutputKind;
    //ReformatID        fOutputID;
    const WCHAR*    fOutputFormatDescr;

    /* output RTF uses multiple fonts, so ignore font change request */
    bool            fMultipleFonts;

    /* storage; either fTextBuf or fpDIB will be NULL */
    char*           fTextBuf;
    long            fTextLen;
    bool            fDoDeleteText;
    MyDIBitmap*     fpDIB;

    //char*         fErrorMsg;
    //ReformatPart  fLastPart;
    //bool          fErrorMessage;      // output buffer holds an error msg
};


/*
 * Static namespace for some NiftyList lookup functions.  Do not instantiate.
 */
class NiftyList {
public:
    // one-time initialization
    static bool AppInit(const WCHAR* fileName);
    // one-time cleanup
    static bool AppCleanup(void);

    static const char* LookupP8MLI(uint8_t code) {
        return Lookup(fP8MLI, code);
    }
    static const char* LookupGSOS(uint16_t code) {
        return Lookup(fGSOS, code);
    }
    static const char* LookupToolbox(uint16_t req) {
        return Lookup(fSystemTools, req);
    }
    static const char* LookupE1Vector(uint16_t addr) {
        return Lookup(fE1Vectors, addr);
    }
    static const char* LookupE0Vector(uint16_t addr) {
        return Lookup(fE0Vectors, addr);
    }
    static const char* Lookup00Addr(uint16_t addr) {
        //if (addr < 0xc000)
        //  return NULL;     // ignore Davex Bxxx values
        return Lookup(f00Addrs, addr);
    }
    static const char* Lookup01Vector(uint16_t addr) {
        return Lookup(f01Vectors, addr);
    }

private:
    NiftyList(void);        // do not instantiate
    ~NiftyList(void);

    /*
     * Structures for holding data.
     */
    typedef struct NameValue {
        const char*     name;
        uint16_t        value;
    } NameValue;
    typedef struct DataSet {
        long            numEntries;
        NameValue*      pEntries;
    } DataSet;

    static DataSet      fP8MLI;
    static DataSet      fGSOS;
    static DataSet      fSystemTools;
    static DataSet      fE1Vectors;
    static DataSet      fE0Vectors;
    static DataSet      f00Addrs;
    static DataSet      f01Vectors;

    typedef enum LoadMode {
        kModeUnknown = 0,
        kModeNormal,
        kModeSkip,
        //kModeByteSwap,
    } LoadMode;
    static bool ReadSection(char** ppData, long* pRemLen, DataSet* pSet,
        LoadMode mode);
    static int ScanLine(const char* pData, long remLen);
    static int SortNameValue(const void *, const void *);
    static uint16_t ConvHexFour(const char* data);
    static void DumpSection(const DataSet& dataSet);

    static const char* Lookup(const DataSet& dataSet, uint16_t key);

    /* we sit on a copy of the entire file */
    static char*    fFileData;

    // make sure app calls AppInit
    static bool     fDataReady;
};

#endif /*REFORMAT_REFORMAT_H*/
