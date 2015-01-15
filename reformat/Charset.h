/*
 * CiderPress
 * Copyright (C) 2015 by faddenSoft.  All Rights Reserved.
 * See the file LICENSE for distribution terms.
 */
#ifndef REFORMAT_CHARSET_H
#define REFORMAT_CHARSET_H

/*
 * Character set conversions.
 */
class Charset {
public:
    // Convert a Mac OS Roman character value (from a IIgs document) to
    // its UTF-16 Unicode equivalent.  This also includes a conversion
    // for the control characters.  The transformation is reversible.
    static uint16_t ConvertMacRomanToUTF16(uint8_t ch) {
        return kUTF16Conv[ch];
    }

    // Convert a Mac OS Roman character value an 8-bit Windows CP1252
    // equivalent.  The transformation is NOT reversible.
    static uint8_t ConvertMacRomanTo1252(uint8_t ch) {
        if (ch < 128)
            return ch;
        else
            return kCP1252Conv[ch-128];
    }

    // Simple Mac OS Roman to Unicode string conversion.
    static CString ConvertMORToUNI(const char* strMOR)
    {
        // We know that all MOR characters are represented in Unicode with a
        // single BMP code point, so we know that strlen(MOR) == wcslen(UNI).
        const size_t len = strlen(strMOR);
        CString strUNI;
        WCHAR* uniBuf = strUNI.GetBuffer(len);
        for (size_t i = 0; i < len; i++) {
            uniBuf[i] = Charset::ConvertMacRomanToUTF16(strMOR[i]);
        }
        strUNI.ReleaseBuffer(len);
        return strUNI;
    }

    static void CheckGSCharConv(void);

private:
    static const uint8_t kCP1252Conv[];
    static const uint16_t kUTF16Conv[];
};

#endif /*REFORMAT_CHARSET_H*/
