/*
 * CiderPress
 * Copyright (C) 2007 by faddenSoft, LLC.  All Rights Reserved.
 * See the file LICENSE for distribution terms.
 */
/*
 * Convert Print Shop graphics.
 */
#ifndef REFORMAT_PRINTSHOP_H
#define REFORMAT_PRINTSHOP_H

#include "ReformatBase.h"

/*
 * Reformat Print Shop clip art.  There are actually two kinds, "classic"
 * (88x52 B&W) and "GS" (88x52 3-bit color).  This handles both.
 */
class ReformatPrintShop : public ReformatGraphics {
public:
    ReformatPrintShop(void) {}
    virtual ~ReformatPrintShop(void) {}

    virtual void Examine(ReformatHolder* pHolder) override;
    virtual int Process(const ReformatHolder* pHolder,
        ReformatHolder::ReformatID id, ReformatHolder::ReformatPart part,
        ReformatOutput* pOutput) override;

private:
    enum { kWidth=88, kHeight=52 };

    MyDIBitmap* ConvertBW(const uint8_t* srcBuf);
    MyDIBitmap* ConvertColor(const uint8_t* srcBuf);
};

#endif /*REFORMAT_PRINTSHOP_H*/
