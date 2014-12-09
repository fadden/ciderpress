/*
 * CiderPress
 * Copyright (C) 2007 by faddenSoft, LLC.  All Rights Reserved.
 * See the file LICENSE for distribution terms.
 */
/*
 * Import BASIC programs from text files.
 *
 * THOUGHT: change the way the dialog works so that it doesn't scan until
 * you say "go".  Have some options for selecting language (BAS vs. INT),
 * and whether to try to identify listings with line breaks (i.e. they
 * neglected to "poke 33,33").  Have an optional "check syntax" box if we
 * want to get really fancy.
 */
#ifndef APP_BASICIMPORT_H
#define APP_BASICIMPORT_H

/*
 * This is a helper class to scan for a token in the list.
 *
 * Ideally we'd create a hash table to make it faster, but that's probably
 * not necessary for the small data sets we're working with.
 */
class BASTokenLookup {
public:
    BASTokenLookup(void)
        : fTokenPtr(NULL), fTokenLen(NULL)
        {}
    ~BASTokenLookup(void) {
        delete[] fTokenPtr;
        delete[] fTokenLen;
    }

    // Initialize the array.  Pass in the info for the token blob.
    void Init(const char* tokenList, int numTokens, int tokenLen);

    // Return the index of the matching token, or -1 if none found.
    int Lookup(const char* str, int len, int* pFoundLen);

    // Return a printable string.
    const char* GetToken(int idx) {
        return fTokenPtr[idx];
    }

private:
    int             fNumTokens;
    const char**    fTokenPtr;
    int*            fTokenLen;
};


/*
 * Import a BASIC program.
 *
 * Currently works for Applesoft.  Might work for Integer someday.
 */
class ImportBASDialog : public CDialog {
public:
    ImportBASDialog(CWnd* pParentWnd = NULL) :
        CDialog(IDD_IMPORT_BAS, pParentWnd), fDirty(false),
            fOutput(NULL), fOutputLen(-1)
        {}
    virtual ~ImportBASDialog(void) {
        delete[] fOutput;
    }

    // did we add something to the archive?
    bool IsDirty(void) const { return fDirty; }

    void SetFileName(const CString& fileName) { fFileName = fileName;  }

private:
    virtual BOOL OnInitDialog(void) override;
    //virtual void DoDataExchange(CDataExchange* pDX);
    virtual void OnOK(void) override;

    afx_msg void OnHelp(void) {
        MyApp::HandleHelp(this, HELP_TOPIC_IMPORT_BASIC);
    }

    /*
     * Import an Applesoft BASIC program from the specified file.
     */
    bool ImportBAS(const WCHAR* fileName);

    /*
     * Do the actual conversion.
     */
    bool ConvertTextToBAS(const char* buf, long fileLen,
        char** pOutBuf, long* pOutLen, ExpandBuffer* pMsgs);

    /*
     * Process a line of Applesoft BASIC text.
     *
     * Writes output to "pOutput".
     *
     * On failure, writes an error message to "msg" and returns false.
     */
    bool ProcessBASLine(const char* buf, int len,
        ExpandBuffer* pOutput, CString& msg);

    /*
     * Fix up the line pointers.  We left dummy nonzero values in them initially.
     */
    bool FixBASLinePointers(char* buf, long len, uint16_t addr);

    /*
     * Look for the end of line.
     *
     * Returns a pointer to the first byte *past* the EOL marker, which will point
     * at unallocated space for last line in the buffer.
     */
    const char* FindEOL(const char* buf, long max);

    /*
     * Find the next non-whitespace character.
     *
     * Updates the buffer pointer and length.
     *
     * Returns "false" if we run off the end without finding another non-ws char.
     */
    bool GetNextNWC(const char** pBuf, int* pLen, char* pCh);

    void SetOutput(char* outBuf, long outLen) {
        delete[] fOutput;
        fOutput = outBuf;
        fOutputLen = outLen;
    }

    BASTokenLookup  fBASLookup;
    bool        fDirty;

    char*       fOutput;
    long        fOutputLen;

    CString fFileName;      // file to open

    DECLARE_MESSAGE_MAP()
};

#endif /*APP_BASICIMPORT_H*/
