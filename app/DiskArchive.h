/*
 * CiderPress
 * Copyright (C) 2007 by faddenSoft, LLC.  All Rights Reserved.
 * See the file LICENSE for distribution terms.
 */
/*
 * Disk image "archive" support.
 */
#ifndef __DISK_ARCHIVE__
#define __DISK_ARCHIVE__

#include "GenericArchive.h"
#include "../diskimg/DiskImg.h"

class RenameEntryDialog;


/*
 * One file in a disk image.
 */
class DiskEntry : public GenericEntry {
public:
	DiskEntry(A2File* pFile) : fpFile(pFile)
		{}
	virtual ~DiskEntry(void) {}

	// retrieve thread data
	virtual int ExtractThreadToBuffer(int which, char** ppText, long* pLength,
		CString* pErrMsg) const;
	virtual int ExtractThreadToFile(int which, FILE* outfp, ConvertEOL conv,
		ConvertHighASCII convHA, CString* pErrMsg) const;
	virtual long GetSelectionSerial(void) const { return -1; }	// idea: T/S block number

	virtual bool GetFeatureFlag(Feature feature) const;

	// return the underlying FS format for this file
	virtual DiskImg::FSFormat GetFSFormat(void) const {
		ASSERT(fpFile != nil);
		return fpFile->GetFSFormat();
	}

	A2File* GetA2File(void) const { return fpFile; }
	void SetA2File(A2File* pFile) { fpFile = pFile; }

private:
	DIError CopyData(A2FileDescr* pOpenFile, FILE* outfp, ConvertEOL conv,
		ConvertHighASCII convHA, CString* pMsg) const;

	A2File*		fpFile;
};


/*
 * Disk image add-ons to GenericArchive.
 */
class DiskArchive : public GenericArchive {
public:
	DiskArchive(void) : fpPrimaryDiskFS(nil), fIsReadOnly(false),
		fpAddDataHead(nil), fpAddDataTail(nil)
		{}
	virtual ~DiskArchive(void) { (void) Close(); }

	/* pass this as the "options" value to the New() function */
	typedef struct {
		DiskImgLib::DiskImg::FSFormat		format;
		DiskImgLib::DiskImg::SectorOrder	sectorOrder;
	} NewOptionsBase;
	typedef union {
		NewOptionsBase	base;

		struct {
			NewOptionsBase	base;
			long			numBlocks;
		} blank;
		struct {
			NewOptionsBase	base;
			const char*		volName;
			long			numBlocks;
		} prodos;
		struct {
			NewOptionsBase	base;
			const char*		volName;
			long			numBlocks;
		} pascalfs;		// "pascal" is reserved token in MSVC++
		struct {
			NewOptionsBase	base;
			const char*		volName;
			long			numBlocks;
		} hfs;
		struct {
			NewOptionsBase	base;
			int				volumeNum;
			long			numTracks;
			int				numSectors;
			bool			allocDOSTracks;
		} dos;
	} NewOptions;

	// One-time initialization; returns an error string.
	static CString AppInit(void);
	// one-time cleanup at app shutdown time
	static void AppCleanup(void);

	virtual OpenResult Open(const char* filename, bool readOnly, CString* pErrMsg);
	virtual CString New(const char* filename, const void* options);
	virtual CString Flush(void);
	virtual CString Reload(void);
	virtual bool IsReadOnly(void) const { return fIsReadOnly; };
	virtual bool IsModified(void) const;
	virtual void GetDescription(CString* pStr) const;
	virtual bool BulkAdd(ActionProgressDialog* pActionProgress,
		const AddFilesDialog* pAddOpts);
	virtual bool AddDisk(ActionProgressDialog* pActionProgress,
		const AddFilesDialog* pAddOpts)
		{ ASSERT(false); return false; }
	virtual bool CreateSubdir(CWnd* pMsgWnd, GenericEntry* pParentEntry,
		const char* newName);
	virtual bool TestSelection(CWnd* pMsgWnd, SelectionSet* pSelSet)
		{ ASSERT(false); return false; }
	virtual bool DeleteSelection(CWnd* pMsgWnd, SelectionSet* pSelSet);
	virtual bool RenameSelection(CWnd* pMsgWnd, SelectionSet* pSelSet);
	virtual CString TestPathName(const GenericEntry* pGenericEntry,
		const CString& basePath, const CString& newName, char newFssep) const;
	virtual bool RenameVolume(CWnd* pMsgWnd, DiskFS* pDiskFS,
		const char* newName);
	virtual CString TestVolumeName(const DiskFS* pDiskFS,
		const char* newName) const;
	virtual bool RecompressSelection(CWnd* pMsgWnd, SelectionSet* pSelSet,
		const RecompressOptionsDialog* pRecompOpts)
		{ ASSERT(false); return false; }
	virtual bool GetComment(CWnd* pMsgWnd, const GenericEntry* pEntry,
		CString* pStr)
		{ ASSERT(false); return false; }
	virtual bool SetComment(CWnd* pMsgWnd, GenericEntry* pEntry,
		const CString& str)
		{ ASSERT(false); return false; }
	virtual bool DeleteComment(CWnd* pMsgWnd, GenericEntry* pEntry)
		{ ASSERT(false); return false; }
	virtual bool SetProps(CWnd* pMsgWnd, GenericEntry* pEntry,
		const FileProps* pProps);
	virtual void PreferencesChanged(void);
	virtual long GetCapability(Capability cap);
	virtual XferStatus XferSelection(CWnd* pMsgWnd, SelectionSet* pSelSet,
		ActionProgressDialog* pActionProgress, const XferFileOptions* pXferOpts);

	const DiskImg* GetDiskImg(void) const { return &fDiskImg; }
	DiskFS* GetDiskFS(void) const { return fpPrimaryDiskFS; }

	/* internal function, used by DiskArchive and DiskEntry */
	static bool ProgressCallback(DiskImgLib::A2FileDescr* pFile,
		DiskImgLib::di_off_t max, DiskImgLib::di_off_t current, void* state);

private:
	virtual CString Close(void);
	virtual void XferPrepare(const XferFileOptions* pXferOpts);
	virtual CString XferFile(FileDetails* pDetails, unsigned char** pDataBuf,
		long dataLen, unsigned char** pRsrcBuf, long rsrcLen);
	virtual void XferAbort(CWnd* pMsgWnd);
	virtual void XferFinish(CWnd* pMsgWnd);

	/* internal function, used during initial scan of volume */
	static bool ScanProgressCallback(void* cookie, const char* str,
		int count);


	/*
	 * Internal class used to keep track of files we're adding.
	 */
	class FileAddData {
	public:
		FileAddData(const FileDetails* pDetails, const char* fsNormalPath) {
			fDetails = *pDetails;

			fFSNormalPath = fsNormalPath;
			fpOtherFork = nil;
			fpNext = nil;
		}
		virtual ~FileAddData(void) {}

		FileAddData* GetNext(void) const { return fpNext; }
		void SetNext(FileAddData* pNext) { fpNext = pNext; }
		FileAddData* GetOtherFork(void) const { return fpOtherFork; }
		void SetOtherFork(FileAddData* pData) { fpOtherFork = pData; }

		const FileDetails* GetDetails(void) const { return &fDetails; }
		const char* GetFSNormalPath(void) const { return fFSNormalPath; }

	private:
		// Three filenames stored here:
		//	fDetails.origName -- the name of the Windows file
		//	fDetails.storageName -- the normalized Windows name
		//	fFSNormalPath -- the FS-normalized version of "storageName"
		FileDetails		fDetails;
		CString			fFSNormalPath;

		FileAddData*	fpOtherFork;
		FileAddData*	fpNext;
	};

	virtual ArchiveKind GetArchiveKind(void) { return kArchiveDiskImage; }
	virtual NuError DoAddFile(const AddFilesDialog* pAddOpts,
		FileDetails* pDetails);
	int InternalReload(CWnd* pMsgWnd);

	static int CompareDisplayNamesDesc(const void* ventry1, const void* ventry2);

	int LoadContents(void);
	int LoadDiskFSContents(DiskFS* pDiskFS, const char* volName);
	void DowncaseSubstring(CString* pStr, int startPos, int endPos,
		bool prevWasSpace);
	static void DebugMsgHandler(const char* file, int line, const char* msg);

	NuResult HandleReplaceExisting(const A2File* pExisting,
		FileDetails* pDetails);
	CString ProcessFileAddData(DiskFS* pDiskFS, int addOptsConvEOL);
	CString LoadFile(const char* pathName, unsigned char** pBuf, long* pLen,
		GenericEntry::ConvertEOL conv, GenericEntry::ConvertHighASCII convHA) const;
	DIError AddForksToDisk(DiskFS* pDiskFS, const DiskFS::CreateParms* pParms,
		const unsigned char* dataBuf, long dataLen,
		const unsigned char* rsrcBuf, long rsrcLen) const;
	void AddToAddDataList(FileAddData* pData);
	void FreeAddDataList(void);
	void ConvertFDToCP(const FileDetails* pDetails,
		DiskFS::CreateParms* pCreateParms);

	bool SetRenameFields(CWnd* pMsgWnd, DiskEntry* pEntry,
		RenameEntryDialog* pDialog);

	DiskImg			fDiskImg;			// DiskImg object for entire disk
	DiskFS*			fpPrimaryDiskFS;	// outermost DiskFS
	bool			fIsReadOnly;

	/* active state while adding files */
	FileAddData*	fpAddDataHead;
	FileAddData*	fpAddDataTail;
	bool			fOverwriteExisting;
	bool			fOverwriteNoAsk;

	/* state during xfer */
	//CString			fXferStoragePrefix;
	DiskFS*			fpXferTargetFS;
};

#endif /*__DISK_ARCHIVE__*/