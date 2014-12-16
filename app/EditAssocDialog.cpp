/*
 * CiderPress
 * Copyright (C) 2007 by faddenSoft, LLC.  All Rights Reserved.
 * See the file LICENSE for distribution terms.
 */
#include "stdafx.h"
#ifdef CAN_UPDATE_FILE_ASSOC
#include "EditAssocDialog.h"
#include "MyApp.h"
#include "Registry.h"

BEGIN_MESSAGE_MAP(EditAssocDialog, CDialog)
    ON_WM_HELPINFO()
    ON_COMMAND(IDHELP, OnHelp)
END_MESSAGE_MAP()

/* this comes from VC++6.0 MSDN help */
#ifndef ListView_SetCheckState
   #define ListView_SetCheckState(hwndLV, i, fCheck) \
      ListView_SetItemState(hwndLV, i, \
      INDEXTOSTATEIMAGEMASK((fCheck)+1), LVIS_STATEIMAGEMASK)
#endif

BOOL EditAssocDialog::OnInitDialog(void)
{
    CListCtrl* pListView = (CListCtrl*) GetDlgItem(IDC_ASSOCIATION_LIST);

    ASSERT(pListView != NULL);
    //pListView->ModifyStyleEx(0, LVS_EX_CHECKBOXES);
    ListView_SetExtendedListViewStyleEx(pListView->m_hWnd,
        LVS_EX_CHECKBOXES, LVS_EX_CHECKBOXES);

    /* move it over slightly so we see some overlap */
    CRect rect;
    GetWindowRect(&rect);
    rect.left += 10;
    rect.right += 10;
    MoveWindow(&rect);


    /*
     * Initialize this before DDX stuff happens.  If the caller didn't
     * provide a set, load our own.
     */
    if (fOurAssociations == NULL) {
        fOurAssociations = new bool[gMyApp.fRegistry.GetNumFileAssocs()];
        Setup(true);
    } else {
        Setup(false);
    }

    return CDialog::OnInitDialog();
}

void EditAssocDialog::Setup(bool loadAssoc)
{
    LOGD("Setup!");

    CListCtrl* pListView = (CListCtrl*) GetDlgItem(IDC_ASSOCIATION_LIST);
    ASSERT(pListView != NULL);

    ASSERT(fOurAssociations != NULL);

    /* two columns */
    CRect rect;
    pListView->GetClientRect(&rect);
    int width;

    width = pListView->GetStringWidth(L"XXExtensionXX");
    pListView->InsertColumn(0, L"Extension", LVCFMT_LEFT, width);
    pListView->InsertColumn(1, L"Association", LVCFMT_LEFT,
        rect.Width() - width);

    int num = gMyApp.fRegistry.GetNumFileAssocs();
    int idx = 0;
    while (num--) {
        CString ext, handler;
        CString dispStr;
        bool ours;

        gMyApp.fRegistry.GetFileAssoc(idx, &ext, &handler, &ours);
        if (handler.IsEmpty()) {
            handler = L"(no association)";
        }

        pListView->InsertItem(idx, ext);
        pListView->SetItemText(idx, 1, handler);

        if (loadAssoc)
            fOurAssociations[idx] = ours;
        idx++;
    }

    //DeleteAllItems(); // for Reload case
}

void EditAssocDialog::DoDataExchange(CDataExchange* pDX)
{
    CListCtrl* pListView = (CListCtrl*) GetDlgItem(IDC_ASSOCIATION_LIST);

    ASSERT(fOurAssociations != NULL);
    if (fOurAssociations == NULL)
        return;

    int num = gMyApp.fRegistry.GetNumFileAssocs();

    if (!pDX->m_bSaveAndValidate) {
        /* load fixed set of file associations */
        int idx = 0;
        while (num--) {
            ListView_SetCheckState(pListView->m_hWnd, idx,
                fOurAssociations[idx]);
            idx++;
        }
    } else {
        /* copy the checkboxes out */
        int idx = 0;
        while (num--) {
            fOurAssociations[idx] =
                        (ListView_GetCheckState(pListView->m_hWnd, idx) != 0);
            idx++;
        }
    }
}

#endif