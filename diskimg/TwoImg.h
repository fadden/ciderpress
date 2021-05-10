/*
 * CiderPress
 * Copyright (C) 2007 by faddenSoft, LLC.  All Rights Reserved.
 * See the file LICENSE for distribution terms.
 */
/*
 * Support for the "2MG"/"2IMG" disk image format.
 *
 * This gets its own header because CiderPress uses these definitions and
 * functions directly.
 */
#ifndef DISKIMG_TWOIMG_H
#define DISKIMG_TWOIMG_H

#include "DiskImg.h"

namespace DiskImgLib {

/*
 * 2IMG header definition (was on http://www.magnet.ch/emutech/Tech/,
 * now on http://www.a2central.com/programming/filetypes/ftne00130.html
 * as filetype $e0/$0130).
 *
 * Meaning of "flags":
 *  bit 31 : disk is "locked"; used by emulators as write-protect sticker.
 *  bit  8 : if set, bits 0-7 specify DOS 3.3 volume number
 *  bit 0-7: if bit 8 is set, use this as DOS volume; else use 254
 *
 * All values are stored little-endian.
 */
class DISKIMG_API TwoImgHeader {
public:
    TwoImgHeader(void) :
        fMagic(0),
        fCreator(0),
        fHeaderLen(0),
        fVersion(0),
        fImageFormat(0),
        fFlags(0),
        fNumBlocks(0),
        fDataOffset(0),
        fDataLen(0),
        fCmtOffset(0),
        fCmtLen(0),
        fCreatorOffset(0),
        fCreatorLen(0),
        fSpare(),

        fDOSVolumeNum(-1),
        fMagicStr(),
        fCreatorStr(),
        fComment(NULL),
        fCreatorChunk(NULL)
    {}
    virtual ~TwoImgHeader(void) {
        delete[] fComment;
        delete[] fCreatorChunk;
    }

    /*
     * Header fields.
     */
    //char            fMagic[4];
    //char            fCreator[4];
    uint32_t    fMagic;
    uint32_t    fCreator;
    uint16_t    fHeaderLen;
    uint16_t    fVersion;
    uint32_t    fImageFormat;
    uint32_t    fFlags;         // may include DOS volume num
    uint32_t    fNumBlocks;     // 512-byte blocks
    uint32_t    fDataOffset;
    uint32_t    fDataLen;
    uint32_t    fCmtOffset;
    uint32_t    fCmtLen;
    uint32_t    fCreatorOffset;
    uint32_t    fCreatorLen;
    uint32_t    fSpare[4];

    /*
     * Related constants.
     */
    enum {
        // imageFormat
        kImageFormatDOS     = 0,
        kImageFormatProDOS  = 1,
        kImageFormatNibble  = 2,
        // flags
        kFlagLocked         = (1L<<31),
        kDOSVolumeSet       = (1L<<8),
        kDOSVolumeMask      = (0xff),
        kDefaultVolumeNum   = 254,

        // constants used when creating a new header
        kOurHeaderLen       = 64,
        kOurVersion         = 1,

        kBlockSize          = 512,
        kMagic              = 0x32494d47,       // 2IMG
        kCreatorCiderPress  = 0x43647250,       // CdrP
        kCreatorSweet16     = 0x574f4f46,       // WOOF
    };

    /*
     * Basic functions.
     *
     * The read header function will read the comment, but the write
     * header function will not.  This is because the GenericFD functions
     * don't allow seeking past the current EOF.
     *
     * ReadHeader/WriteHeader expect the file to be seeked to the initial
     * offset.  WriteFooter expects the file to be seeked just past the
     * end of the data section.  This is done in case the file has some
     * sort of wrapper outside the 2MG header.
     */
    int InitHeader(int imageFormat, uint32_t imageSize, uint32_t imageBlockCount);
    int ReadHeader(FILE* fp, uint32_t totalLength);
    int ReadHeader(GenericFD* pGFD, uint32_t totalLength);
    int WriteHeader(FILE* fp) const;
    int WriteHeader(GenericFD* pGFD) const;
    int WriteFooter(FILE* fp) const;
    int WriteFooter(GenericFD* pGFD) const;
    void DumpHeader(void) const;        // for debugging

    /*
     * Getters & setters.
     */
    const char* GetMagicStr(void) const { return fMagicStr; }
    const char* GetCreatorStr(void) const { return fCreatorStr; }

    int16_t GetDOSVolumeNum(void) const;
    void SetDOSVolumeNum(short dosVolumeNum);
    const char* GetComment(void) const { return fComment; }
    void SetComment(const char* comment);
    const void* GetCreatorChunk(void) const { return fCreatorChunk; }
    void SetCreatorChunk(const void* creatorBlock, long len);

private:
    int UnpackHeader(const uint8_t* buf, uint32_t totalLength);
    void PackHeader(uint8_t* buf) const;
    int GetChunk(GenericFD* pGFD, di_off_t relOffset, long len, void** pBuf);
    int GetChunk(FILE* fp, di_off_t relOffset, long len, void** pBuf);

    int16_t fDOSVolumeNum;      // 8-bit volume number, or -1
    char    fMagicStr[5];
    char    fCreatorStr[5];

    char*   fComment;
    char*   fCreatorChunk;
};

}   // namespace DiskImgLib

#endif /*DISKIMG_TWOIMG_H*/
