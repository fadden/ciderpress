/*
 * CiderPress
 * Copyright (C) 2007 by faddenSoft, LLC.  All Rights Reserved.
 * See the file LICENSE for distribution terms.
 */
/*
 * Functions to manage the "new disk size" radio button set in dialogs.
 */
#ifndef APP_NEWDISKSIZE_H
#define APP_NEWDISKSIZE_H

/*
 * All members are static.  Don't instantiate the class.
 */
class NewDiskSize {
public:
    NewDiskSize(void) { ASSERT(false); }

    /*
     * Return the #of entries in the table.
     */
    static unsigned int GetNumSizeEntries(void);

    /*
     * Return the "size" field from an array entry.
     */
    static long GetDiskSizeByIndex(int idx);

    enum { kSpecified = -1 };

    static void EnableButtons(CDialog* pDialog, BOOL state = true);

    /*
     * Run through the set of radio buttons, disabling any that don't have enough
     * space to hold the ProDOS volume with the specified parameters.
     *
     * The space required is equal to the blocks required for data plus the blocks
     * required for the free-space bitmap.  Since the free-space bitmap size is
     * smaller for smaller volumes, we have to adjust it for each.
     *
     * Pass in the total blocks and #of blocks used on a particular ProDOS volume.
     * This will compute how much space would be required for larger and smaller
     * volumes, and enable or disable radio buttons as appropriate.  (You can get
     * these values from DiskFS::GetFreeBlockCount()).
     */
    static void EnableButtons_ProDOS(CDialog* pDialog, long totalBlocks,
        long blocksUsed);

    /*
     * Compute the #of blocks needed to hold the ProDOS block bitmap.
     */
    static long GetNumBitmapBlocks_ProDOS(long totalBlocks);

    /*
     * Update the "specify size" edit box.
     */
    static void UpdateSpecifyEdit(CDialog* pDialog);

private:
    typedef struct {
        int     ctrlID;
        long    blocks;
    } RadioCtrlMap;

    static const RadioCtrlMap kCtrlMap[];
};

#endif /*APP_NEWDISKSIZE_H*/
