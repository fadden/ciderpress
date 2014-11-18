/*
 * CiderPress
 * Copyright (C) 2007 by faddenSoft, LLC.  All Rights Reserved.
 * See the file LICENSE for distribution terms.
 */
/*
 * Read-only encapsulation of a sound file.
 *
 * Microsoft supplies a number of functions (e.g. mmioOpen) that do a lot of
 * this, but it's built on top of fairly complex machinery designed to
 * support AVI files and read/write WAV files.  It actually requires about
 * the same amount of code to parse a WAV file without resorting to MMIO.
 */
#include "StdAfx.h"
#include "SoundFile.h"


/*
 * Convert four characters into a single 4-byte constant.
 *
 * The constant is expected to match 4-character values read from a binary
 * file, so the value is endian-dependent.
 */
static inline unsigned long
MakeFourCC(unsigned char c0, unsigned char c1, unsigned char c2,
    unsigned char c3)
{
    /* little-endian */
    return  ((unsigned long)c0) | ((unsigned long)c1 << 8) |
            ((unsigned long)c2 << 16) | ((unsigned long)c3 << 24);
}

/*
 * Create the object from a file on disk.
 *
 * Returns 0 on success.
 */
int
SoundFile::Create(const WCHAR* fileName, CString* pErrMsg)
{
    FILE* fp = NULL;
    long fileLen;

    fp = _wfopen(fileName, L"rb");
    if (fp == NULL) {
        int err = errno;
        pErrMsg->Format(L"Unable to open '%ls'", fileName);
        return err;
    }

    fseek(fp, 0, SEEK_END);
    fileLen = ftell(fp);
    rewind(fp);

    return Create(fp, fileLen, true, pErrMsg);
}

/*
 * Create the object from an already-open file pointer.  The file must be
 * seeked to the start of the WAV file.
 *
 * Returns 0 on success.
 */
int
SoundFile::Create(FILE* fp, long len, bool doClose, CString* pErrMsg)
{
    struct {
        unsigned long   riff;
        unsigned long   fileLen;
        unsigned long   wav;
    } fileHeader;
    unsigned long chunkLen;
    int err = 0;

    if (mFP != NULL) {
        WMSG0("SoundFile object already created\n");
        assert(false);
        return -1;
    }
    if (fp == NULL)
        return -1;
    if (len < kWAVMinSize) {
        *pErrMsg = L"File is too short to be WAV";
        return -1;
    }

    /* from here on out, we own fp if doClose==true */
    mFP = fp;
    mDoClose = doClose;
    mFileStart = ftell(mFP);    // so we can rewind if necessary

    /*
     * Check the first header.
     */
    if (fread(&fileHeader, sizeof(fileHeader), 1, mFP) != 1) {
        err = errno ? errno : -1;
        *pErrMsg = L"Failed reading file header";
        goto bail;
    }
    if (fileHeader.riff != MakeFourCC('R','I','F','F') ||
        fileHeader.wav != MakeFourCC('W','A','V','E') ||
        fileHeader.fileLen > (unsigned long) len)
    {
        *pErrMsg = L"File is not a WAV file";
        WMSG3("Not a valid WAV header (0x%08lx %d 0x%08lx)\n",
            fileHeader.riff, fileHeader.fileLen, fileHeader.wav);
        err = -1;
        goto bail;
    }

    /*
     * Find the "fmt " chunk, which holds the WAV format data.  This should
     * immediately follow the file header, but I'm not sure that's mandatory.
     * I *am* going to assume that it precedes the "data" chunk, so I don't
     * have to rewind the file after finding "fmt ".
     */
    err = SkipToHeader(MakeFourCC('f','m','t',' '), &chunkLen);
    if (err != 0)
        goto bail;

    /*
     * Read the WAVEFORMATEX structure directly from the file.  This is useful
     * as an argument to certain DirectX functions.  The structure contains
     * an extra field, so we need to zero it out and use chunkLen rather
     * than the structure size.
     */
    if (chunkLen > sizeof(WAVEFORMATEX)) {
        pErrMsg->Format(L"Bad WAV file: 'fmt ' size is %d, struct is %d",
            chunkLen, sizeof(WAVEFORMATEX));
        err = -1;
        goto bail;
    }
    //memset(&mFormat, 0, sizeof(mFormat));     // done in constructor
    if (fread(&mFormat, chunkLen, 1, mFP) != 1) {
        err = errno ? errno : -1;
        *pErrMsg = L"Failed reading WAVEFORMATEX";
        goto bail;
    }

    /* check the format for compatibility */
    if (mFormat.wFormatTag != WAVE_FORMAT_PCM) {
        *pErrMsg = L"WAV file is not PCM format";
        err = -1;
        goto bail;
    }

    /*
     * Find the "data" chunk, which holds the actual sound samples.
     */
    err = SkipToHeader(MakeFourCC('d','a','t','a'), &chunkLen);
    if (err != 0)
        goto bail;

    mSampleStart = ftell(mFP);
    mSampleLen = chunkLen;

    WMSG4("WAV: chan=%d samples/sec=%d avgBPS=%d block=%d\n",
        mFormat.nChannels, mFormat.nSamplesPerSec, mFormat.nAvgBytesPerSec,
        mFormat.nBlockAlign);
    WMSG3("  bits/sample=%d [start=%d len=%d]\n", mFormat.wBitsPerSample,
        mSampleStart, mSampleLen);

bail:
    return err;
}

/*
 * Skip forward until we find the named chunk.  The file should be
 * positioned immediately before the first chunk.
 */
int
SoundFile::SkipToHeader(unsigned long hdrID, unsigned long* pChunkLen)
{
    struct {
        unsigned long   fourcc;
        unsigned long   chunkLen;
    } chunkHeader;
    int err = 0;

    /* loop until we find the right chunk or run out of file */
    while (true) {
        if (fread(&chunkHeader, sizeof(chunkHeader), 1, mFP) != 1) {
            err = errno ? errno : -1;
            WMSG1("Failed searching for chunk header 0x%08lx\n", hdrID);
            break;
        }

        if (chunkHeader.fourcc == hdrID) {
            *pChunkLen = chunkHeader.chunkLen;
            break;
        }

        /* didn't match, skip contents */
        if (fseek(mFP, chunkHeader.chunkLen, SEEK_CUR) != 0) {
            err = errno;
            WMSG1("Failed seeking past contents of 0x%08lx\n",
                chunkHeader.chunkLen);
            break;
        }
    }

    return err;
}

/*
 * Read a block of data from the specified offset.
 */
int
SoundFile::ReadData(void* buf, long sampleOffset, long len) const
{
    if ((unsigned long)(sampleOffset+len) > mSampleLen) {
        WMSG3("ERROR: invalid read request (%d bytes, %d into %d)\n",
            len, sampleOffset, mSampleLen);
        return -1;
    }

    if (fseek(mFP, mSampleStart+sampleOffset, SEEK_SET) != 0)
        return errno;

    if (fread(buf, len, 1, mFP) != 1) {
        int err = errno ? errno : -1;
        WMSG1("Failed reading %d bytes from sound file\n", len);
        return err;
    }

    return 0;
}
