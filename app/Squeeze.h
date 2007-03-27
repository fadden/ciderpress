/*
 * CiderPress
 * Copyright (C) 2007 by faddenSoft, LLC.  All Rights Reserved.
 * See the file LICENSE for distribution terms.
 */
/*
 * Implementation of SQueeze compression.
 */
#ifndef __SQUEEZE__
#define __SQUEEZE__

NuError UnSqueeze(FILE* fp, unsigned long realEOF, ExpandBuffer* outExp,
	bool fullSqHeader, int blockSize);

#endif /*__SQUEEZE__*/
