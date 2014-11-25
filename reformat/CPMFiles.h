/*
 * CiderPress
 * Copyright (C) 2007 by faddenSoft, LLC.  All Rights Reserved.
 * See the file LICENSE for distribution terms.
 */
/*
 * Reformat CP/M files.
 */
#ifndef REFORMAT_CPMFILES_H
#define REFORMAT_CPMFILES_H

#include "ReformatBase.h"

/*
 * Reformat CP/M text.
 */
class ReformatCPMText : public ReformatText {
public:
    ReformatCPMText(void) {}
    virtual ~ReformatCPMText(void) {}

    virtual void Examine(ReformatHolder* pHolder) override;
    virtual int Process(const ReformatHolder* pHolder,
        ReformatHolder::ReformatID id, ReformatHolder::ReformatPart part,
        ReformatOutput* pOutput) override;
};

#endif /*REFORMAT_CPMFILES_H*/
