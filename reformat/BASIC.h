/*
 * CiderPress
 * Copyright (C) 2007 by faddenSoft, LLC.  All Rights Reserved.
 * See the file LICENSE for distribution terms.
 */
/*
 * Reformat BASIC programs.
 */
#ifndef __LR_BASIC__
#define __LR_BASIC__

#include "ReformatBase.h"

/*
 * Reformat an Applesoft BASIC program into readable text.
 */
class ReformatApplesoft : public ReformatText {
public:
	ReformatApplesoft(void) {}
	virtual ~ReformatApplesoft(void) {}

	virtual void Examine(ReformatHolder* pHolder);
	virtual int Process(const ReformatHolder* pHolder,
		ReformatHolder::ReformatID id, ReformatHolder::ReformatPart part,
		ReformatOutput* pOutput);

	/* share our token list with others */
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

	virtual void Examine(ReformatHolder* pHolder);
	virtual int Process(const ReformatHolder* pHolder,
		ReformatHolder::ReformatID id, ReformatHolder::ReformatPart part,
		ReformatOutput* pOutput);
};

#endif /*__LR_BASIC__*/