/*
 * CiderPress
 * Copyright (C) 2007 by faddenSoft, LLC.  All Rights Reserved.
 * See the file LICENSE for distribution terms.
 */
/*
 * Fill out a CTreeCtrl with the results of a tree search through a DiskFS and
 * its sub-volumes.
 */
#ifndef __DISKFSTREE__
#define __DISKFSTREE__

#include "resource.h"
#include "../diskimg/DiskImg.h"

/*
 * This class could probably be part of DiskArchive, but things are pretty
 * cluttered up there already.
 */
class DiskFSTree {
public:
    DiskFSTree(void) {
        fIncludeSubdirs = false;
        fExpandDepth = 0;

        fpDiskFS = nil;
        fpTargetData = nil;
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
    typedef struct TargetData {
        TargetKind          kind;
        bool                selectable;
        DiskImgLib::DiskFS* pDiskFS;
        DiskImgLib::A2File* pFile;

        // easier to keep a list than to chase through the tree
        struct TargetData*  pNext;
    } TargetData;

private:
    bool AddDiskFS(CTreeCtrl* pTree, HTREEITEM root,
        DiskImgLib::DiskFS* pDiskFS, int depth);
    DiskImgLib::A2File* AddSubdir(CTreeCtrl* pTree, HTREEITEM parent,
        DiskImgLib::DiskFS* pDiskFS, DiskImgLib::A2File* pFile,
        int depth);
    TargetData* AllocTargetData(void);
    void FreeAllTargetData(void);

    void LoadTreeImages(void) {
        if (!fTreeImageList.Create(IDB_TREE_PICS, 16, 1, CLR_DEFAULT))
            WMSG0("GLITCH: list image create failed\n");
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
};

#endif /*__DISKFSTREE__*/
