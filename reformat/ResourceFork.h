/*
 * CiderPress
 * Copyright (C) 2007 by faddenSoft, LLC.  All Rights Reserved.
 * See the file LICENSE for distribution terms.
 */
/*
 * Reformat a resource fork.
 */
#ifndef REFORMAT_RESOURCEFORK_H
#define REFORMAT_RESOURCEFORK_H

#include "ReformatBase.h"

/*
 * Currently pretty simple.
 */
class ReformatResourceFork : public ReformatText {
public:
    ReformatResourceFork(void) {}
    virtual ~ReformatResourceFork(void) {}

    virtual void Examine(ReformatHolder* pHolder) override;
    virtual int Process(const ReformatHolder* pHolder,
        ReformatHolder::ReformatID id, ReformatHolder::ReformatPart part,
        ReformatOutput* pOutput) override;

    static bool GetResource(const uint8_t* srcBuf, long srcLen,
        uint16_t resourceType, uint32_t resourceID,
        const uint8_t** pResource, long* pResourceLen);

private:
    enum { kRsrcMapEntryLen = 0x14 };

    static bool ReadHeader(const uint8_t* srcBuf, long srcLen,
        long* pFileVersion, long* pFileToMap, long* pFileMapSize,
        bool* pLittleEndian);
};

#endif /*REFORMAT_RESOURCEFORK_H*/
