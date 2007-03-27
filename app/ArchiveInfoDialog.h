/*
 * CiderPress
 * Copyright (C) 2007 by faddenSoft, LLC.  All Rights Reserved.
 * See the file LICENSE for distribution terms.
 */
/*
 * Definitions for the ArchiveInfo set of dialog classes.
 */
#ifndef __ARCHIVEINFODIALOG__
#define __ARCHIVEINFODIALOG__

#include "resource.h"
#include "GenericArchive.h"
#include "NufxArchive.h"
#include "DiskArchive.h"
#include "BnyArchive.h"
#include "AcuArchive.h"

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
	afx_msg void OnHelp(void);

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
	// overrides
	virtual BOOL OnInitDialog(void);

	NufxArchive*	fpArchive;
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
	// overrides
	virtual BOOL OnInitDialog(void);

	afx_msg void OnSubVolSelChange(void);

	void FillInVolumeInfo(const DiskFS* pDiskFS);
	void AddSubVolumes(const DiskFS* pDiskFS, const char* prefix,
		int* pIdx);
	void GetReducedSize(long numUnits, int unitSize,
		CString* pOut) const;

	DiskArchive*	fpArchive;

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
	// overrides
	virtual BOOL OnInitDialog(void);

	BnyArchive*	fpArchive;
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
	// overrides
	virtual BOOL OnInitDialog(void);

	AcuArchive*	fpArchive;
};

#endif /*__ARCHIVEINFODIALOG__*/
