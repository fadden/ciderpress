/*
 * CiderPress
 * Copyright (C) 2007 by faddenSoft, LLC.  All Rights Reserved.
 * See the file LICENSE for distribution terms.
 */
/*
 * NuFX archive support.
 */
#ifndef __NUFX_ARCHIVE__
#define __NUFX_ARCHIVE__

#include "GenericArchive.h"
#include "../prebuilt/NufxLib.h"		// ideally this wouldn't be here, only in .cpp


/*
 * One file in an NuFX archive.
 */
class NufxEntry : public GenericEntry {
public:
	NufxEntry(NuArchive* pArchive) : fpArchive(pArchive)
		{}
	virtual ~NufxEntry(void) {}

	NuRecordIdx GetRecordIdx(void) const { return fRecordIdx; }
	void SetRecordIdx(NuRecordIdx idx) { fRecordIdx = idx; }

	// retrieve thread data
	virtual int ExtractThreadToBuffer(int which, char** ppText, long* pLength,
		CString* pErrMsg) const;
	virtual int ExtractThreadToFile(int which, FILE* outfp, ConvertEOL conv,
		ConvertHighASCII convHA, CString* pErrMsg) const;
	virtual long GetSelectionSerial(void) const { return fRecordIdx; }

	virtual bool GetFeatureFlag(Feature feature) const {
		if (feature == kFeaturePascalTypes || feature == kFeatureDOSTypes ||
			feature == kFeatureHasSimpleAccess)
			return false;
		else
			return true;
	}

	// This fills out several GenericEntry fields based on the contents
	// of "*pRecord".
	void AnalyzeRecord(const NuRecord* pRecord);

	friend class NufxArchive;

private:
	void FindThreadInfo(int which, NuThread* pThread, CString* pErrMsg) const;

	NuRecordIdx	fRecordIdx;	// unique record index
	NuArchive*	fpArchive;
};


/*
 * A generic archive plus NuFX-specific goodies.
 */
class NufxArchive : public GenericArchive {
public:
	NufxArchive(void) :
		fpArchive(nil),
		fIsReadOnly(false),
		fProgressAsRecompress(false),
		fNumAdded(-1),
		fpMsgWnd(nil),
		fpAddOpts(nil)
	{}
	virtual ~NufxArchive(void) { (void) Close(); }

	// One-time initialization; returns an error string.
	static CString AppInit(void);

	virtual OpenResult Open(const char* filename, bool readOnly,
		CString* pErrMsg);
	virtual CString New(const char* filename, const void* options);
	virtual CString Flush(void) { return ""; }
	virtual CString Reload(void);
	virtual bool IsReadOnly(void) const { return fIsReadOnly; };
	virtual bool IsModified(void) const { return false; }
	virtual void GetDescription(CString* pStr) const { *pStr = "NuFX"; }
	virtual bool BulkAdd(ActionProgressDialog* pActionProgress,
		const AddFilesDialog* pAddOpts);
	virtual bool AddDisk(ActionProgressDialog* pActionProgress,
		const AddFilesDialog* pAddOpts);
	virtual bool CreateSubdir(CWnd* pMsgWnd, GenericEntry* pParentEntry,
		const char* newName)
		{ ASSERT(false); return false; }
	virtual bool TestSelection(CWnd* pMsgWnd, SelectionSet* pSelSet);
	virtual bool DeleteSelection(CWnd* pMsgWnd, SelectionSet* pSelSet);
	virtual bool RenameSelection(CWnd* pMsgWnd, SelectionSet* pSelSet);
	virtual CString TestPathName(const GenericEntry* pGenericEntry,
		const CString& basePath, const CString& newName, char newFssep) const;
	virtual bool RenameVolume(CWnd* pMsgWnd, DiskFS* pDiskFS,
		const char* newName)
		{ ASSERT(false); return false; }
	virtual CString TestVolumeName(const DiskFS* pDiskFS,
		const char* newName) const
		{ ASSERT(false); return "!"; }
	virtual bool RecompressSelection(CWnd* pMsgWnd, SelectionSet* pSelSet,
		const RecompressOptionsDialog* pRecompOpts);
	virtual XferStatus XferSelection(CWnd* pMsgWnd, SelectionSet* pSelSet,
		ActionProgressDialog* pActionProgress, const XferFileOptions* pXferOpts);
	virtual bool GetComment(CWnd* pMsgWnd, const GenericEntry* pEntry,
		CString* pStr);
	virtual bool SetComment(CWnd* pMsgWnd, GenericEntry* pEntry,
		const CString& str);
	virtual bool DeleteComment(CWnd* pMsgWnd, GenericEntry* pEntry);
	virtual bool SetProps(CWnd* pMsgWnd, GenericEntry* pEntry,
		const FileProps* pProps);
	virtual void PreferencesChanged(void);
	virtual long GetCapability(Capability cap);

	// try not to use this
	NuArchive* GetNuArchivePointer(void) const { return fpArchive; }

	// determine whether a particular type of compression is supported
	static bool IsCompressionSupported(NuThreadFormat format);

	// convert from DateTime format to time_t
	static time_t DateTimeToSeconds(const NuDateTime* pDateTime);

private:
	virtual CString Close(void) {
		if (fpArchive != nil) {
			WMSG0("Closing archive (aborting any un-flushed changes)\n");
			NuAbort(fpArchive);
			NuClose(fpArchive);
			fpArchive = nil;
		}
		return "";
	}
	bool RecompressThread(NufxEntry* pEntry, int threadKind,
		const RecompressOptionsDialog* pRecompOpts, long* pSizeInMemory,
		CString* pErrMsg);

	virtual void XferPrepare(const XferFileOptions* pXferOpts);
	virtual CString XferFile(FileDetails* pDetails, unsigned char** pDataBuf,
		long dataLen, unsigned char** pRsrcBuf, long rsrcLen);
	virtual void XferAbort(CWnd* pMsgWnd);
	virtual void XferFinish(CWnd* pMsgWnd);

	virtual ArchiveKind GetArchiveKind(void) { return kArchiveNuFX; }
	void AddPrep(CWnd* pWnd, const AddFilesDialog* pAddOpts);
	void AddFinish(void);
	virtual NuError DoAddFile(const AddFilesDialog* pAddOpts,
		FileDetails* pDetails);

	static NuResult BulkAddErrorHandler(NuArchive* pArchive, void* vErrorStatus);
	NuResult HandleReplaceExisting(const NuErrorStatus* pErrorStatus);
	NuResult HandleAddNotFound(const NuErrorStatus* pErrorStatus);

	NuError LoadContents(void);
	NuError InternalReload(CWnd* pMsgWnd);
	static NuResult ContentFunc(NuArchive* pArchive, void* vpRecord);

	NuError SetCallbacks(void);

	// handle progress update messages
	static NuResult ProgressUpdater(NuArchive* pArchive, void* vpProgress);

	// handle errors and debug messages from NufxLib.
	static NuResult NufxErrorMsgHandler(NuArchive* pArchive,
		void* vErrorMessage);

	// handle a DataSource resource release request
	static NuResult ArrayDeleteHandler(NuArchive* pArchive, void* ptr);

	NuArchive*		fpArchive;
	bool			fIsReadOnly;

	bool			fProgressAsRecompress;	// tweak progress updater

	/* state while adding files */
	int				fNumAdded;
	CWnd*			fpMsgWnd;
	const AddFilesDialog*	fpAddOpts;
};

#endif /*__NUFX_ARCHIVE__*/