/*
 * CiderPress
 * Copyright (C) 2007 by faddenSoft, LLC.  All Rights Reserved.
 * See the file LICENSE for distribution terms.
 */
/*
 * DiskFSTree implementation.
 */
#include "StdAfx.h"
#include "ChooseAddTargetDialog.h"
#include "HelpTopics.h"

using namespace DiskImgLib;

/*
 * Build the tree.
 */
bool
DiskFSTree::BuildTree(DiskFS* pDiskFS, CTreeCtrl* pTree)
{
	ASSERT(pDiskFS != nil);
	ASSERT(pTree != nil);

	pTree->SetImageList(&fTreeImageList, TVSIL_NORMAL);
	return AddDiskFS(pTree, TVI_ROOT, pDiskFS, 1);
}

/*
 * Load the specified DiskFS into the tree, recursively adding any
 * sub-volumes.
 *
 * Pass in an initial depth of 1.
 *
 * Returns "true" on success, "false" on failure.
 */
bool
DiskFSTree::AddDiskFS(CTreeCtrl* pTree, HTREEITEM parent,
	DiskImgLib::DiskFS* pDiskFS, int depth)
{
	const DiskFS::SubVolume* pSubVol;
	TargetData* pTarget;
	HTREEITEM hLocalRoot;
	TVITEM tvi;
	TVINSERTSTRUCT tvins;

	/*
	 * Insert an entry for the current item.
	 */
	pTarget = AllocTargetData();
	pTarget->kind = kTargetDiskFS;
	pTarget->pDiskFS = pDiskFS;
	pTarget->pFile = nil;	// could also use volume dir for ProDOS
	tvi.mask = TVIF_TEXT | TVIF_IMAGE | TVIF_SELECTEDIMAGE | TVIF_PARAM;
	tvi.pszText = const_cast<char*>(pDiskFS->GetVolumeID());
	tvi.cchTextMax = 0;		// not needed for insertitem
//	tvi.iImage = kTreeImageFolderClosed;
//	tvi.iSelectedImage = kTreeImageFolderOpen;
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
	if (hLocalRoot == nil) {
		WMSG0("Tree root InsertItem failed\n");
		return false;
	}

	/*
	 * Scan for and handle all sub-volumes.
	 */
	pSubVol = pDiskFS->GetNextSubVolume(nil);
	while (pSubVol != nil) {
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
		AddSubdir(pTree, hLocalRoot, pDiskFS, nil, depth);
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


/*
 * Add the subdir and all of the subdirectories of the current subdir.
 *
 * The files are held in a linear list in the DiskFS, so we have to
 * reconstruct the hierarchy from the path names.  Pass in nil for the
 * root volume.
 *
 * Returns a pointer to the next A2File in the list (i.e. the first one
 * that we couldn't digest).  This assumes that the contents of a
 * subdirectory are grouped together in the linear list, so that we can
 * immediately bail when the first misfit is encountered.
 */
DiskImgLib::A2File*
DiskFSTree::AddSubdir(CTreeCtrl* pTree, HTREEITEM parent,
	DiskImgLib::DiskFS* pDiskFS, DiskImgLib::A2File* pParentFile,
	int depth)
{
	A2File* pFile;
	TargetData* pTarget;
	HTREEITEM hLocalRoot;
	TVITEM tvi;
	TVINSERTSTRUCT tvins;

	pFile = pDiskFS->GetNextFile(pParentFile);
	if (pFile == nil && pParentFile == nil) {
		/* this can happen on an empty DOS 3.3 disk; under ProDOS, we always
		   have the volume entry */
		/* note pFile will be nil if this happens to be a subdirectory
		   positioned as the very last file on the disk */
		return nil;
	}

	if (pParentFile == nil) {
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
		 */
		pTarget = AllocTargetData();
		pTarget->kind = kTargetSubdir;
		pTarget->selectable = true;
		pTarget->pDiskFS = pDiskFS;
		pTarget->pFile = pParentFile;
		tvi.mask = TVIF_TEXT | TVIF_IMAGE | TVIF_SELECTEDIMAGE | TVIF_PARAM;
		tvi.pszText = const_cast<char*>(pParentFile->GetFileName());
		tvi.cchTextMax = 0;		// not needed for insertitem
		tvi.iImage = kTreeImageFolderClosed;
		tvi.iSelectedImage = kTreeImageFolderOpen;
		tvi.lParam = (LPARAM) pTarget;
		tvins.item = tvi;
		tvins.hInsertAfter = parent;
		tvins.hParent = parent;
		hLocalRoot = pTree->InsertItem(&tvins);
		if (hLocalRoot == nil) {
			WMSG1("Tree insert '%s' failed\n", tvi.pszText);
			return nil;
		}
	}

	while (pFile != nil) {
		if (pFile->IsDirectory()) {
			 ASSERT(!pFile->IsVolumeDirectory());

			 if (pFile->GetParent() == pParentFile) {
				 /* this is a subdir of us */
				 pFile = AddSubdir(pTree, hLocalRoot, pDiskFS, pFile, depth+1);
				 if (pFile == nil)
					 break;		// out of while -- disk is done
			 } else {
				 /* not one of our subdirs; pop up a level */
				 break;			// out of while -- subdir is done
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


/*
 * Allocate a new TargetData struct, and add it to our list.
 */
DiskFSTree::TargetData*
DiskFSTree::AllocTargetData(void)
{
	TargetData* pNew = new TargetData;

	if (pNew == nil)
		return nil;
	memset(pNew, 0, sizeof(*pNew));

	/* insert it at the head of the list, and update the head pointer */
	pNew->pNext = fpTargetData;
	fpTargetData = pNew;

	return pNew;
}

/*
 * Free up the TargetData structures we created.
 *
 * Rather than  
 */
void
DiskFSTree::FreeAllTargetData(void)
{
	TargetData* pTarget;
	TargetData* pNext;

	pTarget = fpTargetData;
	while (pTarget != nil) {
		pNext = pTarget->pNext;
		delete pTarget;
		pTarget = pNext;
	}

	fpTargetData = nil;
}
