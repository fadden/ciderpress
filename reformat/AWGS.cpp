/*
 * CiderPress
 * Copyright (C) 2007 by faddenSoft, LLC.  All Rights Reserved.
 * See the file LICENSE for distribution terms.
 */
/*
 * Reformat AWGS files.
 */
#include "StdAfx.h"
#include "AWGS.h"

/*
 * Decide whether or not we want to handle this file.
 */
void
ReformatAWGS_WP::Examine(ReformatHolder* pHolder)
{
	ReformatHolder::ReformatApplies applies = ReformatHolder::kApplicNot;

	if (pHolder->GetFileType() == kTypeGWP && pHolder->GetAuxType() == 0x8010)
		applies = ReformatHolder::kApplicYes;

	pHolder->SetApplic(ReformatHolder::kReformatAWGS_WP, applies,
		ReformatHolder::kApplicNot, ReformatHolder::kApplicNot);
}

/*
 * Convert AWGS into formatted text.
 */
int
ReformatAWGS_WP::Process(const ReformatHolder* pHolder,
	ReformatHolder::ReformatID id, ReformatHolder::ReformatPart part,
	ReformatOutput* pOutput)
{
	const unsigned char* srcBuf = pHolder->GetSourceBuf(part);
	long srcLen = pHolder->GetSourceLen(part);
	fUseRTF = true;
	Chunk doc, header, footer;
	unsigned short val;

	CheckGSCharConv();

	/* must at least have the doc header and globals */
	if (srcLen < kMinExpectedLen) {
		WMSG0("Too short to be AWGS\n");
		return -1;
	}

	RTFBegin(kRTFFlagColorTable);

	/*
	 * Pull interesting values out of the document header.
	 */
	val = Get16LE(srcBuf + 0);
	if (val != kExpectedVersion1 && val != kExpectedVersion2) {
		WMSG2("AWGS_WP: unexpected version number (got 0x%04x, wanted 0x%04x)\n",
			val, kExpectedVersion1);
		DebugBreak();
	}
	val = Get16LE(srcBuf + 2);
	if (val != kDocHeaderLen) {
		WMSG2("Unexpected doc header len (got 0x%04x, wanted 0x%04x)\n",
			val, kDocHeaderLen);
		return -1;
	}
	/* the color table is 32 bytes at +56, should we be interested */

	srcBuf += kDocHeaderLen;
	srcLen -= kDocHeaderLen;

	/*
	 * Pull interesting values out of the WP global variables section.
	 */
	val = Get16LE(srcBuf + 0);
	if (val > kExpectedIntVersion) {
		WMSG2("Unexpected internal version number (got %d, expected %d)\n",
			val, kExpectedIntVersion);
		return -1;
	}

	/* date/time are pascal strings */
	WMSG2("File saved at '%.26s' '%.10s'\n", srcBuf + 6, srcBuf + 32);

	srcBuf += kWPGlobalsLen;
	srcLen -= kWPGlobalsLen;

	/*
	 * Now come the three chunks, in order: main document, header, footer.
	 */
	WMSG0("AWGS_WP: scanning doc\n");
	if (!ReadChunk(&srcBuf, &srcLen, &doc))
		return -1;
	WMSG0("AWGS_WP: scanning header\n");
	if (!ReadChunk(&srcBuf, &srcLen, &header))
		return -1;
	WMSG0("AWGS_WP: scanning footer\n");
	if (!ReadChunk(&srcBuf, &srcLen, &footer))
		return -1;

	if (srcLen != 0) {
		WMSG1("AWGS NOTE: %ld bytes left in file\n", srcLen);
	}

	/*
	 * Dump the chunks, starting with header and footer.
	 */
	RTFSetColor(kColorMediumBlue);
	RTFSetFont(kFontCourierNew);
	RTFSetFontSize(10);
	BufPrintf("<header>");
	RTFSetColor(kColorNone);
	RTFNewPara();

	PrintChunk(&header);

	RTFSetColor(kColorMediumBlue);
	RTFSetFont(kFontCourierNew);
	RTFSetFontSize(10);
	BufPrintf("</header>");
	RTFSetColor(kColorNone);
	RTFNewPara();

	RTFSetColor(kColorMediumBlue);
	RTFSetFont(kFontCourierNew);
	RTFSetFontSize(10);
	BufPrintf("<footer>");
	RTFSetColor(kColorNone);
	RTFNewPara();

	PrintChunk(&footer);

	RTFSetColor(kColorMediumBlue);
	RTFSetFont(kFontCourierNew);
	RTFSetFontSize(10);
	BufPrintf("</footer>");
	RTFSetColor(kColorNone);
	RTFNewPara();

	WMSG0("AWGS_WP: rendering document\n");
	PrintChunk(&doc);

	RTFEnd();

	SetResultBuffer(pOutput, true);
	return 0;
}

/*
 * Read one of the chunks of the file.
 */
bool
ReformatAWGS_WP::ReadChunk(const unsigned char** pSrcBuf, long* pSrcLen,
	Chunk* pChunk)
{
	/* starts with the saveArray count */
	pChunk->saveArrayCount = Get16LE(*pSrcBuf);
	if (pChunk->saveArrayCount == 0) {
		/* AWGS always has at least 1 paragraph */
		WMSG0("Save array is empty\n");
		return false;
	}

	*pSrcBuf += 2;
	*pSrcLen -= 2;

	/* locate and move past the SaveArray */
	pChunk->saveArray = *pSrcBuf;

	*pSrcBuf += pChunk->saveArrayCount * kSaveArrayEntryLen;
	*pSrcLen -= pChunk->saveArrayCount * kSaveArrayEntryLen;
	if (*pSrcLen <= 0) {
		WMSG2("SaveArray exceeds file length (count=%d len now %ld)\n",
			pChunk->saveArrayCount, *pSrcLen);
		return false;
	}

	/*
	 * Scan the "save array" to find the highest-numbered ruler.  This tells
	 * us how many rulers there are.
	 */
	pChunk->numRulers = GetNumRulers(pChunk->saveArray, pChunk->saveArrayCount);
	if (*pSrcLen < pChunk->numRulers * kRulerEntryLen) {
		WMSG2("Not enough room for rulers (rem=%ld, needed=%ld)\n",
			*pSrcLen, pChunk->numRulers * kRulerEntryLen);
		return false;
	}
	WMSG1("+++ found %d rulers\n", pChunk->numRulers);

	pChunk->rulers = *pSrcBuf;
	*pSrcBuf += pChunk->numRulers * kRulerEntryLen;
	*pSrcLen -= pChunk->numRulers * kRulerEntryLen;

	/*
	 * Now we're at the docTextBlocks section.
	 */
	pChunk->textBlocks = *pSrcBuf;
	pChunk->numTextBlocks = GetNumTextBlocks(pChunk->saveArray,
											pChunk->saveArrayCount);
	if (!SkipTextBlocks(pSrcBuf, pSrcLen, pChunk->numTextBlocks))
		return false;

	return true;
}

/*
 * Output a single chunk.  We do this by walking down the saveArray.
 */
void
ReformatAWGS_WP::PrintChunk(const Chunk* pChunk)
{
	const int kDefaultStatusBits = kAWGSJustifyLeft | kAWGSSingleSpace;
	SaveArrayEntry sae;
	const unsigned char* saveArray;
	int saCount;
	const unsigned char* blockPtr;
	long blockLen;
	const unsigned char* pRuler;
	unsigned short rulerStatusBits;

	saveArray = pChunk->saveArray;
	saCount = pChunk->saveArrayCount;
	for ( ; saCount > 0; saCount--, saveArray += kSaveArrayEntryLen) {
		UnpackSaveArrayEntry(saveArray, &sae);

		/*
		 * Page-break paragraphs have no real data and an invalid value
		 * in the "rulerNum" field.  So we just throw out a page break
		 * here and call it a day.
		 */
		if (sae.attributes == 0x0001) {
			/* this is a page-break paragraph */
			RTFSetColor(kColorMediumBlue);
			RTFSetFont(kFontCourierNew);
			RTFSetFontSize(10);
			BufPrintf("<page-break>");
			RTFSetColor(kColorNone);
			RTFNewPara();
			RTFPageBreak();		// only supported by Word
			continue;
		}

		if (sae.rulerNum < pChunk->numRulers) {
			pRuler = pChunk->rulers + sae.rulerNum * kRulerEntryLen;
			rulerStatusBits = Get16LE(pRuler + 2);
		} else {
			WMSG1("AWGS_WP GLITCH: invalid ruler index %d\n", sae.rulerNum);
			rulerStatusBits = kDefaultStatusBits;
		}

		if (rulerStatusBits & kAWGSJustifyFull)
			RTFParaJustify();
		else if (rulerStatusBits & kAWGSJustifyRight)
			RTFParaRight();
		else if (rulerStatusBits & kAWGSJustifyCenter)
			RTFParaCenter();
		else if (rulerStatusBits & kAWGSJustifyLeft)
			RTFParaLeft();
		RTFSetPara();

		/*
		 * Find the text block that holds this paragraph.  We could speed
		 * this up by creating an array of entries rather than walking the
		 * list every time.  However, the block count tends to be fairly
		 * small (e.g. 7 for a 16K doc).
		 */
		blockPtr = FindTextBlock(pChunk, sae.textBlock);
		if (blockPtr == nil) {
			WMSG1("AWGS_WP bad textBlock %d\n", sae.textBlock);
			return;
		}
		blockLen = (long) Get32LE(blockPtr);
		if (blockLen <= 0 || blockLen > 65535) {
			WMSG1("AWGS_WP invalid block len %d\n", blockLen);
			return;
		}
		blockPtr += 4;

		if (sae.offset >= blockLen) {
			WMSG2("AWGS_WP bad offset: %d, blockLen=%ld\n",
				sae.offset, blockLen);
			return;
		}
		PrintParagraph(blockPtr + sae.offset, blockLen - sae.offset);
	}
}

/*
 * Print the contents of the text blocks.
 *
 * We're assured that the text block format is correct because we had to
 * skip through them earlier.  We don't really need to worry about running
 * off the end due to a bad file.
 */
const unsigned char*
ReformatAWGS_WP::FindTextBlock(const Chunk* pChunk, int blockNum)
{
	const unsigned char* blockPtr = pChunk->textBlocks;
	long count = pChunk->numTextBlocks;
	unsigned long blockSize;

	while (blockNum--) {
		blockSize = Get32LE(blockPtr);
		blockPtr += 4 + blockSize;
	}

	return blockPtr;
}


/*
 * Print one paragraph.
 *
 * Stop when we hit '\r'.  We watch "maxLen" just to be safe.
 *
 * Returns the #of bytes consumed.
 */
int
ReformatAWGS_WP::PrintParagraph(const unsigned char* ptr, long maxLen)
{
	const unsigned char* startPtr = ptr;
	unsigned short firstFont;
	unsigned char firstStyle, firstSize, firstColor;
	unsigned char uch;

	if (maxLen < 7) {
		WMSG1("AWGS_WP GLITCH: not enough storage for para header (%d)\n",
			maxLen);
		return 1;	// don't return zero or we might loop forever
	}
	/* pull out the paragraph header */
	firstFont = Get16LE(ptr);
	firstStyle = *(ptr + 2);
	firstSize = *(ptr + 3);
	firstColor = *(ptr + 4);

	ptr += 7;
	maxLen -= 7;

	/*
	 * Set the font first; that defines the point size multiplier.  Set
	 * the size second, because the point size determines whether we
	 * show underline.  Set the style last.
	 */
	//WMSG3("+++ Para start: font=0x%04x size=%d style=0x%02x\n",
	//	firstFont, firstSize, firstStyle);
	RTFSetGSFont(firstFont);
	RTFSetGSFontSize(firstSize);
	RTFSetGSFontStyle(firstStyle);

	while (maxLen > 0) {
		uch = *ptr++;
		maxLen--;
		switch (uch) {
		case 0x01:		// font change - two bytes follow
			if (maxLen >= 2) {
				RTFSetGSFont(Get16LE(ptr));
				ptr += 2;
				maxLen -= 2;
			}
			break;
		case 0x02:		// text style change
			if (maxLen >= 1) {
				RTFSetGSFontStyle(*ptr++);
				maxLen--;
			}
			break;
		case 0x03:		// text size change
			if (maxLen >= 1) {
				RTFSetGSFontSize(*ptr++);
				maxLen--;
			}
			break;
		case 0x04:		// color change (0-15)
			if (maxLen >= 1) {
				ptr++;
				maxLen--;
			}
			break;
		case 0x05:		// page token (replace with page #)
		case 0x06:		// date token (replace with date)
		case 0x07:		// time token (replace with time)
			RTFSetColor(kColorMediumBlue);
			if (uch == 0x05)
				BufPrintf("<page>");
			else if (uch == 0x06)
				BufPrintf("<date>");
			else
				BufPrintf("<time>");
			RTFSetColor(kColorNone);
			break;
		case '\r':
			RTFNewPara();
			return ptr - startPtr;
		case '\t':
			RTFTab();
			break;
		default:
			RTFPrintExtChar(ConvertGSChar(uch));
			break;
		}
	}

	WMSG0("AWGS_WP: WARNING: ran out of data before hitting '\r'\n");
	return ptr - startPtr;
}


/*
 * Run through the SaveArray and find the highest-numbered ruler index.
 */
unsigned short
ReformatAWGS_WP::GetNumRulers(const unsigned char* pSaveArray,
	unsigned short saveArrayCount)
{
	SaveArrayEntry sa;
	int maxRuler = -1;

	while (saveArrayCount--) {
		UnpackSaveArrayEntry(pSaveArray, &sa);

		/*
		 * Ignore the record if sa.attributes == 1 (page break).
		 */
		if (sa.attributes == 0 && sa.rulerNum > maxRuler)
			maxRuler = sa.rulerNum;

		pSaveArray += kSaveArrayEntryLen;
	}

	/* there must be at least one paragraph, so this must hold */
	assert(maxRuler >= 0);

	return (unsigned short) (maxRuler+1);
}

/*
 * Run through the SaveArray and find the highest-numbered text block
 * index.
 *
 * These are stored linearly, so we just need to look at the last entry.
 */
unsigned short
ReformatAWGS_WP::GetNumTextBlocks(const unsigned char* pSaveArray,
	unsigned short saveArrayCount)
{
	SaveArrayEntry sa;
	unsigned short maxTextBlock;

	assert(saveArrayCount > 0);
	UnpackSaveArrayEntry(pSaveArray + (saveArrayCount-1) * kSaveArrayEntryLen,
		&sa);
	maxTextBlock = sa.textBlock;

#ifdef _DEBUG
	int maxPara = -1;

	while (saveArrayCount--) {
		UnpackSaveArrayEntry(pSaveArray, &sa);

		/*
		 * Ignore the record if sa.attributes == 1 (page break).
		 */
		if (sa.attributes == 0 && sa.textBlock > maxPara)
			maxPara = sa.textBlock;

		pSaveArray += kSaveArrayEntryLen;
	}
	/* always at least one paragraph */
	assert(maxPara >= 0);

	/* verify our result */
	if (maxPara != maxTextBlock) {
		WMSG2("Max para mismatch (%d vs %d)\n", maxPara, maxTextBlock);
		assert(false);
	}
#endif

	return (unsigned short) (maxTextBlock+1);
}

/*
 * Unpack a SaveArray entry.
 */
void
ReformatAWGS_WP::UnpackSaveArrayEntry(const unsigned char* pSaveArray,
	SaveArrayEntry* pSAE)
{
	pSAE->textBlock = Get16LE(pSaveArray + 0);
	pSAE->offset = Get16LE(pSaveArray + 2);
	pSAE->attributes = Get16LE(pSaveArray + 4);
	pSAE->rulerNum = Get16LE(pSaveArray + 6);
	pSAE->pixelHeight = Get16LE(pSaveArray + 8);
	pSAE->numLines = Get16LE(pSaveArray + 10);

	//WMSG5("SA: textBlock=%d off=%d attr=%d ruler=%d lines=%d\n",
	//	pSAE->textBlock, pSAE->offset, pSAE->attributes, pSAE->rulerNum,
	//	pSAE->numLines);
}

/*
 * Skip past a series of text blocks.
 *
 * Returns "true" on success, "false" on failure.
 */
bool
ReformatAWGS_WP::SkipTextBlocks(const unsigned char** pSrcBuf,
	long* pSrcLen, int textBlockCount)
{
	unsigned long blockSize;
	const unsigned char* srcBuf = *pSrcBuf;
	long srcLen = *pSrcLen;

	WMSG1("Scanning %d text blocks\n", textBlockCount);

	if (srcLen < 4)
		return false;

	while (textBlockCount--) {
		blockSize = Get32LE(srcBuf);
		srcBuf += 4;
		srcLen -= 4;

		WMSG2("+++  blockSize=%lu srcLen=%ld\n", blockSize, srcLen);
		if ((long) blockSize < kMinTextBlockSize) {
			WMSG2("Block size too small (%d - %d)\n",
				blockSize, Get16LE(srcBuf));
			return false;
		}
		if ((long) blockSize > srcLen) {
			WMSG0("Ran off the end in doc text blocks\n");
			return false;
		}
		if (Get16LE(srcBuf) != blockSize || Get16LE(srcBuf+2) != blockSize) {
			WMSG3("AWGS WARNING: inconsistent block size values (%ld vs %d/%d)\n",
				blockSize, Get16LE(srcBuf), Get16LE(srcBuf+2));
			/* okay to ignore it, so long as everything else works out */
		}
		srcBuf += blockSize;
		srcLen -= blockSize;
	}

	*pSrcBuf = srcBuf;
	*pSrcLen = srcLen;

	return true;
}
