/*
 * CiderPress
 * Copyright (C) 2007 by faddenSoft, LLC.  All Rights Reserved.
 * See the file LICENSE for distribution terms.
 */
/*
 * Class that encapsulates a sound file.  Includes loading of sounds from WAV
 * files and other formats.  Does not create or write sound files.
 *
 * [ Copied from libfadden. ]
 */
#ifndef UTIL_SOUNDFILE_H
#define UTIL_SOUNDFILE_H

#include <mmsystem.h>
#include <stdint.h>

/*
 * Class providing read-only access to uncompressed sound samples and
 * associated meta-data.  The sound file is assumed to fit in main memory.
 *
 * Because game sound effects are generally loaded into "secondary sound
 * buffers" allocated by DirectX, this class doesn't load the sound data
 * into local storage unless necessary.
 *
 * WAV data is little-endian.  8-bit data is unsigned, 16-bit data is
 * signed.
 */
class SoundFile {
public:
    SoundFile(void) :
        mFP(NULL),
        mDoClose(false),
        mFileStart(0),
        mSampleStart(-1),
        mSampleLen(-1)
    {
        memset(&mFormat, 0, sizeof(mFormat));
    }
    virtual ~SoundFile(void) {
        if (mDoClose && mFP != NULL)
            fclose(mFP);
    }

    /* create the object from a file on disk; returns 0 on success */
    int Create(const WCHAR* fileName, CString* pErrMsg);
    /* create from FILE*; if doClose==true, file will be closed on error */
    int Create(FILE* fp, long len, bool doClose, CString* pErrMsg);

    /* create object from a buffer of memory */
    //int Create(const void* buf, long len);

    /* read a block of audio samples from the specified offset */
    int ReadData(void* buf, long sampleOffset, long len) const;

    /* seek to an absolute offset within the WAV file */
    int SeekAbs(long offset) { return fseek(mFP, offset, SEEK_SET); }

    long GetDataOffset(void) const { return mSampleStart; }
    uint32_t GetDataLen(void) const { return mSampleLen; }
    const WAVEFORMATEX* GetWaveFormat(void) { return &mFormat; }

    /* returns the #of bytes per sample (all channels) */
    int GetBPS(void) const {
        ASSERT(mFP != NULL);
        return ((mFormat.wBitsPerSample+7)/8) * mFormat.nChannels;
    }

private:
    DECLARE_COPY_AND_OPEQ(SoundFile)

    int SkipToHeader(uint32_t hdrID, uint32_t* pChunkLen);

    enum { kWAVMinSize = 40 };

    //void*         mBuffer;        // pointer to memory we need to delete
    FILE*           mFP;            // currently open sound file
    bool            mDoClose;       // do we own mFP?

    long            mFileStart;     // so we can rewind the sound file
    long            mSampleStart;   // offset in mem or file to sound samples
    uint32_t        mSampleLen;     // length in bytes of audio sample section

    WAVEFORMATEX    mFormat;        // WAV parameters (from mmsystem.h/mmreg.h)
};

#if 0       // contents of the WAVEFORMATEX struct; "cbSize" is not in WAV file
typedef struct tWAVEFORMATEX
{
    WORD    wFormatTag;        /* format type */
    WORD    nChannels;         /* number of channels (i.e. mono, stereo...) */
    DWORD   nSamplesPerSec;    /* sample rate */
    DWORD   nAvgBytesPerSec;   /* for buffer estimation */
    WORD    nBlockAlign;       /* block size of data */
    WORD    wBitsPerSample;    /* Number of bits per sample of mono data */
    WORD    cbSize;            /* The count in bytes of the size of
                                    extra information (after cbSize) */
} WAVEFORMATEX;
#endif

#endif /*UTIL_SOUNDFILE_H*/
