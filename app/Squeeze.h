/*
 * CiderPress
 * Copyright (C) 2007 by faddenSoft, LLC.  All Rights Reserved.
 * See the file LICENSE for distribution terms.
 */
/*
 * Implementation of SQueeze compression.
 */
#ifndef APP_SQUEEZE_H
#define APP_SQUEEZE_H

NuError UnSqueeze(FILE* fp, unsigned long realEOF, ExpandBuffer* outExp,
    bool fullSqHeader, int blockSize);

#endif /*APP_SQUEEZE_H*/
