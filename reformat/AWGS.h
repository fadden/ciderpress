/*
 * CiderPress
 * Copyright (C) 2007 by faddenSoft, LLC.  All Rights Reserved.
 * See the file LICENSE for distribution terms.
 */
/*
 * AWGS file handling.
 */
#ifndef REFORMAT_AWGS_H
#define REFORMAT_AWGS_H

#include "ReformatBase.h"

/*
 * Reformat an AppleWorks GS Word Processor file.
 */
class ReformatAWGS_WP : public ReformatText {
public:
    ReformatAWGS_WP(void) {}
    virtual ~ReformatAWGS_WP(void) {}

    virtual void Examine(ReformatHolder* pHolder) override;
    virtual int Process(const ReformatHolder* pHolder,
        ReformatHolder::ReformatID id, ReformatHolder::ReformatPart part,
        ReformatOutput* pOutput) override;

private:
    /*
     * Definition of a "SaveArray" entry.
     */
    typedef struct SaveArrayEntry {
        uint16_t    textBlock;      // Text Block number
        uint16_t    offset;         // add to offset of text block
        uint16_t    attributes;     // 0=normal text, 1=page break para
        uint16_t    rulerNum;       // number of ruler for this para
        uint16_t    pixelHeight;    // height of para in pixels
        uint16_t    numLines;       // number of lines in this para
    } SaveArrayEntry;

    /*
     * Every document has three chunks: header, footer, main document.  Each
     * has the same general structure.
     */
    typedef struct Chunk {
        const uint8_t*  saveArray;
        const uint8_t*  rulers;
        const uint8_t*  textBlocks;
        uint16_t    saveArrayCount;
        uint16_t    numRulers;
        uint16_t    numTextBlocks;
    } Chunk;

    enum {
        kExpectedVersion1   = 0x1011,   // 1.0v2 and 1.1
        kExpectedVersion2   = 0x0006,   // 1.0? beta?
        kExpectedIntVersion = 0x0002,   // internal version number
        kDocHeaderLen       = 282,
        kWPGlobalsLen       = 386,
        kMinExpectedLen     = kWPGlobalsLen + kDocHeaderLen + 2 +1,
        kSaveArrayEntryLen  = 12,
        kRulerEntryLen      = 52,
        kMinTextBlockSize   = 4,        // two counts
    };

    /* AWGS ruler "statusBits" values */
    enum {
        kAWGSJustifyFull        = 0x80,
        kAWGSJustifyRight       = 0x40,
        kAWGSJustifyCenter      = 0x20,
        kAWGSJustifyLeft        = 0x10,
        kAWGSNoBreakPara        = 0x08,     // para does not span pages
        kAWGSTripleSpace        = 0x04,     // really double
        kAWGSDoubleSpace        = 0x02,     // really 1.5x
        kAWGSSingleSpace        = 0x01
    };

    bool ReadChunk(const uint8_t** pSrcBuf, long* pSrcLen,
        Chunk* pChunk);
    void PrintChunk(const Chunk* pChunk);
    const uint8_t* FindTextBlock(const Chunk* pChunk, int blockNum);
    int PrintParagraph(const uint8_t* ptr, long maxLen);
    uint16_t GetNumRulers(const uint8_t* pSaveArray,
        uint16_t saveArrayCount);
    uint16_t GetNumTextBlocks(const uint8_t* pSaveArray,
        uint16_t saveArrayCount);
    void UnpackSaveArrayEntry(const uint8_t* pSaveArray,
        SaveArrayEntry* pSAE);
    bool SkipTextBlocks(const uint8_t** pSrcBuf,
        long* pSrcLen, int blockCount);
};

#endif /*REFORMAT_AWGS_H*/
