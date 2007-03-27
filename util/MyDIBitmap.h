/*
 * CiderPress
 * Copyright (C) 2007 by faddenSoft, LLC.  All Rights Reserved.
 * See the file LICENSE for distribution terms.
 */
/*
 * DIB (Device-Independent Bitmap) wrapper.
 *
 * [ Ported back from libfadden -- there's a lot of stuff here that we don't
 *   want or need, but it's easier to knock the bugs out if everybody is
 *   using roughly the same code. ]
 */
#ifndef __LF_DIBITMAP__
#define __LF_DIBITMAP__


/*
 * Wraps a device-independent bitmap.
 *
 * When the bitmap is initially loaded, the object may or may not allocate a
 * DIB section in system memory.  If the DIB handle is requested, a conversion
 * will be forced.  This is done for efficiency in situations where the
 * bitmap isn't going to be handed off to GDI, or will only be used as a DDB.
 *
 * Some bitmaps have a transparent color (1-bit alpha) or a full alpha
 * channel.  The behavior works like this:
 *	- If the bitmap is <= 24 bits, alpha defaults to "off".
 *	- If the bitmap is 32 bits, alpha defaults to "full".
 *	- If the app calls SetAlphaColor, alpha is set to "transparency".
 * Calling GetPixelRGB returns a 32-bit value for which the alpha value
 * is always set.  For "none" it's always 255, for "transparency" it's
 * either set to 0 or 255, and for "full" it's set to rgbReserved in a
 * 32-bit bitmap (or 255 for any other value).
 */
class MyDIBitmap {
public:
	typedef enum AlphaType {
		kAlphaUnknown,
		kAlphaOpaque,			// always stuff 255 into rgbReserved
		kAlphaTransparency,		// single-bit alpha
		kAlphaFull				// alpha channel in rgbReserved
	} AlphaType;
	enum { kAlphaMask = 0xff000000 };	// alpha byte in an ARGB DWORD

	MyDIBitmap(void) :
		mhBitmap(nil),
		mpFileBuffer(nil),
		mWidth(-1),
		mHeight(-1),
		mBpp(-1),
		mPitchBytes(0),
		mNumColorsUsed(0),
		mpColorTable(nil),
		mColorTableInitialized(false),
		mpPixels(nil),
		mAlphaType(kAlphaOpaque),
		mTransparentColor(0)
	{
		memset(&mBitmapInfoHdr, 0, sizeof(mBitmapInfoHdr));
	}
	virtual ~MyDIBitmap(void);

	// create an empty bitmap with the specified characteristics; returns a
	// pointer to the pixel storage
	void* Create(int width, int height, int bitsPerPixel, int colorsUsed,
		bool dibSection = false);
	void SetColorTable(const RGBQUAD* pColorTable);
	const RGBQUAD* GetColorTable(void) const { return mpColorTable; }

	// zero out the pixels
	void ClearPixels(void);

	HBITMAP ConvertToDDB(HDC dc) const;

	// create a DIB from a file on disk
	int CreateFromFile(const char* fileName);
	int CreateFromFile(FILE* fp, long len);
	// create from memory buffer; set "doDelete" if object should own mem
	// (if "doDelete" is set, memory will be deleted if function fails)
	// contents of buffer will be analyzed to determine file type
	int CreateFromBuffer(void* buffer, long len, bool doDelete);

	// create a DIB from an embedded resource
	void* CreateFromResource(HINSTANCE hInstance, const char* rsrc);

	// write bitmap to file (for FILE*, make sure it's open in "wb" mode)
	int WriteToFile(const char* fileName) const;
	int WriteToFile(FILE* fp) const;

	// simple getters
	virtual int GetWidth(void) const { return mWidth; }
	virtual int GetHeight(void) const { return mHeight; }
	int GetBpp(void) const { return mBpp; }
	int GetNumColorsUsed(void) const { return mNumColorsUsed; }

	// retrieve current alpha mode
	AlphaType GetAlphaType(void) const { return mAlphaType; }
	// get transparent color; returns "false" if none has been set
	bool GetTransparentColor(RGBQUAD* pColor) const;
	// set transparent color and change alpha to kAlphaTransparency
	void SetTransparentColor(const RGBQUAD* pColor);

	// return the index of the specified color (-1 if not found, -2 on err)
	int LookupColor(const RGBQUAD* pRgbQuad);

	// set/get individual pixel values; the "RGB" functions always set
	//  alpha to zero, while "RGBA" follow the current alpha mode
	void FASTCALL GetPixelRGB(int x, int y, RGBQUAD* pRgbQuad) const;
	void FASTCALL SetPixelRGB(int x, int y, const RGBQUAD* pRgbQuad);
	void FASTCALL GetPixelRGBA(int x, int y, RGBQUAD* pRgbQuad) const;
	void FASTCALL SetPixelRGBA(int x, int y, const RGBQUAD* pRgbQuad);
	void FASTCALL GetPixelIndex(int x, int y, int* pIdx) const;
	void FASTCALL SetPixelIndex(int x, int y, int idx);

	// Return a pointer to the raw pixel storage.
	void* GetPixelBuf(void) const { return mpPixels; }

	// Return the DIB handle; keep in mind that this HBITMAP is different
	// from a DDB HBITMAP, and many calls that take an HBITMAP will fail.
	HBITMAP GetHandle(void) {
		if (mhBitmap == nil && mpFileBuffer != nil)
			ConvertBufToDIBSection();
		return mhBitmap;
	}

	int GetPitch(void) const { return mPitchBytes; }

	// Blit pixels from one bitmap to another.
	static bool Blit(MyDIBitmap* pDstBits, const RECT* pDstRect,
		const MyDIBitmap* pSrcBits, const RECT* pSrcRect);

private:
	enum { kBMPMagic = 0x4d42 };			// "BM"

	/* for .TGA files; does not map directly to file */
	typedef struct TargaHeader {
		unsigned char	idLength;
		unsigned char	colorMapType;
		unsigned char	imageType;
		unsigned short	colorMapOrigin;
		unsigned short	colorMapLen;
		unsigned char	colorMapEntryLen;
		unsigned short	xOffset;
		unsigned short	yOffset;
		unsigned short	width;
		unsigned short	height;
		unsigned char	bitsPerPixel;
		unsigned char	imageDescriptor;
	} TargaHeader;
	enum {
		kTargaHeaderLen = 18,
		kTargaUnmappedRGB = 2,		// for imageType field
	};

	int ImportBMP(void* vbuf, long len);
	int ImportTGA(void* vbuf, long len);
	int ConvertBufToDIBSection(void);
	int CreateFromNewBuffer(void* vbuf, long len);

	BITMAPINFOHEADER	mBitmapInfoHdr;
	/* either mhBitmap or mpFileContents will be non-nil, but not both */
	HBITMAP				mhBitmap;			// DIB section handle, not DDB
	void*				mpFileBuffer;		// buffer with contents of .BMP
	int					mWidth;
	int					mHeight;
	int					mBpp;				// #of bits per pixel
	int					mPitchBytes;		// memory line pitch, in bytes
	int					mNumColorsUsed;
	RGBQUAD*			mpColorTable;
	bool				mColorTableInitialized;
	void*				mpPixels;			// points to system-allocated mem
	AlphaType			mAlphaType;
	DWORD				mTransparentColor;	// for single-bit alpha bitmaps
};

#endif /*__LF_DIBITMAP__*/