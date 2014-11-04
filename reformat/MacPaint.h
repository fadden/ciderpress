/*
 * CiderPress
 * Copyright (C) 2007 by faddenSoft, LLC.  All Rights Reserved.
 * See the file LICENSE for distribution terms.
 */
/*
 * Convert MacPaint files.
 */
#ifndef __LR_MACPAINT__
#define __LR_MACPAINT__

#include "ReformatBase.h"

/*
 * Reformat a B&W MacPaint image.
 */
class ReformatMacPaint : public ReformatGraphics {
public:
    ReformatMacPaint(void) {}
    virtual ~ReformatMacPaint(void) {}

    virtual void Examine(ReformatHolder* pHolder);
    virtual int Process(const ReformatHolder* pHolder,
        ReformatHolder::ReformatID id, ReformatHolder::ReformatPart part,
        ReformatOutput* pOutput);

private:
    MyDIBitmap* ConvertMacPaint(const unsigned char* srcBuf, long length);

    enum {
        kLeadingJunkCount = 512,
        kOutputWidth = 576,
        kOutputHeight = 720,
        kNumColors = 2,
        kMinSize = kLeadingJunkCount + 2*kOutputHeight,
        // max size is 53072, not including MacBinary header
        kMaxSize = 128 + kLeadingJunkCount + kOutputHeight*((kOutputWidth/8)+1),
    };
};

#endif /*__LR_MACPAINT__*/
