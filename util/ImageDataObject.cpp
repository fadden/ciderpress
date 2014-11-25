// ImageDataObject.h: Impementation for IDataObject Interface to be used 
//                       in inserting bitmap to the RichEdit Control.
//
// Author : Hani Atassi  (atassi@arabteam2000.com)
//
// How to use : Just call the static member InsertBitmap with 
//              the appropriate parrameters. 
//
// Known bugs :
//
//
//////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "ImageDataObject.h"

#ifdef _DEBUG
#undef THIS_FILE
static char THIS_FILE[]=__FILE__;
#define new DEBUG_NEW
#endif

//////////////////////////////////////////////////////////////////////
// Static member functions
//////////////////////////////////////////////////////////////////////

void CImageDataObject::InsertBitmap(IRichEditOle* pRichEditOle, HBITMAP hBitmap)
{
    SCODE sc;

    // Get the image data object
    //
    CImageDataObject *pods = new CImageDataObject;
    LPDATAOBJECT lpDataObject;
    pods->QueryInterface(IID_IDataObject, (void **)&lpDataObject);

    pods->SetBitmap(hBitmap);

    // Get the RichEdit container site
    //
    IOleClientSite *pOleClientSite; 
    pRichEditOle->GetClientSite(&pOleClientSite);

    // Initialize a Storage Object
    //
    IStorage *pStorage; 

    LPLOCKBYTES lpLockBytes = NULL;
    sc = ::CreateILockBytesOnHGlobal(NULL, TRUE, &lpLockBytes);
    if (sc != S_OK)
        AfxThrowOleException(sc);
    ASSERT(lpLockBytes != NULL);
    
    sc = ::StgCreateDocfileOnILockBytes(lpLockBytes,
        STGM_SHARE_EXCLUSIVE|STGM_CREATE|STGM_READWRITE, 0, &pStorage);
    if (sc != S_OK)
    {
        VERIFY(lpLockBytes->Release() == 0);
        lpLockBytes = NULL;
        AfxThrowOleException(sc);
    }
    ASSERT(pStorage != NULL);

    // The final ole object which will be inserted in the richedit control
    //
    IOleObject *pOleObject; 
    pOleObject = pods->GetOleObject(pOleClientSite, pStorage);

    // all items are "contained" -- this makes our reference to this object
    //  weak -- which is needed for links to embedding silent update.
    OleSetContainedObject(pOleObject, TRUE);

    // Now Add the object to the RichEdit 
    //
    REOBJECT reobject;
    ZeroMemory(&reobject, sizeof(REOBJECT));
    reobject.cbStruct = sizeof(REOBJECT);
    
    CLSID clsid;
    sc = pOleObject->GetUserClassID(&clsid);
    if (sc != S_OK)
        AfxThrowOleException(sc);

    reobject.clsid = clsid;
    reobject.cp = REO_CP_SELECTION;
    reobject.dvaspect = DVASPECT_CONTENT;
    reobject.poleobj = pOleObject;
    reobject.polesite = pOleClientSite;
    reobject.pstg = pStorage;

    // Insert the bitmap at the current location in the richedit control
    //
    pRichEditOle->InsertObject(&reobject);

    // Release all unnecessary interfaces
    //
    pOleObject->Release();
    pOleClientSite->Release();
    lpLockBytes->Release();     // new
    pStorage->Release();
    lpDataObject->Release();
}

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

void CImageDataObject::SetBitmap(HBITMAP hBitmap)
{
    ASSERT(hBitmap);

    STGMEDIUM stgm;
    stgm.tymed = TYMED_GDI;                 // Storage medium = HBITMAP handle      
    stgm.hBitmap = hBitmap;
    stgm.pUnkForRelease = NULL;             // Use ReleaseStgMedium

    FORMATETC fm;
    fm.cfFormat = CF_BITMAP;                // Clipboard format = CF_BITMAP
    fm.ptd = NULL;                          // Target Device = Screen
    fm.dwAspect = DVASPECT_CONTENT;         // Level of detail = Full content
    fm.lindex = -1;                         // Index = Not applicaple
    fm.tymed = TYMED_GDI;                   // Storage medium = HBITMAP handle

    this->SetData(&fm, &stgm, TRUE);        
}

IOleObject *CImageDataObject::GetOleObject(IOleClientSite *pOleClientSite, IStorage *pStorage)
{
    ASSERT(m_stgmed.hBitmap);

    SCODE sc;
    IOleObject *pOleObject;
    sc = ::OleCreateStaticFromData(this, IID_IOleObject, OLERENDER_FORMAT, 
            &m_fromat, pOleClientSite, pStorage, (void **)&pOleObject);
    if (sc != S_OK)
        AfxThrowOleException(sc);
    return pOleObject;
}
