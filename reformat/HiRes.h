/*
 * CiderPress
 * Copyright (C) 2007 by faddenSoft, LLC.  All Rights Reserved.
 * See the file LICENSE for distribution terms.
 */
/*
 * Reformat hi-res images.
 */
#ifndef __LR_HIRES__
#define __LR_HIRES__

#include "ReformatBase.h"

/*
 * Reformat a HiRes graphic into a bitmap.
 */
class ReformatHiRes : public ReformatGraphics {
public:
	ReformatHiRes(void) { fBlackWhite = false; }
	virtual ~ReformatHiRes(void) {}

	virtual void Examine(ReformatHolder* pHolder);
	virtual int Process(const ReformatHolder* pHolder,
		ReformatHolder::ReformatID id, ReformatHolder::ReformatPart part,
		ReformatOutput* pOutput);

	enum {
		kPixelsPerLine = 280,
		kNumLines = 192,

		kOutputWidth = 560,
		kOutputHeight = 384,
		kExpectedSize = 8192,
	};


	static void InitLineOffset(int* pOffsetBuf);
	MyDIBitmap* HiResScreenToBitmap(const unsigned char* buf);

	int		fLineOffset[kNumLines];
	bool	fBlackWhite;
};

#endif /*__LR_HIRES__*/
