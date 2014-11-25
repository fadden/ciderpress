/*
 * CiderPress
 * Copyright (C) 2007 by faddenSoft, LLC.  All Rights Reserved.
 * See the file LICENSE for distribution terms.
 */
/*
 * Reformat subdirectories.
 */
#ifndef REFORMAT_DIRECTORY_H
#define REFORMAT_DIRECTORY_H

#include "ReformatBase.h"

/*
 * Reformat a ProDOS directory.
 */
class ReformatDirectory : public ReformatText {
public:
    ReformatDirectory(void) {}
    virtual ~ReformatDirectory(void) {}

    virtual void Examine(ReformatHolder* pHolder) override;
    virtual int Process(const ReformatHolder* pHolder,
        ReformatHolder::ReformatID id, ReformatHolder::ReformatPart part,
        ReformatOutput* pOutput) override;

private:
    void PrintDirEntries(const uint8_t* srcBuf,
        long srcLen, bool showDeleted);
};

#endif /*REFORMAT_DIRECTORY_H*/
