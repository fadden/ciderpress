/*
 * CiderPress
 * Copyright (C) 2007 by faddenSoft, LLC.  All Rights Reserved.
 * See the file LICENSE for distribution terms.
 */
/*
 * Support for RecompressOptionsDialog.
 */
#include "stdafx.h"
#include "RecompressOptionsDialog.h"
#include "NufxArchive.h"
#include "HelpTopics.h"

//BEGIN_MESSAGE_MAP(UseSelectionDialog, CDialog)
//  ON_WM_HELPINFO()
//  //ON_COMMAND(IDHELP, OnHelp)
//END_MESSAGE_MAP()


/*
 * Set up our modified version of the "use selection" dialog.
 */
BOOL
RecompressOptionsDialog::OnInitDialog(void)
{
    fCompressionIdx = LoadComboBox((NuThreadFormat) fCompressionType);

    return UseSelectionDialog::OnInitDialog();
}

/*
 * Load strings into the combo box.  Only load formats supported by the
 * NufxLib DLL.
 *
 * Returns the combo box index for the format matching "fmt".
 */
int
RecompressOptionsDialog::LoadComboBox(NuThreadFormat fmt)
{
    static const struct {
        NuThreadFormat  format;
        const WCHAR*    name;
    } kComboStrings[] = {
        { kNuThreadFormatUncompressed,  L"No compression" },
        { kNuThreadFormatHuffmanSQ,     L"Squeeze" },
        { kNuThreadFormatLZW1,          L"Dynamic LZW/1" },
        { kNuThreadFormatLZW2,          L"Dynamic LZW/2" },
        { kNuThreadFormatLZC12,         L"12-bit LZC" },
        { kNuThreadFormatLZC16,         L"16-bit LZC" },
        { kNuThreadFormatDeflate,       L"Deflate" },
        { kNuThreadFormatBzip2,         L"Bzip2" },
    };

    CComboBox* pCombo;
    int idx, comboIdx;
    int retIdx = 0;

    pCombo = (CComboBox*) GetDlgItem(IDC_RECOMP_COMP);
    ASSERT(pCombo != NULL);

    for (idx = comboIdx = 0; idx < NELEM(kComboStrings); idx++) {
        if (NufxArchive::IsCompressionSupported(kComboStrings[idx].format)) {
            pCombo->AddString(kComboStrings[idx].name);
            pCombo->SetItemData(comboIdx, kComboStrings[idx].format);

            if (kComboStrings[idx].format == fmt)
                retIdx = comboIdx;

            comboIdx++;
        }
    }

    return retIdx;
}

/*
 * Convert values.
 */
void
RecompressOptionsDialog::DoDataExchange(CDataExchange* pDX)
{
    DDX_CBIndex(pDX, IDC_RECOMP_COMP, fCompressionIdx);

    if (pDX->m_bSaveAndValidate) {
        CComboBox* pCombo;
        pCombo = (CComboBox*) GetDlgItem(IDC_RECOMP_COMP);
        ASSERT(pCombo != NULL);

        fCompressionType = pCombo->GetItemData(fCompressionIdx);
        WMSG2("DDX got type=%d from combo index %d\n",
            fCompressionType, fCompressionIdx);
    }

    UseSelectionDialog::DoDataExchange(pDX);
}
