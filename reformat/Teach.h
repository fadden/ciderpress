/*
 * CiderPress
 * Copyright (C) 2007 by faddenSoft, LLC.  All Rights Reserved.
 * See the file LICENSE for distribution terms.
 */
/*
 * Teach Text handling.
 */
#ifndef REFORMAT_TEACH_H
#define REFORMAT_TEACH_H

#include "ReformatBase.h"
#include <assert.h>

/*
 * Reformat a generic IIgs text file.
 */
class ReformatGWP : public ReformatText {
public:
    ReformatGWP(void) {}
    virtual ~ReformatGWP(void) {}

    virtual void Examine(ReformatHolder* pHolder) override;
    virtual int Process(const ReformatHolder* pHolder,
        ReformatHolder::ReformatID id, ReformatHolder::ReformatPart part,
        ReformatOutput* pOutput) override;
};

/*
 * Reformat a Teach file
 */
class ReformatTeach : public ReformatText {
public:
    ReformatTeach(void) {}
    virtual ~ReformatTeach(void) {}

    virtual void Examine(ReformatHolder* pHolder) override;
    virtual int Process(const ReformatHolder* pHolder,
        ReformatHolder::ReformatID id, ReformatHolder::ReformatPart part,
        ReformatOutput* pOutput) override;

private:
};

/*
 * Class to hold the style block.
 *
 * The definition allows for an array of rulers, but in practice there
 * is no way to actually use more than one.  Because rulers are
 * variable-length, we take the easy road here and assume there's just
 * one.  To be safe, the interface pretends there could be more than one.
 */
class RStyleBlock {
public:
    RStyleBlock(void) : fpRulers(NULL), fpStyles(NULL), fpStyleItems(NULL) {}
    virtual ~RStyleBlock(void) {
            delete[] fpRulers;
            delete[] fpStyles;
            delete[] fpStyleItems;
        }

    /* unpack the resource; returns false on failure */
    bool Create(const uint8_t* buf, long len);

    /*
     * Class representing a IIgs TERuler object.
     */
    class TERuler {
    public:
        TERuler(void) {}
        virtual ~TERuler(void) {}

        /* unpack the ruler; returns the #of bytes consumed */
        int Create(const uint8_t* buf, long len);

        typedef enum Justification {
            kJustLeft   = 0x0000,
            kJustRight  = 0xffff,
            kJustCenter = 0x0001,
            kJustFull   = 0x0002
        } Justification;

        Justification GetJustification(void) const {
            return (Justification) fJust;
        }

    private:
        enum {
            kMinLen = 0x12,
            kTabArrayEnd = 0xffff,
        };

        typedef struct TabItem {
            uint16_t    tabKind;    // must be $0000
            uint16_t    tabData;    // tab location, in pixels from left
        } TabItem;

        uint16_t    fLeftMargin;    // pixel indent, except para start
        uint16_t    fLeftIndent;    // pixel indent, for para start
        uint16_t    fRightMargin;   // maximum line len, in pixels
        uint16_t    fJust;          // enum Justification
        uint16_t    fExtraLS;       // extra line spacing, in pixels
        uint16_t    fFlags;         // reserved
        uint32_t    fUserData;
        uint16_t    fTabType;       // 0=none, 1=interval, 2=irregular
        // array of TabItem appears here for fTabType==2
        uint16_t    fTabTerminator; // present for fTabType==1 or 2
    };

    /*
     * Class representing a IIgs TEStyle object.
     */
    class TEStyle {
    public:
        TEStyle(void) {}
        virtual ~TEStyle(void) {}

        /* unpack the style; returns the #of bytes consumed */
        void Create(const uint8_t* buf);

        /* Apple IIgs font family number */
        uint16_t GetFontFamily(void) const {
            return (uint16_t) fFontID;
        }
        /* font size, in points */
        uint8_t GetFontSize(void) const {
            return (fFontID >> 24) & 0xff;
        }
        /* return QDII text style */
        uint8_t GetTextStyle(void) const {
            return (fFontID >> 16) & 0xff;
        }
        /* return QDII text color */
        uint16_t GetTextColor(void) const { return fForeColor; }

        /* individual text style getters */
        //bool IsBold(void) const { return (GetTextStyle() & kBold) != 0; }
        //bool IsItalic(void) const { return (GetTextStyle() & kItalic) != 0; }
        //bool IsUnderline(void) const { return (GetTextStyle() & kUnderline) != 0; }

        enum { kDataLen = 12 };

    private:
        /* font ID has family, size, and style */
        uint32_t    fFontID;
        uint16_t    fForeColor;
        uint16_t    fBackColor;
        uint32_t    fUserData;
    };

    /*
     * Class representing a IIgs StyleItem object.
     */
    class StyleItem {
    public:
        StyleItem(void) {}
        virtual ~StyleItem(void) {}

        /* unpack the item */
        void Create(const uint8_t* buf);

        uint32_t GetLength(void) const { return fLength; }
        uint32_t GetOffset(void) const { return fOffset; }
        uint32_t GetStyleIndex(void) const {
            return fOffset / TEStyle::kDataLen;
        }

        enum {
            kItemDataLen = 8,
            kUnusedItem = 0xffffffff,   // in fLength field
        };

    private:
        uint32_t    fLength;    // #of characters affected by this style
        uint32_t    fOffset;    // offset in bytes into TEStyle list
    };

    enum {
        kExpectedVersion = 0,
        kMinLen = 4*3 +2,
    };


    /*
     * More RStyleBlock declarations.
     */
    TERuler* GetRuler(int idx) {
        assert(idx >= 0 && idx < fNumRulers);
        return &fpRulers[idx];
    }
    TEStyle* GetStyle(int idx) {
        assert(idx >= 0 && idx < fNumStyles);
        return &fpStyles[idx];
    }
    int GetNumStyleItems(void) const { return fNumStyleItems; }
    StyleItem* GetStyleItem(int idx) {
        assert(idx >= 0 && idx < fNumStyleItems);
        return &fpStyleItems[idx];
    }

private:
    int     fNumRulers;
    int     fNumStyles;
    int     fNumStyleItems;

    TERuler*    fpRulers;
    TEStyle*    fpStyles;
    StyleItem*  fpStyleItems;
};

#endif /*REFORMAT_TEACH_H*/
