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

/*
 * Reformat a generic IIgs text file.
 */
class ReformatGWP : public ReformatText {
public:
    ReformatGWP(void) {}
    virtual ~ReformatGWP(void) {}

    virtual void Examine(ReformatHolder* pHolder);
    virtual int Process(const ReformatHolder* pHolder,
        ReformatHolder::ReformatID id, ReformatHolder::ReformatPart part,
        ReformatOutput* pOutput);
};

/*
 * Reformat a Teach file
 */
class ReformatTeach : public ReformatText {
public:
    ReformatTeach(void) {}
    virtual ~ReformatTeach(void) {}

    virtual void Examine(ReformatHolder* pHolder);
    virtual int Process(const ReformatHolder* pHolder,
        ReformatHolder::ReformatID id, ReformatHolder::ReformatPart part,
        ReformatOutput* pOutput);

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
    bool Create(const unsigned char* buf, long len);

    /*
     * Class representing a IIgs TERuler object.
     */
    class TERuler {
    public:
        TERuler(void) {}
        virtual ~TERuler(void) {}

        /* unpack the ruler; returns the #of bytes consumed */
        int Create(const unsigned char* buf, long len);

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
            unsigned short  tabKind;    // must be $0000
            unsigned short  tabData;    // tab location, in pixels from left
        } TabItem;

        unsigned short  fLeftMargin;    // pixel indent, except para start
        unsigned short  fLeftIndent;    // pixel indent, for para start
        unsigned short  fRightMargin;   // maximum line len, in pixels
        unsigned short  fJust;          // enum Justification
        unsigned short  fExtraLS;       // extra line spacing, in pixels
        unsigned short  fFlags;         // reserved
        unsigned long   fUserData;
        unsigned short  fTabType;       // 0=none, 1=interval, 2=irregular
        // array of TabItem appears here for fTabType==2
        unsigned short  fTabTerminator; // present for fTabType==1 or 2
    };

    /*
     * Class representing a IIgs TEStyle object.
     */
    class TEStyle {
    public:
        TEStyle(void) {}
        virtual ~TEStyle(void) {}

        /* unpack the style; returns the #of bytes consumed */
        void Create(const unsigned char* buf);

        /* Apple IIgs font family number */
        unsigned short GetFontFamily(void) const {
            return (unsigned short) fFontID;
        }
        /* font size, in points */
        int GetFontSize(void) const {
            return (fFontID >> 24) & 0xff;
        }
        /* return QDII text style */
        unsigned char GetTextStyle(void) const {
            return (unsigned char) ((fFontID >> 16) & 0xff);
        }
        /* return QDII text color */
        unsigned short GetTextColor(void) const { return fForeColor; }

        /* individual text style getters */
        //bool IsBold(void) const { return (GetTextStyle() & kBold) != 0; }
        //bool IsItalic(void) const { return (GetTextStyle() & kItalic) != 0; }
        //bool IsUnderline(void) const { return (GetTextStyle() & kUnderline) != 0; }

        enum { kDataLen = 12 };

    private:
        unsigned long   fFontID;
        unsigned short  fForeColor;
        unsigned short  fBackColor;
        unsigned long   fUserData;
    };

    /*
     * Class representing a IIgs StyleItem object.
     */
    class StyleItem {
    public:
        StyleItem(void) {}
        virtual ~StyleItem(void) {}

        /* unpack the item */
        void Create(const unsigned char* buf);

        unsigned long GetLength(void) const { return fLength; }
        unsigned long GetOffset(void) const { return fOffset; }
        int GetStyleIndex(void) const {
            /* MSVC++6.0 won't let me use TEStyle::kDataLen here */
            return fOffset / 12;
        }

        enum {
            kDataLen = 8,
            kUnusedItem = 0xffffffff,   // in fLength field
        };

    private:
        unsigned long   fLength;    // #of characters affected by this style
        unsigned long   fOffset;    // offset in bytes into TEStyle list
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
