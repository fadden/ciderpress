/*
 * CiderPress
 * Copyright (C) 2007 by faddenSoft, LLC.  All Rights Reserved.
 * See the file LICENSE for distribution terms.
 */
/*
 * Functions to manage the "new disk size" radio button set in dialogs.
 */
#ifndef __NEWDISKSIZE__
#define __NEWDISKSIZE__

/*
 * All members are static.  Don't instantiate the class.
 */
class NewDiskSize {
public:
    NewDiskSize(void) { ASSERT(false); }

    static unsigned int GetNumSizeEntries(void);
    static long GetDiskSizeByIndex(int idx);
    enum { kSpecified = -1 };

    static void EnableButtons(CDialog* pDialog, BOOL state = true);
    static void EnableButtons_ProDOS(CDialog* pDialog, long totalBlocks,
        long blocksUsed);
    static long GetNumBitmapBlocks_ProDOS(long totalBlocks);
    static void UpdateSpecifyEdit(CDialog* pDialog);

private:
    typedef struct {
        int     ctrlID;
        long    blocks;
    } RadioCtrlMap;

    static const RadioCtrlMap kCtrlMap[];
};

#endif /*__NEWDISKSIZE__*/