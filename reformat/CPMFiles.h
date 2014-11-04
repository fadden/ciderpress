/*
 * CiderPress
 * Copyright (C) 2007 by faddenSoft, LLC.  All Rights Reserved.
 * See the file LICENSE for distribution terms.
 */
/*
 * Reformat CP/M files.
 */
#ifndef __LR_CPMFILES__
#define __LR_CPMFILES__

#include "ReformatBase.h"

/*
 * Reformat CP/M text.
 */
class ReformatCPMText : public ReformatText {
public:
    ReformatCPMText(void) {}
    virtual ~ReformatCPMText(void) {}

    virtual void Examine(ReformatHolder* pHolder);
    virtual int Process(const ReformatHolder* pHolder,
        ReformatHolder::ReformatID id, ReformatHolder::ReformatPart part,
        ReformatOutput* pOutput);
};

#endif /*__LR_CPMFILES__*/
