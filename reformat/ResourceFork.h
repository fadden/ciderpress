/*
 * CiderPress
 * Copyright (C) 2007 by faddenSoft, LLC.  All Rights Reserved.
 * See the file LICENSE for distribution terms.
 */
/*
 * Reformat a resource fork.
 */
#ifndef __LR_RESOURCEFORK__
#define __LR_RESOURCEFORK__

#include "ReformatBase.h"

/*
 * Currently pretty simple.
 */
class ReformatResourceFork : public ReformatText {
public:
	ReformatResourceFork(void) {}
	virtual ~ReformatResourceFork(void) {}

	virtual void Examine(ReformatHolder* pHolder);
	virtual int Process(const ReformatHolder* pHolder,
		ReformatHolder::ReformatID id, ReformatHolder::ReformatPart part,
		ReformatOutput* pOutput);

	static bool GetResource(const unsigned char* srcBuf, long srcLen,
		unsigned short resourceType, unsigned long resourceID,
		const unsigned char** pResource, long* pResourceLen);

private:
	enum { kRsrcMapEntryLen = 0x14 };

	static bool ReadHeader(const unsigned char* srcBuf, long srcLen,
		long* pFileVersion, long* pFileToMap, long* pFileMapSize,
		bool* pLittleEndian);
};

#endif /*__LR_RESOURCEFORK__*/