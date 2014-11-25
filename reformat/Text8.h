/*
 * CiderPress
 * Copyright (C) 2009 by Ciderpress authors.  All Rights Reserved.
 * Copyright (C) 2007 by faddenSoft, LLC.  All Rights Reserved.
 * See the file LICENSE for distribution terms.
 */
/*
 * Reformat 8-bit word processor files.
 */
#ifndef REFORMAT_TEXT8_H
#define REFORMAT_TEXT8_H

#include "ReformatBase.h"

/*
 * Magic Window / Magic Window II
 */
class ReformatMagicWindow : public ReformatText {
public:
    ReformatMagicWindow(void) {}
    virtual ~ReformatMagicWindow(void) {}

    virtual void Examine(ReformatHolder* pHolder) override;
    virtual int Process(const ReformatHolder* pHolder,
        ReformatHolder::ReformatID id, ReformatHolder::ReformatPart part,
        ReformatOutput* pOutput) override;

private:
    static bool IsFormatted(const ReformatHolder* pHolder);

    enum { kHeaderLen = 256 };
};

/*
 * Gutenberg Word Processor
 */
class ReformatGutenberg : public ReformatText {
public:
    ReformatGutenberg(void) {}
    virtual ~ReformatGutenberg(void) {}

    virtual void Examine(ReformatHolder* pHolder) override;
    virtual int Process(const ReformatHolder* pHolder,
        ReformatHolder::ReformatID id, ReformatHolder::ReformatPart part,
        ReformatOutput* pOutput) override;

};

#endif /*REFORMAT_TEXT8_H*/
