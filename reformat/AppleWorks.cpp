/*
 * CiderPress
 * Copyright (C) 2007 by faddenSoft, LLC.  All Rights Reserved.
 * See the file LICENSE for distribution terms.
 */
/*
 * Convert AppleWorks 3.0 documents.
 */
#include "StdAfx.h"
#include "AppleWorks.h"

/*
 * ===========================================================================
 *		AppleWorks WP
 * ===========================================================================
 */

/*
 * AppleWorks word processor file format, from FTN.1A.xxxx.
 *
 * The overall file format is:
 *
 *	file header
 *	array of line records
 *	$ff $ff
 *	optional tags
 */

/*
 * Decide whether or not we want to handle this file.
 */
void
ReformatAWP::Examine(ReformatHolder* pHolder)
{
	ReformatHolder::ReformatApplies applies = ReformatHolder::kApplicNot;

	if (pHolder->GetFileType() == kTypeAWP)
		applies = ReformatHolder::kApplicProbably;

	pHolder->SetApplic(ReformatHolder::kReformatAWP, applies,
		ReformatHolder::kApplicNot, ReformatHolder::kApplicNot);
}

/*
 * Reformat an AppleWorks WP document.
 */
int
ReformatAWP::Process(const ReformatHolder* pHolder,
	ReformatHolder::ReformatID id, ReformatHolder::ReformatPart part,
	ReformatOutput* pOutput)
{
	const unsigned char* srcPtr = pHolder->GetSourceBuf(part);
	long srcLen = pHolder->GetSourceLen(part);
	long length = srcLen;
	int retval = -1;

	bool skipRecord;
	uchar lineRecCode, lineRecData;

	if (srcLen > 65536)
		fUseRTF = false;

	//fUseRTF = false;
	//fShowEmbeds = false;

	/* expect header plus EOF bytes at least */
	if (srcLen <= kFileHeaderSize) {
		WMSG0("  AWP truncated?\n");
		goto bail;
	}

	RTFBegin(kRTFFlagColorTable);

	/*
	 * Grab the file header.
	 */
	assert(sizeof(fFileHeader) == kFileHeaderSize);

	memcpy(&fFileHeader, srcPtr, sizeof(fFileHeader));
	srcPtr += sizeof(fFileHeader);
	length -= sizeof(fFileHeader);

	/* do some quick sanity checks */
	if (fFileHeader.seventyNine != kSeventyNine) {
		WMSG2("ERROR: expected %d in signature byte, found %d\n",
			kSeventyNine, fFileHeader.seventyNine);
		goto bail;
	}
	if (fFileHeader.sfMinVers && fFileHeader.sfMinVers != kSFMinVers30) {
		WMSG1("WARNING: unexpected value %d for sfMinVers\n",
			fFileHeader.sfMinVers);
		/* keep going */
	}

	InitDocState();

	/* if first line record is invalid, skip it */
	skipRecord = false;
	if (fFileHeader.sfMinVers == kSFMinVers30)
		skipRecord = true;

	/* set margins to 1.0 inches at 10cpi */
	RTFLeftMargin(10);
	RTFRightMargin(10);

	/*
	 * Read the line records.
	 */
	while (1) {
		lineRecData = Read8(&srcPtr, &length);
		lineRecCode = Read8(&srcPtr, &length);

		if (length < 0) {
			WMSG0(" AWP truncated file\n");
			goto bail;
		}

		if (skipRecord) {
			skipRecord = false;
			continue;
		}

		/* end of data reached? */
		if (lineRecData == kEOFMarker && lineRecCode == kEOFMarker)
			break;

		if (ProcessLineRecord(lineRecData, lineRecCode, &srcPtr, &length) != 0)
		{
			WMSG0("ProcessLineRecord failed, bailing\n");
			goto bail;
		}
	}


	/* 
	 * Read the optional tags.
	 */
	/* (nah) */

	RTFEnd();

	SetResultBuffer(pOutput);
	retval = 0;

bail:
	return retval;
}

/*
 * Initialize the DocState structure.
 */
void
ReformatAWP::InitDocState(void)
{
	memset(&fDocState, 0, sizeof(fDocState));
	fDocState.line = 1;
}

/*
 * Process a line record.
 */
int
ReformatAWP::ProcessLineRecord(uchar lineRecData, uchar lineRecCode,
	const unsigned char** pSrcPtr, long* pLength)
{
	int err = 0;

	//WMSG2(" AWP line rec <0x%02x><0x%02x>\n", lineRecCode, lineRecData);

	if (lineRecCode == kLineRecordCarriageReturn) {
		/* ignore the horizontal offset for now */
		RTFNewPara();
	} else if (lineRecCode == kLineRecordText) {
		err = HandleTextRecord(lineRecData, pSrcPtr, pLength);
	} else if (lineRecCode >= kLineRecordCommandMin &&
			   lineRecCode <= kLineRecordCommandMax)
	{
		switch (lineRecCode) {
		case kLineRecordCommandCenter:
			RTFParaCenter();
			break;
		case kLineRecordCommandRightJustify:
			RTFParaRight();
			break;
		case kLineRecordCommandUnjustify:
			RTFParaLeft();
			break;
		case kLineRecordCommandJustify:
			RTFParaJustify();
			break;
		case kLineRecordCommandLeftMargin:
			RTFLeftMargin(lineRecData);
			break;
		case kLineRecordCommandRightMargin:
			RTFRightMargin(lineRecData);
			break;

		/* we handle these by showing them in the text */
		case kLineRecordCommandPageNumber:
			if (fShowEmbeds) {
				RTFSetColor(kColorBlue);
				BufPrintf("<set-page-number %d>", lineRecData);
				RTFSetColor(kColorNone);
				RTFNewPara();
			}
			break;
		case kLineRecordCommandPageHeader:
			if (fShowEmbeds) {
				RTFSetColor(kColorBlue);
				BufPrintf("<page-header>");
				RTFSetColor(kColorNone);
				RTFNewPara();
			}
			break;
		case kLineRecordCommandPageHeaderEnd:
			if (fShowEmbeds) {
				RTFSetColor(kColorBlue);
				BufPrintf("</page-header>");
				RTFSetColor(kColorNone);
				RTFNewPara();
			}
			break;
		case kLineRecordCommandPageFooter:
			if (fShowEmbeds) {
				RTFSetColor(kColorBlue);
				BufPrintf("<page-footer>");
				RTFSetColor(kColorNone);
				RTFNewPara();
			}
			break;
		case kLineRecordCommandPageFooterEnd:
			if (fShowEmbeds) {
				RTFSetColor(kColorBlue);
				BufPrintf("</page-footer>");
				RTFSetColor(kColorNone);
				RTFNewPara();
			}
			break;
		case kLineRecordCommandNewPage:
			if (fUseRTF)
				RTFPageBreak();
			else if (fShowEmbeds) {
				RTFSetColor(kColorBlue);	// won't do anything
				BufPrintf("<page-break>");
				RTFSetColor(kColorNone);
			}
			break;

		case kLineRecordCommandPlatenWidth:
		case kLineRecordCommandCharsPerInch:
		case kLineRecordCommandProportional1:
		case kLineRecordCommandProportional2:
		case kLineRecordCommandIndent:
		case kLineRecordCommandPaperLength:
		case kLineRecordCommandTopMargin:
		case kLineRecordCommandBottomMargin:
		case kLineRecordCommandLinesPerInch:
		case kLineRecordCommandSingleSpace:
		case kLineRecordCommandDoubleSpace:
		case kLineRecordCommandTripleSpace:
		case kLineRecordCommandGroupBegin:
		case kLineRecordCommandGroupEnd:
		case kLineRecordCommandSkipLines:
		case kLineRecordCommandPauseEachPage:
		case kLineRecordCommandPauseHere:
		case kLineRecordCommandSetMarker:
		case kLineRecordCommandSetPageNumber:
		case kLineRecordCommandPageBreak:
		case kLineRecordCommandPageBreak256:
		case kLineRecordCommandPageBreakPara:
		case kLineRecordCommandPageBreakPara256:
		default:
			WMSG2(" AWP cmd <0x%02x><0x%02x>\n", lineRecCode, lineRecData);
			break;
		}
	} else {
		/* bad command */
		WMSG2("WARNING: unrecognized code 0x%02x at 0x%08lx\n", lineRecCode,
			*pSrcPtr);
		fDocState.softFailures++;
		if (fDocState.softFailures > kMaxSoftFailures) {
			WMSG0("ERROR: too many failures, giving up\n");
			err = -1;
		}
	}

	return err;
}

/*
 * Handle a text record.  The first two bytes are flags, the rest is
 * either the text or a ruler.  Special codes may be embedded in the text.
 *
 * "lineRecData" has the number of bytes of input that we have yet to read.
 */
int
ReformatAWP::HandleTextRecord(uchar lineRecData,
	const unsigned char** pSrcPtr, long* pLength)
{
	int err = 0;
	uchar tabFlags;
	uchar byteCountPlusCR;
	int byteCount = lineRecData;
	bool noOutput = false;
	int ic;

	tabFlags = Read8(pSrcPtr, pLength);
	byteCount--;
	byteCountPlusCR = Read8(pSrcPtr, pLength);
	byteCount--;
	if (*pLength < 0) {
		err = -1;
		goto bail;
	}

	if (byteCount <= 0) {
		WMSG2("WARNING: line %ld: short line (%d)\n",
			fDocState.line, byteCount);
		/* this is bad, but keep going anyway */
	}

	if ((byteCountPlusCR & ~kCRatEOL) != byteCount) {
		WMSG3("WARNING: line %ld: byteCount now %d, offset 3 count %d\n",
			fDocState.line, byteCount, byteCountPlusCR & ~kCRatEOL);
		/* not sure why this would legally happen */
	}

	if (tabFlags == kTabFlagsIsRuler)
		noOutput = true;

	while (byteCount--) {
		ic = Read8(pSrcPtr, pLength);
		if (*pLength < 0) {
			err = -1;
			goto bail;
		}

		if (noOutput)
			continue;

		if (ic < kMinTextChar) {
			switch (ic) {
			case kSpecialCharBoldBegin:
				RTFBoldOn();
				break;
			case kSpecialCharBoldEnd:
				RTFBoldOff();
				break;
			case kSpecialCharSuperscriptBegin:
				RTFSuperscriptOn();
				break;
			case kSpecialCharSuperscriptEnd:
				RTFSuperscriptOff();
				break;
			case kSpecialCharSubscriptBegin:
				RTFSubscriptOn();
				break;
			case kSpecialCharSubscriptEnd:
				RTFSubscriptOff();
				break;
			case kSpecialCharUnderlineBegin:
				RTFUnderlineOn();
				break;
			case kSpecialCharUnderlineEnd:
				RTFUnderlineOff();
				break;
			case kSpecialCharEnterKeyboard:
				if (fShowEmbeds) {
					RTFSetColor(kColorBlue);
					BufPrintf("<kdb-entry>");
					RTFSetColor(kColorNone);
				}
				break;
			case kSpecialCharPrintPageNumber:
				if (fShowEmbeds) {
					RTFSetColor(kColorBlue);
					BufPrintf("<page#>");
					RTFSetColor(kColorNone);
				}
				break;
			case kSpecialCharStickySpace:
				/* MSWord uses "\~", but RichEdit ignores that */
				BufPrintf(" ");
				break;
			case kSpecialCharMailMerge:
				if (fShowEmbeds) {
					RTFSetColor(kColorBlue);
					BufPrintf("<mail-merge>");
					RTFSetColor(kColorNone);
				}
			case kSpecialCharPrintDate:
				if (fShowEmbeds) {
					RTFSetColor(kColorBlue);
					BufPrintf("<date>");
					RTFSetColor(kColorNone);
				}
				break;
			case kSpecialCharPrintTime:
				if (fShowEmbeds) {
					RTFSetColor(kColorBlue);
					BufPrintf("<time>");
					RTFSetColor(kColorNone);
				}
				break;
			case kSpecialCharTab:
				if (fUseRTF)
					RTFTab();
				else
					BufPrintf("\t");
				break;
			case kSpecialCharTabFill:
				/* tab fill char, not vis in doc */
				BufPrintf(" ");
				break;
			default:
				WMSG1(" AWP unhandled special char 0x%02x\n", ic);
				if (fShowEmbeds) {
					RTFSetColor(kColorBlue);
					BufPrintf("^");
					RTFSetColor(kColorNone);
				}
			}
		} else {
			if (fUseRTF)
				RTFPrintChar(ic);
			else
				BufPrintf("%c", PrintableChar(ic));
		}
	}

	/* if there's a carriage return at the end of the line, output it now */
	if (byteCountPlusCR & kCRatEOL) {
		RTFNewPara();
	}

	/* another line processed, advance the line counter */
	fDocState.line++;

bail:
	return err;
}


/*
 * ===========================================================================
 *		AppleWorks DB
 * ===========================================================================
 */

/*
 * AppleWorks database file format, from FTN.19.xxxx.
 *
 * The overall file format is:
 *
 *	variable-sized file header
 *	0 to 8 (0 to 30 in 3.0) report records, 600 bytes each
 *	variable-sized data records
 *	$ff $ff
 *	optional tags
 */

/*
 * Decide whether or not we want to handle this file.
 */
void
ReformatADB::Examine(ReformatHolder* pHolder)
{
	ReformatHolder::ReformatApplies applies = ReformatHolder::kApplicNot;

	if (pHolder->GetFileType() == kTypeADB)
		applies = ReformatHolder::kApplicProbably;

	pHolder->SetApplic(ReformatHolder::kReformatADB, applies,
		ReformatHolder::kApplicNot, ReformatHolder::kApplicNot);
}

/*
 * Reformat an AppleWorks DB document.
 */
int
ReformatADB::Process(const ReformatHolder* pHolder,
	ReformatHolder::ReformatID id, ReformatHolder::ReformatPart part,
	ReformatOutput* pOutput)
{
	const unsigned char* srcPtr = pHolder->GetSourceBuf(part);
	long srcLen = pHolder->GetSourceLen(part);
	long length = srcLen;
	int retval = -1;
	int headerLen, numCats, numRecs, numReports;

	fUseRTF = false;

	/* expect header plus EOF bytes at least */
	if (srcLen <= kMinHeaderLen) {
		WMSG0("  ADB truncated?\n");
		goto bail;
	}

	headerLen = Get16LE(srcPtr);
	if (headerLen < kMinHeaderLen || headerLen > length) {
		WMSG2("  ADB bad headerLen %d, file len is %d\n", headerLen,
			srcLen);
		goto bail;
	}

	RTFBegin();

	/* offset +035: #of categories in file */
	numCats = *(srcPtr + 35);
	if (numCats < 1 || numCats > 0x1e) {
		WMSG1("  ADB GLITCH: unexpected numCats %d\n", numCats);
		/* keep going... */
	}
	WMSG1("  ADB should be %d categories\n", numCats);

	/* offset +036-037: #of records in file */
	numRecs = Get16LE(srcPtr + 36) & 0x7fff;
	WMSG1("  ADB should be %d records\n", numRecs);

	/* offset +038: #of reports in file */
	numReports = *(srcPtr + 38);
	WMSG1("  ADB should be %d reports\n", numReports);

	/* dump category names as first record */
	const unsigned char* catPtr;
	int catCount;
	catPtr = srcPtr + 357;
	catCount = numCats;
	while (catCount--) {
		if (catCount == numCats-1)
			BufPrintf("\"");
		else
			BufPrintf(",\"");

		int nameLen = *catPtr;
		const unsigned char* namePtr = catPtr + 1;
		while (nameLen--) {
			if (*namePtr == '"')
				BufPrintf("\"\"");
			else
				BufPrintf("%c", *namePtr);
			namePtr++;
		}

		BufPrintf("\"");

		catPtr += kCatNameLen+2;
	}
	BufPrintf("\r\n");

	/*
	 * Advance pointer to first data record.  The first record contains
	 * "standard values".
	 *
	 * Each record looks like this:
	 *	$00-$01: count of bytes in remainder of record
	 *	$02    : control byte, one of:
	 *			 $01-$7f: number of following bytes for this category
	 *			 $81-$9e: this (minus $80) is #of categories to skip
	 *			 $ff    : end of record
	 *
	 * The data within the categories may have special meanings, e.g. if it
	 * starts with $c0 it's a date record and $d4 is a time record.
	 */
	int offsetToData;
	offsetToData = kOffsetToFirstCatHeader +
				numCats*kCatHeaderLen + numReports*kReportRecordLen;
	WMSG1("  ADB data records begin at offset 0x%08lx\n", offsetToData);
	if (offsetToData >= length) {
		WMSG1("  ADB GLITCH: offset >= length %ld\n", length);
		goto bail;
	}

	srcPtr += offsetToData;
	length -= offsetToData;

	int rr;
	for (rr = 0; rr < numRecs && length > 0; rr++) {
		int recordRem = Read16(&srcPtr, &length);
		if (rr == 0) {
			/* skip first record */
			srcPtr += recordRem;
			length -= recordRem;
			if (*(srcPtr-1) != 0xff) {
				WMSG1("  ADB GLITCH: first record skipped past 0x%02x\n",
					*(srcPtr-1));
				/* keep going, I guess */
			}
			continue;
		}

		int catNum = 0;

		/* scan through all categories in this record */
		int ctrl = Read8(&srcPtr, &length);
		while (ctrl != 0xff && length > 0) {
			if (ctrl >= 0x01 && ctrl <= 0x7f) {
				/* just data */
				if (catNum == 0)
					BufPrintf("\"");
				else
					BufPrintf(",\"");
				if (*srcPtr == 0xc0) {
					static const char kMonths[12][4] = {
						"Jan", "Feb", "Mar", "Apr", "May", "Jun",
						"Jul", "Aug", "Sep", "Oct", "Nov", "Dec"
					};
					/* date entry */
					Read8(&srcPtr, &length);	// throw out the 0xc0
					char year[2], month, day[2];
					year[0] = Read8(&srcPtr, &length);
					year[1] = Read8(&srcPtr, &length);
					month = Read8(&srcPtr, &length);
					day[0] = Read8(&srcPtr, &length);
					day[1] = Read8(&srcPtr, &length);
					if (day[0] == ' ')
						day[0] = '0';
					BufPrintf("%c%c-%s-%s%c%c",
						day[0], day[1],
						month >= 'A' && month <= 'L' ? kMonths[month-'A'] : "???",
						year[0] < '7' ? "20" : "19", year[0], year[1]);
				} else if (*srcPtr == 0xd4) {
					/* time entry */
					Read8(&srcPtr, &length);	// throw out the 0xd4
					char hour, minute[2];
					hour = Read8(&srcPtr, &length);
					minute[0] = Read8(&srcPtr, &length);
					minute[1] = Read8(&srcPtr, &length);
					if (hour >= 'A' && hour < 'M') {
						if (hour == 'A')		// don't show 00:00
							hour = 'A' + 12;
						BufPrintf("%02d:%c%c AM",
							hour - 'A', minute[0], minute[1]);
					} else if (hour >= 'M' && hour <= 'X') {
						if (hour == 'M')		// don't show 00:00
							hour = 'M' + 12;
						BufPrintf("%02d:%c%c PM",
							hour - 'M', minute[0], minute[1]);
					}
				} else {
					while (ctrl--) {
						unsigned char ch = Read8(&srcPtr, &length);
						BufPrintf("%c", ch);
						if (ch == '"')
							BufPrintf("%c", ch);
					}
				}
				BufPrintf("\"");
			} else if (ctrl >= 0x81 && ctrl <= 0x9e) {
				/* skip over empty categories */
				ctrl -= 0x80;
				while (ctrl--) {
					BufPrintf(",");
					catNum++;
				}
				catNum--;	// don't double-count this category
			} else {
				WMSG1("  ADB GLITCH: invalid ctrl byte 0x%02x\n", ctrl);
				break;
				/* keep going anyway? */
			}

			catNum++;
			ctrl = Read8(&srcPtr, &length);
		}
		while (catNum < numCats) {
			BufPrintf(",");
			catNum++;
		}

		/* end of record */
		RTFNewPara();
	}
	WMSG2("  ADB at exit rr=%d numRecs=%d\n", rr, numRecs);

	int checkEnd;
	checkEnd = Read16(&srcPtr, &length);
	if (checkEnd != 0xffff) {
		WMSG1("  ADB GLITCH: last read returned 0x%04x\n", checkEnd);
	} else {
		WMSG0("  ADB found EOF; success\n");
	}

	/* 
	 * Read the optional tags.
	 */
	/* (nah) */

	RTFEnd();

	SetResultBuffer(pOutput);
	pOutput->SetOutputKind(ReformatOutput::kOutputCSV);
	retval = 0;

bail:
	return retval;
}


/*
 * ===========================================================================
 *		AppleWorks SS
 * ===========================================================================
 */

/*
 * AppleWorks spreadsheet file format, from FTN.1b.xxxx.
 *
 * The overall file format is:
 *
 *	fixed-sized file header
 *	series of variable-length row records
 *		collection of cell data
 *	$ff $ff
 *	optional tags
 *
 * The cell data can take several different forms.
 */

/*
 * Decide whether or not we want to handle this file.
 */
void
ReformatASP::Examine(ReformatHolder* pHolder)
{
	ReformatHolder::ReformatApplies applies = ReformatHolder::kApplicNot;

	if (pHolder->GetFileType() == kTypeASP)
		applies = ReformatHolder::kApplicProbably;

	pHolder->SetApplic(ReformatHolder::kReformatASP, applies,
		ReformatHolder::kApplicNot, ReformatHolder::kApplicNot);
}

/*
 * Reformat an AppleWorks SS document.
 */
int
ReformatASP::Process(const ReformatHolder* pHolder,
	ReformatHolder::ReformatID id, ReformatHolder::ReformatPart part,
	ReformatOutput* pOutput)
{
	const unsigned char* srcPtr = pHolder->GetSourceBuf(part);
	long srcLen = pHolder->GetSourceLen(part);
	long length = srcLen;
	int retval = -1;
	const FileHeader* pFileHeader;
	bool aw30flag;

	ASSERT(sizeof(FileHeader) == kFileHeaderSize);

	fUseRTF = false;

	/* must at least have the header */
	if (length < kFileHeaderSize) {
		WMSG0("  ADB truncated?\n");
		goto bail;
	}

	RTFBegin();

	pFileHeader = (const FileHeader*) srcPtr;
	aw30flag = false;
	if (pFileHeader->ssMinVers != 0)
		aw30flag = true;
	WMSG2("  ASP ssMinVers=0x%02x, aw30flag=%d\n",
		pFileHeader->ssMinVers, aw30flag);

	/*
	 * Advance pointer past file header.  v3.0 adds a couple of extra bytes
	 * right after the end of the header that the FTN says we should just
	 * ignore.
	 */
	srcPtr += kFileHeaderSize;
	length -= kFileHeaderSize;
	if (aw30flag) {
		srcPtr += 2;
		length -= 2;
	}

	/*
	 * Loop through the file, reading one row at a time.  There is no row
	 * count because spreadsheets are sparse.
	 *
	 * We assume that rows are stored from top to bottom.  The count begins
	 * with 1, not zero.
	 */
	fCurrentRow = 1;
	while (length > 0) {
		unsigned short rowLen;
		int rowNum;

		/* row length or EOF marker */
		rowLen = Read16(&srcPtr, &length);
		if (rowLen == 0xffff) {
			WMSG0("  ASP found EOF marker, we're done\n");
			break;
		}

		rowNum = Read16(&srcPtr, &length);
		//WMSG2("  ASP  process row %d (cur=%d)\n", rowNum, currentRow);

		/* fill out empty rows */
		ASSERT(fCurrentRow <= rowNum);
		while (fCurrentRow < rowNum) {
			BufPrintf("\"\"\r\n");
			fCurrentRow++;
		}

		if (ProcessRow(rowNum, &srcPtr, &length) != 0)
			break;

		fCurrentRow++;
	}

	/* 
	 * Read the optional tags.
	 */
	/* (nah) */

	RTFEnd();

	SetResultBuffer(pOutput);
	pOutput->SetOutputKind(ReformatOutput::kOutputCSV);
	retval = 0;

bail:
	return retval;
}

/*
 * Process one row of spreadsheet data.
 */
int
ReformatASP::ProcessRow(int rowNum, const unsigned char** pSrcPtr,
	long* pLength)
{
	uchar ctrl;
	bool first = true;

	fCurrentCol = 0;
	while (*pLength > 0) {
		ctrl = Read8(pSrcPtr, pLength);
		if (ctrl >= 0x01 && ctrl <= 0x7f) {
			if (!first)
				BufPrintf(",");
			else
				first = false;
			/* read cell entry contents */
			if (ctrl > *pLength) {
				WMSG2("  ASP GLITCH: cell len exceeds file len (%d %d)\n",
					*pLength, ctrl);
				break;
			}
			ProcessCell(*pSrcPtr, ctrl);
			(*pSrcPtr) += ctrl;
			*pLength -= ctrl;
		} else if (ctrl >= 0x81 && ctrl <= 0xfe) {
			/* skip this many columns */
			if (!first)
				BufPrintf(",");
			else
				first = false;

			ctrl -= 0x80;
			ctrl--;
			while (ctrl--) {
				BufPrintf(",");
				fCurrentCol++;
			}
		} else if (ctrl == 0xff) {
			/* end of row */
			break;
		} else {
			/* unexpected 0x00 or 0x80 */
			WMSG1("  ASP GLITCH: unexpected ctrl byte 0x%02x\n", ctrl);
			break;
		}

		fCurrentCol++;
	}

	BufPrintf("\r\n");

	return 0;
}

/*
 * Process the contents of a single cell.
 */
void
ReformatASP::ProcessCell(const unsigned char* srcPtr, long cellLength)
{
	uchar flag1, flag2;
	double dval;
	int i;

	BufPrintf("\"");

	flag1 = *srcPtr++;
	cellLength--;

	if (flag1 & 0x80) {		/* bit 7 set? */
		/* this is a value, not a label */
		flag2 = *srcPtr++;
		cellLength--;

		if (flag1 & 0x20) {	/* bit 5 set? */
			/* this is a "value constant" */
			dval = ConvertSANEDouble(srcPtr);
			BufPrintf("%f", dval);
		} else {
			if (flag2 & 0x08) {	/* bit 3 set? */
				/* this is a "value label", AW30+ only */
				/* skip over cached string result */
				if (*srcPtr >= cellLength) {
					WMSG0("  ASP GLITCH: invalid value label str len\n");
					BufPrintf("GLITCH");
				} else {
					srcPtr += *srcPtr +1;
					/* output tokens */
					while (cellLength > 0)
						PrintToken(&srcPtr, &cellLength);
				}
			} else {
				/* this is a "value formula" */
				//dval = ConvertSANEDouble(srcPtr);
				/* skip over cached computation result */
				if (cellLength <= kSANELen) {
					WMSG0("  ASP GLITCH: invalid value formula len\n");
					BufPrintf("GLITCH");
				} else {
					srcPtr += kSANELen;
					cellLength -= kSANELen;
					/* [tokens] */
					while (cellLength > 0)
						PrintToken(&srcPtr, &cellLength);
				}
			}
		}
	} else {
		/* this is a label, not a value */

		if (flag1 & 0x20) {	/* bit 5 set? */
			/* propagated label cell */
			for (i = 0; i < kPropCount; i++)
				BufPrintQChar(*srcPtr);
		} else {
			/* regular label cell */
			for (i = 0; i < cellLength; i++)
				BufPrintQChar(srcPtr[i]);
		}
	}

	BufPrintf("\"");
}

/*
 * Print the AppleWorks SS token pointed to by srcPtr.  Some tokens require
 * several bytes to express.
 */
void
ReformatASP::PrintToken(const unsigned char** pSrcPtr, long* pLength)
{
	/* string constants; note these must NOT contain '"' chars */
	const int kTokenStart = 0xc0;
	const char* tokenTable[0x100-kTokenStart] = {
		/*0xc0*/ "@Deg",	"@Rad",		"@Pi",		"@True",
		/*0xc4*/ "@False",	"@Not",		"@IsBlank",	"@IsNA",
		/*0xc8*/ "@IsError", "@Exp",	"@Ln",		"@Log",
		/*0xcc*/ "@Cos",	"@Sin",		"@Tan",		"@ACos",
		/*0xd0*/ "@ASin",	"@ATan2",	"@ATan",	"@Mod",
		/*0xd4*/ "@FV",		"@PV",		"@PMT",		"@Term",
		/*0xd8*/ "@Rate",	"@Round",	"@Or",		"@And",
		/*0xdc*/ "@Sum",	"@Avg",		"@Choose",	"@Count",
		/*0xe0*/ "@Error",	"@IRR",		"@If",		"@Int",
		/*0xe4*/ "@Lookup",	"@Max",		"@Min",		"@NA",
		/*0xe8*/ "@NPV",	"@Sqrt",	"@Abs",		"",
		/*0xec*/ "<>",		">=",		"<=",		"=",
		/*0xf0*/ ">",		"<",		",",		"^",
		/*0xf4*/ ")",		"-",		"+",		"/",
		/*0xf8*/ "*",		"(",		"-" /*unary*/, "+" /*unary*/,
		/*0xfc*/ "...",		"",			"",			""
	};
	uchar token;

	token = Read8(pSrcPtr, pLength);
	if (token < kTokenStart) {
		WMSG1("  ASP GLITCH: funky token 0x%02x\n", token);
		return;
	}

	BufPrintf("%s", tokenTable[token - kTokenStart]);
	if (token == 0xe0 || token == 0xe7) {
		/* @Error and @NA followed by three zero bytes */
		if (*pLength < 3) {
			WMSG0("  ASP GLITCH: ran off end processing tokens\n");
			return;
		}
		*pSrcPtr += 3;
		*pLength -= 3;
	} else if (token == 0xfd) {
		/* SANE double number */
		if (*pLength < 8) {
			WMSG0("  ASP GLITCH: not enough left to grab a SANE\n");
			return;
		}
		double dval = ConvertSANEDouble(*pSrcPtr);
		BufPrintf("%f", dval);
		*pSrcPtr += 8;
		*pLength -= 8;
	} else if (token == 0xfe) {
		/* row, column reference (relative to current cell) */
		int row, col;
		col = Read8(pSrcPtr, pLength);
		if (col >= 128)
			col -= 256;
		row = Read16(pSrcPtr, pLength);
		if (row >= 32768)
			row -= 65536;
		BufPrintf("%s%d", PrintCol(fCurrentCol+col), fCurrentRow+row);
	} else if (token == 0xff) {
		/* Pascal string */
		int i;
		i = Read8(pSrcPtr, pLength);
		if (i > *pLength) {
			WMSG0("  ASP GLITCH: string exceeds cell len\n");
			return;
		}
		while (i--) {
			unsigned char ch = Read8(pSrcPtr, pLength);
			BufPrintQChar(ch);
		}
	}
}

/*
 * Format the current column number into something like 'A' or 'BA'.  This
 * stores the value in fPrintColBuf and returns a pointer to it.
 */
const char*
ReformatASP::PrintCol(int col)
{
	if (col < 0 || col >= 702) {
		WMSG1("  ASP can't PrintCol(%d)\n", col);
		fPrintColBuf[0] = fPrintColBuf[1] = '?';
		fPrintColBuf[2] = '\0';
	} else if (col < 26) {
		fPrintColBuf[0] = 'A' + col;
		fPrintColBuf[1] = '\0';
	} else {
		fPrintColBuf[0] = 'A' + col / 26;
		fPrintColBuf[1] = 'A' + col % 26;
		fPrintColBuf[2] = '\0';
	}
	return fPrintColBuf;
}

/*
 * Convert a 64-bit SANE double to an x86 double.  The format is the same as
 * IEEE 754, which happily is the same used by the VC++6.0 compiler.
 *
 * Info from http://www.cs.trinity.edu/About/The_Courses/cs2322/ieee-fp.html
 * (also http://www.psc.edu/general/software/packages/ieee/ieee.html).
 *
 * -----
 * The 64-bit double format is divided into three fields as shown below:
 *
 *	  1       11               52
 *	+-------------------------------------+
 *	| s |     e    |           f          |
 *	+-------------------------------------+
 *
 * The value v of the number is determined by these fields as shown in
 * the following table:
 *
 * Values of double-format numbers (64 bits) 
 * ___________________________________________________________
 * e        f         v                           class of v
 * ___________________________________________________________
 * 0<e<2047 (any)     v=(-1)s x 2(e-1023) x (1.f)  normalized
 * e=0      f!=0      v=(-1)s x 2(e-1022) x (0.f)  denormalized
 * e=0      f=0       v=(-1)s x 0                 zero
 * e=2047   f=0       v=(-1)s x infinity          infinity
 * e=2047   f!=0      v is a NaN                  NaN
 *
 * For example, the double representation (in hex notation) of 1.5 is 
 * 3FF8000000000000
 * is 
 * 3F847AE147AE147A
 * -----
 */
double
ReformatASP::ConvertSANEDouble(const unsigned char* srcPtr)
{
	double newVal;
	unsigned char* dptr;
	int i;

	ASSERT(sizeof(newVal) == kSANELen);

	dptr = (unsigned char*) &newVal;
	for (i = 0; i < kSANELen; i++)
		*dptr++ = *srcPtr++;

	return newVal;
}
