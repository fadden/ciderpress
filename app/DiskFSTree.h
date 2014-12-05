/*
 * CiderPress
 * Copyright (C) 2007 by faddenSoft, LLC.  All Rights Reserved.
 * See the file LICENSE for distribution terms.
 */
/*
 * Fill out a CTreeCtrl with the results of a tree search through a DiskFS and
 * its sub-volumes.
 */
#ifndef APP_DISKFSTREE_H
#define APP_DISKFSTREE_H

#include "resource.h"
#include "../diskimg/DiskImg.h"

/*
 * Utility class for extracting a directory hierarchy from a DiskFS and
 * adding it to a CTreeCtrl.
 *
 * The storage for some of the strings provided to the tree control is
 * managed by this class, so delete this object after the CTreeCtrl is
 * deleted.  (Generally, this should be paired with a CTreeCtrl in a dialog
 * object.)
 */
class DiskFSTree {
public:
    DiskFSTree(void) {
        fIncludeSubdirs = false;
        fExpandDepth = 0;

        fpDiskFS = NULL;
        fpTargetData = NULL;
        LoadTreeImages();
    }
    virtual ~DiskFSTree(void) { FreeAllTargetData(); }

    /*
     * Create the contents of the tree control.
     */
    bool BuildTree(DiskImgLib::DiskFS* pDiskFS, CTreeCtrl* pTree);

    /* if set, includes folders as well as disks */
    bool    fIncludeSubdirs;

    /* start with the tree expanded to this depth (0=none, -1=all) */
    int     fExpandDepth;

    typedef enum {
        kTargetUnknown = 0, kTargetDiskFS, kTargetSubdir
    } TargetKind;
    struct TargetData {
        TargetData()
            : kind(kTargetUnknown), selectable(false), pDiskFS(NULL),
              pFile(NULL), pNext(NULL)
            {}
        TargetKind          kind;
        bool                selectable;
        DiskImgLib::DiskFS* pDiskFS;
        DiskImgLib::A2File* pFile;

        // easier to keep a list than to chase through the tree
        struct TargetData*  pNext;
    };

private:
    /*
     * Load the specified DiskFS into the tree, recursively adding any
     * sub-volumes.  Pass in an initial depth of 1.
     *
     * Returns true on success.
     */
    bool AddDiskFS(CTreeCtrl* pTree, HTREEITEM root,
        DiskImgLib::DiskFS* pDiskFS, int depth);

    /*
     * Add the subdir and all of the subdirectories of the current subdir.
     *
     * The files are held in a linear list in the DiskFS, so we have to
     * reconstruct the hierarchy from the path names.  Pass in NULL for the
     * root volume.
     *
     * Returns a pointer to the next A2File in the list (i.e. the first one
     * that we couldn't digest).  This assumes that the contents of a
     * subdirectory are grouped together in the linear list, so that we can
     * immediately bail when the first misfit is encountered.
     */
    DiskImgLib::A2File* AddSubdir(CTreeCtrl* pTree, HTREEITEM parent,
        DiskImgLib::DiskFS* pDiskFS, DiskImgLib::A2File* pFile,
        int depth);

    /*
     * Allocate a new TargetData struct, and add it to our list.
     */
    TargetData* AllocTargetData(void);

    /*
     * Free up the TargetData structures we created.
     */
    void FreeAllTargetData(void);

    /*
     * Load bitmaps used in the tree control.
     */
    void LoadTreeImages(void) {
        if (!fTreeImageList.Create(IDB_TREE_PICS, 16, 1, CLR_DEFAULT)) {
            LOGW("GLITCH: list image create failed");
        }
        fTreeImageList.SetBkColor(::GetSysColor(COLOR_WINDOW));
    }

    enum {  // defs for IDB_TREE_PICS
        kTreeImageFolderClosed = 0,
        kTreeImageFolderOpen = 1,
        kTreeImageHardDriveRW = 2,
        kTreeImageHardDriveRO = 3,
    };
    CImageList      fTreeImageList;

    DiskImgLib::DiskFS* fpDiskFS;
    TargetData*     fpTargetData;

    // Storage for wide strings that were converted from DiskFS narrow strings.
    CStringArray    fStringHolder;
};

#endif /*APP_DISKFSTREE_H*/
