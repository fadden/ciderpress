/*
 * CiderPress
 * Copyright (C) 2009 by Ciderpress authors.  All Rights Reserved.
 * Copyright (C) 2007 by faddenSoft, LLC.  All Rights Reserved.
 * See the file LICENSE for distribution terms.
 */
/*
 * Reformat 8-bit word processor files.
 */
#ifndef __LR_TEXT8__
#define __LR_TEXT8__

#include "ReformatBase.h"

/*
 * Magic Window / Magic Window II
 */
class ReformatMagicWindow : public ReformatText {
public:
	ReformatMagicWindow(void) {}
	virtual ~ReformatMagicWindow(void) {}

	virtual void Examine(ReformatHolder* pHolder);
	virtual int Process(const ReformatHolder* pHolder,
		ReformatHolder::ReformatID id, ReformatHolder::ReformatPart part,
		ReformatOutput* pOutput);

private:
	static bool IsFormatted(const ReformatHolder* pHolder);

	enum { kHeaderLen = 256 };
};

/*
 * Guterberg Word Processor
 */
class ReformatGutenberg : public ReformatText {
public:
	ReformatGutenberg(void) {}
	virtual ~ReformatGutenberg(void) {}

	virtual void Examine(ReformatHolder* pHolder);
	virtual int Process(const ReformatHolder* pHolder,
		ReformatHolder::ReformatID id, ReformatHolder::ReformatPart part,
		ReformatOutput* pOutput);

};

#endif /*__LR_TEXT8__*/
