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
#ifndef UTIL_DIBITMAP_H
#define UTIL_DIBITMAP_H


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
 *  - If the bitmap is <= 24 bits, alpha defaults to "off".
 *  - If the bitmap is 32 bits, alpha defaults to "full".
 *  - If the app calls SetAlphaColor, alpha is set to "transparency".
 * Calling GetPixelRGB returns a 32-bit value for which the alpha value
 * is always set.  For "none" it's always 255, for "transparency" it's
 * either set to 0 or 255, and for "full" it's set to rgbReserved in a
 * 32-bit bitmap (or 255 for any other value).
 */
class MyDIBitmap {
public:
    typedef enum AlphaType {
        kAlphaUnknown,
        kAlphaOpaque,           // always stuff 255 into rgbReserved
        kAlphaTransparency,     // single-bit alpha
        kAlphaFull              // alpha channel in rgbReserved
    } AlphaType;
    enum { kAlphaMask = 0xff000000 };   // alpha byte in an ARGB DWORD

    MyDIBitmap(void) :
        mhBitmap(NULL),
        mpFileBuffer(NULL),
        mWidth(-1),
        mHeight(-1),
        mBpp(-1),
        mPitchBytes(0),
        mNumColorsUsed(0),
        mpColorTable(NULL),
        mColorTableInitialized(false),
        mpPixels(NULL),
        mAlphaType(kAlphaOpaque),
        mTransparentColor(0)
    {
        memset(&mBitmapInfoHdr, 0, sizeof(mBitmapInfoHdr));
    }

    /*
     * Destroys allocated memory and delete the DIB object.
     */
    virtual ~MyDIBitmap(void);

    /*
     * Creates a blank DIB with the requested dimensions.
     *
     * The DIB requires that the array of bytes defining the pixels of the
     * bitmap be padded with zeroes to end on a "LONG data-type boundary",
     * i.e. the start of each line must be 32-bit aligned.  This is done
     * automatically behind the scenes.
     *
     * Returns a pointer to the pixel storage on success, or NULL on failure.
     */
    void* Create(int width, int height, int bitsPerPixel, int colorsUsed,
        bool dibSection = false);

    /*
     * Set the values in the color table.
     */
    void SetColorTable(const RGBQUAD* pColorTable);

    const RGBQUAD* GetColorTable(void) const { return mpColorTable; }

    /*
     * Zero out a bitmap's pixels.  Does not touch the color table.
     */
    void ClearPixels(void);

    /*
     * Create a DDB from the current bitmap in the specified DC, and return its
     * handle.  The returned handle must eventually be disposed with DeleteObject.
     *
     * Since we're just supplying pointers to various pieces of data, there's no
     * need for us to have a DIB section.
     *
     * Returns NULL on failure.
     */
    HBITMAP ConvertToDDB(HDC dc) const;

    /*
     * Opens the file and call the FILE* version.
     */
    int CreateFromFile(const WCHAR* fileName);

    /*
     * Create a DIB by reading a BMP or TGA file into memory.
     */
    int CreateFromFile(FILE* fp, long len);

    /*
     * Creates object from a copy of the file in memory.  Set "doDelete" to
     * transfer ownership of object.
     *
     * We want to hang on to the data buffer, but if we don't own it then we
     * have to make a copy.
     *
     * If "doDelete" is set, memory will be deleted if function fails.
     */
    int CreateFromBuffer(void* buffer, long len, bool doDelete);

    /*
     * Creates the object from a resource embedded in the application.
     *
     * Use MAKEINTRESOURCE to load a resource by ordinal.
     */
    void* CreateFromResource(HINSTANCE hInstance, const WCHAR* rsrc);

    /*
     * Write the bitmap to the named file.  Opens the file and calls the FILE*
     * function.
     */
    int WriteToFile(const WCHAR* fileName) const;

    /*
     * Write the bitmap to a file.
     *
     * Pass in an open, seeked file pointer (make sure to use "wb" mode).
     *
     * Returns 0 on success, or nonzero (errno) on failure.
     */
    int WriteToFile(FILE* fp) const;

    // simple getters
    virtual int GetWidth(void) const { return mWidth; }
    virtual int GetHeight(void) const { return mHeight; }
    int GetBpp(void) const { return mBpp; }
    int GetNumColorsUsed(void) const { return mNumColorsUsed; }

    // retrieve current alpha mode
    AlphaType GetAlphaType(void) const { return mAlphaType; }

    /*
     * Retrieve the transparency color key, if any.
     *
     * Returns "false" if no color key has been set.
     */
    bool GetTransparentColor(RGBQUAD* pColor) const;

    /*
     * Set the transparent color.  Changes the alpha mode to kAlphaTransparency.
     */
    void SetTransparentColor(const RGBQUAD* pColor);

    /*
     * Look up an RGB color in an indexed color table.
     *
     * Returns the index of the color, or -1 if not found (-2 on error, e.g. this
     * isn't an indexed-color bitmap or the color table hasn't been created).
     */
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
        if (mhBitmap == NULL && mpFileBuffer != NULL)
            ConvertBufToDIBSection();
        return mhBitmap;
    }

    int GetPitch(void) const { return mPitchBytes; }

    /*
     * Blit a block of pixels from one bitmap to another.
     *
     * The bitmaps must share a common format, and the rectangles must be the
     * same size.  We could implement color conversion and resizing here, but
     * for now let's not.
     */
    static bool Blit(MyDIBitmap* pDstBits, const RECT* pDstRect,
        const MyDIBitmap* pSrcBits, const RECT* pSrcRect);

private:
    DECLARE_COPY_AND_OPEQ(MyDIBitmap)

    enum { kBMPMagic = 0x4d42 };            // "BM"

    /* for .TGA files; does not map directly to file */
    typedef struct TargaHeader {
        unsigned char   idLength;
        unsigned char   colorMapType;
        unsigned char   imageType;
        unsigned short  colorMapOrigin;
        unsigned short  colorMapLen;
        unsigned char   colorMapEntryLen;
        unsigned short  xOffset;
        unsigned short  yOffset;
        unsigned short  width;
        unsigned short  height;
        unsigned char   bitsPerPixel;
        unsigned char   imageDescriptor;
    } TargaHeader;
    enum {
        kTargaHeaderLen = 18,
        kTargaUnmappedRGB = 2,      // for imageType field
    };

    /*
     * Set up internal structures for the BMP file.
     *
     * On error, "vbuf" is discarded.
     */
    int ImportBMP(void* vbuf, long len);

    /*
     * Set up internal structures for the TGA file.
     *
     * We handle 16-, 24-, and 32-bit .TGA files only.  They happen to use the
     * same byte layout as BMP files, so we do very little work here.  If we
     * tried to write the raw data to a BMP file we could end up in trouble,
     * because we don't force the "pitch must be multiple of 4 bytes" rule.
     *
     * On error, "vbuf" is discarded.
     */
    int ImportTGA(void* vbuf, long len);

    /*
     * If the bitmap wasn't initially created as a DIB section, transform it now
     * so the application can use it in GDI calls.
     *
     * Returns 0 on success, -1 on error.
     */
    int ConvertBufToDIBSection(void);

    /*
     * Create object from a buffer of new[]-created memory that we own.
     *
     * The memory will be discarded if this function fails.
     *
     * We don't want to create a DIB section if the eventual user of this data
     * doesn't need it (e.g. it's just getting converted into a 3D texture
     * without using any GDI calls), so we just leave the pixels in the file
     * buffer for now.
     */
    int CreateFromNewBuffer(void* vbuf, long len);

    BITMAPINFOHEADER    mBitmapInfoHdr;
    /* either mhBitmap or mpFileContents will be non-NULL, but not both */
    HBITMAP             mhBitmap;           // DIB section handle, not DDB
    void*               mpFileBuffer;       // buffer with contents of .BMP
    int                 mWidth;
    int                 mHeight;
    int                 mBpp;               // #of bits per pixel
    int                 mPitchBytes;        // memory line pitch, in bytes
    int                 mNumColorsUsed;
    RGBQUAD*            mpColorTable;
    bool                mColorTableInitialized;
    void*               mpPixels;           // points to system-allocated mem
    AlphaType           mAlphaType;
    DWORD               mTransparentColor;  // for single-bit alpha bitmaps
};

#endif /*UTIL_DIBITMAP_H*/
