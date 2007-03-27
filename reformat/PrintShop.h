/*
 * CiderPress
 * Copyright (C) 2007 by faddenSoft, LLC.  All Rights Reserved.
 * See the file LICENSE for distribution terms.
 */
/*
 * Convert Print Shop graphics.
 */
#ifndef __LR_PRINTSHOP__
#define __LR_PRINTSHOP__

#include "ReformatBase.h"

/*
 * Reformat Print Shop clip art.  There are actually two kinds, "classic"
 * (88x52 B&W) and "GS" (88x52 3-bit color).  This handles both.
 */
class ReformatPrintShop : public ReformatGraphics {
public:
	ReformatPrintShop(void) {}
	virtual ~ReformatPrintShop(void) {}

	virtual void Examine(ReformatHolder* pHolder);
	virtual int Process(const ReformatHolder* pHolder,
		ReformatHolder::ReformatID id, ReformatHolder::ReformatPart part,
		ReformatOutput* pOutput);

private:
	enum { kWidth=88, kHeight=52 };

	MyDIBitmap* ConvertBW(const unsigned char* srcBuf);
	MyDIBitmap* ConvertColor(const unsigned char* srcBuf);
};

#endif /*__LR_PRINTSHOP__*/
