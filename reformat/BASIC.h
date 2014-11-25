/*
 * CiderPress
 * Copyright (C) 2007, 2008 by faddenSoft, LLC.  All Rights Reserved.
 * See the file LICENSE for distribution terms.
 */
/*
 * Reformat BASIC programs.
 */
#ifndef REFORMAT_BASIC_H
#define REFORMAT_BASIC_H

#include "ReformatBase.h"

/*
 * Reformat an Applesoft BASIC program into readable text.
 */
class ReformatApplesoft : public ReformatText {
public:
    ReformatApplesoft(void) {}
    virtual ~ReformatApplesoft(void) {}

    virtual void Examine(ReformatHolder* pHolder) override;
    virtual int Process(const ReformatHolder* pHolder,
        ReformatHolder::ReformatID id, ReformatHolder::ReformatPart part,
        ReformatOutput* pOutput) override;

    /* share our token list with others */
    // TODO: this is a hack; find a better way to do this
    enum { kTokenLen = 8, kTokenCount = 107 };
    static const char* GetApplesoftTokens(void);
};

/*
 * Reformat an Integer BASIC program into readable text.
 */
class ReformatInteger : public ReformatText {
public:
    ReformatInteger(void) {}
    virtual ~ReformatInteger(void) {}

    virtual void Examine(ReformatHolder* pHolder) override;
    virtual int Process(const ReformatHolder* pHolder,
        ReformatHolder::ReformatID id, ReformatHolder::ReformatPart part,
        ReformatOutput* pOutput) override;
};

/*
 * Reformat an Apple /// Business BASIC program into readable text.
 */
class ReformatBusiness : public ReformatText {
public:
    ReformatBusiness(void) {}
    virtual ~ReformatBusiness(void) {}

    virtual void Examine(ReformatHolder* pHolder) override;
    virtual int Process(const ReformatHolder* pHolder,
        ReformatHolder::ReformatID id, ReformatHolder::ReformatPart part,
        ReformatOutput* pOutput) override;
};

#endif /*REFORMAT_BASIC_H*/
