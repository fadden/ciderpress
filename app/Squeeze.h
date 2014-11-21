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

/*
 * Expand "SQ" format.  Archive file should already be seeked.
 *
 * If "outExp" is NULL, no output is produced (useful for "test" mode).
 */
NuError UnSqueeze(FILE* fp, unsigned long realEOF, ExpandBuffer* outExp,
    bool fullSqHeader, int blockSize);

#endif /*APP_SQUEEZE_H*/
