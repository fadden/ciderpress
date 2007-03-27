/*
 * CiderPress
 * Copyright (C) 2007 by faddenSoft, LLC.  All Rights Reserved.
 * See the file LICENSE for distribution terms.
 */
/*
 * AWGS file handling.
 */
#ifndef __LR_AWGS__
#define __LR_AWGS__

#include "ReformatBase.h"

/*
 * Reformat an AppleWorks GS Word Processor file.
 */
class ReformatAWGS_WP : public ReformatText {
public:
	ReformatAWGS_WP(void) {}
	virtual ~ReformatAWGS_WP(void) {}

	virtual void Examine(ReformatHolder* pHolder);
	virtual int Process(const ReformatHolder* pHolder,
		ReformatHolder::ReformatID id, ReformatHolder::ReformatPart part,
		ReformatOutput* pOutput);

private:
	/*
	 * Definition of a "SaveArray" entry.
	 */
	typedef struct SaveArrayEntry {
		unsigned short	textBlock;		// Text Block number
		unsigned short	offset;			// add to offset of text block
		unsigned short	attributes;		// 0=normal text, 1=page break para
		unsigned short	rulerNum;		// number of ruler for this para
		unsigned short	pixelHeight;	// height of para in pixels
		unsigned short	numLines;		// number of lines in this para
	} SaveArrayEntry;

	/*
	 * Every document has three chunks: header, footer, main document.  Each
	 * has the same general structure.
	 */
	typedef struct Chunk {
		const unsigned char*	saveArray;
		const unsigned char*	rulers;
		const unsigned char*	textBlocks;
		unsigned short	saveArrayCount;
		unsigned short	numRulers;
		unsigned short	numTextBlocks;
	} Chunk;

	enum {
		kExpectedVersion1	= 0x1011,	// 1.0v2 and 1.1
		kExpectedVersion2	= 0x0006,	// 1.0? beta?
		kExpectedIntVersion	= 0x0002,	// internal version number
		kDocHeaderLen		= 282,
		kWPGlobalsLen		= 386,
		kMinExpectedLen		= kWPGlobalsLen + kDocHeaderLen + 2 +1,
		kSaveArrayEntryLen	= 12,
		kRulerEntryLen		= 52,
		kMinTextBlockSize	= 4,		// two counts
	};

	/* AWGS ruler "statusBits" values */
	enum {
		kAWGSJustifyFull		= 0x80,
		kAWGSJustifyRight		= 0x40,
		kAWGSJustifyCenter		= 0x20,
		kAWGSJustifyLeft		= 0x10,
		kAWGSNoBreakPara		= 0x08,		// para does not span pages
		kAWGSTripleSpace		= 0x04,		// really double
		kAWGSDoubleSpace		= 0x02,		// really 1.5x
		kAWGSSingleSpace		= 0x01
	};

	bool ReadChunk(const unsigned char** pSrcBuf, long* pSrcLen,
		Chunk* pChunk);
	void PrintChunk(const Chunk* pChunk);
	const unsigned char* FindTextBlock(const Chunk* pChunk, int blockNum);
	int PrintParagraph(const unsigned char* ptr, long maxLen);
	unsigned short GetNumRulers(const unsigned char* pSaveArray,
		unsigned short saveArrayCount);
	unsigned short GetNumTextBlocks(const unsigned char* pSaveArray,
		unsigned short saveArrayCount);
	void UnpackSaveArrayEntry(const unsigned char* pSaveArray,
		SaveArrayEntry* pSAE);
	bool SkipTextBlocks(const unsigned char** pSrcBuf,
		long* pSrcLen, int blockCount);
};

#endif /*__LR_AWGS__*/
