/*
 * CiderPress
 * Copyright (C) 2007 by faddenSoft, LLC.  All Rights Reserved.
 * See the file LICENSE for distribution terms.
 */
/*
 * Declarations common within but private to the DiskImg library.
 *
 * External code should not include this.
 */
#ifndef DISKIMG_DISKIMGPRIV_H
#define DISKIMG_DISKIMGPRIV_H

#include "DiskImgDetail.h"
#include <errno.h>
#include <assert.h>
// "GenericFD.h" included at end

using namespace DiskImgLib;     // make life easy for all internal code

namespace DiskImgLib {

/*
 * Debug logging macros.
 *
 * The macro choice implies a severity level, but we don't currently
 * support that in the callback interface, so it's not used.
 */
#define DLOG_BASE(file, line, format, ...) \
        Global::PrintDebugMsg((file), (line), (format), __VA_ARGS__)

#ifdef SHOW_LOGV
# define LOGV(format, ...) DLOG_BASE(__FILE__, __LINE__, (format), __VA_ARGS__)
#else
# define LOGV(format, ...) ((void) 0)
#endif
#define LOGD(format, ...) DLOG_BASE(__FILE__, __LINE__, (format), __VA_ARGS__)
#define LOGI(format, ...) DLOG_BASE(__FILE__, __LINE__, (format), __VA_ARGS__)
#define LOGW(format, ...) DLOG_BASE(__FILE__, __LINE__, (format), __VA_ARGS__)
#define LOGE(format, ...) DLOG_BASE(__FILE__, __LINE__, (format), __VA_ARGS__)

// TODO: remove these
#define WMSG0(fmt) LOGI(fmt)
#define WMSG1(fmt, arg0) LOGI(fmt, arg0)
#define WMSG2(fmt, arg0, arg1) LOGI(fmt, arg0, arg1)
#define WMSG3(fmt, arg0, arg1, arg2) LOGI(fmt, arg0, arg1, arg2)
#define WMSG4(fmt, arg0, arg1, arg2, arg3) LOGI(fmt, arg0, arg1, arg2, arg3)
#define WMSG5(fmt, arg0, arg1, arg2, arg3, arg4) LOGI(fmt, arg0, arg1, arg2, arg3, arg4)

/* put this in to break on interesting events when built debug */
#if defined(_DEBUG)
# define DebugBreak() { assert(false); }
#else
# define DebugBreak() ((void) 0)
#endif

/*
 * Standard goodies.
 */
#define NELEM(x) ((int) (sizeof(x) / sizeof(x[0])))

#define ErrnoOrGeneric() (errno != 0 ? (DIError) errno : kDIErrGeneric)


/* filename manipulation functions */
const char* FilenameOnly(const char* pathname, char fssep);
const char* FindExtension(const char* pathname, char fssep);
char* StrcpyNew(const char* str);

/* get/set integer values out of a memory buffer */
unsigned short GetShortLE(const unsigned char* buf);
unsigned long GetLongLE(const unsigned char* buf);
unsigned short GetShortBE(const unsigned char* buf);
unsigned long GetLongBE(const unsigned char* buf);
unsigned long Get24BE(const unsigned char* ptr);
void PutShortLE(unsigned char* ptr, unsigned short val);
void PutLongLE(unsigned char* ptr, unsigned long val);
void PutShortBE(unsigned char* ptr, unsigned short val);
void PutLongBE(unsigned char* ptr, unsigned long val);

/* little-endian read/write, for file headers (mainly 2MG and DC42) */
DIError ReadShortLE(GenericFD* pGFD, short* pBuf);
DIError ReadLongLE(GenericFD* pGFD, long* pBuf);
DIError WriteShortLE(FILE* fp, unsigned short val);
DIError WriteLongLE(FILE* fp, unsigned long val);
DIError WriteShortLE(GenericFD* pGFD, unsigned short val);
DIError WriteLongLE(GenericFD* pGFD, unsigned long val);
DIError WriteShortBE(GenericFD* pGFD, unsigned short val);
DIError WriteLongBE(GenericFD* pGFD, unsigned long val);

#ifdef _WIN32
/* Windows helpers */
DIError LastErrorToDIError(void);
bool IsWin9x(void);
#endif


/*
 * Provide access to a buffer of data as if it were a circular buffer.
 * Access is through the C array operator ([]).
 *
 * This DOES NOT own the array it is handed, and will not try to
 * free it.
 */
class CircularBufferAccess {
public:
    CircularBufferAccess(unsigned char* buf, long len) :
        fBuf(buf), fLen(len)
        { assert(fLen > 0); assert(fBuf != NULL); }
    CircularBufferAccess(const unsigned char* buf, long len) :
        fBuf(const_cast<unsigned char*>(buf)), fLen(len)
        { assert(fLen > 0); assert(fBuf != NULL); }
    ~CircularBufferAccess(void) {}

    /*
     * Be circular.  Assume that we won't stray far past the end, so
     * it's cheaper to subtract than mod.
     */
    unsigned char& operator[](int idx) const {
        if (idx < 0) {
            assert(false);
        }
        while (idx >= fLen)
            idx -= fLen;
        return fBuf[idx];
    }

    //unsigned char* GetPointer(int idx) const {
    //    while (idx >= fLen)
    //        idx -= fLen;
    //    return &fBuf[idx];
    //}

    int Normalize(int idx) const {
        while (idx >= fLen)
            idx -= fLen;
        return idx;
    }

    long GetSize(void) const {
        return fLen;
    }

private:
    unsigned char*  fBuf;
    long            fLen;
};

/*
 * Manage an output buffer into which we write one bit at a time.
 *
 * Bits fill in from the MSB to the LSB.  If we write 10 bits, the
 * output buffer will look like this:
 *
 *  xxxxxxxx xx000000
 *
 * Call WriteBit() repeatedly.  When done, call Finish() to write any pending
 * data and return the number of bits in the buffer.
 */
class BitOutputBuffer {
public:
    /* pass in the output buffer and the output buffer's size */
    BitOutputBuffer(unsigned char* buf, int size) {
        fBufStart = fBuf = buf;
        fBufSize = size;
        fBitMask = 0x80;
        fByte = 0;
        fOverflow = false;
    }
    virtual ~BitOutputBuffer(void) {}

    /* write a single bit */
    void WriteBit(int val) {
        if (fBuf - fBufStart >= fBufSize) {
            if (!fOverflow) {
                WMSG0("Overran bit output buffer\n");
                DebugBreak();
                fOverflow = true;
            }
            return;
        }

        if (val)
            fByte |= fBitMask;
        fBitMask >>= 1;
        if (fBitMask == 0) {
            *fBuf++ = fByte;
            fBitMask = 0x80;
            fByte = 0;
        }
    }

    /* flush pending bits; returns length in bits (or -1 on overrun) */
    int Finish(void) {
        int outputBits;

        if (fOverflow)
            return -1;

        outputBits = (fBuf - fBufStart) * 8;

        if (fBitMask != 0x80) {
            *fBuf++ = fByte;

            assert(fBitMask != 0);
            while (fBitMask != 0x80) {
                outputBits++;
                fBitMask <<= 1;
            }
        }
        return outputBits;
    }

private:
    unsigned char*  fBufStart;
    unsigned char*  fBuf;
    int             fBufSize;
    unsigned char   fBitMask;
    unsigned char   fByte;
    bool            fOverflow;
};

/*
 * Extract data from the buffer one bit or one byte at a time.
 */
class BitInputBuffer {
public:
    BitInputBuffer(const unsigned char* buf, int bitCount) {
        fBufStart = fBuf = buf;
        fBitCount = bitCount;
        fCurrentBit = 0;
        fBitPosn = 7;
        fBitsConsumed = 0;
    }
    virtual ~BitInputBuffer(void) {}

    /*
     * Get the next bit.  Returns 0 or 1.
     *
     * If we wrapped around to the start of the buffer, and "pWrap" is
     * non-null, set "*pWrap". (This does *not* set it to "false" if we
     * don't wrap.)
     */
    unsigned char GetBit(bool* pWrap) {
        unsigned char val;

        //assert(fBitPosn == 7 - (fCurrentBit & 0x07));

        if (fCurrentBit == fBitCount) {
            /* end reached, wrap to start */
            fCurrentBit = 0;
            fBitPosn = 7;
            fBuf = fBufStart;
            //fByte = *fBuf++;
            if (pWrap != NULL)
                *pWrap = true;
        }

        val = (*fBuf >> fBitPosn) & 0x01;

        fCurrentBit++;
        fBitPosn--;
        if (fBitPosn < 0) {
            fBitPosn = 7;
            fBuf++;
        }

        fBitsConsumed++;
        return val;
    }

    /*
     * Get the next 8 bits.
     */
    unsigned char GetByte(bool* pWrap) {
        unsigned char val;
        int i;

        if (true || fCurrentBit > fBitCount-8) {
            /* near end, use single-bit function iteratively */
            val = 0;
            for (i = 0; i < 8; i++)
                val = (val << 1) | GetBit(pWrap);
        } else {
            /* room to spare, grab it in one or two chunks */
            assert(false);
        }
        return val;
    }

    /*
     * Set the start position.
     */
    void SetStartPosition(int bitOffset) {
        assert(bitOffset >= 0 && bitOffset < fBitCount);
        fCurrentBit = bitOffset;
        fBitPosn = 7 - (bitOffset & 0x07);      // mod 8, 0 to MSB
        fBuf = fBufStart + (bitOffset >> 3);    // div 8
    }

    /* used to ensure we consume exactly 100% of bits */
    void ResetBitsConsumed(void) { fBitsConsumed = 0; }
    int GetBitsConsumed(void) const { return fBitsConsumed; }

private:
    const unsigned char*    fBufStart;
    const unsigned char*    fBuf;
    int             fBitCount;          // #of bits in buffer
    int             fCurrentBit;        // where we are in buffer
    int             fBitPosn;           // which bit to access within byte
    //unsigned char fByte;

    int             fBitsConsumed;      // sanity check - all bits used?
};

/*
 * Linear bitmap.  Suitable for use as a bad block map.
 */
class LinearBitmap {
public:
    LinearBitmap(int numBits) {
        assert(numBits > 0);
        fBits = new unsigned char[(numBits + 7) / 8];
        memset(fBits, 0, (numBits + 7) / 8);
        fNumBits = numBits;
    }
    ~LinearBitmap(void) {
        delete[] fBits;
    }

    /*
     * Set or get the status of bit N.
     */
    bool IsSet(int bit) const {
        assert(bit >= 0 && bit < fNumBits);
        return ((fBits[bit >> 3] >> (bit & 0x07)) & 0x01) != 0;
    }
    void Set(int bit) {
        assert(bit >= 0 && bit < fNumBits);
        fBits[bit >> 3] |= 1 << (bit & 0x07);
    }

private:
    unsigned char*  fBits;
    int             fNumBits;
};


}  // namespace DiskImgLib

/*
 * Most of the code needs these.
 */
#include "GenericFD.h"

#endif /*DISKIMG_DISKIMGPRIV_H*/
