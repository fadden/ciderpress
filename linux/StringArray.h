/*
 * CiderPress
 * Copyright (C) 2007 by faddenSoft, LLC.  All Rights Reserved.
 * See the file LICENSE for distribution terms.
 */
/*
 * An expandable array of strings.
 */
#ifndef __STRING_ARRAY__
#define __STRING_ARRAY__

#include <stdlib.h>
#include <string.h>
#include <memory.h>

//
// This is a simple container for an array of strings.  You can add strings
// to the list and sort them.
//
class StringArray {
public:
    StringArray()
        : mMax(0), mCurrent(0), mArray(NULL)
        {}
    virtual ~StringArray()
    {
        for (int i = 0; i < mCurrent; i++)
            delete[] mArray[i];
        delete[] mArray;
    }

    //
    // Add a string.  A copy of the string is made.
    //
    bool Add(const char* str)
    {
        if (mCurrent >= mMax) {
            char** tmp;

            if (mMax == 0)
                mMax = 16;
            else
                mMax *= 2;

            tmp = new char*[mMax];
            if (tmp == NULL)
                return false;

            memcpy(tmp, mArray, mCurrent * sizeof(char*));
            delete[] mArray;
            mArray = tmp;
        }

        int len = strlen(str);
        mArray[mCurrent] = new char[len+1];
        memcpy(mArray[mCurrent], str, len+1);
        mCurrent++;

        return true;
    }

    //
    // Sort the array.  Supply a sort function that takes two strings
    // and returns <0, 0, or >0 if the first argument is less than,
    // equal to, or greater than the second argument.  (strcmp works.)
    //
    void Sort(int (*compare)(const void*, const void*))
    {
        qsort(mArray, mCurrent, sizeof(char*), compare);
    }

    //
    // Use this as an argument to the sort routine.
    //
    static int CmpAscendingAlpha(const void* pstr1, const void* pstr2)
    {
        return strcmp(*(const char**)pstr1, *(const char**)pstr2);
    }

    //
    // Get the #of items in the array.
    //
    inline int GetCount(void) const { return mCurrent; }

    //
    // Get entry N.
    //
    const char* GetEntry(int idx) const
    {
        if (idx < 0 || idx >= mCurrent)
            return NULL;
        return mArray[idx];
    }

private:
    int     mMax;
    int     mCurrent;
    char**  mArray;
};

#endif /*__STRING_ARRAY__*/
