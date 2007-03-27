/*
 * CiderPress
 * Copyright (C) 2007 by faddenSoft, LLC.  All Rights Reserved.
 * See the file LICENSE for distribution terms.
 */
/*
 * DreamGrafix super-hi-res conversions.
 *
 * Based on code provided by Jason Andersen.
 */
#include "StdAfx.h"
#include "SuperHiRes.h"


/*
 * ==========================================================================
 *		ReformatDG256SHR
 * ==========================================================================
 */

/*
 * Decide whether or not we want to handle this file.
 */
void
ReformatDG256SHR::Examine(ReformatHolder* pHolder)
{
	ReformatHolder::ReformatApplies applies = ReformatHolder::kApplicNot;

	if (fDG.ScanDreamGrafix(pHolder) && fDG.fNumColors == 256)
		applies = ReformatHolder::kApplicYes;

	pHolder->SetApplic(ReformatHolder::kReformatSHR_DG256, applies,
		ReformatHolder::kApplicNot, ReformatHolder::kApplicNot);
}

/*
 * Convert a 256-color DreamGrafix Super Hi-Res Image.
 */
int
ReformatDG256SHR::Process(const ReformatHolder* pHolder,
	ReformatHolder::ReformatID id, ReformatHolder::ReformatPart part,
	ReformatOutput* pOutput)
{
	const unsigned char* srcBuf = pHolder->GetSourceBuf(part);
	long srcLen = pHolder->GetSourceLen(part);
	int retval = -1;

	if (fDG.UnpackDG(srcBuf, srcLen, &fScreen, NULL)) {
		MyDIBitmap* pDib = SHRScreenToBitmap8(&fScreen);
		if (pDib != nil) {
			SetResultBuffer(pOutput, pDib);
			retval = 0;
		}
	}

	return retval;
}


/*
 * ==========================================================================
 *		ReformatDG3200SHR
 * ==========================================================================
 */

/*
 * Decide whether or not we want to handle this file.
 */
void
ReformatDG3200SHR::Examine(ReformatHolder* pHolder)
{
	ReformatHolder::ReformatApplies applies = ReformatHolder::kApplicNot;

	if (fDG.ScanDreamGrafix(pHolder) && fDG.fNumColors == 3200)
		applies = ReformatHolder::kApplicYes;

	pHolder->SetApplic(ReformatHolder::kReformatSHR_DG3200, applies,
		ReformatHolder::kApplicNot, ReformatHolder::kApplicNot);
}

/*
 * Convert a 3200-color DreamGrafix Super Hi-Res Image.
 */
int
ReformatDG3200SHR::Process(const ReformatHolder* pHolder,
	ReformatHolder::ReformatID id, ReformatHolder::ReformatPart part,
	ReformatOutput* pOutput)
{
	const unsigned char* srcBuf = pHolder->GetSourceBuf(part);
	long srcLen = pHolder->GetSourceLen(part);
	int retval = -1;

	if (fDG.UnpackDG(srcBuf, srcLen, &fScreen, fExtColorTable)) {
		MyDIBitmap* pDib = SHR3200ToBitmap24();
		if (pDib != nil) {
			SetResultBuffer(pOutput, pDib);
			retval = 0;
		}
	}

	return retval;
}




/*
 * ==========================================================================
 *		ReformatDreamGrafix
 * ==========================================================================
 */

/*
 * Examine a DreamGrafix file.  This figures out if its any of the
 * DreamGrafix formats, i.e. 256-color, 3200-color, packed, or unpacked.
 */
bool
DreamGrafix::ScanDreamGrafix(ReformatHolder* pHolder)
{
	long fileType = pHolder->GetFileType();
	long auxType = pHolder->GetAuxType();
	long fileLen = pHolder->GetSourceLen(ReformatHolder::kPartData);
	bool relaxed, couldBe;

	relaxed = pHolder->GetOption(ReformatHolder::kOptRelaxGfxTypeCheck) != 0;
	couldBe = false;

	if (fileType == Reformat::kTypePNT && auxType == 0x8005)
		couldBe = true;		// $c0/8005 DG packed
	else if (fileType == Reformat::kTypePIC && auxType == 0x8003)
		couldBe = false;	// $c1/8003 DG unpacked -- not supported
	else if (relaxed) {
		if (fileType == Reformat::kTypeBIN && auxType == 0x8005)
			couldBe = true;
	}
	if (fileLen < 256)
		couldBe = false;

	if (!couldBe)
		return false;

	const unsigned char* ptr;
	ptr = pHolder->GetSourceBuf(ReformatHolder::kPartData)
			+ fileLen - kHeaderOffset;

	if (memcmp(ptr+6, "\x0a""DreamWorld", 11) != 0)
		return false;

	fNumColors = (*ptr == 0) ? 256 : 3200;
	fHeight = Reformat::Get16LE(ptr + 2);
	fWidth = Reformat::Get16LE(ptr + 4);
	if (fWidth != 320 || fHeight != 200) {
		WMSG2("ODD: strange height %dx%x in DG\n", fWidth, fHeight);
		return false;
	}

	return true;
}

/*
 * Unpack a DreamGrafix SHR image compressed with LZW.
 *
 * Returns true on success, false if the uncompress step failed to produce
 * exactly 32768+32*200 bytes.
 */
bool
DreamGrafix::UnpackDG(const unsigned char* srcBuf, long srcLen,
	ReformatSHR::SHRScreen* pScreen, unsigned char* extColorTable)
{
	int expectedLen;
	unsigned char* tmpBuf;
	int actual;

	if (extColorTable == NULL) {
		/*
		 * 32000 for pixels (320*200*0.5 bytes)
		 * 256 for SCB (200 + 56 unused)
		 * 512 for basic palette (16 sets * 16 colors * 2 bytes)\
		 * 512 optional/unused?
		 */
		expectedLen = 33280;
	} else {
		/*
		 * 32000 for pixels (320*200*0.5 bytes)
		 * 6400 for palette (200 lines * 16 entries * 2 bytes)
		 * 512 optional/unused?
		 */
		expectedLen = 38912;
	}

	/* over-alloc -- our LZW decoder doesn't check often */
	tmpBuf = new unsigned char[expectedLen + 1024];
	if (tmpBuf == NULL)
		return false;

	actual = UnpackLZW(srcBuf, srcLen, tmpBuf, expectedLen);
	if (actual != expectedLen && actual != (expectedLen-512)) {
		WMSG2("UnpackLZW expected %d, got %d\n", expectedLen, actual);
		free(tmpBuf);
		return false;
	}

	memcpy(pScreen->pixels, tmpBuf, 32000);
	if (extColorTable == NULL) {
		memcpy(pScreen->scb, tmpBuf + 32000, 256);
		memcpy(pScreen->colorTable, tmpBuf + 32256, 512);
	} else {
		const unsigned short* pSrcTable;
		unsigned short* pDstTable;
		
		pSrcTable = (const unsigned short*) (tmpBuf + 32000);
		pDstTable = (unsigned short*) extColorTable;
		int table;
		for (table = 0; table < ReformatSHR::kNumLines; table++) {
			int entry;
			for (entry = 0; entry < ReformatSHR::kNumEntriesPerColorTable; entry++) {
				pDstTable[(ReformatSHR::kNumEntriesPerColorTable-1) - entry] =
					*pSrcTable++;
			}
			pDstTable += ReformatSHR::kNumEntriesPerColorTable;
		}
	}

	free(tmpBuf);
	return true;
}

/*
 * Constants for LZW.
 */
static const int kClearCode = 256;
static const int kEofCode = 257;
static const int kFirstFreeCode = 258;

static const unsigned int bitMasks[] = {
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0x1ff, 0x3ff, 0x7ff, 0xfff
};

/* initialize the table */
#define INIT_TABLE() {							\
		nBitMod1 = 9;							\
		nBitMask = bitMasks[nBitMod1];			\
		maxCode = 1 << nBitMod1;				\
		freeCode = kFirstFreeCode;				\
	};

/* add a new code to our data structure */
#define ADD_CODE() {							\
		hashChar[freeCode] = k;					\
		hashNext[freeCode] = oldCode;			\
		freeCode++;								\
	};

/* read the next code and put the value into iCode */
#define READ_CODE() {							\
		int bit_idx  = bitOffset & 0x7;			\
		int byte_idx = bitOffset >> 3;			\
												\
		iCode = srcBuf[ byte_idx ];				\
		iCode &= 0xFF;							\
		iCode |= (srcBuf[ byte_idx+1 ]<<8);		\
		iCode &= 0xFFFF;						\
		iCode |= (srcBuf[ byte_idx+2 ]<<16);	\
		iCode >>= bit_idx;						\
		iCode &= nBitMask;						\
		bitOffset += nBitMod1;					\
	};

/*static*/ int
DreamGrafix::UnpackLZW(const unsigned char* srcBuf, long srcLen,
	unsigned char* dstBuf, long dstLen)
{
	unsigned short finChar, oldCode, inCode, freeCode, maxCode, k;
	unsigned short nBitMod1, nBitMask;
	int bitOffset;
	unsigned short hashNext[4096];
	unsigned short hashChar[4096];

	unsigned char* pOrigDst = dstBuf;
	int iCode;
	short stack[32768];
	int stackIdx = 0;

	/* initialize table and code reader */
	INIT_TABLE();
	bitOffset = 0;

	while (true) {
		if (dstBuf - pOrigDst > dstLen) {
			WMSG0("LZW overrun\n");
			return -1;
		}

		int A;
		int Y;
		READ_CODE();
		if (iCode == kEofCode) {
			break;
		}

		if (iCode == kClearCode) {
			// Got Clear
			INIT_TABLE();
			READ_CODE();
			oldCode = iCode;
			k = iCode;
			finChar = iCode;
			*dstBuf++ = (unsigned char)iCode;
			continue;
		}

		A = inCode = iCode;

		if (iCode < freeCode) {
			goto inTable;
		}

		stack[stackIdx] = finChar;
		stackIdx++;
		A = oldCode;
inTable:
		if (A < 256) {
			goto gotChar;
		}
		while (A >= 256) {
			Y = A;
			A = hashChar[Y];
	
			stack[stackIdx] = A;
			stackIdx++;
	
			A = hashNext[Y];
		}
gotChar:
		A &= 0xFF;
		finChar = A;
		k = A;
		Y = 0;

		dstBuf[Y++] = A;

		while (stackIdx) {
			stackIdx--;
			A = stack[stackIdx];
			dstBuf[Y++] = A;
		}

		dstBuf += Y;

		ADD_CODE();

		oldCode = inCode;

		if (freeCode < maxCode) {
			continue; // goto nextCode;
		}

		if (12 == nBitMod1) {
			continue; // goto nextCode;
		}

		nBitMod1++;
		nBitMask = bitMasks[nBitMod1];
		//printf("nBitMod1 = %d, nBitMask = %04x\n",
	 	//			nBitMod1, nBitMask);
		maxCode <<= 1;
	}

	return dstBuf - pOrigDst;
}
