/*
 * CiderPress
 * Copyright (C) 2007 by faddenSoft, LLC.  All Rights Reserved.
 * See the file LICENSE for distribution terms.
 */
/*
 * AppleLink Compression Utility archive support.
 */
#ifndef __ACU_ARCHIVE__
#define __ACU_ARCHIVE__

#include "GenericArchive.h"


class AcuArchive;

/*
 * One file in an ACU archive.
 */
class AcuEntry : public GenericEntry {
public:
	AcuEntry(AcuArchive* pArchive) :
		fpArchive(pArchive), fIsSqueezed(false), fOffset(-1)
		{}
	virtual ~AcuEntry(void) {}

	// retrieve thread data
	virtual int ExtractThreadToBuffer(int which, char** ppText, long* pLength,
		CString* pErrMsg) const;
	virtual int ExtractThreadToFile(int which, FILE* outfp, ConvertEOL conv,
		ConvertHighASCII convHA, CString* pErrMsg) const;
	virtual long GetSelectionSerial(void) const { return -1; }	// doesn't matter

	virtual bool GetFeatureFlag(Feature feature) const {
		if (feature == kFeaturePascalTypes || feature == kFeatureDOSTypes ||
			feature == kFeatureHasSimpleAccess)
			return false;
		else
			return true;
	}

	NuError TestEntry(CWnd* pMsgWnd);

	bool GetSqueezed(void) const { return fIsSqueezed; }
	void SetSqueezed(bool val) { fIsSqueezed = val; }
	long GetOffset(void) const { return fOffset; }
	void SetOffset(long offset) { fOffset = offset; }

private:
	NuError CopyData(FILE* outfp, ConvertEOL conv, ConvertHighASCII convHA,
		CString* pMsg) const;
	//NuError BNYUnSqueeze(ExpandBuffer* outExp) const;

	AcuArchive*	fpArchive;		// holds FILE* for archive
	bool		fIsSqueezed;
	long		fOffset;
};


/*
 * ACU archive definition.
 */
class AcuArchive : public GenericArchive {
public:
	AcuArchive(void) : fIsReadOnly(false), fFp(nil)
		{}
	virtual ~AcuArchive(void) { (void) Close(); }

	// One-time initialization; returns an error string.
	static CString AppInit(void);

	virtual OpenResult Open(const char* filename, bool readOnly,
		CString* pErrMsg);
	virtual CString New(const char* filename, const void* options);
	virtual CString Flush(void) { return ""; }
	virtual CString Reload(void);
	virtual bool IsReadOnly(void) const { return fIsReadOnly; };
	virtual bool IsModified(void) const { return false; }
	virtual void GetDescription(CString* pStr) const { *pStr = "AppleLink ACU"; }
	virtual bool BulkAdd(ActionProgressDialog* pActionProgress,
		const AddFilesDialog* pAddOpts)
		{ ASSERT(false); return false; }
	virtual bool AddDisk(ActionProgressDialog* pActionProgress,
		const AddFilesDialog* pAddOpts)
		{ ASSERT(false); return false; }
	virtual bool CreateSubdir(CWnd* pMsgWnd, GenericEntry* pParentEntry,
		const char* newName)
		{ ASSERT(false); return false; }
	virtual bool TestSelection(CWnd* pMsgWnd, SelectionSet* pSelSet);
	virtual bool DeleteSelection(CWnd* pMsgWnd, SelectionSet* pSelSet)
		{ ASSERT(false); return false; }
	virtual bool RenameSelection(CWnd* pMsgWnd, SelectionSet* pSelSet)
		{ ASSERT(false); return false; }
	virtual bool RenameVolume(CWnd* pMsgWnd, DiskFS* pDiskFS,
		const char* newName)
		{ ASSERT(false); return false; }
	virtual CString TestVolumeName(const DiskFS* pDiskFS,
		const char* newName) const
		{ ASSERT(false); return "!"; }
	virtual CString TestPathName(const GenericEntry* pGenericEntry,
		const CString& basePath, const CString& newName, char newFssep) const
		{ ASSERT(false); return "!"; }
	virtual bool RecompressSelection(CWnd* pMsgWnd, SelectionSet* pSelSet,
		const RecompressOptionsDialog* pRecompOpts)
		{ ASSERT(false); return false; }
	virtual XferStatus XferSelection(CWnd* pMsgWnd, SelectionSet* pSelSet,
		ActionProgressDialog* pActionProgress, const XferFileOptions* pXferOpts)
		{ ASSERT(false); return kXferFailed; }
	virtual bool GetComment(CWnd* pMsgWnd, const GenericEntry* pEntry,
		CString* pStr)
		{ ASSERT(false); return false; }
	virtual bool SetComment(CWnd* pMsgWnd, GenericEntry* pEntry,
		const CString& str)
		{ ASSERT(false); return false; }
	virtual bool DeleteComment(CWnd* pMsgWnd, GenericEntry* pEntry)
		{ ASSERT(false); return false; }
	virtual bool SetProps(CWnd* pMsgWnd, GenericEntry* pEntry,
		const FileProps* pProps)
		{ ASSERT(false); return false; }
	virtual void PreferencesChanged(void) {}
	virtual long GetCapability(Capability cap);

	friend class AcuEntry;

private:
	virtual CString Close(void) {
		if (fFp != nil) {
			fclose(fFp);
			fFp = nil;
		}
		return "";
	}
	virtual void XferPrepare(const XferFileOptions* pXferOpts)
		{ ASSERT(false); }
	virtual CString XferFile(FileDetails* pDetails, unsigned char** pDataBuf,
		long dataLen, unsigned char** pRsrcBuf, long rsrcLen)
		{ ASSERT(false); return "!"; }
	virtual void XferAbort(CWnd* pMsgWnd)
		{ ASSERT(false); }
	virtual void XferFinish(CWnd* pMsgWnd)
		{ ASSERT(false); }

	virtual ArchiveKind GetArchiveKind(void) { return kArchiveACU; }
	virtual NuError DoAddFile(const AddFilesDialog* pAddOpts,
		FileDetails* pDetails)
		{ ASSERT(false); return kNuErrGeneric; }

	enum {
		kAcuMaxFileName		= 256,		// nice big number

		kAcuMasterHeaderLen	= 20,
		kAcuEntryHeaderLen	= 54,
	};

	/*
	 * The header at the front of an ACU archive.
	 */
	typedef struct AcuMasterHeader {
		unsigned short	fileCount;
		unsigned short	unknown1;		// 0x01 00 -- might be "version 1?"
		unsigned char	fZink[6];		// "fZink", low ASCII
		unsigned char	unknown2[11];	// 0x01 36 00 00 00 00 00 00 00 00 dd
	} AcuMasterHeader;

	/*
	 * An entry in an ACU archive.  Each archive is essentially a stream
	 * of files; only the "filesToFollow" value gives any indication that
	 * something else follows this entry.
	 *
	 * We read this from the archive and then unpack the interesting parts
	 * into GenericEntry fields in an AcuEntry.
	 */
	struct AcuFileEntry;
	friend struct AcuFileEntry;
	typedef struct AcuFileEntry {
		unsigned char	compressionType;
		unsigned short	dataChecksum;		// ??
		unsigned short	blockCount;			// total blocks req'd to hold file
		unsigned long	dataStorageLen;		// length of data within archive
		unsigned short	access;
		unsigned short	fileType;
		unsigned long	auxType;
		unsigned char	storageType;
		unsigned long	dataEof;
		unsigned short	prodosModDate;
		unsigned short	prodosModTime;
		NuDateTime		modWhen;			// computed from previous two fields
		unsigned short	prodosCreateDate;
		unsigned short	prodosCreateTime;
		NuDateTime		createWhen; 		// computed from previous two fields
		unsigned short	fileNameLen;
		unsigned short	headerChecksum;		// ??
		char			fileName[kAcuMaxFileName+1];

		// possibilities for mystery fields:
		// - OS type (note ProDOS is $00)
		// - forked file support
	} AcuFileEntry;

	/* known compression types */
	enum CompressionType {
		kAcuCompNone = 0,
		kAcuCompSqueeze = 3,
	};

	int LoadContents(void);
	int ReadMasterHeader(int* pNumEntries);
	NuError ReadFileHeader(AcuFileEntry* pEntry);
	void DumpFileHeader(const AcuFileEntry* pEntry);
	int CreateEntry(const AcuFileEntry* pEntry);

	bool IsDir(const AcuFileEntry* pEntry);
	NuError AcuRead(void* buf, size_t nbyte);
	NuError AcuSeek(long offset);
	void AcuConvertDateTime(unsigned short prodosDate,
		unsigned short prodosTime, NuDateTime* pWhen);

	FILE*		fFp;
	bool		fIsReadOnly;
};

#endif /*__ACU_ARCHIVE__*/