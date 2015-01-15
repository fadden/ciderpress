/*
 * CiderPress
 * Copyright (C) 2007 by faddenSoft, LLC.  All Rights Reserved.
 * See the file LICENSE for distribution terms.
 */
#include "StdAfx.h"
#include "DiskFSTree.h"
#include "ChooseAddTargetDialog.h"
#include "../reformat/Charset.h"

using namespace DiskImgLib;

bool DiskFSTree::BuildTree(DiskFS* pDiskFS, CTreeCtrl* pTree)
{
    ASSERT(pDiskFS != NULL);
    ASSERT(pTree != NULL);

    pTree->SetImageList(&fTreeImageList, TVSIL_NORMAL);
    return AddDiskFS(pTree, TVI_ROOT, pDiskFS, 1);
}

bool DiskFSTree::AddDiskFS(CTreeCtrl* pTree, HTREEITEM parent,
    DiskImgLib::DiskFS* pDiskFS, int depth)
{
    const DiskFS::SubVolume* pSubVol;
    TargetData* pTarget;
    HTREEITEM hLocalRoot;
    TVITEM tvi;
    TVINSERTSTRUCT tvins;

    /*
     * Insert an entry for the current item.
     *
     * The TVITEM struct wants a pointer to WCHAR* storage for the item
     * text.  The DiskFS only provides narrow chars, so we need to create
     * some local storage for the widened version.  Some calls that take a
     * TVITEM allow the text to be edited, so the field is LPWSTR rather
     * than LPCWSTR, but our uses don't allow editing, so we're okay
     * passing it a pointer to storage inside a CString.
     */
    pTarget = AllocTargetData();
    pTarget->kind = kTargetDiskFS;
    pTarget->pDiskFS = pDiskFS;
    pTarget->pFile = NULL;   // could also use volume dir for ProDOS
    tvi.mask = TVIF_TEXT | TVIF_IMAGE | TVIF_SELECTEDIMAGE | TVIF_PARAM;
    CString volumeId(Charset::ConvertMORToUNI(pDiskFS->GetVolumeID()));
    int index = fStringHolder.Add(volumeId);
    tvi.pszText = (LPWSTR)(LPCWSTR) fStringHolder.GetAt(index);
    tvi.cchTextMax = 0;     // not needed for insertitem
//  tvi.iImage = kTreeImageFolderClosed;
//  tvi.iSelectedImage = kTreeImageFolderOpen;
    if (pDiskFS->GetReadWriteSupported() && !pDiskFS->GetFSDamaged()) {
        tvi.iImage = kTreeImageHardDriveRW;
        pTarget->selectable = true;
    } else {
        tvi.iImage = kTreeImageHardDriveRO;
        pTarget->selectable = false;
    }
    tvi.iSelectedImage = tvi.iImage;
    tvi.lParam = (LPARAM) pTarget;
    tvins.item = tvi;
    tvins.hInsertAfter = parent;
    tvins.hParent = parent;
    hLocalRoot = pTree->InsertItem(&tvins);
    if (hLocalRoot == NULL) {
        LOGW("Tree root InsertItem failed");
        return false;
    }

    /*
     * Scan for and handle all sub-volumes.
     */
    pSubVol = pDiskFS->GetNextSubVolume(NULL);
    while (pSubVol != NULL) {
        if (!AddDiskFS(pTree, hLocalRoot, pSubVol->GetDiskFS(), depth+1))
            return false;

        pSubVol = pDiskFS->GetNextSubVolume(pSubVol);
    }

    /*
     * If this volume has sub-directories, and is read-write, add the subdirs
     * to the tree.
     *
     * We use "depth" rather than "depth+1" because the first subdir entry
     * (the volume dir) doesn't get its own entry.  We use the disk entry
     * to represent the disk's volume dir.
     */
    if (fIncludeSubdirs && pDiskFS->GetReadWriteSupported() &&
        !pDiskFS->GetFSDamaged())
    {
        AddSubdir(pTree, hLocalRoot, pDiskFS, NULL, depth);
    }

    /*
     * If we're above the max expansion depth, expand the node.
     */
    if (fExpandDepth == -1 || depth <= fExpandDepth)
        pTree->Expand(hLocalRoot, TVE_EXPAND);

    /*
     * Finally, if this is the root node, select it.
     */
    if (parent == TVI_ROOT) {
        pTree->Select(hLocalRoot, TVGN_CARET);
    }

    return true;
}

DiskImgLib::A2File* DiskFSTree::AddSubdir(CTreeCtrl* pTree, HTREEITEM parent,
    DiskImgLib::DiskFS* pDiskFS, DiskImgLib::A2File* pParentFile,
    int depth)
{
    A2File* pFile;
    TargetData* pTarget;
    HTREEITEM hLocalRoot;
    TVITEM tvi;
    TVINSERTSTRUCT tvins;

    pFile = pDiskFS->GetNextFile(pParentFile);
    if (pFile == NULL && pParentFile == NULL) {
        /* this can happen on an empty DOS 3.3 disk; under ProDOS, we always
           have the volume entry */
        /* note pFile will be NULL if this happens to be a subdirectory
           positioned as the very last file on the disk */
        return NULL;
    }

    if (pParentFile == NULL) {
        /*
         * This is the root of the disk.  We already have a DiskFS entry for
         * it, so don't add a new tree item here.
         *
         * Check to see if this disk has a volume directory entry.
         */
        if (pFile->IsVolumeDirectory()) {
            pParentFile = pFile;
            pFile = pDiskFS->GetNextFile(pFile);
        }
        hLocalRoot = parent;
    } else {
        /*
         * Add an entry for this subdir (the "parent" entry).
         *
         * This has the same wide-vs-narrow storage issues as AddDiskFS().
         */
        pTarget = AllocTargetData();
        pTarget->kind = kTargetSubdir;
        pTarget->selectable = true;
        pTarget->pDiskFS = pDiskFS;
        pTarget->pFile = pParentFile;
        tvi.mask = TVIF_TEXT | TVIF_IMAGE | TVIF_SELECTEDIMAGE | TVIF_PARAM;
        CString fileNameW(pParentFile->GetFileName());
        int index = fStringHolder.Add(fileNameW);
        tvi.pszText = (LPWSTR)(LPCWSTR) fStringHolder.GetAt(index);
        tvi.cchTextMax = 0;     // not needed for insertitem
        tvi.iImage = kTreeImageFolderClosed;
        tvi.iSelectedImage = kTreeImageFolderOpen;
        tvi.lParam = (LPARAM) pTarget;
        tvins.item = tvi;
        tvins.hInsertAfter = parent;
        tvins.hParent = parent;
        hLocalRoot = pTree->InsertItem(&tvins);
        if (hLocalRoot == NULL) {
            LOGW("Tree insert '%ls' failed", tvi.pszText);
            return NULL;
        }
    }

    while (pFile != NULL) {
        if (pFile->IsDirectory()) {
             ASSERT(!pFile->IsVolumeDirectory());

             if (pFile->GetParent() == pParentFile) {
                 /* this is a subdir of us */
                 pFile = AddSubdir(pTree, hLocalRoot, pDiskFS, pFile, depth+1);
                 if (pFile == NULL)
                     break;     // out of while -- disk is done
             } else {
                 /* not one of our subdirs; pop up a level */
                 break;         // out of while -- subdir is done
             }
        } else {
            pFile = pDiskFS->GetNextFile(pFile);
        }
    }

    /* expand as appropriate */
    if (fExpandDepth == -1 || depth <= fExpandDepth)
        pTree->Expand(hLocalRoot, TVE_EXPAND);

    return pFile;
}

DiskFSTree::TargetData* DiskFSTree::AllocTargetData(void)
{
    TargetData* pNew = new TargetData;

    if (pNew == NULL)
        return NULL;

    /* insert it at the head of the list, and update the head pointer */
    pNew->pNext = fpTargetData;
    fpTargetData = pNew;

    return pNew;
}

void DiskFSTree::FreeAllTargetData(void)
{
    TargetData* pTarget;
    TargetData* pNext;

    pTarget = fpTargetData;
    while (pTarget != NULL) {
        pNext = pTarget->pNext;
        delete pTarget;
        pTarget = pNext;
    }

    fpTargetData = NULL;
}
