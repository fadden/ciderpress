/*
 * CiderPress
 * Copyright (C) 2007 by faddenSoft, LLC.  All Rights Reserved.
 * See the file LICENSE for distribution terms.
 */
/*
 * Reformat AppleWorks documents.
 */
#ifndef __LR_APPLEWORKS__
#define __LR_APPLEWORKS__

#include "ReformatBase.h"

/*
 * Reformat an AppleWorks WP document into formatted text.
 */
class ReformatAWP : public ReformatText {
public:
	ReformatAWP(void) { fShowEmbeds = true; }
	virtual ~ReformatAWP(void) {}

	virtual void Examine(ReformatHolder* pHolder);
	virtual int Process(const ReformatHolder* pHolder,
		ReformatHolder::ReformatID id, ReformatHolder::ReformatPart part,
		ReformatOutput* pOutput);

private:
	typedef unsigned char uchar;

	/*
	 * Constants.
	 */
	enum {
		kFileHeaderSize = 300,		/* expected size of FileHeader */
		kSeventyNine = 79,			/* value of FileHeader.seventyNine */

		kSFMinVers30 = 30,			/* indicates AW 3.0 */
		kTabFlagsIsRuler = 0xff,	/* compare against byte 2 of text record */
		kCRatEOL = 0x80,			/* flag on byte 3 of the text record */
		kMinTextChar = 0x20,		/* values from 0x01 - 0x1f are special */
		kEOFMarker = 0xff,			/* two of these found at end of file */

		kLineRecordText = 0x00,
		kLineRecordCarriageReturn = 0xd0,
		kLineRecordCommandMin = 0xd4,
		kLineRecordCommandMax = 0xff,

		kLineRecordCommandPageHeaderEnd = 0xd5,
		kLineRecordCommandPageFooterEnd = 0xd6,
		kLineRecordCommandRightJustify = 0xd7,
		kLineRecordCommandPlatenWidth = 0xd8,		// 10ths of an inch
		kLineRecordCommandLeftMargin = 0xd9,		// 10ths of an inch
		kLineRecordCommandRightMargin = 0xda,		// 10ths of an inch
		kLineRecordCommandCharsPerInch = 0xdb,
		kLineRecordCommandProportional1 = 0xdc,
		kLineRecordCommandProportional2 = 0xdd,
		kLineRecordCommandIndent = 0xde,
		kLineRecordCommandJustify = 0xdf,
		kLineRecordCommandUnjustify = 0xe0,
		kLineRecordCommandCenter = 0xe1,
		kLineRecordCommandPaperLength = 0xe2,
		kLineRecordCommandTopMargin = 0xe3,
		kLineRecordCommandBottomMargin = 0xe4,
		kLineRecordCommandLinesPerInch = 0xe5,
		kLineRecordCommandSingleSpace = 0xe6,
		kLineRecordCommandDoubleSpace = 0xe7,
		kLineRecordCommandTripleSpace = 0xe8,
		kLineRecordCommandNewPage = 0xe9,
		kLineRecordCommandGroupBegin = 0xea,
		kLineRecordCommandGroupEnd = 0xeb,
		kLineRecordCommandPageHeader = 0xec,
		kLineRecordCommandPageFooter = 0xed,
		kLineRecordCommandSkipLines = 0xee,
		kLineRecordCommandPageNumber = 0xef,
		kLineRecordCommandPauseEachPage = 0xf0,
		kLineRecordCommandPauseHere = 0xf1,
		kLineRecordCommandSetMarker = 0xf2,
		kLineRecordCommandSetPageNumber = 0xf3,
		kLineRecordCommandPageBreak = 0xf4,
		kLineRecordCommandPageBreak256 = 0xf5,
		kLineRecordCommandPageBreakPara = 0xf6,
		kLineRecordCommandPageBreakPara256 = 0xf7,
		// end of file is 0xff

		kSpecialCharBoldBegin = 0x01,
		kSpecialCharBoldEnd = 0x02,
		kSpecialCharSuperscriptBegin = 0x03,
		kSpecialCharSuperscriptEnd = 0x04,
		kSpecialCharSubscriptBegin = 0x05,
		kSpecialCharSubscriptEnd = 0x06,
		kSpecialCharUnderlineBegin = 0x07,
		kSpecialCharUnderlineEnd = 0x08,
		kSpecialCharPrintPageNumber = 0x09,
		kSpecialCharEnterKeyboard = 0x0a,
		kSpecialCharStickySpace = 0x0b,
		kSpecialCharMailMerge = 0x0c,
		// reserved = 0x0d,
		kSpecialCharPrintDate = 0x0e,
		kSpecialCharPrintTime = 0x0f,
		// Special Code N = 0x10-15
		kSpecialCharTab = 0x16,
		kSpecialCharTabFill = 0x17,	/* tab fill char, not vis in doc */
		// reserved = 0x18

		kMaxSoftFailures = 8,		/* give up if it looks like junk */
	};

	/*
	 * File header, mapped directly on top of the input.  This structure must
	 * be exactly 300 bytes.
	 */
	typedef struct FileHeader {
		uchar		unused1[4];		/* 000 - 003: not used */
		uchar		seventyNine;	/* 004      : $4f (79) */
		uchar		tabStops[80];	/* 005 - 084: tab stops, one of "=<^>.|" */
		uchar		zoomFlag;		/* 085      : boolean Zoom flag */
		uchar		unused2[4];		/* 086 - 089: not used */
		uchar		paginatedFlag;	/* 090      : boolean Paginated flag */
		uchar		minLeftMargin;	/* 091      : minimum "unseen" left margin */
		uchar		mailMergeFlag;	/* 092      : boolean - file has merge cmds */
		uchar		unused3[83];	/* 093 - 175: not used, reserved */
		uchar		multiRulerFlag;	/* 176      : (3.0) boolean Multiple Rulers */
		uchar		tabRulers[6];	/* 177 - 182: (3.0) used internally */
		uchar		sfMinVers;		/* 183      : (3.0) min version of AW req */
		uchar		unused4[66];	/* 184 - 249: reserved */
		uchar		unused5[50];	/* 250 - 299: available */
	} FileHeader;

	/*
	 * Current state of the document.
	 */
	typedef struct DocState {
		long		softFailures;
		long		line;

		/* not using these yet */
		int			leftMargin;
	} DocState;


	void InitDocState(void);
	int ProcessLineRecord(uchar lineRecData, uchar lineRecCode,
		const unsigned char** pSrcPtr, long* pLength);
	int HandleTextRecord(uchar lineRecData,
		const unsigned char** pSrcPtr, long* pLength);

	FileHeader	fFileHeader;
	DocState	fDocState;
	bool		fShowEmbeds;
};

/*
 * Reformat an AppleWorks DB document into CSV.
 */
class ReformatADB : public ReformatText {
public:
	ReformatADB(void) {}
	virtual ~ReformatADB(void) {}

	virtual void Examine(ReformatHolder* pHolder);
	virtual int Process(const ReformatHolder* pHolder,
		ReformatHolder::ReformatID id, ReformatHolder::ReformatPart part,
		ReformatOutput* pOutput);

private:
	enum {
		kOffsetToFirstCatHeader = 357,
		kMinHeaderLen = 379,
		kReportRecordLen = 600,
		kCatNameLen = 20,
		kCatHeaderLen = 22,
	};
};

/*
 * Reformat an AppleWorks SS document into CSV.
 */
class ReformatASP : public ReformatText {
public:
	ReformatASP(void) { fCurrentRow = fCurrentCol = 1; }
	virtual ~ReformatASP(void) {}

	virtual void Examine(ReformatHolder* pHolder);
	virtual int Process(const ReformatHolder* pHolder,
		ReformatHolder::ReformatID id, ReformatHolder::ReformatPart part,
		ReformatOutput* pOutput);

private:
	enum {
		kFileHeaderSize = 300,
		kPropCount = 8,
		kSANELen = 8,
	};

	typedef unsigned char uchar;

	/*
	 * File header, mapped directly on top of the input.  This structure must
	 * be exactly 300 bytes.
	 */
	typedef struct FileHeader {
		uchar		unused1[4];		/* 000-003: not used */
		uchar		colWidth[127];	/* 004-130: column width for each column */
		uchar		recalcOrder;	/* 131    : recalc order ('R' or 'C') */
		uchar		recalcFreq;		/* 132    : recalc frequency ('A' or 'M') */
		uchar		lastRowRef[2];	/* 133-134: last row referenced */
		uchar		lastColRef;		/* 135    : last column referenced */
		uchar		numWindows;		/* 136    : '1'/'S'/'T' */
		uchar		windowSync;		/* 137    : if two windows, are sync'ed? */
		uchar		winInfo1[24];	/* 138-161: struct with info for 1st window */
		uchar		winInfo2[24];	/* 162-185: struct with info for 2nd window */
		uchar		unused2[27];	/* 186-212: not used */
		uchar		cellProt;		/* 213    : cell protection on/off */
		uchar		unused3;		/* 214    : not used */
		uchar		platenWidth;	/* 215    : platen width */
		uchar		leftMargin;		/* 216    : left margin, in 1/10th inches */
		uchar		rightMargin;	/* 217    : right margin */
		uchar		cpi;			/* 218    : characters per inch */
		uchar		paperLength;	/* 219    : paper length, 1/10th inches */
		uchar		topMargin;		/* 220    : top margin */
		uchar		bottomMargin;	/* 221    : bottom margin */
		uchar		lpi;			/* 222    : lines per inch (6 or 8) */
		uchar		spacing;		/* 223    : 'S'ingle, 'D'ouble, or 'T'riple */
		uchar		printerCodes[14];/*224-237: "special codes" for printer */
		uchar		printDash;		/* 238    : print dash when entry is blank */
		uchar		reportHeader;	/* 239    : print report header */
		uchar		zoomed;			/* 240    : zoomed to show formulas */
		uchar		reserved1;		/* 241    : used internally (v2.1) */
		uchar		ssMinVers;		/* 242    : holds 0 for AW2.x, 0x1e for AW3.x */
		uchar		reserved2[7];	/* 243-249: reserved */
		uchar		available[50];	/* 250-299: available for non-AW app use */
	} FileHeader;

	int	fCurrentRow, fCurrentCol;
	char fPrintColBuf[3];

	int ProcessRow(int rowNum, const unsigned char** pSrcPtr,
		long* pLength);
	void ProcessCell(const unsigned char* srcPtr, long cellLength);
	void PrintToken(const unsigned char** pSrcPtr, long* pLength);
	const char* PrintCol(int col);
	double ConvertSANEDouble(const unsigned char* srcPtr);
};

#endif /*__LR_APPLEWORKS__*/
