/*
 * CiderPress
 * Copyright (C) 2007 by faddenSoft, LLC.  All Rights Reserved.
 * See the file LICENSE for distribution terms.
 */
/*
 * Convert the various Super HiRes formats.
 */
#ifndef __LR_SUPERHIRES__
#define __LR_SUPERHIRES__

#include "ReformatBase.h"

/*
 * Reformat SHR images (abstract base class).
 *
 * THOUGHT: add a "convert single line" function that copies to bytes or
 * RGBTRIPLEs or whatever.  Use it in the usual functions and for converting
 * odd-sized APF-format images.  Args are the bits for the line, 320/640 mode
 * for source-pixel interpretation, and the color palette for that line.  We
 * need to orient less on the format we're converting *to* (e.g. 320/640x200)
 * and more on what we're converting *from* (arbitrary sized image with
 * certain GS-specific characteristics).
 */
class ReformatSHR : public ReformatGraphics {
public:
	enum {
		kNumLines = 200,
		kPixelBytesPerLine = 160,
		kNumColorTables = 16,
		kNumEntriesPerColorTable = 16,
		kColorTableEntrySize = 2,
		kOutputWidth = 640,			// use pixel-doubling so we can get both
		kOutputHeight = 400,		//   320 and 640 modes
		kTotalSize = 32768,
		kMaxPixelsPerScan = 1280,	// allow up to 1280x1024
		kMaxScanLines = 1024,		//  in APF images

		kSCBColorTableMask = 0x0f,
		kSCBFillMode = 0x20,
		kSCBInterrupts = 0x40,
		kSCBNumPixels = 0x80,		// 0=320, 1=640
	};

	/*
	 * This holds one SHR screen; the size must be 32768 bytes.
	 */
	typedef struct SHRScreen {
		unsigned char	pixels[kNumLines * kPixelBytesPerLine];
		unsigned char	scb[kNumLines];
		unsigned char	reserved[256-kNumLines];
		unsigned char	colorTable[kNumColorTables * kNumEntriesPerColorTable *
									kColorTableEntrySize];
	} SHRScreen;

	/* convert 0RGB into (R,G,B) */
	void GSColor(int color, RGBTRIPLE* pTriple) {
		pTriple->rgbtRed = ((color >> 8) & 0x0f) * 17;
		pTriple->rgbtGreen = ((color >> 4) & 0x0f) * 17;
		pTriple->rgbtBlue = (color & 0x0f) * 17;
	}
	void GSColor(int color, RGBQUAD* pQuad) {
		pQuad->rgbRed = ((color >> 8) & 0x0f) * 17;
		pQuad->rgbGreen = ((color >> 4) & 0x0f) * 17;
		pQuad->rgbBlue = (color & 0x0f) * 17;
		pQuad->rgbReserved = 0;
	}

	/* 16 palettes of 16 colors */
	RGBQUAD		fColorTables[kNumColorTables][kNumEntriesPerColorTable];

	MyDIBitmap* SHRScreenToBitmap8(const SHRScreen* pScreen);
	MyDIBitmap* SHRDataToBitmap8(const unsigned char* pPixels,
		const unsigned char* pSCB, const unsigned char* pColorTable,
		int pixelBytesPerLine, int numScanLines,
		int outputWidth, int outputHeight);
};

/*
 * Reformat an unpacked SHR graphic into a bitmap.
 */
class ReformatUnpackedSHR : public ReformatSHR {
public:
	ReformatUnpackedSHR(void) {}
	virtual ~ReformatUnpackedSHR(void) {}

	virtual void Examine(ReformatHolder* pHolder);
	virtual int Process(const ReformatHolder* pHolder,
		ReformatHolder::ReformatID id, ReformatHolder::ReformatPart part,
		ReformatOutput* pOutput);

private:
	SHRScreen	fScreen;
};

/*
 * Reformat an unpacked ".APP" SHR graphic from John Elway Quarterback GS
 * into a bitmap.  (Implementing this was trivial if slightly silly.)
 */
class ReformatJEQSHR : public ReformatSHR {
public:
	ReformatJEQSHR(void) {}
	virtual ~ReformatJEQSHR(void) {}

	virtual void Examine(ReformatHolder* pHolder);
	virtual int Process(const ReformatHolder* pHolder,
		ReformatHolder::ReformatID id, ReformatHolder::ReformatPart part,
		ReformatOutput* pOutput);

private:
	enum { kExpectedLen = 32288 };
	SHRScreen	fScreen;
};


/*
 * Reformat a Paintworks SHR graphic ($c0/0000) into a bitmap.
 */
class ReformatPaintworksSHR : public ReformatSHR {
public:
	ReformatPaintworksSHR(void) {}
	virtual ~ReformatPaintworksSHR(void) {}

	virtual void Examine(ReformatHolder* pHolder);
	virtual int Process(const ReformatHolder* pHolder,
		ReformatHolder::ReformatID id, ReformatHolder::ReformatPart part,
		ReformatOutput* pOutput);

private:
	enum { kMinSize = 223 };
	//SHRScreen	fScreen;
};


/*
 * Reformat a packed SHR graphic ($c0/0001) into a bitmap.
 */
class ReformatPackedSHR : public ReformatSHR {
public:
	ReformatPackedSHR(void) {}
	virtual ~ReformatPackedSHR(void) {}

	virtual void Examine(ReformatHolder* pHolder);
	virtual int Process(const ReformatHolder* pHolder,
		ReformatHolder::ReformatID id, ReformatHolder::ReformatPart part,
		ReformatOutput* pOutput);

private:
	SHRScreen	fScreen;
};

/*
 * Reformat an Apple-Preferred Format SHR graphic ($c0/0002) into a bitmap.
 *
 * The graphic could be a standard SHR image, a larger or smaller than normal
 * SHR image, or a 3200-color image.
 */
class ReformatAPFSHR : public ReformatSHR {
public:
	ReformatAPFSHR(void) : fNonStandard(false), fPixelStore(nil),
		fSCBStore(nil) {}
	virtual ~ReformatAPFSHR(void) {
		if (fPixelStore != nil && fPixelStore != fScreen.pixels)
			delete[] fPixelStore;
		if (fSCBStore != nil && fSCBStore != fScreen.scb)
			delete[] fSCBStore;
	}

	virtual void Examine(ReformatHolder* pHolder);
	virtual int Process(const ReformatHolder* pHolder,
		ReformatHolder::ReformatID id, ReformatHolder::ReformatPart part,
		ReformatOutput* pOutput);

private:
	int UnpackMain(const unsigned char* srcPtr, long srcLen);
	int UnpackMultipal(unsigned char* dstPtr,
		const unsigned char* srcPtr, long srcLen);
	void UnpackNote(const unsigned char* srcPtr, long srcLen);

	/* use this for standard-sized images */
	SHRScreen		fScreen;

	/* set for non-standard images */
	bool			fNonStandard;

	/* use this for non-standard-sized images */
	unsigned char*	fPixelStore;
	unsigned char*	fSCBStore;
	int				fNumScanLines;		// #of scan lines in image
	int				fPixelsPerScanLine;
	int				fPixelBytesPerLine;
	int				fOutputWidth;
	int				fOutputHeight;		// scan lines * 2
};

/*
 * Reformat a 3200-color SHR graphic into a bitmap.
 */
class Reformat3200SHR : public ReformatSHR {
public:
	Reformat3200SHR(void) {}
	virtual ~Reformat3200SHR(void) {}

	// alternate construction, used by APFSHR
	Reformat3200SHR(SHRScreen* pScreen, unsigned char* multiPal) {
		memcpy(&fScreen, pScreen, sizeof(fScreen));
		memcpy(fExtColorTable, multiPal, sizeof(fExtColorTable));
	}

	virtual void Examine(ReformatHolder* pHolder);
	virtual int Process(const ReformatHolder* pHolder,
		ReformatHolder::ReformatID id, ReformatHolder::ReformatPart part,
		ReformatOutput* pOutput);

	MyDIBitmap* SHR3200ToBitmap24(void);

protected:
	enum {
		kExtNumColorTables = kNumLines,
		kExtTotalSize = 38400,
	};

	SHRScreen	fScreen;		// only "pixels" is valid

	/* this holds the 200 color tables, with the entries switched to normal */
	unsigned char	fExtColorTable[kExtNumColorTables *
							kNumEntriesPerColorTable * kColorTableEntrySize];
};

/*
 * Reformat a packed 3200-color SHR graphic.
 */
class Reformat3201SHR : public Reformat3200SHR {
public:
	Reformat3201SHR(void) {}
	virtual ~Reformat3201SHR(void) {}

	virtual void Examine(ReformatHolder* pHolder);
	virtual int Process(const ReformatHolder* pHolder,
		ReformatHolder::ReformatID id, ReformatHolder::ReformatPart part,
		ReformatOutput* pOutput);
};


/*
 * Generic DreamGrafix code.
 */
class DreamGrafix {
public:
	DreamGrafix(void) : fWidth(0), fHeight(0), fNumColors(0) {}
	~DreamGrafix(void) {}

	/*
	 * Scan the file.  If it's a DreamGrafix image, extract values from the
	 * header fields and return "true".
	 */
	bool ScanDreamGrafix(ReformatHolder* pHolder);

	/*
	 * Unpack a DreamGrafix SHR image compressed with LZW.
	 */
	bool UnpackDG(const unsigned char* srcBuf, long srcLen,
		ReformatSHR::SHRScreen* pScreen, unsigned char* extColorTable);

	int fWidth;
	int fHeight;
	int fNumColors;

private:
	static int UnpackLZW(const unsigned char* srcBuf, long srcLen,
		unsigned char* dstBuf, long dstLen);
	enum { kHeaderOffset = 17 };
};

/*
 * Reformat a DreamGrafix 256-color SHR graphic.
 */
class ReformatDG256SHR : public ReformatSHR {
public:
	ReformatDG256SHR(void) {}
	virtual ~ReformatDG256SHR(void) {}

	virtual void Examine(ReformatHolder* pHolder);
	virtual int Process(const ReformatHolder* pHolder,
		ReformatHolder::ReformatID id, ReformatHolder::ReformatPart part,
		ReformatOutput* pOutput);

private:
	DreamGrafix fDG;
	SHRScreen	fScreen;
};

/*
 * Reformat a DreamGrafix 3200-color SHR graphic.
 */
class ReformatDG3200SHR : public Reformat3200SHR {
public:
	ReformatDG3200SHR(void) {}
	virtual ~ReformatDG3200SHR(void) {}

	virtual void Examine(ReformatHolder* pHolder);
	virtual int Process(const ReformatHolder* pHolder,
		ReformatHolder::ReformatID id, ReformatHolder::ReformatPart part,
		ReformatOutput* pOutput);

private:
	DreamGrafix fDG;
};

#endif /*__LR_SUPERHIRES__*/