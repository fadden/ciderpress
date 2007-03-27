/*
 * CiderPress
 * Copyright (C) 2007 by faddenSoft, LLC.  All Rights Reserved.
 * See the file LICENSE for distribution terms.
 */
/*
 * Reformat subdirectories.
 */
#ifndef __LR_DIRECTORY__
#define __LR_DIRECTORY__

#include "ReformatBase.h"

/*
 * Reformat a ProDOS directory.
 */
class ReformatDirectory : public ReformatText {
public:
	ReformatDirectory(void) {}
	virtual ~ReformatDirectory(void) {}

	virtual void Examine(ReformatHolder* pHolder);
	virtual int Process(const ReformatHolder* pHolder,
		ReformatHolder::ReformatID id, ReformatHolder::ReformatPart part,
		ReformatOutput* pOutput);

private:
	void PrintDirEntries(const unsigned char* srcBuf,
		long srcLen, bool showDeleted);
};

#endif /*__LR_DIRECTORY__*/
