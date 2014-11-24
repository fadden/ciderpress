/*
 * CiderPress
 * Copyright (C) 2007 by faddenSoft, LLC.  All Rights Reserved.
 * See the file LICENSE for distribution terms.
 */
/*
 * DiskFS base class.
 */
#include "StdAfx.h"
#include "DiskImgPriv.h"


/*
 * ===========================================================================
 *      A2File
 * ===========================================================================
 */

/*
 * Set the quality level (a/k/a damage level) of a file.
 *
 * Refuse to "improve" the quality level of a file.
 */
void A2File::SetQuality(FileQuality quality)
{
    if (quality == kQualityGood &&
        (fFileQuality == kQualitySuspicious || fFileQuality == kQualityDamaged))
    {
        assert(false);
        return;
    }
    if (quality == kQualitySuspicious && fFileQuality == kQualityDamaged) {
        //assert(false);
        return;
    }

    fFileQuality = quality;
}

/*
 * Reset the quality level after making repairs.
 */
void A2File::ResetQuality(void)
{
    fFileQuality = kQualityGood;
}


/*
 * ===========================================================================
 *      DiskFS
 * ===========================================================================
 */

/*
 * Set the DiskImg pointer.  We add or subtract from the DiskImg's ref count
 * so that it can be sure there are no DiskFS objects left dangling when the
 * DiskImg is deleted.
 */
void DiskFS::SetDiskImg(DiskImg* pImg)
{
    if (pImg == NULL && fpImg == NULL) {
        LOGI("SetDiskImg: no-op (both NULL)");
        return;
    } else if (fpImg == pImg) {
        LOGI("SetDiskImg: no-op (old == new)");
        return;
    }

    if (fpImg != NULL)
        fpImg->RemoveDiskFS(this);
    if (pImg != NULL)
        pImg->AddDiskFS(this);
    fpImg = pImg;
}

/*
 * Flush changes to disk.
 */
DIError DiskFS::Flush(DiskImg::FlushMode mode)
{
    SubVolume* pSubVol = GetNextSubVolume(NULL);
    DIError dierr;

    while (pSubVol != NULL) {

        // quick sanity check
        assert(pSubVol->GetDiskFS()->GetDiskImg() == pSubVol->GetDiskImg());

        dierr = pSubVol->GetDiskFS()->Flush(mode);  // recurse
        if (dierr != kDIErrNone)
            return dierr;

        pSubVol = GetNextSubVolume(pSubVol);
    }

    assert(fpImg != NULL);

    return fpImg->FlushImage(mode);
}

/*
 * Set the "read only" flag on our DiskImg and those of our sub-volumes.
 */
void DiskFS::SetAllReadOnly(bool val)
{
    SubVolume* pSubVol = GetNextSubVolume(NULL);

    /* put current volume in read-only mode */
    if (fpImg != NULL)
        fpImg->SetReadOnly(val);

    /* handle our kids */
    while (pSubVol != NULL) {
        // quick sanity check
        assert(pSubVol->GetDiskFS()->GetDiskImg() == pSubVol->GetDiskImg());

        //pSubVol->GetDiskImg()->SetReadOnly(val);
        pSubVol->GetDiskFS()->SetAllReadOnly(val);  // recurse

        pSubVol = GetNextSubVolume(pSubVol);
    }
}


/*
 * The file list looks something like this:
 *
 * volume-dir
 * file1
 * file2
 * subdir1
 * subdir1:file1
 * subdir1:file2
 * subdir1:subsub1
 * subdir1:subsub1:file1
 * subdir1:subsub2
 * subdir1:subsub2:file1
 * subdir1:subsub2:file2
 * subdir1:file3
 * file3
 *
 * Everything contained within a subdir comes after the subdir entry and
 * before any entries from later subdirs at the same level.
 *
 * It's unclear whether a linear list or a hierarchical tree structure is
 * the most appropriate way to hold the data.  The tree is easier to update,
 * but the linear list corresponds to the primary view in CiderPress, and
 * lists are simpler and easier to manage.  For now I'm sticking with a list.
 *
 * The files MUST be in the order in which they came from the disk.  This
 * doesn't matter most of the time, but for Pascal volumes it's essential
 * for ensuring that the Write command doesn't run over the next file.
 */

/*
 * Add a file to the end of our list.
 */
void DiskFS::AddFileToList(A2File* pFile)
{
    assert(pFile->GetNext() == NULL);

    if (fpA2Head == NULL) {
        assert(fpA2Tail == NULL);
        fpA2Head = fpA2Tail = pFile;
    } else {
        pFile->SetPrev(fpA2Tail);
        fpA2Tail->SetNext(pFile);
        fpA2Tail = pFile;
    }
}

/*
 * Insert a file into its appropriate place in the list, based on a file
 * hierarchy.
 *
 * Pass in the thing to be added ("pFile") and the previous entry ("pPrev").
 * An empty hierarchic filesystem will have an entry for the volume dir, so
 * we should never have an empty list or a NULL pPrev.
 *
 * The part where things go pear-shaped happens if "pPrev" is a subdirectory.
 * If so, we need to come after all of the subdir's entries, including any
 * entries for sub-subdirs.  There's no graceful way to go about this in a
 * linear list.
 *
 * (We'd love to be able to find the *next* entry and then back up one,
 * but odds are that there isn't a "next" entry if we're busily creating
 * files.)
 */
void DiskFS::InsertFileInList(A2File* pFile, A2File* pPrev)
{
    assert(pFile->GetNext() == NULL);

    if (fpA2Head == NULL) {
        assert(pPrev == NULL);
        fpA2Head = fpA2Tail = pFile;
        return;
    } else if (pPrev == NULL) {
        // create two entries on DOS disk, delete first, add new file
        pFile->SetNext(fpA2Head);
        fpA2Head = pFile;
        return;
    }

    /*
     * If we're inserting after the parent (i.e. we're the very first thing
     * in a subdir) or after a plain file, just drop it in.
     *
     * If we're inserting after a subdir, go fish.
     */
    if (pPrev->IsDirectory() && pFile->GetParent() != pPrev) {
        pPrev = SkipSubdir(pPrev);
    }

    pFile->SetNext(pPrev->GetNext());
    pPrev->SetNext(pFile);
}

/*
 * Skip over all entries in the subdir we're pointing to.
 *
 * The return value is the very last entry in the subdir.
 */
A2File* DiskFS::SkipSubdir(A2File* pSubdir)
{
    if (pSubdir->GetNext() == NULL)
        return pSubdir;     // end of list reached -- subdir is empty

    A2File* pCur = pSubdir;
    A2File* pNext = NULL;

    assert(pCur != NULL);    // at least one time through the loop

    while (pCur != NULL) {
        pNext = pCur->GetNext();
        if (pNext == NULL)               // end of list reached
            return pCur;

        if (pNext->GetParent() != pSubdir)  // end of dir reached
            return pCur;
        if (pNext->IsDirectory())
            pCur = SkipSubdir(pNext);   // get last entry in dir
        else
            pCur = pNext;               // advance forward one
    }

    /* should never get here */
    assert(false);
    return pNext;
}

/*
 * Delete a member from the list.
 *
 * We're currently singly-linked, making this rather expensive.
 */
void DiskFS::DeleteFileFromList(A2File* pFile)
{
    if (fpA2Head == pFile) {
        /* delete the head of the list */
        fpA2Head = fpA2Head->GetNext();
        delete pFile;
    } else {
        A2File* pCur = fpA2Head;
        while (pCur != NULL) {
            if (pCur->GetNext() == pFile) {
                /* found it */
                A2File* pNextNext = pCur->GetNext()->GetNext();
                delete pCur->GetNext();
                pCur->SetNext(pNextNext);
                break;
            }
            pCur = pCur->GetNext();
        }

        if (pCur == NULL) {
            LOGI("GLITCH: couldn't find element to delete!");
            assert(false);
        }
    }
}


/*
 * Access the "next" pointer.
 *
 * Because we apparently can't declare an anonymous class as a friend
 * in MSVC++6.0, this can't be an inline function.
 */
A2File* DiskFS::GetNextFile(A2File* pFile) const
{
    if (pFile == NULL)
        return fpA2Head;
    else
        return pFile->GetNext();
}

/*
 * Return the #of elements in the linear file list.
 *
 * Right now the only code that calls this is the disk info panel in
 * CiderPress, so we don't need it to be efficient.
 */
long DiskFS::GetFileCount(void) const
{
    long count = 0;

    A2File* pFile = fpA2Head;
    while (pFile != NULL) {
        count++;
        pFile = pFile->GetNext();
    }

    return count;
}

/*
 * Delete all entries in the list.
 */
void DiskFS::DeleteFileList(void)
{
    A2File* pFile;
    A2File* pNext;

    pFile = fpA2Head;
    while (pFile != NULL) {
        pNext = pFile->GetNext();
        delete pFile;
        pFile = pNext;
    }
}

/*
 * Dump file list.
 */
void DiskFS::DumpFileList(void)
{
    A2File* pFile;

    LOGI("DiskFS file list contents:");

    pFile = GetNextFile(NULL);
    while (pFile != NULL) {
        LOGI(" %s", pFile->GetPathName());
        pFile = GetNextFile(pFile);
    }
}

/*
 * Run through the list of files and find one that matches (case-insensitive).
 *
 * This does not attempt to open files in sub-volumes.  We could, but it's
 * likely that the application has "decorated" the name in some fashion,
 * e.g. by prepending the sub-volume's volume name to the filename.  May
 * be best to let the application dig for the sub-volume.
 */
A2File* DiskFS::GetFileByName(const char* fileName, StringCompareFunc func)
{
    A2File* pFile;

    if (func == NULL)
        func = ::strcasecmp;

    pFile = GetNextFile(NULL);
    while (pFile != NULL) {
        if ((*func)(pFile->GetPathName(), fileName) == 0)
            return pFile;

        pFile = GetNextFile(pFile);
    }

    return NULL;
}


/*
 * Add a sub-volume to the end of our list.
 *
 * Copies some parameters from "this" into pDiskFS, such as whether to
 * scan for sub-volumes and the various DiskFS parameters.
 *
 * Note this happens AFTER the disk has been scanned.
 */
void DiskFS::AddSubVolumeToList(DiskImg* pDiskImg, DiskFS* pDiskFS)
{
    SubVolume* pSubVol;

    /*
     * Check the arguments.
     */
    if (pDiskImg == NULL || pDiskFS == NULL) {
        LOGI(" DiskFS bogus sub volume ptrs %08lx %08lx",
            (long) pDiskImg, (long) pDiskFS);
        assert(false);
        return;
    }
    if (pDiskImg == fpImg || pDiskFS == this) {
        LOGI(" DiskFS attempt to add self to sub-vol list");
        assert(false);
        return;
    }
    if (pDiskFS->GetDiskImg() == NULL) {
        LOGI(" DiskFS lacks a DiskImg pointer");
        assert(false);
        return;
    }
    pSubVol = fpSubVolumeHead;
    while (pSubVol != NULL) {
        if (pSubVol->GetDiskImg() == pDiskImg ||
            pSubVol->GetDiskFS() == pDiskFS)
        {
            LOGI(" DiskFS multiple adds on diskimg or diskfs");
            assert(false);
            return;
        }
        pSubVol = pSubVol->GetNext();
    }

    assert(pDiskFS->GetDiskImg() == pDiskImg);

    /*
     * Looks good.  Add it.
     */
    pSubVol = new SubVolume;
    if (pSubVol == NULL)
        return;

    pSubVol->Create(pDiskImg, pDiskFS);

    if (fpSubVolumeHead == NULL) {
        assert(fpSubVolumeTail == NULL);
        fpSubVolumeHead = fpSubVolumeTail = pSubVol;
    } else {
        pSubVol->SetPrev(fpSubVolumeTail);
        fpSubVolumeTail->SetNext(pSubVol);
        fpSubVolumeTail = pSubVol;
    }

    /* make sure inheritable stuff gets copied */
    CopyInheritables(pDiskFS);
}

/*
 * Copy parameters to a sub-volume.
 */
void DiskFS::CopyInheritables(DiskFS* pNewFS)
{
    for (int i = 0; i < (int) NELEM(fParmTable); i++)
        pNewFS->fParmTable[i] = fParmTable[i];

    pNewFS->fScanForSubVolumes = fScanForSubVolumes;

#if 0
    /* copy scan progress update stuff */
    pNewFS->fpScanProgressCallback = fpScanProgressCallback;
    pNewFS->fpScanProgressCookie = fpScanProgressCookie;
    pNewFS->fpScanCount = -1;
    strcpy(pNewFS->fpScanMsg, "HEY");
#endif
}

/*
 * Access the "next" pointer.
 *
 * Because we apparently can't declare an anonymous class as a friend
 * in MSVC++6.0, this can't be an inline function.
 */
DiskFS::SubVolume* DiskFS::GetNextSubVolume(const SubVolume* pSubVol) const
{
    if (pSubVol == NULL)
        return fpSubVolumeHead;
    else
        return pSubVol->GetNext();
}

/*
 * Delete all entries in the list.
 */
void DiskFS::DeleteSubVolumeList(void)
{
    SubVolume* pSubVol;
    SubVolume* pNext;

    pSubVol = fpSubVolumeHead;
    while (pSubVol != NULL) {
        pNext = pSubVol->GetNext();
        delete pSubVol;
        pSubVol = pNext;
    }
}


/*
 * Get a parameter.
 */
long DiskFS::GetParameter(DiskFSParameter parm)
{
    assert(parm > kParmUnknown && parm < kParmMax);
    return fParmTable[parm];
}

/*
 * Set a parameter.
 *
 * The setting propagates to all sub-volumes.
 */
void DiskFS::SetParameter(DiskFSParameter parm, long val)
{
    assert(parm > kParmUnknown && parm < kParmMax);
    fParmTable[parm] = val;

    SubVolume* pSubVol = GetNextSubVolume(NULL);
    while (pSubVol != NULL) {
        pSubVol->GetDiskFS()->SetParameter(parm, val);
        pSubVol = GetNextSubVolume(pSubVol);
    }
}


/*
 * Scan for damaged or suspicious files.
 */
void DiskFS::ScanForDamagedFiles(bool* pDamaged, bool* pSuspicious)
{
    A2File* pFile;

    *pDamaged = *pSuspicious = false;

    pFile = GetNextFile(NULL);
    while (pFile != NULL) {
        if (pFile->GetQuality() == A2File::kQualityDamaged)
            *pDamaged = true;
        if (pFile->GetQuality() != A2File::kQualityGood)
            *pSuspicious = true;
        pFile = GetNextFile(pFile);
    }
}
