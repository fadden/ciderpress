/*
 * CiderPress
 * Copyright (C) 2007 by faddenSoft, LLC.  All Rights Reserved.
 * See the file LICENSE for distribution terms.
 */
/*
 * [Ported back from libfadden]
 *
 * Implementation of DIBitmap class.  This began life as a simple DIBSection
 * wrapper, and has gradually evolved into a bitmap class that creates
 * DIBSections and DDBs when necessary.
 *
 * BMP file format is:
 *  file header (BITMAPFILEHEADER)
 *  information header (BITMAPINFO; size determined by header.vfOffBits)
 *      information (BITMAPINFOHEADER)
 *      color table (RGBQUAD[])
 *  pixel bits
 *
 * The DIBSECTION struct includes BITMAP and BITMAPINFOHEADER.
 *
 * The DIB we create doesn't really have a color table.  The color table is
 * set when we convert to a DDB or write the bitmap to a file.
 */
#include "stdafx.h"
#include "MyDIBitmap.h"
#include "Util.h"

//#include "libfadden.h"
//using namespace libfadden;


MyDIBitmap::~MyDIBitmap(void)
{
    if (mhBitmap != NULL)
        ::DeleteObject(mhBitmap);
    delete[] mpFileBuffer;
    delete[] mpColorTable;

    /* fpPixels point to system-allocated memory inside fhBitmap */
}

void* MyDIBitmap::Create(int width, int height, int bitsPerPixel, int colorsUsed,
    bool dibSection /*=false*/)
{
     // We probably don't need to allocate DIB storage here.  We can do most
     // operations ourselves, in local memory, and only convert when needed.

    if (mhBitmap != NULL || mpPixels != NULL || mpFileBuffer != NULL) {
        LOGI(" DIB GLITCH: already created");
        assert(false);
        return NULL;
    }

    assert(width > 0 && height > 0);
    assert(bitsPerPixel == 1 ||
           bitsPerPixel == 4 ||
           bitsPerPixel == 8 ||
           bitsPerPixel == 16 ||
           bitsPerPixel == 24 ||
           bitsPerPixel == 32);
    assert(bitsPerPixel == 24 || bitsPerPixel == 32 || colorsUsed > 0);

    // should include a warning if line stride is not a multiple of 4 bytes
    if ((width & 0x03) != 0) {
        LOGW(" DIB stride must be multiple of 4 bytes (got %d)", width);
    }

    mBitmapInfoHdr.biSize = sizeof(mBitmapInfoHdr); // BITMAPINFOHEADER
    mBitmapInfoHdr.biWidth = width;
    mBitmapInfoHdr.biHeight = height;
    mBitmapInfoHdr.biPlanes = 1;
    mBitmapInfoHdr.biBitCount = bitsPerPixel;
    mBitmapInfoHdr.biCompression = BI_RGB;      // has implications for 16-bit
    mBitmapInfoHdr.biSizeImage = 0;
    mBitmapInfoHdr.biXPelsPerMeter = 0;
    mBitmapInfoHdr.biYPelsPerMeter = 0;
    mBitmapInfoHdr.biClrUsed = colorsUsed;
    mBitmapInfoHdr.biClrImportant = 0;

    mNumColorsUsed = colorsUsed;
    if (colorsUsed) {
        mpColorTable = new RGBQUAD[colorsUsed];
        if (mpColorTable == NULL)
            return NULL;
    }

    if (dibSection) {
        /*
         * Create an actual blank DIB section.
         */
        mhBitmap = ::CreateDIBSection(NULL, (BITMAPINFO*) &mBitmapInfoHdr,
                        DIB_RGB_COLORS, &mpPixels, NULL, 0);
        if (mhBitmap == NULL) {
            DWORD err = ::GetLastError();
            //CString msg;
            //GetWin32ErrorString(err, &msg);
            //LOGI(" DIB CreateDIBSection failed (err=%d msg='%ls')",
            //  err, (LPCWSTR) msg);
            LOGE(" DIB CreateDIBSection failed (err=%d)", err);
            LogHexDump(&mBitmapInfoHdr, sizeof(BITMAPINFO));
            LOGI("  &mpPixels = 0x%p", &mpPixels);
            DebugBreak();
            return NULL;
        }

        /*
        * Save some bitmap statistics.
        */
        BITMAP info;
        int gotten;
        /* or GetObject(hBitmap, sizeof(DIBSECTION), &dibsection) */
        gotten = ::GetObject(mhBitmap, sizeof(info), &info);
        if (gotten != sizeof(info))
            return NULL;
        mPitchBytes = info.bmWidthBytes;
    } else {
        /*
         * Create a buffer in memory.
         */
        mPitchBytes = ((width * mBitmapInfoHdr.biBitCount) +7) / 8;
        mPitchBytes = (mPitchBytes + 3) & ~(0x03);      // 32-bit bounds

        /* we're not allocating full file buffer; should be okay */
        mpFileBuffer = new char[mPitchBytes * mBitmapInfoHdr.biHeight];
        mpPixels = mpFileBuffer;
    }

    mWidth = width;
    mHeight = height;
    mBpp = bitsPerPixel;

    /* clear the bitmap, possibly not needed for DIB section */
    assert(mpPixels != NULL);
    memset(mpPixels, 0, mPitchBytes * mBitmapInfoHdr.biHeight);

    //LOGI("+++  allocated %d bytes for bitmap pixels (mPitchBytes=%d)",
    //  mPitchBytes * mBitmapInfoHdr.biHeight, mPitchBytes);

    return mpPixels;
}

int MyDIBitmap::CreateFromFile(const WCHAR* fileName)
{
    FILE* fp = NULL;
    int err;

    fp = _wfopen(fileName, L"rb");
    if (fp == NULL) {
        err = errno ? errno : -1;
        LOGI("Unable to read bitmap from file '%ls' (err=%d)",
            fileName, err);
        return err;
    }

    long fileLen;
    fseek(fp, 0, SEEK_END);
    fileLen = ftell(fp);
    rewind(fp);

    err = CreateFromFile(fp, fileLen);
    fclose(fp);
    return err;
}

int MyDIBitmap::CreateFromFile(FILE* fp, long len)
{
    void* buf = NULL;
    int err = -1;

    buf = new unsigned char[len];
    if (buf == NULL)
        return err;

    if (fread(buf, len, 1, fp) != 1) {
        err = errno ? errno : -1;
        LOGI(" DIB failed reading %ld bytes (err=%d)", len, err);
        goto bail;
    }

    err = CreateFromNewBuffer(buf, len);

bail:
    return err;
}

int MyDIBitmap::CreateFromBuffer(void* buf, long len, bool doDelete)
{
    assert(len > 0);

    if (doDelete) {
        return CreateFromNewBuffer(buf, len);
    } else {
        void* newBuf = new unsigned char[len];
        if (newBuf == NULL)
            return -1;
        memcpy(newBuf, buf, len);

        return CreateFromNewBuffer(newBuf, len);
    }
}

int MyDIBitmap::CreateFromNewBuffer(void* vbuf, long len)
{
    BITMAPFILEHEADER* pHeader = (BITMAPFILEHEADER*) vbuf;
    unsigned char* buf = (unsigned char*) vbuf;

    assert(pHeader != NULL);
    assert(len > 0);

    if (len > 16 && pHeader->bfType == kBMPMagic && (long) pHeader->bfSize == len)
    {
        return ImportBMP(vbuf, len);
    } else if (len > 16 && buf[0x01] == 0 &&
        buf[0x02] == 2 && buf[0x05] == 0 && buf[0x06] == 0 &&
        (buf[0x10] == 16 || buf[0x10] == 24 || buf[0x10] == 32))
    {
        return ImportTGA(vbuf, len);
    } else {
        LOGI(" DIB invalid bitmap file (type=0x%04x size=%ld)",
            pHeader->bfType, pHeader->bfSize);
        delete[] vbuf;
        return -1;
    }
}

int MyDIBitmap::ImportBMP(void* vbuf, long len)
{
    BITMAPFILEHEADER* pHeader = (BITMAPFILEHEADER*) vbuf;
    BITMAPINFO* pInfo;
    unsigned char* pBits;
    int err = -1;

    pInfo = (BITMAPINFO*) (pHeader+1);
    pBits = (unsigned char*) pHeader + pHeader->bfOffBits;

    // no need to memset mBitmapInfoHdr; that's done during object construction

    if (pInfo->bmiHeader.biSize == sizeof(BITMAPCOREHEADER)) {
        /* deal with older bitmaps */
        BITMAPCOREHEADER* pCore = (BITMAPCOREHEADER*) pInfo;
        assert(mBitmapInfoHdr.biSize == 0);     // we memset in constructor
        mBitmapInfoHdr.biSize = sizeof(mBitmapInfoHdr); // BITMAPINFOHEADER
        mBitmapInfoHdr.biWidth = pCore->bcWidth;
        mBitmapInfoHdr.biHeight = pCore->bcSize;
        mBitmapInfoHdr.biPlanes = pCore->bcPlanes;
        mBitmapInfoHdr.biBitCount = pCore->bcBitCount;
        mBitmapInfoHdr.biCompression = BI_RGB;
    } else {
        /* has at least a BITMAPINFOHEADER in it, use existing fields */
        assert(mBitmapInfoHdr.biSize == 0);     // we memset in constructor
        assert(pInfo->bmiHeader.biSize >= sizeof(BITMAPINFOHEADER));
        mBitmapInfoHdr.biSize = sizeof(mBitmapInfoHdr); // BITMAPINFOHEADER
        mBitmapInfoHdr.biWidth = pInfo->bmiHeader.biWidth;
        mBitmapInfoHdr.biHeight = pInfo->bmiHeader.biHeight;
        mBitmapInfoHdr.biPlanes = pInfo->bmiHeader.biPlanes;
        mBitmapInfoHdr.biBitCount = pInfo->bmiHeader.biBitCount;
        mBitmapInfoHdr.biCompression = pInfo->bmiHeader.biCompression;
    }

    mWidth = mBitmapInfoHdr.biWidth;
    mHeight = mBitmapInfoHdr.biHeight;
    mBpp = mBitmapInfoHdr.biBitCount;
    mNumColorsUsed = mBitmapInfoHdr.biClrUsed;
    mPitchBytes = ((mWidth * mBitmapInfoHdr.biBitCount) +7) / 8;
    mPitchBytes = (mPitchBytes + 3) & ~(0x03);      // round up to mult of 4
    //LOGI(" DIB +++ width=%d bits=%d pitch=%d", mWidth,
    //  mBitmapInfoHdr.biBitCount, mPitchBytes);

    /* prepare the color table, if any */
    if (mBpp <= 8) {
        if (mNumColorsUsed == 0) {
            mNumColorsUsed = 1 << mBpp;
            mBitmapInfoHdr.biClrUsed = mNumColorsUsed;
        }
        mpColorTable = new RGBQUAD[mNumColorsUsed];
        if (mpColorTable == NULL)
            goto bail;
        SetColorTable(pInfo->bmiColors);
    }

    /* use the buffered bits */
    mpPixels = pBits;
    mpFileBuffer = vbuf;
    err = 0;

bail:
    if (err != 0)
        delete[] vbuf;
    return err;
}

int MyDIBitmap::ImportTGA(void* vbuf, long len)
{
    TargaHeader targaHdr;
    unsigned char* hdr = (unsigned char*) vbuf;
    unsigned char* pBits;
    int err = -1;

    /* pull the header out of the file */
    targaHdr.idLength = hdr[0x00];
    targaHdr.colorMapType = hdr[0x01];
    targaHdr.imageType = hdr[0x02];
    targaHdr.colorMapOrigin = hdr[0x03] | hdr[0x04] << 8;
    targaHdr.colorMapLen = hdr[0x05] | hdr[0x06] << 8;
    targaHdr.colorMapEntryLen = hdr[0x07];
    targaHdr.xOffset = hdr[0x08] | hdr[0x09] << 8;
    targaHdr.yOffset = hdr[0x0a] | hdr[0x0b] << 8;
    targaHdr.width = hdr[0x0c] | hdr[0x0d] << 8;
    targaHdr.height = hdr[0x0e] | hdr[0x0f] << 8;
    targaHdr.bitsPerPixel = hdr[0x10];
    targaHdr.imageDescriptor = hdr[0x11];

    pBits = hdr + kTargaHeaderLen + targaHdr.idLength;

    // no need to memset mBitmapInfoHdr; that's done during object construction

    assert(mBitmapInfoHdr.biSize == 0);     // we memset in constructor
    mBitmapInfoHdr.biSize = sizeof(mBitmapInfoHdr); // BITMAPINFOHEADER
    mBitmapInfoHdr.biWidth = targaHdr.width;
    mBitmapInfoHdr.biHeight = targaHdr.height;
    mBitmapInfoHdr.biPlanes = 1;
    mBitmapInfoHdr.biBitCount = targaHdr.bitsPerPixel;
    mBitmapInfoHdr.biCompression = BI_RGB;

    mWidth = mBitmapInfoHdr.biWidth;
    mHeight = mBitmapInfoHdr.biHeight;
    mBpp = mBitmapInfoHdr.biBitCount;
    mNumColorsUsed = mBitmapInfoHdr.biClrUsed;
    mPitchBytes = ((mWidth * mBitmapInfoHdr.biBitCount) +7) / 8;
    if ((mPitchBytes & 0x03) != 0) {
        /* should only be a problem if we try to save to BMP or conv to DIB */
        LOGI(" DIB WARNING: pitchBytes=%d in TGA may not work",
            mPitchBytes);
    }
//  mPitchBytes = (mPitchBytes + 3) & ~(0x03);      // round up to power of 2

/*
|        |        |  This field specifies (width) x (height) pixels.  Each     |
|        |        |  pixel specifies an RGB color value, which is stored as    |
|        |        |  an integral number of bytes.                              |
|        |        |                                                            |
|        |        |  The 2 byte entry is broken down as follows:               |
|        |        |  ARRRRRGG GGGBBBBB, where each letter represents a bit.    |
|        |        |  But, because of the lo-hi storage order, the first byte   |
|        |        |  coming from the file will actually be GGGBBBBB, and the   |
|        |        |  second will be ARRRRRGG. "A" represents an attribute bit. |
|        |        |                                                            |
|        |        |  The 3 byte entry contains 1 byte each of blue, green,     |
|        |        |  and red.                                                  |
|        |        |                                                            |
|        |        |  The 4 byte entry contains 1 byte each of blue, green,     |
|        |        |  red, and attribute.  For faster speed (because of the     |
|        |        |  hardware of the Targa board itself), Targa 24 images are  |
|        |        |  sometimes stored as Targa 32 images.                      |
|        |        |                                                            |
*/

    /* use the buffered bits */
    mpPixels = pBits;
    mpFileBuffer = vbuf;
    err = 0;
    //LOGI("+++ successfully imported %d-bit TGA", mBpp);

    if (mBpp == 32) {
        /* 32-bit TGA is a full-alpha format */
        mAlphaType = kAlphaFull;
    }

//bail:
    if (err != 0)
        delete[] vbuf;
    return err;
}

int MyDIBitmap::ConvertBufToDIBSection(void)
{
    void* oldPixels = mpPixels;

    assert(mhBitmap == NULL);
    assert(mpFileBuffer != NULL);

    LOGI(" DIB converting buf to DIB Section");

    /* alloc storage */
    mpPixels = NULL;
    mhBitmap = ::CreateDIBSection(NULL, (BITMAPINFO*) &mBitmapInfoHdr,
                    DIB_RGB_COLORS, &mpPixels, NULL, 0);
    if (mhBitmap == NULL) {
        DWORD err = ::GetLastError();
        LOGI(" DIB CreateDIBSection failed (err=%d)", err);
        LogHexDump(&mBitmapInfoHdr, sizeof(BITMAPINFO));
        LOGI("  &mpPixels = 0x%p", &mpPixels);
        DebugBreak();
        mpPixels = oldPixels;
        return -1;
    }
    assert(mpPixels != NULL);

    /*
     * This shouldn't be necessary; I don't think a DIB section uses a
     * different bmWidthBytes than a file on disk.  If it does, we'll have
     * to scan-convert it.
     */
    BITMAP info;
    int gotten;
    // or GetObject(hBitmap, sizeof(DIBSECTION), &dibsection)
    gotten = ::GetObject(mhBitmap, sizeof(info), &info);
    if (gotten != sizeof(info))
        return NULL;
    assert(mPitchBytes == info.bmWidthBytes);
    //mPitchBytes = info.bmWidthBytes;

    /* copy the bits in */
    memcpy(mpPixels, oldPixels, mPitchBytes * mHeight);

    /* throw out the old storage */
    delete[] mpFileBuffer;
    mpFileBuffer = NULL;

    return 0;
}

void* MyDIBitmap::CreateFromResource(HINSTANCE hInstance, const WCHAR* rsrc)
{
    mhBitmap = (HBITMAP) ::LoadImage(hInstance, rsrc, IMAGE_BITMAP, 0, 0,
                    LR_DEFAULTCOLOR | LR_CREATEDIBSECTION);
    if (mhBitmap == NULL) {
        DWORD err = ::GetLastError();
        //CString msg;
        //GetWin32ErrorString(err, &msg);
        //LOGI(" DIB CreateDIBSection failed (err=%d msg='%ls')",
        //  err, (LPCWSTR) msg);
        LOGI(" DIB LoadImage failed (err=%d)", err);
        return NULL;
    }

    /*
     * Pull out bitmap details.
     */
    DIBSECTION info;
    int gotten;
    gotten = ::GetObject(mhBitmap, sizeof(info), &info);
    if (gotten != sizeof(info))
        return NULL;
    mPitchBytes = info.dsBm.bmWidthBytes;
    mWidth = info.dsBm.bmWidth;
    mHeight = info.dsBm.bmHeight;
    mBpp = info.dsBm.bmBitsPixel;
    mpPixels = info.dsBm.bmBits;
    mNumColorsUsed = info.dsBmih.biClrUsed;

    if (mBpp <= 8) {
        if (mNumColorsUsed == 0)
            mNumColorsUsed = 1 << mBpp;
        mpColorTable = new RGBQUAD[mNumColorsUsed];
        if (mpColorTable == NULL)
            goto bail;      // should reset mpPixels?

        /*
         * Extracting the color table from a DIB is annoying.  I don't
         * entirely understand the need for all these HDC gymnastics, but
         * it doesn't work without both handles.
         */
        HDC tmpDC;
        HDC memDC;
        HGDIOBJ oldBits;
        int count;

        tmpDC = GetDC(NULL);
        assert(tmpDC != NULL);
        memDC = CreateCompatibleDC(tmpDC);
        oldBits = SelectObject(memDC, mhBitmap);
        count = GetDIBColorTable(memDC, 0, mNumColorsUsed, mpColorTable);
        if (count == 0) {
            DWORD err = ::GetLastError();
            CString buf;
            GetWin32ErrorString(err, &buf);
            LOGW(" DIB GetDIBColorTable failed (err=0x%x '%ls')",
                err, (LPCWSTR) buf);
        }
        SelectObject(memDC, oldBits);
        DeleteDC(memDC);
        ReleaseDC(NULL, tmpDC);
    }

    /*
     * Unfortunately it appears that LoadImage sets up the mPitchBytes
     * improperly for a DIB.  I believe DIBs need 4-byte-aligned widths
     * while compatible bitmaps only need 2-byte-aligned.  We need to
     * tweak mPitchBytes or we'll get garbage.
     */
    if (mPitchBytes & 0x03) {
        LOGI(" DIB altering LoadImage pitchBytes (currently %d)", mPitchBytes);
        mPitchBytes = (mPitchBytes + 3) & ~(0x03);
    }

bail:
    assert(mpPixels != NULL);
    return mpPixels;
}
#if 0   // this might be a better way??
            HRSRC   hResInfo;
            HGLOBAL hResData;
            DWORD   dwSize;
            VOID*   pvRes;
           // Loading it as a file failed, so try it as a resource
            if( NULL == ( hResInfo = FindResource( NULL, strFileName, TEXT("WAVE") ) ) )
            {
                if( NULL == ( hResInfo = FindResource( NULL, strFileName, TEXT("WAV") ) ) )
                    return DXTRACE_ERR( TEXT("FindResource"), E_FAIL );
            }

            if( NULL == ( hResData = LoadResource( NULL, hResInfo ) ) )
                return DXTRACE_ERR( TEXT("LoadResource"), E_FAIL );

            if( 0 == ( dwSize = SizeofResource( NULL, hResInfo ) ) ) 
                return DXTRACE_ERR( TEXT("SizeofResource"), E_FAIL );

            if( NULL == ( pvRes = LockResource( hResData ) ) )
                return DXTRACE_ERR( TEXT("LockResource"), E_FAIL );

            m_pResourceBuffer = new CHAR[ dwSize ];
            memcpy( m_pResourceBuffer, pvRes, dwSize );
#endif

void MyDIBitmap::ClearPixels(void)
{
    assert(mpPixels != NULL);

    //LOGI(" DIB clearing entire bitmap (%d bytes)", mPitchBytes * mHeight);
    memset(mpPixels, 0, mPitchBytes * mHeight);
}

void MyDIBitmap::SetColorTable(const RGBQUAD* pColorTable)
{
    assert(pColorTable != NULL);

    /* scan for junk */
    for (int i = 0; i < mNumColorsUsed; i++) {
        if (pColorTable[i].rgbReserved != 0) {
            /*
             * PhotoShop v5.x sets rgbReserved to 1 on every 8th color table
             * entry on 8-bit images.  No idea why.
             */
            //LOGI(" DIB warning: bogus color entry %d (res=%d)", i,
            //  pColorTable[i].rgbReserved);
            //DebugBreak();
        }
    }

    /* structs are the same, so just copy it over */
    memcpy(mpColorTable, pColorTable, mNumColorsUsed * sizeof(RGBQUAD));
    mColorTableInitialized = true;
}

bool MyDIBitmap::GetTransparentColor(RGBQUAD* pColor) const
{
    if (mAlphaType != kAlphaTransparency)
        return false;
    *(DWORD*)pColor = mTransparentColor;
    return true;
}

void MyDIBitmap::SetTransparentColor(const RGBQUAD* pColor)
{
    if (mAlphaType == kAlphaFull) {
        LOGI(" NOTE: switching from full alpha to transparent-color alpha");
    }
    mTransparentColor = *(const DWORD*)pColor;
    mTransparentColor &= ~kAlphaMask;   // strip alpha off, want color only
    mAlphaType = kAlphaTransparency;
}

int MyDIBitmap::LookupColor(const RGBQUAD* pRgbQuad)
{
    if (mBpp > 8) {
        LOGI(" DIB LookupColor on %d-bit image", mBpp);
        return -2;
    }
    if (!mColorTableInitialized) {
        LOGI(" DIB can't LookupColor, color table not initialized");
        return -2;
    }

    /* set the rgbReserved field to zero */
    unsigned long color = *(unsigned long*) pRgbQuad;
    color &= ~(kAlphaMask);

    int idx;
    for (idx = 0; idx < mNumColorsUsed; idx++) {
        if (color == *(unsigned long*)&mpColorTable[idx])
            return idx;
    }

    return -1;
}

/*
 * Return the RGB value of a single pixel in a bitmap.
 *
 * "rgbReserved" is always set to zero.
 */
void MyDIBitmap::GetPixelRGB(int x, int y, RGBQUAD* pRgbQuad) const
{
    GetPixelRGBA(x, y, pRgbQuad);
    pRgbQuad->rgbReserved = 0;
}

/*
 * Return the RGBA value of a single pixel in a bitmap.
 *
 * This sets rgbReserved appropriately for the current alpha mode.
 */
void MyDIBitmap::GetPixelRGBA(int x, int y, RGBQUAD* pRgbQuad) const
{
    assert(x >= 0 && x < mWidth && y >= 0 && y < mHeight);
    y = mHeight - y -1; // upside-down

    if (mBpp == 32) {
        assert((mPitchBytes % 4) == 0);
        assert(sizeof(RGBQUAD) == 4);

        RGBQUAD* lptr = (RGBQUAD*) mpPixels;
        lptr += y * (mPitchBytes >> 2) + x;
        *pRgbQuad = *lptr;
    } else if (mBpp == 24) {
        unsigned char* ptr = (unsigned char*) mpPixels;

        ptr += y * mPitchBytes + (x << 1) + x;
        pRgbQuad->rgbBlue = *ptr++;
        pRgbQuad->rgbGreen = *ptr++;
        pRgbQuad->rgbRed = *ptr++;
        //pRgbQuad->rgbReserved = 0;
    } else if (mBpp == 16) {
        /* format is XRRRRRGGGGGBBBBB; must convert 0-31 to 0-255 */
        static const unsigned int conv[32] = {
            0,   8,  16,  25,  33,  41,  49,  58, 
            66,  74,  82,  90,  99, 107, 115, 123, 
            132, 140, 148, 156, 165, 173, 181, 189, 
            197, 206, 214, 222, 230, 239, 247, 255
        };
        unsigned short* ptr = (unsigned short*) mpPixels;
        unsigned short val;

        ptr += y * (mPitchBytes >> 1) + x;
        val = *ptr;
        pRgbQuad->rgbBlue = conv[val & 0x1f];
        pRgbQuad->rgbGreen = conv[(val >> 5) & 0x1f];
        pRgbQuad->rgbRed = conv[(val >> 10) & 0x1f];
        //pRgbQuad->rgbReserved = 0;
    } else if (mBpp == 8) {
        unsigned char* ptr = (unsigned char*) mpPixels;
        int idx;

        ptr += y * mPitchBytes + x;
        idx = *ptr;
        *pRgbQuad = mpColorTable[idx];
    } else if (mBpp == 4) {
        assert(mpColorTable != NULL);
        unsigned char* ptr = (unsigned char*) mpPixels;
        int idx;

        ptr += y * mPitchBytes + (x >> 1);
        if (x & 0x01)
            idx = (*ptr & 0x0f);
        else
            idx = (*ptr & 0xf0) >> 4;
        *pRgbQuad = mpColorTable[idx];
    } else if (mBpp == 1) {
        assert(sizeof(RGBQUAD) == sizeof(DWORD));

        unsigned char* ptr = (unsigned char*) mpPixels;
        ptr += y * mPitchBytes + (x >> 3);
        if (*ptr & (0x80 >> (x & 0x07)))
            *(DWORD*)pRgbQuad = 0xffffff00;
        else
            *(DWORD*)pRgbQuad = 0x00000000;
    } else {
        assert(false);  // bit depth not implemented
    }

    /*
     * Fix up the "rgbReserved" field.  Windows says it must always be zero,
     * so unless the application changes the alpha type, we leave it that way.
     */
    if (mAlphaType == kAlphaOpaque) {
        pRgbQuad->rgbReserved = 255;    // always force to 255, even on 32bpp
    } else if (mAlphaType == kAlphaTransparency) {
        /* test for the transparent color, ignoring alpha */
        if (((*(DWORD*)pRgbQuad) & ~kAlphaMask) == mTransparentColor)
            pRgbQuad->rgbReserved = 0;              // fully transparent
        else
            pRgbQuad->rgbReserved = 255;            // fully opaque
    } else {
        assert(mAlphaType == kAlphaFull);
        assert(mBpp == 32);
        /* full alpha in 32-bit data, leave it be */
    }
}

/*
 * Set the RGB value of a single pixel in a bitmap.
 *
 * The "rgbReserved" channel is forced to zero.
 */
void MyDIBitmap::SetPixelRGB(int x, int y, const RGBQUAD* pRgbQuad)
{
    if (pRgbQuad->rgbReserved == 0) {
        SetPixelRGBA(x, y, pRgbQuad);
    } else {
        RGBQUAD tmp = *pRgbQuad;
        tmp.rgbReserved = 0;
        SetPixelRGBA(x, y, &tmp);
    }
}

/*
 * Set the RGBA value of a single pixel in a bitmap.
 *
 * For index-color bitmaps, this requires a (slow) table lookup.
 */
void MyDIBitmap::SetPixelRGBA(int x, int y, const RGBQUAD* pRgbQuad)
{
    assert(x >= 0 && x < mWidth && y >= 0 && y < mHeight);
    y = mHeight - y -1; // upside-down

    if (mBpp == 32) {
        assert((mPitchBytes % 4) == 0);
        assert(sizeof(RGBQUAD) == 4);

        RGBQUAD* lptr = (RGBQUAD*) mpPixels;
        lptr += y * (mPitchBytes >> 2) + x;
        *lptr = *pRgbQuad;
    } else if (mBpp == 24) {
        unsigned char* ptr = (unsigned char*) mpPixels;

        ptr += y * mPitchBytes + (x << 1) + x;
        *ptr++ = pRgbQuad->rgbBlue;
        *ptr++ = pRgbQuad->rgbGreen;
        *ptr++ = pRgbQuad->rgbRed;
    } else if (mBpp == 8 || mBpp == 4) {
        int idx = LookupColor(pRgbQuad);
        if (idx < 0) {
            LOGI(" DIB WARNING: unable to set pixel to (%d,%d,%d)",
                pRgbQuad->rgbRed, pRgbQuad->rgbGreen, pRgbQuad->rgbBlue);
        } else {
            SetPixelIndex(x, (mHeight - y -1), idx);
        }
    } else {
        assert(false);  // not implemented
    }
}

/*
 * Get the color table index of the specified pixel.
 *
 * Only works on indexed-color formats (8bpp or less).
 */
void MyDIBitmap::GetPixelIndex(int x, int y, int* pIdx) const
{
    assert(x >= 0 && x < mWidth && y >= 0 && y < mHeight);
    y = mHeight - y -1; // upside-down

    if (mBpp == 8) {
        unsigned char* ptr = (unsigned char*) mpPixels;

        ptr += y * mPitchBytes + x;
        *pIdx = *ptr;
    } else if (mBpp == 4) {
        unsigned char* ptr = (unsigned char*) mpPixels;

        ptr += y * mPitchBytes + (x >> 1);
        if (x & 0x01)
            *pIdx = (*ptr & 0x0f);
        else
            *pIdx = (*ptr & 0xf0) >> 4;
    } else {
        assert(false);      // not implemented
    }
}

/*
 * Set the index value of a pixel in an indexed-color bitmap (8bpp or less).
 */
void MyDIBitmap::SetPixelIndex(int x, int y, int idx)
{
    if (x < 0 || x >= mWidth || y < 0 || y >= mHeight) {
        LOGI("BAD x=%d y=%d idx=%d", x, y, idx);
        LOGI("   width=%d height=%d", mWidth, mHeight);
    }
    assert(x >= 0 && x < mWidth && y >= 0 && y < mHeight);
    y = mHeight - y -1; // upside-down

    if (mBpp == 8) {
        assert(idx >= 0 && idx < 256);
        unsigned char* ptr = (unsigned char*) mpPixels;

        ptr += y * mPitchBytes + x;
        *ptr = idx;
    } else if (mBpp == 4) {
        assert(idx >= 0 && idx < 16);
        unsigned char* ptr = (unsigned char*) mpPixels;

        ptr += y * mPitchBytes + (x >> 1);
        if (x & 0x01)
            *ptr = (*ptr & 0xf0) | idx;
        else
            *ptr = (*ptr & 0x0f) | idx << 4;
    } else {
        assert(false);  // not implemented
    }
}

/*static*/ bool MyDIBitmap::Blit(MyDIBitmap* pDstBits, const RECT* pDstRect,
    const MyDIBitmap* pSrcBits, const RECT* pSrcRect)
{
    if (pDstRect->right - pDstRect->left !=
        pSrcRect->right - pSrcRect->left)
    {
        LOGW("DIB blit: widths differ");
        return false;
    }
    if (pDstRect->bottom - pDstRect->top !=
        pSrcRect->bottom - pSrcRect->top)
    {
        LOGW("DIB blit: heights differ");
        return false;
    }
    if (pSrcBits->mBpp != pDstBits->mBpp) {
        LOGW("DIB blit: different formats");
        return false;
    }
    if (pDstRect->right <= pDstRect->left ||
        pDstRect->bottom <= pDstRect->top)
    {
        LOGW("DIB blit: poorly formed rect");
        return false;
    }

    int srcX, srcY, dstX, dstY;
    srcY = pSrcRect->top;
    dstY = pDstRect->top;

    /*
     * A decidedly non-optimized blit function.
     *
     * Copy by index when appropriate.
     */
    if (pDstBits->mBpp <= 8) {
        int idx;
        while (srcY < pSrcRect->bottom) {
            srcX = pSrcRect->left;
            dstX = pDstRect->left;
            while (srcX < pSrcRect->right) {
                pSrcBits->GetPixelIndex(srcX, srcY, &idx);
                pDstBits->SetPixelIndex(dstX, dstY, idx);
                srcX++;
                dstX++;
            }
            srcY++;
            dstY++;
        }
    } else {
        RGBQUAD color;
        while (srcY < pSrcRect->bottom) {
            srcX = pSrcRect->left;
            dstX = pDstRect->left;
            while (srcX < pSrcRect->right) {
                pSrcBits->GetPixelRGBA(srcX, srcY, &color);
                pDstBits->SetPixelRGBA(dstX, dstY, &color);
                srcX++;
                dstX++;
            }
            srcY++;
            dstY++;
        }
    }

    return true;
}

HBITMAP MyDIBitmap::ConvertToDDB(HDC dc) const
{
    HBITMAP hBitmap = NULL;

    if (mNumColorsUsed != 0 && !mColorTableInitialized) {
        LOGI(" DIB color table not initialized!");
        return NULL;
    }

    /*
     * Create a BITMAPINFO structure with the BITMAPINFOHEADER from the
     * DIB and a copy of the color table (if any).
     *
     * (We slightly over-allocate here, because the size of the BITMAPINFO
     * struct actually includes the first color entry.)
     */
    BITMAPINFO* pNewInfo = NULL;
    int colorTableSize = sizeof(RGBQUAD) * mNumColorsUsed;
    pNewInfo = (BITMAPINFO*)
                    new unsigned char[sizeof(BITMAPINFO) + colorTableSize];
    if (pNewInfo == NULL)
        return NULL;

    pNewInfo->bmiHeader = mBitmapInfoHdr;
    if (colorTableSize != 0)
        memcpy(&pNewInfo->bmiColors, mpColorTable, colorTableSize);

#if 0       // this fails under Win98SE, works under Win2K
    /*
     * Create storage.
     */
    hBitmap = ::CreateDIBitmap(dc, &mBitmapInfoHdr, 0, NULL, NULL, 0);
    if (hBitmap == NULL) {
        LOGI(" DIB CreateDIBBitmap failed!");
        return NULL;
    }

    LOGI(" PARM hbit=0x%08lx hgt=%d fpPixels=0x%08lx pNewInfo=0x%08lx",
        hBitmap, mBitmapInfoHdr.biHeight, fpPixels, pNewInfo);
    LogHexDump(&mBitmapInfoHdr, sizeof(mBitmapInfoHdr));
    LOGI("  pNewInfo (sz=%d colorTableSize=%d):\n", sizeof(BITMAPINFO),
        colorTableSize);
    LogHexDump(pNewInfo, sizeof(BITMAPINFO) + colorTableSize);

    /*
     * Transfer the bits.
     */
    int count = ::SetDIBits(NULL, hBitmap, 0, mBitmapInfoHdr.biHeight,
                    mpPixels, pNewInfo, DIB_RGB_COLORS);

    if (count != mBitmapInfoHdr.biHeight) {
        DWORD err = ::GetLastError();

        LOGI(" DIB SetDIBits failed, count was %d", count);
        ::DeleteObject(hBitmap);
        hBitmap = NULL;

        CString msg;
        GetWin32ErrorString(err, &msg);
        LOGI(" DIB CreateDIBSection failed (err=%d msg='%ls')",
            err, (LPCWSTR) msg);
        //ASSERT(false);        // stop & examine this
        return NULL;
    }
#else
    /*
     * Create storage.
     */
    hBitmap = ::CreateDIBitmap(dc, &mBitmapInfoHdr, CBM_INIT, mpPixels,
                    pNewInfo, DIB_RGB_COLORS);
    if (hBitmap == NULL) {
        LOGI(" DIB CreateDIBBitmap failed!");
        return NULL;
    }
#endif

    delete[] pNewInfo;
    return hBitmap;
}

int MyDIBitmap::WriteToFile(const WCHAR* fileName) const
{
    FILE* fp = NULL;
    int err;

    assert(fileName != NULL);

    fp = _wfopen(fileName, L"wb");
    if (fp == NULL) {
        err = errno ? errno : -1;
        LOGI("Unable to open bitmap file '%ls' (err=%d)", fileName, err);
        return err;
    }

    err = WriteToFile(fp);
    fclose(fp);
    return err;
}

int MyDIBitmap::WriteToFile(FILE* fp) const
{
    BITMAPFILEHEADER fileHeader;
    long pixelBufSize;
    long startOffset;
    int result = -1;

    assert(fp != NULL);

    startOffset = ftell(fp);

    /* make sure all GDI operations on this bitmap have completed */
    GdiFlush();

    pixelBufSize = mPitchBytes * mBitmapInfoHdr.biHeight;

    fileHeader.bfType = kBMPMagic;
    fileHeader.bfReserved1 = 0;
    fileHeader.bfReserved2 = 0;
    fileHeader.bfOffBits = sizeof(fileHeader) + sizeof(mBitmapInfoHdr) +
        sizeof(RGBQUAD) * mNumColorsUsed;
    fileHeader.bfSize = fileHeader.bfOffBits + pixelBufSize;

    LOGI(" DIB writing bfOffBits=%d, bfSize=%d, pixelBufSize=%d",
        fileHeader.bfOffBits, fileHeader.bfSize, pixelBufSize);

    if (fwrite(&fileHeader, sizeof(fileHeader), 1, fp) != 1) {
        result = errno ? errno : -1;
        goto bail;
    }
    if (fwrite(&mBitmapInfoHdr, sizeof(mBitmapInfoHdr), 1, fp) != 1) {
        result = errno ? errno : -1;
        goto bail;
    }
    if (mNumColorsUsed != 0) {
        assert(mpColorTable != NULL);
        if (fwrite(mpColorTable, sizeof(RGBQUAD) * mNumColorsUsed, 1, fp) != 1)
        {
            result = errno ? errno : -1;
            goto bail;
        }
    }
    if (fwrite(mpPixels, pixelBufSize, 1, fp) != 1) {
        result = errno ? errno : -1;
        goto bail;
    }

    /* push it out to disk */
    fflush(fp);

    /* verify the length; useful for detecting "w" vs "wb" */
    if (ftell(fp) - startOffset != (long) fileHeader.bfSize) {
        LOGI("DIB tried to write %ld, wrote %ld (check for \"wb\")",
            fileHeader.bfSize, ftell(fp) - startOffset);
        assert(false);
    }

    result = 0;

bail:
    return result;
}
