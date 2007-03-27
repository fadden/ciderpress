/*
 * CiderPress
 * Copyright (C) 2007 by faddenSoft, LLC.  All Rights Reserved.
 * See the file LICENSE for distribution terms.
 */
/*
 * Bridge between NufxLib and GenericArchive.
 */
#include "stdafx.h"
#include "NufxArchive.h"
#include "ConfirmOverwriteDialog.h"
#include "RenameEntryDialog.h"
#include "RecompressOptionsDialog.h"
#include "AddClashDialog.h"
#include "Main.h"
#include "../prebuilt/NufxLib.h"

/*
 * NufxLib doesn't currently allow an fssep of '\0', so we use this instead
 * to indicate the absence of an fssep char.  Not quite right, but it'll do
 * until NufxLib gets fixed.
 */
const unsigned char kNufxNoFssep = 0xff;


/*
 * ===========================================================================
 *		NufxEntry
 * ===========================================================================
 */

/*
 * Extract data from a thread into a buffer.
 *
 * If "*ppText" is non-nil and "*pLength" is > 0, the data will be read into
 * the pointed-to buffer so long as it's shorter than *pLength bytes.  The
 * value in "*pLength" will be set to the actual length used.
 *
 * If "*ppText" is nil or the length is <= 0, the uncompressed data will be
 * placed into a buffer allocated with "new[]".
 *
 * Returns IDOK on success, IDCANCEL if the operation was cancelled by the
 * user, and -1 value on failure.  On failure, "*ppText" and "*pLength" will
 * be valid but point at an error message.
 *
 * "which" is an anonymous GenericArchive enum.
 */
int
NufxEntry::ExtractThreadToBuffer(int which, char** ppText, long* pLength,
	CString* pErrMsg) const
{
	NuError nerr;
	char* dataBuf = nil;
	NuDataSink* pDataSink = nil;
	NuThread thread;
	unsigned long actualThreadEOF;
	NuThreadIdx threadIdx;
	bool needAlloc = true;
	int result = -1;

	ASSERT(IDOK != -1 && IDCANCEL != -1);	// make sure return vals don't clash

	if (*ppText != nil)
		needAlloc = false;

	FindThreadInfo(which, &thread, pErrMsg);
	if (!pErrMsg->IsEmpty())
		goto bail;
	threadIdx = thread.threadIdx;
	actualThreadEOF = thread.actualThreadEOF;

	/*
	 * We've got the right thread.  Create an appropriately-sized buffer
	 * and extract the data into it (WITHOUT doing EOL conversion).
	 *
	 * First check for a length of zero.
	 */
	if (actualThreadEOF == 0) {
		WMSG0("Empty thread\n");
		if (needAlloc) {
			*ppText = new char[1];
			**ppText = '\0';
		}
		*pLength = 0;
		result = IDOK;
		goto bail;
	}

	if (needAlloc) {
		dataBuf = new char[actualThreadEOF];
		if (dataBuf == nil) {
			pErrMsg->Format("allocation of %ld bytes failed",
				actualThreadEOF);
			goto bail;
		}
	} else {
		if (*pLength < (long) actualThreadEOF) {
			pErrMsg->Format("buf size %ld too short (%ld)",
				*pLength, actualThreadEOF);
			goto bail;
		}
		dataBuf = *ppText;
	}
	nerr = NuCreateDataSinkForBuffer(true, kNuConvertOff,
			(unsigned char*)dataBuf, actualThreadEOF, &pDataSink);
	if (nerr != kNuErrNone) {
		pErrMsg->Format("unable to create buffer data sink: %s",
			NuStrError(nerr));
		goto bail;
	}

	SET_PROGRESS_BEGIN();
	nerr = NuExtractThread(fpArchive, threadIdx, pDataSink);
	if (nerr != kNuErrNone) {
		if (nerr == kNuErrAborted) {
			result = IDCANCEL;
			//::sprintf(errorBuf, "Cancelled.\n");
		} else if (nerr == kNuErrBadFormat) {
			pErrMsg->Format("The compression method used on this file is not supported "
							"by your copy of \"nufxlib2.dll\".  For more information, "
							"please visit us on the web at "
							"http://www.faddensoft.com/ciderpress/");
		} else {
			pErrMsg->Format("unable to extract thread %ld: %s",
				threadIdx, NuStrError(nerr));
		}
		goto bail;
	}

	if (needAlloc)
		*ppText = dataBuf;
	*pLength = actualThreadEOF;
	result = IDOK;

bail:
	if (result == IDOK) {
		SET_PROGRESS_END();
		ASSERT(pErrMsg->IsEmpty());
	} else {
		ASSERT(result == IDCANCEL || !pErrMsg->IsEmpty());
		if (needAlloc) {
			delete[] dataBuf;
			ASSERT(*ppText == nil);
		}
	}
	if (pDataSink != nil)
		NuFreeDataSink(pDataSink);
	return result;
}

/*
 * Extract data from a thread to a file.  Since we're not copying to memory,
 * we can't assume that we're able to hold the entire file all at once.
 *
 * Returns IDOK on success, IDCANCEL if the operation was cancelled by the
 * user, and -1 value on failure.  On failure, "*pMsg" holds an
 * error message.
 */
int
NufxEntry::ExtractThreadToFile(int which, FILE* outfp, ConvertEOL conv,
	ConvertHighASCII convHA, CString* pErrMsg) const
{
	NuDataSink* pDataSink = nil;
	NuError nerr;
	NuThread thread;
	unsigned long actualThreadEOF;
	NuThreadIdx threadIdx;
	int result = -1;

	ASSERT(outfp != nil);

	//CString errMsg;
	FindThreadInfo(which, &thread, pErrMsg);
	if (!pErrMsg->IsEmpty())
		goto bail;
	threadIdx = thread.threadIdx;
	actualThreadEOF = thread.actualThreadEOF;

	/* we've got the right thread, see if it's empty */
	if (actualThreadEOF == 0) {
		WMSG0("Empty thread\n");
		result = IDOK;
		goto bail;
	}

	/* set EOL conversion flags */
	NuValue nuConv;
	switch (conv) {
	case kConvertEOLOff:	nuConv = kNuConvertOff;		break;
	case kConvertEOLOn:		nuConv = kNuConvertOn;		break;
	case kConvertEOLAuto:	nuConv = kNuConvertAuto;	break;
	default:
		ASSERT(false);
		pErrMsg->Format("internal error: bad conv flag %d", conv);
		goto bail;
	}
	if (which == kDiskImageThread) {
		/* override the above; never EOL-convert a disk image */
		nuConv = kNuConvertOff;
	}

	switch (convHA) {
	case kConvertHAOff:
		nerr = NuSetValue(fpArchive, kNuValueStripHighASCII, false);
		break;
	case kConvertHAOn:
	case kConvertHAAuto:
		nerr = NuSetValue(fpArchive, kNuValueStripHighASCII, true);
		break;
	default:
		ASSERT(false);
		pErrMsg->Format("internal error: bad convHA flag %d", convHA);
		goto bail;
	}

	/* make sure we convert to CRLF */
	nerr = NuSetValue(fpArchive, kNuValueEOL, kNuEOLCRLF);	// for Win32
	if (nerr != kNuErrNone) {
		pErrMsg->Format("failed setting EOL value: %s", NuStrError(nerr));
		goto bail;
	}

	/* create a data sink for "outfp" */
	nerr = NuCreateDataSinkForFP(true, nuConv, outfp, &pDataSink);
	if (nerr != kNuErrNone) {
		pErrMsg->Format("unable to create FP data sink: %s",
			NuStrError(nerr));
		goto bail;
	}
	
	/* extract the thread to the file */
	SET_PROGRESS_BEGIN();
	nerr = NuExtractThread(fpArchive, threadIdx, pDataSink);
	if (nerr != kNuErrNone) {
		if (nerr == kNuErrAborted) {
			/* user hit the "cancel" button */
			*pErrMsg = _T("cancelled");
			result = IDCANCEL;
		} else if (nerr == kNuErrBadFormat) {
			pErrMsg->Format("The compression method used on this file is not supported "
							"by your copy of \"nufxlib2.dll\".  For more information, "
							"please visit us on the web at "
							"http://www.faddensoft.com/ciderpress/");
		} else {
			pErrMsg->Format("unable to extract thread %ld: %s",
				threadIdx, NuStrError(nerr));
		}
		goto bail;
	}

	result = IDOK;

bail:
	if (result == IDOK) {
		SET_PROGRESS_END();
	}
	if (pDataSink != nil)
		NuFreeDataSink(pDataSink);
	return result;
}

/*
 * Find info for the thread we're about to extract.
 *
 * Given the NuRecordIdx stored in the object, find the thread whose
 * ThreadID matches "which".  Copies the NuThread structure into
 * "*pThread".
 *
 * On failure, "pErrMsg" will have a nonzero length, and contain an error
 * message describing the problem.
 */
void
NufxEntry::FindThreadInfo(int which, NuThread* pRetThread,
	CString* pErrMsg) const
{
	NuError nerr;

	ASSERT(pErrMsg->IsEmpty());

	/*
	 * Retrieve the record from the archive.
	 */
	const NuRecord* pRecord;
	nerr = NuGetRecord(fpArchive, fRecordIdx, &pRecord);
	if (nerr != kNuErrNone) {
		pErrMsg->Format("NufxLib unable to locate record %ld: %s",
			fRecordIdx, NuStrError(nerr));
		goto bail;
	}

	/*
	 * Find the right thread.
	 */
	const NuThread* pThread;
	unsigned long wantedThreadID;
	switch (which) {
	case kDataThread:		wantedThreadID = kNuThreadIDDataFork;		break;
	case kRsrcThread:		wantedThreadID = kNuThreadIDRsrcFork;		break;
	case kDiskImageThread:	wantedThreadID = kNuThreadIDDiskImage;		break;
	case kCommentThread:	wantedThreadID = kNuThreadIDComment;		break;
	default:
		pErrMsg->Format("looking for bogus thread 0x%02x", which);
		goto bail;
	}

	int i;
	pThread = nil;
	for (i = 0; i < (int)NuRecordGetNumThreads(pRecord); i++) {
		pThread = NuGetThread(pRecord, i);
		if (NuGetThreadID(pThread) == wantedThreadID)
			break;
	}
	if (i == (int)NuRecordGetNumThreads(pRecord)) {
		/* didn't find the thread we wanted */
		pErrMsg->Format("searched %d threads but couldn't find 0x%02x",
			NuRecordGetNumThreads(pRecord), which);
		goto bail;
	}

	memcpy(pRetThread, pThread, sizeof(*pRetThread));

bail:
	return;
}


//static const char* gShortFormatNames[] = {
//    "unc", "squ", "lz1", "lz2", "u12", "u16", "dfl", "bzp"
//};
static const char* gFormatNames[] = {
	"Uncompr", "Squeeze", "LZW/1", "LZW/2", "LZC-12",
	"LZC-16", "Deflate", "Bzip2"
};

/*
 * Analyze the contents of a record to determine if it's a disk, file,
 * or "other".	Compute the total compressed and uncompressed lengths
 * of all data threads.  Return the "best" format.
 *
 * The "best format" and "record type" stuff assume that the entire
 * record contains only a disk thread or a file thread, and that any
 * format is interesting so long as it isn't "no compression".	In
 * general these will be true, because ShrinkIt and NuLib create files
 * this way.
 *
 * You could, of course, create a single record with a data thread and
 * a disk image thread, but it's a fair bet ShrinkIt would ignore one
 * or the other.
 *
 * NOTE: we don't currently work around the GSHK zero-length file bug.
 * Such records, which have a filename thread but no data threads at all,
 * will be categorized as "unknown".  We could detect the situation and
 * correct it, but we might as well flag it in a user-visible way.
 */
void
NufxEntry::AnalyzeRecord(const NuRecord* pRecord)
{
	const NuThread* pThread;
	NuThreadID threadID;
	unsigned long idx;
	RecordKind recordKind;
	unsigned long uncompressedLen;
	unsigned long compressedLen;
	unsigned short format;

	recordKind = kRecordKindUnknown;
	uncompressedLen = compressedLen = 0;
	format = kNuThreadFormatUncompressed;

	for (idx = 0; idx < pRecord->recTotalThreads; idx++) {
		pThread = NuGetThread(pRecord, idx);
		ASSERT(pThread != nil);

		threadID = NuMakeThreadID(pThread->thThreadClass,
					pThread->thThreadKind);

		if (pThread->thThreadClass == kNuThreadClassData) {
			/* replace what's there if this might be more interesting */
			if (format == kNuThreadFormatUncompressed)
				format = (unsigned short) pThread->thThreadFormat;

			if (threadID == kNuThreadIDRsrcFork)
				recordKind = kRecordKindForkedFile;
			else if (threadID == kNuThreadIDDiskImage)
				recordKind = kRecordKindDisk;
			else if (threadID == kNuThreadIDDataFork &&
					recordKind == kRecordKindUnknown)
				recordKind = kRecordKindFile;

			/* sum up, so we get both forks of forked files */
			//uncompressedLen += pThread->actualThreadEOF;
			compressedLen += pThread->thCompThreadEOF;
		}

		if (threadID == kNuThreadIDDataFork) {
			if (!GetHasDataFork() && !GetHasDiskImage()) {
				SetHasDataFork(true);
				SetDataForkLen(pThread->actualThreadEOF);
			} else {
				WMSG0("WARNING: ignoring second disk image / data fork\n");
			}
		}
		if (threadID == kNuThreadIDRsrcFork) {
			if (!GetHasRsrcFork()) {
				SetHasRsrcFork(true);
				SetRsrcForkLen(pThread->actualThreadEOF);
			} else {
				WMSG0("WARNING: ignoring second data fork\n");
			}
		}
		if (threadID == kNuThreadIDDiskImage) {
			if (!GetHasDiskImage() && !GetHasDataFork()) {
				SetHasDiskImage(true);
				SetDataForkLen(pThread->actualThreadEOF);
			} else {
				WMSG0("WARNING: ignoring second disk image / data fork\n");
			}
		}
		if (threadID == kNuThreadIDComment) {
			SetHasComment(true);
			if (pThread->actualThreadEOF != 0)
				SetHasNonEmptyComment(true);
		}
	}

	SetRecordKind(recordKind);
	//SetUncompressedLen(uncompressedLen);
	SetCompressedLen(compressedLen);

	if (format >= 0 && format < NELEM(gFormatNames))
		SetFormatStr(gFormatNames[format]);
	else
		SetFormatStr("Unknown");
}


/*
 * ===========================================================================
 *		NufxArchive
 * ===========================================================================
 */

/*
 * Perform one-time initialization of the NufxLib library.
 *
 * Returns with an error if the NufxLib version is off.  Major version must
 * match (since it indicates an interface change), minor version must be
 * >= what we expect (in case we're relying on recent behavior changes).
 *
 * Returns 0 on success, nonzero on error.
 */
/*static*/ CString
NufxArchive::AppInit(void)
{
	NuError nerr;
	CString result("");
	long major, minor, bug;

	nerr = NuGetVersion(&major, &minor, &bug, NULL, NULL);
	if (nerr != kNuErrNone) {
		result = "Unable to get version number from NufxLib.";
		goto bail;
	}

	if (major != kNuVersionMajor || minor < kNuVersionMinor) {
		result.Format("Older or incompatible version of NufxLib DLL found.\r\r"
						"Wanted v%d.%d.x, found %ld.%ld.%ld.",
				kNuVersionMajor, kNuVersionMinor,
				major, minor, bug);
		goto bail;
	}
	if (bug != kNuVersionBug) {
		WMSG2("Different 'bug' version (built vX.X.%d, dll vX.X.%d)\n",
			kNuVersionBug, bug);
	}

	/* set NufxLib's global error message handler */
	NuSetGlobalErrorMessageHandler(NufxErrorMsgHandler);

bail:
	return result;
}


/*
 * Determine whether a particular kind of compression is supported by
 * NufxLib.
 *
 * Returns "true" if supported, "false" if not.
 */
/*static*/ bool
NufxArchive::IsCompressionSupported(NuThreadFormat format)
{
	NuFeature feature;

	switch (format) {
	case kNuThreadFormatUncompressed:
		return true;

	case kNuThreadFormatHuffmanSQ:
		feature = kNuFeatureCompressSQ;
		break;
	case kNuThreadFormatLZW1:
	case kNuThreadFormatLZW2:
		feature = kNuFeatureCompressLZW;
		break;
	case kNuThreadFormatLZC12:
	case kNuThreadFormatLZC16:
		feature = kNuFeatureCompressLZC;
		break;
	case kNuThreadFormatDeflate:
		feature = kNuFeatureCompressDeflate;
		break;
	case kNuThreadFormatBzip2:
		feature = kNuFeatureCompressBzip2;
		break;

	default:
		ASSERT(false);
		return false;
	}

	NuError nerr;
	nerr = NuTestFeature(feature);
	if (nerr == kNuErrNone)
		return true;
	return false;
}

/*
 * Display error messages... or not.
 */
NuResult
NufxArchive::NufxErrorMsgHandler(NuArchive* /*pArchive*/, void* vErrorMessage)
{
#if defined(_DEBUG_LOG)
	const NuErrorMessage* pErrorMessage = (const NuErrorMessage*) vErrorMessage;
	CString msg(pErrorMessage->message);
	
	msg += "\n";

	if (pErrorMessage->isDebug)
		msg = "[D] " + msg;
	fprintf(gLog, "%05u NufxLib %s(%d) : %s",
		gPid, pErrorMessage->file, pErrorMessage->line, msg);

#elif defined(_DEBUG)
	const NuErrorMessage* pErrorMessage = (const NuErrorMessage*) vErrorMessage;
	CString msg(pErrorMessage->message);
	
	msg += "\n";

	if (pErrorMessage->isDebug)
		msg = "[D] " + msg;
	_CrtDbgReport(_CRT_WARN, pErrorMessage->file, pErrorMessage->line,
		pErrorMessage->function, msg);
#endif

	return kNuOK;
}

/*
 * Display our progress.
 *
 * "oldName" ends up on top, "newName" on bottom.
 */
/*static*/NuResult
NufxArchive::ProgressUpdater(NuArchive* pArchive, void* vpProgress)
{
	const NuProgressData* pProgress = (const NuProgressData*) vpProgress;
	NufxArchive* pThis;
	MainWindow* pMainWin = (MainWindow*)::AfxGetMainWnd();
	int status;
	const char* oldName;
	const char* newName;
	int perc;

	ASSERT(pProgress != nil);
	ASSERT(pMainWin != nil);

	ASSERT(pArchive != nil);
	(void) NuGetExtraData(pArchive, (void**) &pThis);
	ASSERT(pThis != nil);

	oldName = newName = nil;
	if (pProgress->operation == kNuOpAdd) {
		oldName = pProgress->origPathname;
		newName = pProgress->pathname;
		if (pThis->fProgressAsRecompress)
			oldName = "-";
	} else if (pProgress->operation == kNuOpTest) {
		oldName = pProgress->pathname;
	} else if (pProgress->operation == kNuOpExtract) {
		if (pThis->fProgressAsRecompress) {
			oldName = pProgress->origPathname;
			newName = "-";
		}
	}

	perc = pProgress->percentComplete;
	if (pProgress->state == kNuProgressDone)
		perc = 100;

	//WMSG3("Progress: %d%% '%s' '%s'\n", perc,
	//	oldName == nil ? "(nil)" : oldName,
	//	newName == nil ? "(nil)" : newName);

	//status = pMainWin->SetProgressUpdate(perc, oldName, newName);
	status = SET_PROGRESS_UPDATE2(perc, oldName, newName);

	/* check to see if user hit the "cancel" button on the progress dialog */
	if (pProgress->state == kNuProgressAborted) {
		WMSG0("(looks like we're aborting)\n");
		ASSERT(status == IDCANCEL);
	}
	
	if (status == IDCANCEL) {
		WMSG0("Signaling NufxLib to abort\n");
		return kNuAbort;
	} else
		return kNuOK;
}


/*
 * Finish instantiating a NufxArchive object by opening an existing file.
 *
 * Returns an error string on failure, or nil on success.
 */
GenericArchive::OpenResult
NufxArchive::Open(const char* filename, bool readOnly, CString* pErrMsg)
{
	NuError nerr;
	CString errMsg;

	ASSERT(fpArchive == nil);

	if (!readOnly) {
		CString tmpname = GenDerivedTempName(filename);
		WMSG2("Opening file '%s' rw (tmp='%s')\n", filename, tmpname);
		fIsReadOnly = false;
		nerr = NuOpenRW(filename, tmpname, 0, &fpArchive);
	}
	if (nerr == kNuErrFileAccessDenied || nerr == EACCES) {
		WMSG0("Read-write failed with access denied, trying read-only\n");
		readOnly = true;
	}
	if (readOnly) {
		WMSG1("Opening file '%s' ro\n", filename);
		fIsReadOnly = true;
		nerr = NuOpenRO(filename, &fpArchive);
	}
	if (nerr != kNuErrNone) {
		errMsg = "Unable to open '";
		errMsg += filename;
		errMsg += "': ";
		errMsg += NuStrError(nerr);
		goto bail;
	} else {
		//WMSG0("FILE OPEN SUCCESS\n");
	}

	nerr = SetCallbacks();
	if (nerr != kNuErrNone) {
		errMsg = "Callback init failed";
		goto bail;
	}

	nerr = LoadContents();
	if (nerr != kNuErrNone) {
		errMsg = "Failed reading archive contents: ";
		errMsg += NuStrError(nerr);
	}

	SetPathName(filename);

bail:
	*pErrMsg = errMsg;
	if (!errMsg.IsEmpty())
		return kResultFailure;
	else
		return kResultSuccess;
}


/*
 * Finish instantiating a NufxArchive object by creating a new archive.
 *
 * Returns an error string on failure, or "" on success.
 */
CString
NufxArchive::New(const char* filename, const void* options)
{
	NuError nerr;
	CString retmsg("");

	ASSERT(fpArchive == nil);
	ASSERT(options == nil);

	CString tmpname = GenDerivedTempName(filename);
	WMSG2("Creating file '%s' (tmp='%s')\n", filename, tmpname);
	fIsReadOnly = false;
	nerr = NuOpenRW(filename, tmpname, kNuOpenCreat | kNuOpenExcl, &fpArchive);
	if (nerr != kNuErrNone) {
		retmsg = "Unable to open '";
		retmsg += filename;
		retmsg += "': ";
		retmsg += NuStrError(nerr);
		goto bail;
	} else {
		WMSG0("NEW FILE SUCCESS\n");
	}


	nerr = SetCallbacks();
	if (nerr != kNuErrNone) {
		retmsg = "Callback init failed";
		goto bail;
	}

	SetPathName(filename);

bail:
	return retmsg;
}

/*
 * Set some standard callbacks and feature flags.
 */
NuError
NufxArchive::SetCallbacks(void)
{
	NuError nerr;

    nerr = NuSetExtraData(fpArchive, this);
	if (nerr != kNuErrNone)
		goto bail;
//    nerr = NuSetSelectionFilter(fpArchive, SelectionFilter);
//	if (nerr != kNuErrNone)
//		goto bail;
//    nerr = NuSetOutputPathnameFilter(fpArchive, OutputPathnameFilter);
//	if (nerr != kNuErrNone)
//		goto bail;
    NuSetProgressUpdater(fpArchive, ProgressUpdater);
//    nerr = NuSetErrorHandler(fpArchive, ErrorHandler);
//	if (nerr != kNuErrNone)
//		goto bail;
    NuSetErrorMessageHandler(fpArchive, NufxErrorMsgHandler);

	/* let NufxLib worry about buggy records without data threads */
	nerr = NuSetValue(fpArchive, kNuValueMaskDataless, kNuValueTrue);
	if (nerr != kNuErrNone)
		goto bail;

	/* set any values based on Preferences values */
	PreferencesChanged();

bail:
	return nerr;
}

/*
 * User has updated their preferences.  Take note.
 *
 * (This is also called the first time through.)
 */
void
NufxArchive::PreferencesChanged(void)
{
	NuError nerr;
	const Preferences* pPreferences = GET_PREFERENCES();
	bool val;

	val = pPreferences->GetPrefBool(kPrMimicShrinkIt);
    nerr = NuSetValue(fpArchive, kNuValueMimicSHK, val);
	if (nerr != kNuErrNone) {
		WMSG2("NuSetValue(kNuValueMimicSHK, %d) failed, err=%d\n", val, nerr);
		ASSERT(false);
	} else {
		WMSG1("Set MimicShrinkIt to %d\n", val);
	}

	val = pPreferences->GetPrefBool(kPrReduceSHKErrorChecks);
    NuSetValue(fpArchive, kNuValueIgnoreLZW2Len, val);
    NuSetValue(fpArchive, kNuValueIgnoreCRC, val);

	val = pPreferences->GetPrefBool(kPrBadMacSHK);
	NuSetValue(fpArchive, kNuValueHandleBadMac, val);
}

/*
 * Report on what NuFX is capable of.
 */
long
NufxArchive::GetCapability(Capability cap)
{
	switch (cap) {
	case kCapCanTest:
		return true;
		break;
	case kCapCanRenameFullPath:
		return true;
		break;
	case kCapCanRecompress:
		return true;
		break;
	case kCapCanEditComment:
		return true;
		break;
	case kCapCanAddDisk:
		return true;
		break;
	case kCapCanConvEOLOnAdd:
		return false;
		break;
	case kCapCanCreateSubdir:
		return false;
		break;
	case kCapCanRenameVolume:
		return false;
		break;
	default:
		ASSERT(false);
		return -1;
		break;
	}
}

/*
 * Load the contents of an archive into the GenericEntry/NufxEntry list.
 *
 * We will need to set an error handler if we want to be able to do things
 * like "I found a bad CRC, did you want me to keep trying anyway?".
 */
NuError
NufxArchive::LoadContents(void)
{
	long counter = 0;
	NuError result;

	WMSG0("NufxArchive LoadContents\n");
	ASSERT(fpArchive != nil);

	{
		MainWindow* pMain = GET_MAIN_WINDOW();
		ExclusiveModelessDialog* pWaitDlg = new ExclusiveModelessDialog;
		pWaitDlg->Create(IDD_LOADING, pMain);
		pWaitDlg->CenterWindow();
		pMain->PeekAndPump();	// redraw
		CWaitCursor waitc;

		result = NuContents(fpArchive, ContentFunc);

		SET_PROGRESS_COUNTER(-1);

		pWaitDlg->DestroyWindow();
		//pMain->PeekAndPump();	// redraw
	}

	return result;
}

/*
 * Reload the contents.
 */
CString
NufxArchive::Reload(void)
{
	NuError nerr;
	CString errMsg;

	fReloadFlag = true;		// tell everybody that cached data is invalid

	DeleteEntries();		// a GenericArchive operation

	nerr = LoadContents();
	if (nerr != kNuErrNone) {
		errMsg.Format("ERROR: unable to reload archive contents: %s.",
			NuStrError(nerr));

		DeleteEntries();
		fIsReadOnly = true;
	}

	return errMsg;
}

/*
 * Reload the contents of the archive, showing an error message if the
 * reload fails.
 */
NuError
NufxArchive::InternalReload(CWnd* pMsgWnd)
{
	CString errMsg;

	errMsg = Reload();

	if (!errMsg.IsEmpty()) {
		ShowFailureMsg(pMsgWnd, errMsg, IDS_FAILED);
		return kNuErrGeneric;
	}

	return kNuErrNone;
}

/*
 * Static callback function.  Used for scanning the contents of an archive.
 */
NuResult
NufxArchive::ContentFunc(NuArchive* pArchive, void* vpRecord)
{
	const NuRecord* pRecord = (const NuRecord*) vpRecord;
	NufxArchive* pThis;
	NufxEntry* pNewEntry;

	ASSERT(pArchive != nil);
	ASSERT(vpRecord != nil);

	NuGetExtraData(pArchive, (void**) &pThis);

	pNewEntry = new NufxEntry(pArchive);

	pNewEntry->SetPathName(pRecord->filename);
	pNewEntry->SetFssep(NuGetSepFromSysInfo(pRecord->recFileSysInfo));
	pNewEntry->SetFileType(pRecord->recFileType);
	pNewEntry->SetAuxType(pRecord->recExtraType);
	pNewEntry->SetAccess(pRecord->recAccess);
	pNewEntry->SetCreateWhen(DateTimeToSeconds(&pRecord->recCreateWhen));
	pNewEntry->SetModWhen(DateTimeToSeconds(&pRecord->recModWhen));

	/*
	 * Our files are always ProDOS format.  This is especially important
	 * when cutting & pasting, so that the DOS high ASCII converter gets
	 * invoked at appropriate times.
	 */
	pNewEntry->SetSourceFS(DiskImg::kFormatProDOS);

	pNewEntry->AnalyzeRecord(pRecord);
	pNewEntry->SetRecordIdx(pRecord->recordIdx);

	pThis->AddEntry(pNewEntry);
	if ((pThis->GetNumEntries() % 10) == 0)
		SET_PROGRESS_COUNTER(pThis->GetNumEntries());

	return kNuOK;
}

/*
 * Convert a NuDateTime structure to a time_t.
 */
/*static*/ time_t
NufxArchive::DateTimeToSeconds(const NuDateTime* pDateTime)
{
	if (pDateTime->second == 0 &&
		pDateTime->minute == 0 &&
		pDateTime->hour == 0 &&
		pDateTime->year == 0 &&
		pDateTime->day == 0 &&
		pDateTime->month == 0 &&
		pDateTime->extra == 0 &&
		pDateTime->weekDay == 0)
	{
		return kDateNone;
	}

	int year;
	if (pDateTime->year < 40)
		year = pDateTime->year + 2000;
	else
		year = pDateTime->year + 1900;

	if (year < 1969) {
		/*
		 * Years like 1963 are valid on an Apple II but cannot be represented
		 * as a time_t, which starts in 1970.  (Depending on GMT offsets,
		 * it actually starts a few hours earlier at the end of 1969.)
		 *
		 * I'm catching this here because of an assert in the CTime
		 * constructor.  The constructor seems to do the right thing, and the
		 * assert won't be present in the shipping version, but it's annoying
		 * during debugging.
		 */
		//WMSG1(" Ignoring funky year %ld\n", year);
		return kDateInvalid;
	}
	if (pDateTime->month > 11)
		return kDateInvalid;

	CTime modTime(year,
				  pDateTime->month+1,
				  pDateTime->day+1,
				  pDateTime->hour,
				  pDateTime->minute,
				  pDateTime->second);
	return (time_t) modTime.GetTime();
}

/*
 * Callback from a DataSource that is done with a buffer.  Use for memory
 * allocated with new[].
 */
/*static*/ NuResult
NufxArchive::ArrayDeleteHandler(NuArchive* pArchive, void* ptr)
{
	delete[] ptr;
	return kNuOK;
}


/*
 * ===========================================================================
 *		NufxArchive -- add files (or disks)
 * ===========================================================================
 */

/*
 * Process a bulk "add" request.
 *
 * This calls into the GenericArchive "AddFile" function, which does
 * Win32-specific processing.  That function calls our DoAddFile function,
 * which does the NuFX stuff.
 *
 * Returns "true" on success, "false" on failure.
 */
bool
NufxArchive::BulkAdd(ActionProgressDialog* pActionProgress,
	const AddFilesDialog* pAddOpts)
{
	NuError nerr;
	CString errMsg;
	char curDir[MAX_PATH] = "";
	bool retVal = false;

	WMSG2("Opts: '%s' typePres=%d\n",
		pAddOpts->fStoragePrefix, pAddOpts->fTypePreservation);
	WMSG3("      sub=%d strip=%d ovwr=%d\n",
		pAddOpts->fIncludeSubfolders, pAddOpts->fStripFolderNames,
		pAddOpts->fOverwriteExisting);

	AddPrep(pActionProgress, pAddOpts);

	pActionProgress->SetArcName("(Scanning files to be added...)");
	pActionProgress->SetFileName("");

	/* initialize count */
	fNumAdded = 0;

	const char* buf = pAddOpts->GetFileNames();
	WMSG2("Selected path = '%s' (offset=%d)\n", buf,
		pAddOpts->GetFileNameOffset());

	if (GetCurrentDirectory(sizeof(curDir), curDir) == 0) {
		errMsg = "Unable to get current directory.\n";
		ShowFailureMsg(fpMsgWnd, errMsg, IDS_FAILED);
		goto bail;
	}
	if (SetCurrentDirectory(buf) == false) {
		errMsg.Format("Unable to set current directory to '%s'.\n", buf);
		ShowFailureMsg(fpMsgWnd, errMsg, IDS_FAILED);
		goto bail;
	}

	buf += pAddOpts->GetFileNameOffset();
	while (*buf != '\0') {
		WMSG1("  file '%s'\n", buf);

		/* this just provides the list of files to NufxLib */
		nerr = AddFile(pAddOpts, buf, &errMsg);
		if (nerr != kNuErrNone) {
			if (errMsg.IsEmpty())
				errMsg.Format("Failed while adding file '%s': %s.",
					(LPCTSTR) buf, NuStrError(nerr));
			if (nerr != kNuErrAborted) {
				ShowFailureMsg(fpMsgWnd, errMsg, IDS_FAILED);
			}
			goto bail;
		}

		buf += strlen(buf)+1;
	}

	/* actually do the work */
	long statusFlags;
	nerr = NuFlush(fpArchive, &statusFlags);
	if (nerr != kNuErrNone) {
		if (nerr != kNuErrAborted) {
			errMsg.Format("Unable to add files: %s.", NuStrError(nerr));
			ShowFailureMsg(fpMsgWnd, errMsg, IDS_FAILED);
		}

		/* see if it got converted to read-only status */
		if (statusFlags & kNuFlushReadOnly)
			fIsReadOnly = true;
		goto bail;
	}

	if (!fNumAdded) {
		errMsg = "No files added.\n";
		fpMsgWnd->MessageBox(errMsg, "CiderPress", MB_OK | MB_ICONWARNING);
	} else {
		if (InternalReload(fpMsgWnd) == kNuErrNone)
			retVal = true;
		else
			errMsg = "Reload failed.";
	}

bail:
	NuAbort(fpArchive);		// abort anything that didn't get flushed
	if (SetCurrentDirectory(curDir) == false) {
		errMsg.Format("Unable to reset current directory to '%s'.\n", buf);
		ShowFailureMsg(fpMsgWnd, errMsg, IDS_FAILED);
		// bummer, but don't signal failure
	}
	AddFinish();
	return retVal;
}

/*
 * Add a single disk to the archive.
 */
bool
NufxArchive::AddDisk(ActionProgressDialog* pActionProgress,
	const AddFilesDialog* pAddOpts)
{
	NuError nerr;
	CString errMsg;
	const int kBlockSize = 512;
	PathProposal pathProp;
	PathName pathName;
	DiskImg* pDiskImg;
	NuDataSource* pSource = nil;
	unsigned char* diskData = nil;
	char curDir[MAX_PATH] = "\\";
	bool retVal = false;

	WMSG2("AddDisk: '%s' %d\n", pAddOpts->GetFileNames(),
		pAddOpts->GetFileNameOffset());
	WMSG2("Opts: '%s' type=%d\n",
		pAddOpts->fStoragePrefix, pAddOpts->fTypePreservation);
	WMSG3("      sub=%d strip=%d ovwr=%d\n",
		pAddOpts->fIncludeSubfolders, pAddOpts->fStripFolderNames,
		pAddOpts->fOverwriteExisting);

	pDiskImg = pAddOpts->fpDiskImg;
	ASSERT(pDiskImg != nil);

	/* allocate storage for the disk */
	diskData = new unsigned char[pDiskImg->GetNumBlocks() * kBlockSize];
	if (diskData == nil) {
		errMsg.Format("Unable to allocate %d bytes.",
			pDiskImg->GetNumBlocks() * kBlockSize);
		ShowFailureMsg(fpMsgWnd, errMsg, IDS_FAILED);
		goto bail;
	}

	/* prepare to add */
	AddPrep(pActionProgress, pAddOpts);

	const char* buf;
	buf = pAddOpts->GetFileNames();
	WMSG2("Selected path = '%s' (offset=%d)\n", buf,
		pAddOpts->GetFileNameOffset());

	if (GetCurrentDirectory(sizeof(curDir), curDir) == 0) {
		errMsg = "Unable to get current directory.\n";
		ShowFailureMsg(fpMsgWnd, errMsg, IDS_FAILED);
		goto bail;
	}
	if (SetCurrentDirectory(buf) == false) {
		errMsg.Format("Unable to set current directory to '%s'.\n", buf);
		ShowFailureMsg(fpMsgWnd, errMsg, IDS_FAILED);
		goto bail;
	}

	buf += pAddOpts->GetFileNameOffset();
	WMSG1("  file '%s'\n", buf);

	/* strip off preservation stuff, and ignore it */
	pathProp.Init(buf);
	pathProp.fStripDiskImageSuffix = true;
	pathProp.LocalToArchive(pAddOpts);

	/* fill in the necessary file details */
	NuFileDetails details;
	memset(&details, 0, sizeof(details));
	details.threadID = kNuThreadIDDiskImage;
	details.storageType = kBlockSize;
	details.access = kNuAccessUnlocked;
	details.extraType = pAddOpts->fpDiskImg->GetNumBlocks();
	details.origName = buf;
	details.storageName = pathProp.fStoredPathName;
	details.fileSysID = kNuFileSysUnknown;
	details.fileSysInfo = PathProposal::kDefaultStoredFssep;

	time_t now, then;

	pathName = buf;
	now = time(nil);
	then = pathName.GetModWhen();
	UNIXTimeToDateTime(&now, &details.archiveWhen);
	UNIXTimeToDateTime(&then, &details.modWhen);
	UNIXTimeToDateTime(&then, &details.createWhen);

	/* set up the progress updater */
	pActionProgress->SetArcName(details.storageName);
	pActionProgress->SetFileName(details.origName);

	/* read the disk now that we have progress update titles in place */
	int block, numBadBlocks;
	unsigned char* bufPtr;
	numBadBlocks = 0;
	for (block = 0, bufPtr = diskData; block < pDiskImg->GetNumBlocks(); block++)
	{
		DIError dierr;
		dierr = pDiskImg->ReadBlock(block, bufPtr);
		if (dierr != kDIErrNone)
			numBadBlocks++;
		bufPtr += kBlockSize;
	}
	if (numBadBlocks > 0) {
		CString appName, msg;
		appName.LoadString(IDS_MB_APP_NAME);
		msg.Format("Skipped %ld unreadable block%s.", numBadBlocks,
			numBadBlocks == 1 ? "" : "s");
		fpMsgWnd->MessageBox(msg, appName, MB_OK | MB_ICONWARNING);
		// keep going -- just a warning
	}

	/* create a data source for the disk */
	nerr = NuCreateDataSourceForBuffer(kNuThreadFormatUncompressed, 0,
			diskData, 0, pAddOpts->fpDiskImg->GetNumBlocks() * kBlockSize,
			nil, &pSource);
	if (nerr != kNuErrNone) {
		errMsg = "Unable to create NufxLib data source.";
		ShowFailureMsg(fpMsgWnd, errMsg, IDS_FAILED);
		goto bail;
	}

	/* add the record; name conflicts cause the error handler to fire */
	NuRecordIdx recordIdx;
	nerr = NuAddRecord(fpArchive, &details, &recordIdx);
	if (nerr != kNuErrNone) {
		if (nerr != kNuErrAborted) {
			errMsg.Format("Failed adding record: %s.", NuStrError(nerr));
			ShowFailureMsg(fpMsgWnd, errMsg, IDS_FAILED);
		}
		goto bail;
	}

	/* do the compression */
	nerr = NuAddThread(fpArchive, recordIdx, kNuThreadIDDiskImage,
			pSource, nil);
	if (nerr != kNuErrNone) {
		errMsg.Format("Failed adding thread: %s.", NuStrError(nerr));
		ShowFailureMsg(fpMsgWnd, errMsg, IDS_FAILED);
		goto bail;
	}
	pSource = nil;		/* NufxLib owns it now */

	/* actually do the work */
	long statusFlags;
	nerr = NuFlush(fpArchive, &statusFlags);
	if (nerr != kNuErrNone) {
		if (nerr != kNuErrAborted) {
			errMsg.Format("Unable to add disk: %s.", NuStrError(nerr));
			ShowFailureMsg(fpMsgWnd, errMsg, IDS_FAILED);
		}

		/* see if it got converted to read-only status */
		if (statusFlags & kNuFlushReadOnly)
			fIsReadOnly = true;
		goto bail;
	}

	if (InternalReload(fpMsgWnd) == kNuErrNone)
		retVal = true;

bail:
	delete[] diskData;
	NuAbort(fpArchive);		// abort anything that didn't get flushed
	NuFreeDataSource(pSource);
	if (SetCurrentDirectory(curDir) == false) {
		errMsg.Format("Unable to reset current directory to '%s'.\n", buf);
		ShowFailureMsg(fpMsgWnd, errMsg, IDS_FAILED);
		// bummer
	}
	AddFinish();
	return retVal;
}

/*
 * Do the archive-dependent part of the file add, including things like
 * adding comments.  This is eventually called by AddFile() during bulk
 * adds.
 */
NuError
NufxArchive::DoAddFile(const AddFilesDialog* pAddOpts,
	FileDetails* pDetails)
{
	NuError err;
	NuRecordIdx recordIdx = 0;
	NuFileDetails nuFileDetails;
	
retry:
	nuFileDetails = *pDetails;	// stuff class contents into struct
	err = NuAddFile(fpArchive, pDetails->origName /*pathname*/,
			&nuFileDetails, false, &recordIdx);

	if (err == kNuErrNone) {
		fNumAdded++;
	} else if (err == kNuErrSkipped) {
		/* "maybe overwrite" UI causes this if user declines */
		// fall through with the error
		WMSG1("DoAddFile: skipped '%s'\n", pDetails->origName);
	} else if (err == kNuErrRecordExists) {
		AddClashDialog dlg;

		dlg.fWindowsName = pDetails->origName;
		dlg.fStorageName = pDetails->storageName;
		if (dlg.DoModal() != IDOK) {
			err = kNuErrAborted;
			goto bail_quiet;
		}
		if (dlg.fDoRename) {
			WMSG1("add clash: rename to '%s'\n", (const char*) dlg.fNewName);
			pDetails->storageName = dlg.fNewName;
			goto retry;
		} else {
			WMSG0("add clash: skip");
			err = kNuErrSkipped;
			// fall through with error
		}
	}
	//if (err != kNuErrNone)
	//	goto bail;

//bail:
	if (err != kNuErrNone && err != kNuErrAborted && err != kNuErrSkipped) {
		CString msg;
		msg.Format("Unable to add file '%s': %s.", pDetails->origName,
			NuStrError(err));
		ShowFailureMsg(fpMsgWnd, msg, IDS_FAILED);
	}
bail_quiet:
	return err;
}

/*
 * Prepare to add files.
 */
void
NufxArchive::AddPrep(CWnd* pMsgWnd, const AddFilesDialog* pAddOpts)
{
	NuError nerr;
	const Preferences* pPreferences = GET_PREFERENCES();
	int defaultCompression;

	ASSERT(fpArchive != nil);

	fpMsgWnd = pMsgWnd;
	ASSERT(fpMsgWnd != nil);

	fpAddOpts = pAddOpts;
	ASSERT(fpAddOpts != nil);

	//fBulkProgress = true;

	defaultCompression = pPreferences->GetPrefLong(kPrCompressionType);
	nerr = NuSetValue(fpArchive, kNuValueDataCompression,
			defaultCompression + kNuCompressNone);
	if (nerr != kNuErrNone) {
		WMSG1("GLITCH: unable to set compression type to %d\n",
			defaultCompression);
		/* keep going */
	}

	if (pAddOpts->fOverwriteExisting)
		NuSetValue(fpArchive, kNuValueHandleExisting, kNuAlwaysOverwrite);
	else
		NuSetValue(fpArchive, kNuValueHandleExisting, kNuMaybeOverwrite);

    NuSetErrorHandler(fpArchive, BulkAddErrorHandler);
	NuSetExtraData(fpArchive, this);
}

/*
 * Reset some things after we finish adding files.  We don't necessarily
 * want these to stay in effect for other operations, e.g. extracting
 * (though that is currently handled within CiderPress).
 */
void
NufxArchive::AddFinish(void)
{
    NuSetErrorHandler(fpArchive, nil);
	NuSetValue(fpArchive, kNuValueHandleExisting, kNuMaybeOverwrite);
	fpMsgWnd = nil;
	fpAddOpts = nil;
	//fBulkProgress = false;
}


/*
 * Error handler callback for "bulk" adds.
 */
/*static*/ NuResult
NufxArchive::BulkAddErrorHandler(NuArchive* pArchive, void* vErrorStatus)
{
	const NuErrorStatus* pErrorStatus = (const NuErrorStatus*)vErrorStatus;
	NufxArchive* pThis;
	NuResult result;

	ASSERT(pArchive != nil);
	(void) NuGetExtraData(pArchive, (void**) &pThis);
	ASSERT(pThis != nil);
	ASSERT(pArchive == pThis->fpArchive);

	/* default action is to abort the current operation */
	result = kNuAbort;

	/*
	 * When adding files, the NuAddFile and NuAddRecord calls can return
	 * immediate, specific results for a single add.  The only reasons for
	 * calling here are to decide if an existing record should be replaced
	 * or not (without even an option to rename), or to decide what to do
	 * when the NuFlush call runs into a problem while adding a file.
	 */
	if (pErrorStatus->operation != kNuOpAdd) {
		ASSERT(false);
		return kNuAbort;
	}

	if (pErrorStatus->err == kNuErrRecordExists) {
		/* if they want to update or freshen, don't hassle them */
		//if (NState_GetModFreshen(pState) || NState_GetModUpdate(pState))
		if (pThis->fpAddOpts->fOverwriteExisting) {
			ASSERT(false);	// should be handled by AddPrep()/NufxLib
			result = kNuOverwrite;
		} else
			result = pThis->HandleReplaceExisting(pErrorStatus);
	} else if (pErrorStatus->err == kNuErrFileNotFound) {
		/* file was specified with NuAdd but removed during NuFlush */
		result = pThis->HandleAddNotFound(pErrorStatus);
	}

	return result;
}

/*
 * Decide whether or not to replace an existing file (during extract)
 * or record (during add).
 */
NuResult
NufxArchive::HandleReplaceExisting(const NuErrorStatus* pErrorStatus)
{
	NuResult result = kNuOK;

	ASSERT(pErrorStatus != nil);
	ASSERT(pErrorStatus->pathname != nil);

	ASSERT(pErrorStatus->canOverwrite);
	ASSERT(pErrorStatus->canSkip);
	ASSERT(pErrorStatus->canAbort);
	ASSERT(!pErrorStatus->canRename);

	/* no firm policy, ask the user */
	ConfirmOverwriteDialog confOvwr;
	PathName path(pErrorStatus->pathname);
	
	confOvwr.fExistingFile = pErrorStatus->pRecord->filename;
	confOvwr.fExistingFileModWhen =
		DateTimeToSeconds(&pErrorStatus->pRecord->recModWhen);
	if (pErrorStatus->origPathname != nil) {
		confOvwr.fNewFileSource = pErrorStatus->origPathname;
		PathName checkPath(confOvwr.fNewFileSource);
		confOvwr.fNewFileModWhen = checkPath.GetModWhen();
	} else {
		confOvwr.fNewFileSource = "???";
		confOvwr.fNewFileModWhen = kDateNone;
	}

	confOvwr.fAllowRename = false;
	if (confOvwr.DoModal() == IDCANCEL) {
		result = kNuAbort;
		goto bail;
	}
	if (confOvwr.fResultRename) {
		ASSERT(false);
		result = kNuAbort;
		goto bail;
	}
	if (confOvwr.fResultApplyToAll) {
		if (confOvwr.fResultOverwrite) {
			(void) NuSetValue(fpArchive, kNuValueHandleExisting,
					kNuAlwaysOverwrite);
		} else {
			(void) NuSetValue(fpArchive, kNuValueHandleExisting,
					kNuNeverOverwrite);
		}
	}
	if (confOvwr.fResultOverwrite)
		result = kNuOverwrite;
	else
		result = kNuSkip;

bail:
	return result;
}

/*
 * A file that used to be there isn't anymore.
 *
 * This should be exceedingly rare.
 */
NuResult
NufxArchive::HandleAddNotFound(const NuErrorStatus* pErrorStatus)
{
	CString errMsg;

	errMsg.Format("Failed while adding '%s': file no longer exists.",
		pErrorStatus->pathname);
	ShowFailureMsg(fpMsgWnd, errMsg, IDS_FAILED);

	return kNuAbort;
}


/*
 * ===========================================================================
 *		NufxArchive -- test files
 * ===========================================================================
 */

/*
 * Test the records represented in the selection set.
 */
bool
NufxArchive::TestSelection(CWnd* pMsgWnd, SelectionSet* pSelSet)
{
	NuError nerr;
	NufxEntry* pEntry;
	CString errMsg;
	bool retVal = false;

	ASSERT(fpArchive != nil);

	WMSG1("Testing %d entries\n", pSelSet->GetNumEntries());

	SelectionEntry* pSelEntry = pSelSet->IterNext();
	while (pSelEntry != nil) {
		pEntry = (NufxEntry*) pSelEntry->GetEntry();

		WMSG2("  Testing %ld '%s'\n", pEntry->GetRecordIdx(),
			pEntry->GetPathName());
		nerr = NuTestRecord(fpArchive, pEntry->GetRecordIdx());
		if (nerr != kNuErrNone) {
			if (nerr == kNuErrAborted) {
				CString title;
				title.LoadString(IDS_MB_APP_NAME);
				errMsg = "Cancelled.";
				pMsgWnd->MessageBox(errMsg, title, MB_OK);
			} else {
				errMsg.Format("Failed while testing '%s': %s.",
					pEntry->GetPathName(), NuStrError(nerr));
				ShowFailureMsg(pMsgWnd, errMsg, IDS_FAILED);
			}
			goto bail;
		}

		pSelEntry = pSelSet->IterNext();
	}

	/* show success message */
	errMsg.Format("Tested %d file%s, no errors found.",
		pSelSet->GetNumEntries(),
		pSelSet->GetNumEntries() == 1 ? "" : "s");
	pMsgWnd->MessageBox(errMsg);
	retVal = true;

bail:
	return retVal;
}


/*
 * ===========================================================================
 *		NufxArchive -- delete files
 * ===========================================================================
 */

/*
 * Delete the records represented in the selection set.
 */
bool
NufxArchive::DeleteSelection(CWnd* pMsgWnd, SelectionSet* pSelSet)
{
	NuError nerr;
	NufxEntry* pEntry;
	CString errMsg;
	bool retVal = false;

	ASSERT(fpArchive != nil);

	WMSG1("Deleting %d entries\n", pSelSet->GetNumEntries());

	/* mark entries for deletion */
	SelectionEntry* pSelEntry = pSelSet->IterNext();
	while (pSelEntry != nil) {
		pEntry = (NufxEntry*) pSelEntry->GetEntry();

		WMSG2("  Deleting %ld '%s'\n", pEntry->GetRecordIdx(),
			pEntry->GetPathName());
		nerr = NuDeleteRecord(fpArchive, pEntry->GetRecordIdx());
		if (nerr != kNuErrNone) {
			errMsg.Format("Unable to delete record %d: %s.",
				pEntry->GetRecordIdx(), NuStrError(nerr));
			ShowFailureMsg(pMsgWnd, errMsg, IDS_FAILED);
			goto bail;
		}

		pSelEntry = pSelSet->IterNext();
	}

	/* actually do the delete */
	long statusFlags;
	nerr = NuFlush(fpArchive, &statusFlags);
	if (nerr != kNuErrNone) {
		errMsg.Format("Unable to delete all files: %s.", NuStrError(nerr));
		ShowFailureMsg(pMsgWnd, errMsg, IDS_FAILED);

		/* see if it got converted to read-only status */
		if (statusFlags & kNuFlushReadOnly)
			fIsReadOnly = true;
	}

	if (InternalReload(fpMsgWnd) == kNuErrNone)
		retVal = true;

bail:
	return retVal;
}


/*
 * ===========================================================================
 *		NufxArchive -- rename files
 * ===========================================================================
 */

/*
 * Rename the records represented in the selection set.
 */
bool
NufxArchive::RenameSelection(CWnd* pMsgWnd, SelectionSet* pSelSet)
{
	CString errMsg;
	NuError nerr;
	bool retVal = false;

	ASSERT(fpArchive != nil);

	WMSG1("Renaming %d entries\n", pSelSet->GetNumEntries());

	/*
	 * Figure out if we're allowed to change the entire path.  (This is
	 * doing it the hard way, but what the hell.)
	 */
	long cap = GetCapability(GenericArchive::kCapCanRenameFullPath);
	bool renameFullPath = (cap != 0);

	WMSG1("Rename, fullpath=%d\n", renameFullPath);

	/*
	 * For each item in the selection set, bring up the "rename" dialog,
	 * and ask the GenericEntry to process it.
	 *
	 * If they hit "cancel" or there's an error, we still flush the
	 * previous changes.  This is so that we don't have to create the
	 * same sort of deferred-write feature when renaming things in other
	 * sorts of archives (e.g. disk archives).
	 */
	SelectionEntry* pSelEntry = pSelSet->IterNext();
	while (pSelEntry != nil) {
		NufxEntry* pEntry = (NufxEntry*) pSelEntry->GetEntry();
		WMSG1("  Renaming '%s'\n", pEntry->GetPathName());

		RenameEntryDialog renameDlg(pMsgWnd);
		renameDlg.SetCanRenameFullPath(renameFullPath);
		renameDlg.SetCanChangeFssep(true);
		renameDlg.fOldName = pEntry->GetPathName();
		renameDlg.fFssep = pEntry->GetFssep();
		renameDlg.fpArchive = this;
		renameDlg.fpEntry = pEntry;

		int result = renameDlg.DoModal();
		if (result == IDOK) {
			if (renameDlg.fFssep == '\0')
				renameDlg.fFssep = kNufxNoFssep;
			nerr = NuRename(fpArchive, pEntry->GetRecordIdx(),
					renameDlg.fNewName, renameDlg.fFssep);
			if (nerr != kNuErrNone) {
				errMsg.Format("Unable to rename '%s': %s.", pEntry->GetPathName(),
					NuStrError(nerr));
				ShowFailureMsg(pMsgWnd, errMsg, IDS_FAILED);
				break;
			}
			WMSG2("Rename of '%s' to '%s' succeeded\n",
				pEntry->GetDisplayName(), renameDlg.fNewName);
		} else if (result == IDCANCEL) {
			WMSG0("Canceling out of remaining renames\n");
			break;
		} else {
			/* 3rd possibility is IDIGNORE, i.e. skip this entry */
			WMSG1("Skipping rename of '%s'\n", pEntry->GetDisplayName());
		}

		pSelEntry = pSelSet->IterNext();
	}

	/* flush pending rename calls */
	{
		CWaitCursor waitc;

		long statusFlags;
		nerr = NuFlush(fpArchive, &statusFlags);
		if (nerr != kNuErrNone) {
			errMsg.Format("Unable to rename all files: %s.",
				NuStrError(nerr));
			ShowFailureMsg(pMsgWnd, errMsg, IDS_FAILED);

			/* see if it got converted to read-only status */
			if (statusFlags & kNuFlushReadOnly)
				fIsReadOnly = true;
		}
	}

	/* reload GenericArchive from NufxLib */
	if (InternalReload(fpMsgWnd) == kNuErrNone)
		retVal = true;

	return retVal;
}


/*
 * Verify that the a name is suitable.  Called by RenameEntryDialog.
 *
 * Tests for context-specific syntax and checks for duplicates.
 *
 * Returns an empty string on success, or an error message on failure.
 */
CString
NufxArchive::TestPathName(const GenericEntry* pGenericEntry,
	const CString& basePath, const CString& newName, char newFssep) const
{
	CString errMsg("");
	ASSERT(pGenericEntry != nil);

	ASSERT(basePath.IsEmpty());

	/* can't start or end with fssep */
	if (newName.Left(1) == newFssep || newName.Right(1) == newFssep) {
		errMsg.Format("Names in NuFX archives may not start or end with a "
				"path separator character (%c).",
			newFssep);
		goto bail;
	}

	/* if it's a disk image, don't allow complex paths */
	if (pGenericEntry->GetRecordKind() == GenericEntry::kRecordKindDisk) {
		if (newName.Find(newFssep) != -1) {
			errMsg.Format("Disk image names may not contain a path separator "
					"character (%c).",
				newFssep);
			goto bail;
		}
	}

	/*
	 * Test for case-sensitive name collisions.  Each individual path
	 * component must be compared
	 */
	GenericEntry* pEntry;
	pEntry = GetEntries();
	while (pEntry != nil) {
		if (pEntry != pGenericEntry &&
			ComparePaths(pEntry->GetPathName(), pEntry->GetFssep(),
						 newName, newFssep) == 0)
		{
			errMsg.Format("An entry with that name already exists.");
		}

		pEntry = pEntry->GetNext();
	}

bail:
	return errMsg;
}


/*
 * ===========================================================================
 *		NufxArchive -- recompress
 * ===========================================================================
 */

/*
 * Recompress the files in the selection set.
 *
 * We have to uncompress the files into memory and then recompress them.
 * We don't want to flush after every file (too slow), but we can't wait
 * until they're expanded (unbounded memory requirements).  So we have
 * to keep expanding until we reach a certain limit, then call flush to
 * push the changes out.
 *
 * Since we're essentially making the changes in place (it's actually
 * all getting routed through the temp file), we need to delete the thread
 * and re-add it.  This isn't quite as thorough as "launder", which
 * actually reconstructs the entire record.
 */
bool
NufxArchive::RecompressSelection(CWnd* pMsgWnd, SelectionSet* pSelSet,
	const RecompressOptionsDialog* pRecompOpts)
{
	const int kMaxSizeInMemory = 2 * 1024 * 1024;	// 2MB
	CString errMsg;
	NuError nerr;
	bool retVal = false;

	/* set the compression type */
	nerr = NuSetValue(fpArchive, kNuValueDataCompression,
			pRecompOpts->fCompressionType + kNuCompressNone);
	if (nerr != kNuErrNone) {
		WMSG1("GLITCH: unable to set compression type to %d\n",
			pRecompOpts->fCompressionType);
		/* keep going */
	}

	fProgressAsRecompress = true;

	/*
	 * Loop over all items in the selection set.  Because the selection
	 * set has one entry for each interesting thread, we don't need to
	 * pry the NuRecord open and play with it.
	 *
	 * We should only be here for data forks, resource forks, and disk
	 * images.  Comments and filenames are not compressed, and so cannot
	 * be recompressed.
	 */
	SelectionEntry* pSelEntry = pSelSet->IterNext();
	long sizeInMemory = 0;
	bool result = true;
	NufxEntry* pEntry = nil;
	for ( ; pSelEntry != nil; pSelEntry = pSelSet->IterNext()) {
		pEntry = (NufxEntry*) pSelEntry->GetEntry();

		/*
		 * Compress each thread in turn.
		 */
		if (pEntry->GetHasDataFork()) {
			result = RecompressThread(pEntry, GenericEntry::kDataThread,
						pRecompOpts, &sizeInMemory, &errMsg);
			if (!result)
				break;
		}
		if (pEntry->GetHasRsrcFork()) {
			result = RecompressThread(pEntry, GenericEntry::kRsrcThread,
						pRecompOpts, &sizeInMemory, &errMsg);
			if (!result)
				break;
		}
		if (pEntry->GetHasDiskImage()) {
			result = RecompressThread(pEntry, GenericEntry::kDiskImageThread,
						pRecompOpts, &sizeInMemory, &errMsg);
			if (!result)
				break;
		}
		/* don't do anything with comments */

		/* if we're sitting on too much, push it out */
		if (sizeInMemory > kMaxSizeInMemory) {
			/* flush anything pending */
			long statusFlags;
			nerr = NuFlush(fpArchive, &statusFlags);
			if (nerr != kNuErrNone) {
				if (nerr != kNuErrAborted) {
					errMsg.Format("Unable to recompress all files: %s.",
						NuStrError(nerr));
					ShowFailureMsg(pMsgWnd, errMsg, IDS_FAILED);
				} else {
					WMSG0("Cancelled out of sub-flush/compress\n");
				}

				/* see if it got converted to read-only status */
				if (statusFlags & kNuFlushReadOnly)
					fIsReadOnly = true;

				goto bail;
			}

			sizeInMemory = 0;
		}
	}

	/* handle errors that threw us out of the while loop */
	if (!result) {
		ASSERT(pEntry != nil);
		CString dispStr;
		dispStr.Format("Failed while recompressing '%s': %s.",
			pEntry->GetDisplayName(), errMsg);
		ShowFailureMsg(pMsgWnd, errMsg, IDS_FAILED);
		goto bail;
	}


	/* flush anything pending */
	long statusFlags;
	nerr = NuFlush(fpArchive, &statusFlags);
	if (nerr != kNuErrNone) {
		if (nerr != kNuErrAborted) {
			errMsg.Format("Unable to recompress all files: %s.",
				NuStrError(nerr));
			ShowFailureMsg(pMsgWnd, errMsg, IDS_FAILED);
		} else {
			WMSG0("Cancelled out of flush/compress\n");
		}

		/* see if it got converted to read-only status */
		if (statusFlags & kNuFlushReadOnly)
			fIsReadOnly = true;
	} else {
		retVal = true;
	}

bail:
	/* abort anything that didn't get flushed */
	NuAbort(fpArchive);
	/* reload to pick up changes */
	(void) InternalReload(pMsgWnd);

	fProgressAsRecompress = false;
	return retVal;
}

/*
 * Recompress one thread.
 *
 * Returns "true" if things went okay, "false" on a fatal failure.
 */
bool
NufxArchive::RecompressThread(NufxEntry* pEntry, int threadKind,
	const RecompressOptionsDialog* pRecompOpts, long* pSizeInMemory,
	CString* pErrMsg)
{
	NuThread thread;
	NuThreadID threadID;
	NuError nerr;
	NuDataSource* pSource = nil;
	CString subErrMsg;
	bool retVal = false;
	char* buf = nil;
	long len = 0;

	WMSG2("  Recompressing %ld '%s'\n", pEntry->GetRecordIdx(),
		pEntry->GetDisplayName());

	/* get a copy of the thread header */
	pEntry->FindThreadInfo(threadKind, &thread, pErrMsg);
	if (!pErrMsg->IsEmpty()) {
		pErrMsg->Format("Unable to locate thread for %s (type %d)",
			pEntry->GetDisplayName(), threadKind);
		goto bail;
	}
	threadID = NuGetThreadID(&thread);

	/* if it's already in the target format, skip it */
	if (thread.thThreadFormat == pRecompOpts->fCompressionType) {
		WMSG2("Skipping (fmt=%d) '%s'\n",
			pRecompOpts->fCompressionType, pEntry->GetDisplayName());
		return true;
	}

	/* extract the thread */
	int result;
	result = pEntry->ExtractThreadToBuffer(threadKind, &buf, &len, &subErrMsg);
	if (result == IDCANCEL) {
		WMSG0("Cancelled during extract!\n");
		ASSERT(buf == nil);
		goto bail;	/* abort anything that was pending */
	} else if (result != IDOK) {
		pErrMsg->Format("Failed while extracting '%s': %s",
			pEntry->GetDisplayName(), subErrMsg);
		goto bail;
	}
	*pSizeInMemory += len;

	/* create a data source for it */
	nerr = NuCreateDataSourceForBuffer(kNuThreadFormatUncompressed,
			0, (const unsigned char*)buf, 0, len, ArrayDeleteHandler,
			&pSource);
	if (nerr != kNuErrNone) {
		pErrMsg->Format("Unable to create NufxLib data source (len=%d).",
			len);
		goto bail;
	}
	buf = nil;		// data source owns it now

	/* delete the existing thread */
	//WMSG1("+++ DELETE threadIdx=%d\n", thread.threadIdx);
	nerr = NuDeleteThread(fpArchive, thread.threadIdx);
	if (nerr != kNuErrNone) {
		pErrMsg->Format("Unable to delete thread %d: %s",
			pEntry->GetRecordIdx(), NuStrError(nerr));
		goto bail;
	}

	/* mark the new thread for addition */
	//WMSG1("+++ ADD threadID=0x%08lx\n", threadID);
	nerr = NuAddThread(fpArchive, pEntry->GetRecordIdx(), threadID,
				pSource, nil);
	if (nerr != kNuErrNone) {
		pErrMsg->Format("Unable to add thread type %d: %s",
			threadID, NuStrError(nerr));
		goto bail;
	}
	pSource = nil;		// now owned by nufxlib

	/* at this point, we just wait for the flush in the outer loop */
	retVal = true;

bail:
	NuFreeDataSource(pSource);
	return retVal;
}


/*
 * ===========================================================================
 *		NufxArchive -- transfer files to another archive
 * ===========================================================================
 */

/*
 * Transfer the selected files out of this archive and into another.
 *
 * We get one entry in the selection set per record.
 *
 * I think this now throws kXferCancelled whenever it's supposed to.  Not
 * 100% sure, but it looks good.
 */
GenericArchive::XferStatus
NufxArchive::XferSelection(CWnd* pMsgWnd, SelectionSet* pSelSet,
	ActionProgressDialog* pActionProgress, const XferFileOptions* pXferOpts)
{
	WMSG0("NufxArchive XferSelection!\n");
	XferStatus retval = kXferFailed;
	unsigned char* dataBuf = nil;
	unsigned char* rsrcBuf = nil;
	CString errMsg, dispMsg;

	pXferOpts->fTarget->XferPrepare(pXferOpts);

	SelectionEntry* pSelEntry = pSelSet->IterNext();
	for ( ; pSelEntry != nil; pSelEntry = pSelSet->IterNext()) {
		long dataLen=-1, rsrcLen=-1;
		NufxEntry* pEntry = (NufxEntry*) pSelEntry->GetEntry();
		FileDetails fileDetails;
		CString errMsg;

		ASSERT(dataBuf == nil);
		ASSERT(rsrcBuf == nil);

		/* in case we start handling CRC errors better */
		if (pEntry->GetDamaged()) {
			WMSG1("  XFER skipping damaged entry '%s'\n",
				pEntry->GetDisplayName());
			continue;
		}

		WMSG1(" XFER converting '%s'\n", pEntry->GetDisplayName());

		fileDetails.storageName = pEntry->GetDisplayName();
		fileDetails.fileType = pEntry->GetFileType();
		fileDetails.fileSysFmt = DiskImg::kFormatUnknown;
		fileDetails.fileSysInfo = PathProposal::kDefaultStoredFssep;
		fileDetails.access = pEntry->GetAccess();
		fileDetails.extraType = pEntry->GetAuxType();
		fileDetails.storageType = kNuStorageSeedling;

		time_t when;
		when = time(nil);
		UNIXTimeToDateTime(&when, &fileDetails.archiveWhen);
		when = pEntry->GetModWhen();
		UNIXTimeToDateTime(&when, &fileDetails.modWhen);
		when = pEntry->GetCreateWhen();
		UNIXTimeToDateTime(&when, &fileDetails.createWhen);

		pActionProgress->SetArcName(fileDetails.storageName);
		if (pActionProgress->SetProgress(0) == IDCANCEL) {
			retval = kXferCancelled;
			goto bail;
		}

		/*
		 * Handle all relevant threads in this record.  We assume it's either
		 * a data/rsrc pair or a disk image.
		 */
		if (pEntry->GetHasDataFork()) {
			/*
			 * Found a data thread.
			 */
			int result;
			dataBuf = nil;
			dataLen = 0;
			result = pEntry->ExtractThreadToBuffer(GenericEntry::kDataThread,
						(char**) &dataBuf, &dataLen, &errMsg);
			if (result == IDCANCEL) {
				WMSG0("Cancelled during data extract!\n");
				retval = kXferCancelled;
				goto bail;	/* abort anything that was pending */
			} else if (result != IDOK) {
				dispMsg.Format("Failed while extracting '%s': %s.",
					pEntry->GetDisplayName(), errMsg);
				ShowFailureMsg(pMsgWnd, dispMsg, IDS_FAILED);
				goto bail;
			}
			ASSERT(dataBuf != nil);
			ASSERT(dataLen >= 0);

		} else if (pEntry->GetHasDiskImage()) {
			/*
			 * No data thread found.  Look for a disk image.
			 */
			int result;
			dataBuf = nil;
			dataLen = 0;
			result = pEntry->ExtractThreadToBuffer(GenericEntry::kDiskImageThread,
						(char**) &dataBuf, &dataLen, &errMsg);
			if (result == IDCANCEL) {
				WMSG0("Cancelled during data extract!\n");
				goto bail;	/* abort anything that was pending */
			} else if (result != IDOK) {
				dispMsg.Format("Failed while extracting '%s': %s.",
					pEntry->GetDisplayName(), errMsg);
				ShowFailureMsg(pMsgWnd, dispMsg, IDS_FAILED);
				goto bail;
			}
			ASSERT(dataBuf != nil);
			ASSERT(dataLen >= 0);
		}

		/*
		 * See if there's a resource fork in here (either by itself or
		 * with a data fork).
		 */
		if (pEntry->GetHasRsrcFork()) {
			int result;
			rsrcBuf = nil;
			rsrcLen = 0;
			result = pEntry->ExtractThreadToBuffer(GenericEntry::kRsrcThread,
						(char**) &rsrcBuf, &rsrcLen, &errMsg);
			if (result == IDCANCEL) {
				WMSG0("Cancelled during rsrc extract!\n");
				goto bail;	/* abort anything that was pending */
			} else if (result != IDOK) {
				dispMsg.Format("Failed while extracting '%s': %s.",
					pEntry->GetDisplayName(), errMsg);
				ShowFailureMsg(pMsgWnd, dispMsg, IDS_FAILED);
				goto bail;
			}

			fileDetails.storageType = kNuStorageExtended;
		} else {
			ASSERT(rsrcBuf == nil);
		}

		if (dataLen < 0 && rsrcLen < 0) {
			WMSG1(" XFER: WARNING: nothing worth transferring in '%s'\n",
				pEntry->GetDisplayName());
			continue;
		}

		errMsg = pXferOpts->fTarget->XferFile(&fileDetails, &dataBuf, dataLen,
					&rsrcBuf, rsrcLen);
		if (!errMsg.IsEmpty()) {
			WMSG0("XferFile failed!\n");
			errMsg.Format("Failed while transferring '%s': %s.",
				pEntry->GetDisplayName(), (const char*) errMsg);
			ShowFailureMsg(pMsgWnd, errMsg, IDS_FAILED);
			goto bail;
		}
		ASSERT(dataBuf == nil);
		ASSERT(rsrcBuf == nil);

		if (pActionProgress->SetProgress(100) == IDCANCEL) {
			retval = kXferCancelled;
			goto bail;
		}
	}

	retval = kXferOK;

bail:
	if (retval != kXferOK)
		pXferOpts->fTarget->XferAbort(pMsgWnd);
	else
		pXferOpts->fTarget->XferFinish(pMsgWnd);
	delete[] dataBuf;
	delete[] rsrcBuf;
	return retval;
}

/*
 * Prepare to transfer files into a NuFX archive.
 *
 * We set the "allow duplicates" flag because DOS 3.3 volumes can have
 * files with duplicate names.
 */
void
NufxArchive::XferPrepare(const XferFileOptions* pXferOpts)
{
	WMSG0("  NufxArchive::XferPrepare\n");
	(void) NuSetValue(fpArchive, kNuValueAllowDuplicates, true);
}

/*
 * Transfer the data and optional resource fork of a single file into the
 * NuFX archive.
 *
 * "dataLen" and "rsrcLen" will be -1 if the corresponding fork doesn't
 * exist.
 *
 * Returns 0 on success, -1 on failure.  On success, "*pDataBuf" and
 * "*pRsrcBuf" are set to nil (ownership transfers to NufxLib).
 */
CString
NufxArchive::XferFile(FileDetails* pDetails, unsigned char** pDataBuf,
	long dataLen, unsigned char** pRsrcBuf, long rsrcLen)
{
	NuError nerr;
	const int kFileTypeTXT = 0x04;
	NuDataSource* pSource = nil;
	CString errMsg;

	WMSG1("  NufxArchive::XferFile '%s'\n", pDetails->storageName);
	WMSG4("  dataBuf=0x%08lx dataLen=%ld rsrcBuf=0x%08lx rsrcLen=%ld\n",
		*pDataBuf, dataLen, *pRsrcBuf, rsrcLen);
	ASSERT(pDataBuf != nil);
	ASSERT(pRsrcBuf != nil);

	/* NuFX doesn't explicitly store directories */
	if (pDetails->entryKind == FileDetails::kFileKindDirectory) {
		delete[] *pDataBuf;
		delete[] *pRsrcBuf;
		*pDataBuf = *pRsrcBuf = nil;
		goto bail;
	}

	ASSERT(dataLen >= 0 || rsrcLen >= 0);
	ASSERT(*pDataBuf != nil || *pRsrcBuf != nil);

	/* add the record; we have "allow duplicates" enabled for clashes */
	NuRecordIdx recordIdx;
	NuFileDetails nuFileDetails;
	nuFileDetails = *pDetails;

	/*
	 * Odd bit of trivia: NufxLib refuses to accept an fssep of '\0'.  It
	 * really wants to have one.  Which is annoying, since files coming
	 * from DOS or Pascal don't have one.  We therefore need to supply
	 * one, so we provide 0xff on the theory that nobody in their right
	 * mind would have it in an Apple II filename.
	 *
	 * Since we don't strip Pascal and ProDOS names down when we load
	 * the disks, it's possible the for 0xff to occur if the disk got
	 * damaged.  For ProDOS we don't care, since it has an fssep, but
	 * Pascal could be at risk.  DOS and RDOS are sanitized and so should
	 * be okay.
	 *
	 * One issue: we don't currently allow changing the fssep when renaming
	 * a file.  We need to fix this, or else there's no way to rename a
	 * file into a subdirectory once it has been pasted in this way.
	 */
	if (NuGetSepFromSysInfo(nuFileDetails.fileSysInfo) == 0) {
		nuFileDetails.fileSysInfo =
			NuSetSepInSysInfo(nuFileDetails.fileSysInfo, kNufxNoFssep);
	}

	nerr = NuAddRecord(fpArchive, &nuFileDetails, &recordIdx);
	if (nerr != kNuErrNone) {
		if (nerr != kNuErrAborted) {
			errMsg.Format("Failed adding record: %s", NuStrError(nerr));
			//ShowFailureMsg(fpMsgWnd, errMsg, IDS_FAILED);
		}
		// else the add was cancelled
		goto bail;
	}

	if (dataLen >= 0) {
		ASSERT(*pDataBuf != nil);

		/* strip the high ASCII from DOS and RDOS text files */
		if (pDetails->entryKind != FileDetails::kFileKindDiskImage &&
			pDetails->fileType == kFileTypeTXT &&
			DiskImg::UsesDOSFileStructure(pDetails->fileSysFmt))
		{
			WMSG1(" Stripping high ASCII from '%s'\n", pDetails->storageName);
			unsigned char* ucp = *pDataBuf;
			long len = dataLen;

			while (len--)
				*ucp++ &= 0x7f;
		}

		/* create a data source for the data fork; might be zero len */
		nerr = NuCreateDataSourceForBuffer(kNuThreadFormatUncompressed, 0,
				*pDataBuf, 0, dataLen, ArrayDeleteHandler, &pSource);
		if (nerr != kNuErrNone) {
			errMsg = "Unable to create NufxLib data source.";
			//ShowFailureMsg(fpMsgWnd, errMsg, IDS_FAILED);
			goto bail;
		}
		*pDataBuf = nil;		/* owned by data source */

		/* add the data fork, as a disk image if appropriate */
		NuThreadID targetID;
		if (pDetails->entryKind == FileDetails::kFileKindDiskImage)
			targetID = kNuThreadIDDiskImage;
		else
			targetID = kNuThreadIDDataFork;

		nerr = NuAddThread(fpArchive, recordIdx, targetID, pSource, nil);
		if (nerr != kNuErrNone) {
			errMsg.Format("Failed adding thread: %s.", NuStrError(nerr));
			//ShowFailureMsg(fpMsgWnd, errMsg, IDS_FAILED);
			goto bail;
		}
		pSource = nil;		/* NufxLib owns it now */
	}

	/* add the resource fork, if one was provided */
	if (rsrcLen >= 0) {
		ASSERT(*pRsrcBuf != nil);

		nerr = NuCreateDataSourceForBuffer(kNuThreadFormatUncompressed, 0,
				*pRsrcBuf, 0, rsrcLen, ArrayDeleteHandler, &pSource);
		if (nerr != kNuErrNone) {
			errMsg = "Unable to create NufxLib data source.";
			//ShowFailureMsg(fpMsgWnd, errMsg, IDS_FAILED);
			goto bail;
		}
		*pRsrcBuf = nil;		/* owned by data source */

		/* add the data fork */
		nerr = NuAddThread(fpArchive, recordIdx, kNuThreadIDRsrcFork,
				pSource, nil);
		if (nerr != kNuErrNone) {
			errMsg.Format("Failed adding thread: %s.", NuStrError(nerr));
			//ShowFailureMsg(fpMsgWnd, errMsg, IDS_FAILED);
			goto bail;
		}
		pSource = nil;		/* NufxLib owns it now */
	}

bail:
	NuFreeDataSource(pSource);
	return errMsg;
}

/*
 * Abort the transfer.
 *
 * Since we don't do any interim flushes, we can just call NuAbort.  If that
 * weren't the case, we would need to delete all records and flush.
 */
void
NufxArchive::XferAbort(CWnd* pMsgWnd)
{
	NuError nerr;
	CString errMsg;

	WMSG0("  NufxArchive::XferAbort\n");

	nerr = NuAbort(fpArchive);
	if (nerr != kNuErrNone) {
		errMsg.Format("Failed while aborting procedure: %s.", NuStrError(nerr));
		ShowFailureMsg(fpMsgWnd, errMsg, IDS_FAILED);
	}
}

/*
 * Flush all changes to the archive.
 */
void
NufxArchive::XferFinish(CWnd* pMsgWnd)
{
	NuError nerr;
	CString errMsg;

	WMSG0("  NufxArchive::XferFinish\n");

	/* actually do the work */
	long statusFlags;
	nerr = NuFlush(fpArchive, &statusFlags);
	if (nerr != kNuErrNone) {
		if (nerr != kNuErrAborted) {
			errMsg.Format("Unable to add file: %s.", NuStrError(nerr));
			ShowFailureMsg(fpMsgWnd, errMsg, IDS_FAILED);
		}

		/* see if it got converted to read-only status */
		if (statusFlags & kNuFlushReadOnly)
			fIsReadOnly = true;
		goto bail;
	}

	(void) InternalReload(fpMsgWnd);

bail:
	return;
}


/*
 * ===========================================================================
 *		NufxArchive -- add/update/delete comments
 * ===========================================================================
 */

/*
 * Extract a comment from the archive, converting line terminators to CRLF.
 *
 * Returns "true" on success, "false" on failure.
 */
bool
NufxArchive::GetComment(CWnd* pMsgWnd, const GenericEntry* pGenericEntry,
	CString* pStr)
{
	NufxEntry* pEntry = (NufxEntry*) pGenericEntry;
	CString errMsg;
	const char* kNewEOL = "\r\n";
	int result;
	char* buf;
	long len;

	ASSERT(pGenericEntry->GetHasComment());

	/* use standard extract function to pull comment out */
	buf = nil;
	len = 0;
	result = pEntry->ExtractThreadToBuffer(GenericEntry::kCommentThread,
				&buf, &len, &errMsg);
	if (result != IDOK) {
		WMSG1("Failed getting comment: %s\n", buf);
		ASSERT(buf == nil);
		return false;
	}

	/* convert EOL and add '\0' */
	CString convStr;
	const char* ccp;

	ccp = buf;
	while (len-- && *ccp != '\0') {
		if (len > 1 && *ccp == '\r' && *(ccp+1) == '\n') {
			ccp++;
			len--;
			convStr += kNewEOL;
		} else if (*ccp == '\r' || *ccp == '\n') {
			convStr += kNewEOL;
		} else {
			convStr += *ccp;
		}
		ccp++;
	}

	*pStr = convStr;
	delete[] buf;
	return true;
}

/*
 * Set the comment.  This requires either adding a new comment or updating
 * an existing one.  The latter is constrained by the maximum size of the
 * comment buffer.
 *
 * We want to update in place whenever possible because it's faster (don't
 * have to rewrite the entire archive), but that really only holds for new
 * archives or if we foolishly set the kNuValueModifyOrig flag.
 *
 * Cleanest approach is to delete the existing thread and add a new one.
 * If somebody complains we can try to be smarter about it.
 *
 * Returns "true" on success, "false" on failure.
 */
bool
NufxArchive::SetComment(CWnd* pMsgWnd, GenericEntry* pGenericEntry,
	const CString& str)
{
	NuDataSource* pSource = nil;
	NufxEntry* pEntry = (NufxEntry*) pGenericEntry;
	NuError nerr;
	bool retVal = false;

	/* convert CRLF to CR */
	CString newStr(str);
	char* srcp;
	char* dstp;
	srcp = dstp = newStr.GetBuffer(0);
	while (*srcp != '\0') {
		if (*srcp == '\r' && *(srcp+1) == '\n') {
			srcp++;
			*dstp = '\r';
		} else {
			*dstp = *srcp;
		}
		srcp++;
		dstp++;
	}
	*dstp = '\0';
	newStr.ReleaseBuffer();

	/* get the thread info */
	CString errMsg;
	NuThread thread;
	NuThreadIdx threadIdx;

	pEntry->FindThreadInfo(GenericEntry::kCommentThread, &thread, &errMsg);
	threadIdx = thread.threadIdx;
	if (errMsg.IsEmpty()) {
		/* delete existing thread */
		nerr = NuDeleteThread(fpArchive, threadIdx);
		if (nerr != kNuErrNone) {
			errMsg.Format("Unable to delete thread: %s.", NuStrError(nerr));
			goto bail;
		}
	}

	/* set a maximum pre-size value for the thread */
	long maxLen;
	maxLen = ((newStr.GetLength() + 99) / 100) * 100;
	if (maxLen < 200)
		maxLen = 200;


	/* create a data source to write from */
	nerr = NuCreateDataSourceForBuffer(kNuThreadFormatUncompressed,
			maxLen, (const unsigned char*)(const char*)newStr, 0,
			newStr.GetLength(), nil, &pSource);
	if (nerr != kNuErrNone) {
		errMsg.Format("Unable to create NufxLib data source (len=%d, maxLen=%d).",
			newStr.GetLength(), maxLen);
		goto bail;
	}

	/* add the new thread */
	nerr = NuAddThread(fpArchive, pEntry->GetRecordIdx(),
			kNuThreadIDComment, pSource, nil);
	if (nerr != kNuErrNone) {
		errMsg.Format("Unable to add comment thread: %s.",
			NuStrError(nerr));
		goto bail;
	}
	pSource = nil;	// nufxlib owns it now

	/* flush changes */
	long statusFlags;
	nerr = NuFlush(fpArchive, &statusFlags);
	if (nerr != kNuErrNone) {
		errMsg.Format("Unable to flush comment changes: %s.",
			NuStrError(nerr));
		goto bail;
	}

	/* reload GenericArchive from NufxLib */
	if (InternalReload(fpMsgWnd) == kNuErrNone)
		retVal = true;

bail:
	NuFreeDataSource(pSource);
	if (!retVal) {
		WMSG1("FAILED: %s\n", (LPCTSTR) errMsg);
		NuAbort(fpArchive);
	}
	return retVal;
}

/*
 * Remove a comment.
 *
 * Returns "true" on success, "false" on failure.
 */
bool
NufxArchive::DeleteComment(CWnd* pMsgWnd, GenericEntry* pGenericEntry)
{
	CString errMsg;
	NuError nerr;
	NufxEntry* pEntry = (NufxEntry*) pGenericEntry;
	NuThread thread;
	NuThreadIdx threadIdx;
	bool retVal = false;

	pEntry->FindThreadInfo(GenericEntry::kCommentThread, &thread, &errMsg);
	if (!errMsg.IsEmpty())
		goto bail;
	threadIdx = thread.threadIdx;

	nerr = NuDeleteThread(fpArchive, threadIdx);
	if (nerr != kNuErrNone) {
		errMsg.Format("Unable to delete thread: %s.", NuStrError(nerr));
		goto bail;
	}

	/* flush changes */
	long statusFlags;
	nerr = NuFlush(fpArchive, &statusFlags);
	if (nerr != kNuErrNone) {
		errMsg.Format("Unable to flush comment deletion: %s.",
			NuStrError(nerr));
		goto bail;
	}

	/* reload GenericArchive from NufxLib */
	if (InternalReload(pMsgWnd) == kNuErrNone)
		retVal = true;

bail:
	if (retVal != 0) {
		WMSG1("FAILED: %s\n", (LPCTSTR) errMsg);
		NuAbort(fpArchive);
	}
	return retVal;
}


/*
 * Set file properties via the NuSetRecordAttr call.
 *
 * Get the existing properties, copy the fields from FileProps over, and
 * set them.
 *
 * [currently only supports file type, aux type, and access flags]
 *
 * Technically we should reload the GenericArchive from the NufxArchive,
 * but the set of changes is pretty small, so we just make them here.
 */
bool
NufxArchive::SetProps(CWnd* pMsgWnd, GenericEntry* pEntry,
	const FileProps* pProps)
{
	NuError nerr;
	NufxEntry* pNufxEntry = (NufxEntry*) pEntry;
	const NuRecord* pRecord;
	NuRecordAttr recordAttr;

	WMSG3(" SET fileType=0x%02x auxType=0x%04x access=0x%02x\n",
		pProps->fileType, pProps->auxType, pProps->access);

	nerr = NuGetRecord(fpArchive, pNufxEntry->GetRecordIdx(), &pRecord);
	if (nerr != kNuErrNone) {
		WMSG2("ERROR: couldn't find recordIdx %ld: %s\n",
			pNufxEntry->GetRecordIdx(), NuStrError(nerr));
		return false;
	}

	NuRecordCopyAttr(&recordAttr, pRecord);
	recordAttr.fileType = pProps->fileType;
	recordAttr.extraType = pProps->auxType;
	recordAttr.access = pProps->access;

	nerr = NuSetRecordAttr(fpArchive, pNufxEntry->GetRecordIdx(), &recordAttr);
	if (nerr != kNuErrNone) {
		WMSG2("ERROR: couldn't set recordAttr %ld: %s\n",
			pNufxEntry->GetRecordIdx(), NuStrError(nerr));
		return false;
	}

	long statusFlags;
	nerr = NuFlush(fpArchive, &statusFlags);
	if (nerr != kNuErrNone) {
		WMSG1("ERROR: NuFlush failed: %s\n", NuStrError(nerr));

		/* see if it got converted to read-only status */
		if (statusFlags & kNuFlushReadOnly)
			fIsReadOnly = true;
		return false;
	}

	WMSG0("Props set\n");

	/* do this in lieu of reloading GenericArchive */
	pEntry->SetFileType(pProps->fileType);
	pEntry->SetAuxType(pProps->auxType);
	pEntry->SetAccess(pProps->access);

	return true;
}
