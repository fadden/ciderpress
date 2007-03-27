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
#ifndef __BASICIMPORT__
#define __BASICIMPORT__

/*
 * This is a helper class to scan for a token in the list.
 *
 * Ideally we'd create a hash table to make it faster, but that's probably
 * not necessary for the small data sets we're working with.
 */
class BASTokenLookup {
public:
	BASTokenLookup(void)
		: fTokenPtr(nil), fTokenLen(nil)
		{}
	~BASTokenLookup(void) {
		delete[] fTokenPtr;
		delete[] fTokenLen;
	}

	// Initialize the array.
	void Init(const char* tokenList, int numTokens, int tokenLen);

	// Return the index of the matching token, or -1 if none found.
	int Lookup(const char* str, int len, int* pFoundLen);

	// Return a printable string.
	const char* GetToken(int idx) {
		return fTokenPtr[idx];
	}

private:
	int				fNumTokens;
	const char**	fTokenPtr;
	int*			fTokenLen;
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
			fOutput(nil), fOutputLen(-1)
		{}
	virtual ~ImportBASDialog(void) {
		delete[] fOutput;
	}

	CString fFileName;		// file to open

	// did we add something to the archive?
	bool IsDirty(void) const { return fDirty; }

private:
	virtual BOOL OnInitDialog(void);
	//virtual void DoDataExchange(CDataExchange* pDX);
	virtual void OnOK(void);

	afx_msg void OnHelp(void);

	bool ImportBAS(const char* fileName);
	bool ConvertTextToBAS(const char* buf, long fileLen,
		char** pOutBuf, long* pOutLen, ExpandBuffer* pMsgs);
	bool ProcessBASLine(const char* buf, int len,
		ExpandBuffer* pOutput, CString& msg);
	bool FixBASLinePointers(char* buf, long len, unsigned short addr);

	const char* FindEOL(const char* buf, long max);
	bool GetNextNWC(const char** pBuf, int* pLen, char* pCh);

	void SetOutput(char* outBuf, long outLen) {
		delete[] fOutput;
		fOutput = outBuf;
		fOutputLen = outLen;
	}

	BASTokenLookup	fBASLookup;
	bool		fDirty;

	char*		fOutput;
	long		fOutputLen;

	DECLARE_MESSAGE_MAP()
};

#endif /*__BASICIMPORT__*/