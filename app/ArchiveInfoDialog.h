/*
 * CiderPress
 * Copyright (C) 2007 by faddenSoft, LLC.  All Rights Reserved.
 * See the file LICENSE for distribution terms.
 */
/*
 * Definitions for the ArchiveInfo set of dialog classes.
 */
#ifndef APP_ARCHIVEINFODIALOG_H
#define APP_ARCHIVEINFODIALOG_H

#include "resource.h"
#include "GenericArchive.h"
#include "NufxArchive.h"
#include "DiskArchive.h"
#include "BnyArchive.h"
#include "AcuArchive.h"
#include "AppleSingleArchive.h"

/*
 * This is an abstract base class for the archive info dialogs.  There is
 * one dialog for each kind of archive (i.e. each GenericArchive sub-class).
 */
class ArchiveInfoDialog : public CDialog {
public:
    ArchiveInfoDialog(UINT dialogID, CWnd* pParentWnd = NULL) :
        CDialog(dialogID, pParentWnd)
        {}
    virtual ~ArchiveInfoDialog(void) {}

private:
    /*
     * Show general help for the archive info dialogs.
     */
    afx_msg void OnHelp(void) {
        MyApp::HandleHelp(this, HELP_TOPIC_ARCHIVE_INFO);
    }

    DECLARE_MESSAGE_MAP()
};

/*
 * NuFX archive info.
 */
class NufxArchiveInfoDialog : public ArchiveInfoDialog {
public:
    NufxArchiveInfoDialog(NufxArchive* pArchive, CWnd* pParentWnd = NULL) :
        fpArchive(pArchive),
        ArchiveInfoDialog(IDD_ARCHIVEINFO_NUFX, pParentWnd)
        {}
    virtual ~NufxArchiveInfoDialog(void) {}

private:
    virtual BOOL OnInitDialog(void) override;

    NufxArchive*    fpArchive;
};

/*
 * Disk image info.
 */
class DiskArchiveInfoDialog : public ArchiveInfoDialog {
public:
    DiskArchiveInfoDialog(DiskArchive* pArchive, CWnd* pParentWnd = NULL) :
        fpArchive(pArchive),
        ArchiveInfoDialog(IDD_ARCHIVEINFO_DISK, pParentWnd)
        {}
    virtual ~DiskArchiveInfoDialog(void) {}

private:
    virtual BOOL OnInitDialog(void) override;

    /*
     * The user has changed their selection in the sub-volume pulldown menu.
     */
    afx_msg void OnSubVolSelChange(void);

    /*
     * Fill in the volume-specific info fields.
     */
    void FillInVolumeInfo(const DiskFS* pDiskFS);

    /*
     * Recursively add sub-volumes to the list.
     */
    void AddSubVolumes(const DiskFS* pDiskFS, const WCHAR* prefix,
        int* pIdx);

    /*
     * Reduce a size to something meaningful (KB, MB, GB).
     */
    void GetReducedSize(long numUnits, int unitSize,
        CString* pOut) const;

    DiskArchive*    fpArchive;

    DECLARE_MESSAGE_MAP()
};

/*
 * Binary II archive info.
 */
class BnyArchiveInfoDialog : public ArchiveInfoDialog {
public:
    BnyArchiveInfoDialog(BnyArchive* pArchive, CWnd* pParentWnd = NULL) :
        fpArchive(pArchive),
        ArchiveInfoDialog(IDD_ARCHIVEINFO_BNY, pParentWnd)
        {}
    virtual ~BnyArchiveInfoDialog(void) {}

private:
    virtual BOOL OnInitDialog(void) override;

    BnyArchive* fpArchive;
};

/*
 * ACU archive info.
 */
class AcuArchiveInfoDialog : public ArchiveInfoDialog {
public:
    AcuArchiveInfoDialog(AcuArchive* pArchive, CWnd* pParentWnd = NULL) :
        fpArchive(pArchive),
        ArchiveInfoDialog(IDD_ARCHIVEINFO_ACU, pParentWnd)
        {}
    virtual ~AcuArchiveInfoDialog(void) {}

private:
    virtual BOOL OnInitDialog(void) override;

    AcuArchive* fpArchive;
};

/*
 * AppleSingle archive info.
 */
class AppleSingleArchiveInfoDialog : public ArchiveInfoDialog {
public:
    AppleSingleArchiveInfoDialog(AppleSingleArchive* pArchive, CWnd* pParentWnd = NULL) :
        fpArchive(pArchive),
        ArchiveInfoDialog(IDD_ARCHIVEINFO_APPLESINGLE, pParentWnd)
        {}
    virtual ~AppleSingleArchiveInfoDialog(void) {}

private:
    virtual BOOL OnInitDialog(void) override;

    AppleSingleArchive* fpArchive;
};

#endif /*APP_ARCHIVEINFODIALOG_H*/
