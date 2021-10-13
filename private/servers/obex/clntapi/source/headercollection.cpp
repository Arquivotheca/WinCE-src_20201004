//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// Use of this source code is subject to the terms of the Microsoft shared
// source or premium shared source license agreement under which you licensed
// this source code. If you did not accept the terms of the license agreement,
// you are not authorized to use this source code. For the terms of the license,
// please see the license agreement between you and Microsoft or, if applicable,
// see the SOURCE.RTF on your install media or the root of your tools installation.
// THE SOURCE CODE IS PROVIDED "AS IS", WITH NO WARRANTIES OR INDEMNITIES.
//
/**************************************************************************/
/*  HeaderCollection.cpp                                                  */
/*                                                                        */
/*  This file contains routines hold OBEX headers                         */
/*                                                                        */
/*  Functions included:                                                   */
/*                                                                        */
/*  Other related files:                                                  */
/*                                                                        */
/*                                                                        */
/**************************************************************************/
#include "common.h"
#include "HeaderEnum.h"
#include "HeaderCollection.h"
#include "obexpacketinfo.h"



/*****************************************************************************/
/*   CHeaderCollection::CHeaderCollection()                                  */
/*   construct a header collection                                           */  
/*****************************************************************************/
CHeaderCollection::CHeaderCollection() : pHeaders(0), _refCount(1)
{ 
    DEBUGMSG(OBEX_HEADERCOLLECTION_ZONE,(L"CHeaderCollection::CHeaderCollection\n"));
}

/*****************************************************************************/
/*   CHeaderCollection::!CHeaderCollection()                                 */
/*   destruct a header collection                                            */ 
/*      begin by looping through linked list of nodes, deleting each         */
/*      with proper routine                                                  */ 
/*****************************************************************************/
CHeaderCollection::~CHeaderCollection()
{
    DEBUGMSG(OBEX_HEADERCOLLECTION_ZONE,(L"CHeaderCollection::~CHeaderCollection\n"));
   
    while(pHeaders)
    {
        CHeaderNode *pTemp = pHeaders;

        //clean up memory in the header collection
        if((pTemp->pItem->bId & OBEX_TYPE_MASK) == OBEX_TYPE_BYTESEQ && pTemp->pItem->value.ba.pbaData)
            delete [] pTemp->pItem->value.ba.pbaData;     
        else if((pTemp->pItem->bId & OBEX_TYPE_MASK) == OBEX_TYPE_UNICODE && pTemp->pItem->value.pszData) {
            PREFAST_SUPPRESS(307, "this is a BSTR must use SysFreeString");
            SysFreeString((BSTR)pTemp->pItem->value.pszData);
        }

        if(pTemp->pItem)
            delete pTemp->pItem;

        pHeaders = pHeaders->pNext;
        delete pTemp;
    }

    pHeaders = 0;
}


/*****************************************************************************/
/*   CHeaderCollection::InsertHeaderEnd()                                    */
/*   Insert a header at the end of the linked list this will force           */
/*   the item to be the FIRST header sent (Required for connectionID)        */
/*****************************************************************************/
HRESULT STDMETHODCALLTYPE 
CHeaderCollection::InsertHeaderEnd(OBEX_HEADER *pHeader)
{
    DEBUGMSG(OBEX_HEADERCOLLECTION_ZONE,(L"CHeaderCollection::InsertHeaderEnd\n"));


	//
	//  Check to make sure this header isnt already here
	//
	CHeaderNode *pTemp = pHeaders;
	while(pTemp)
	{
		if(pTemp->pItem->bId == pHeader->bId)
		{
			DEBUGMSG(OBEX_HEADERCOLLECTION_ZONE,(L"CHeaderCollection::InsertHeaderEnd -- trying to put in multiple headers of same type!"));
			SVSUTIL_ASSERT(FALSE);				
			return E_FAIL;
		}
		pTemp = pTemp->pNext;
	}
    
    CHeaderNode *pNewNode = new CHeaderNode();
    
    if(NULL == pNewNode)
        return E_OUTOFMEMORY;        
   
    pNewNode->pItem = pHeader;
    pNewNode->pNext = NULL;

    if(!pHeaders)
        pHeaders = pNewNode;
    else
    {
        //now find the next to last header in the list
        pTemp = pHeaders;
        while(pTemp->pNext)
            pTemp = pTemp->pNext;

        pTemp->pNext = pNewNode;
    }   
    return S_OK;
}


/*****************************************************************************/
/*   CHeaderCollection::InsertHeader()                                       */
/*   Insert a header at the beginning of the linked list this will force     */
/*   the item to be the LAST header sent (well.. if you add another it will  */
/*   be last                                                                 */
/*****************************************************************************/
HRESULT STDMETHODCALLTYPE 
CHeaderCollection::InsertHeader(OBEX_HEADER *pHeader)
{
	//
	//  Check to make sure this header isnt already here
	//
	CHeaderNode *pTemp = pHeaders;
	while(pTemp)
	{
		if(pTemp->pItem->bId == pHeader->bId)
		{
			DEBUGMSG(OBEX_HEADERCOLLECTION_ZONE,(L"CHeaderCollection::InsertHeader -- trying to put in multiple headers of same type!"));
			SVSUTIL_ASSERT(FALSE);					
			return E_FAIL;
		}
		pTemp = pTemp->pNext;
	}
	
    DEBUGMSG(OBEX_HEADERCOLLECTION_ZONE,(L"CHeaderCollection::InsertHeader\n"));
    CHeaderNode *pNewNode = new CHeaderNode();   
    
    if(NULL == pNewNode)
        return E_OUTOFMEMORY;
    
    pNewNode->pItem = pHeader;
    pNewNode->pNext = pHeaders;
    pHeaders = pNewNode;    
    return S_OK;
}

 
/*****************************************************************************/
/*   CHeaderCollection::AddByteArray()                                       */
/*   Add a byte array to the linked list                                     */
/*****************************************************************************/
HRESULT STDMETHODCALLTYPE 
CHeaderCollection::AddByteArray (byte Id, unsigned long ulSize, byte *pData)
{
    DEBUGMSG(OBEX_HEADERCOLLECTION_ZONE,(L"CHeaderCollection::AddByteArray\n"));
    
    if(ulSize >= OBEX_MAX_PACKET_SIZE) 
        return E_INVALIDARG;           

    OBEX_HEADER *pOBHeader = new OBEX_HEADER;
    
    if(NULL == pOBHeader)
        return E_OUTOFMEMORY;
            
    pOBHeader->bId = Id;
    pOBHeader->value.ba.pbaData = (ulSize?new BYTE[ulSize]:0);
    
    if(ulSize && NULL == pOBHeader->value.ba.pbaData)
        return E_OUTOFMEMORY;
        
    if(ulSize)  
        memcpy(pOBHeader->value.ba.pbaData, pData, ulSize);
        
    pOBHeader->value.ba.dwSize = ulSize;
    return InsertHeader(pOBHeader);
}


/*****************************************************************************/
/*   CHeaderCollection::AddLong()                                            */
/*   Add a long to the linked list                                           */
/*****************************************************************************/
HRESULT STDMETHODCALLTYPE 
CHeaderCollection::AddLong (byte Id, unsigned long ulData)
{
    DEBUGMSG(OBEX_HEADERCOLLECTION_ZONE,(L"CHeaderCollection::AddLong\n"));
    OBEX_HEADER *pOBHeader = new OBEX_HEADER;
    
    if(NULL == pOBHeader)
        return E_OUTOFMEMORY;
        
    pOBHeader->bId = Id;
    pOBHeader->value.dwData = ulData;
    return InsertHeader(pOBHeader);    
}

HRESULT STDMETHODCALLTYPE 
CHeaderCollection::AddByte (byte Id, byte pData)
{
    DEBUGMSG(OBEX_HEADERCOLLECTION_ZONE,(L"CHeaderCollection::AddByte\n"));
    OBEX_HEADER *pOBHeader = new OBEX_HEADER;
    
    if(NULL == pOBHeader)
        return E_OUTOFMEMORY;
        
    pOBHeader->bId = Id;
    pOBHeader->value.bData = pData;
    return InsertHeader(pOBHeader);       
}

HRESULT STDMETHODCALLTYPE 
CHeaderCollection::AddUnicodeString (byte Id, LPCWSTR pszData)
{
    DEBUGMSG(OBEX_HEADERCOLLECTION_ZONE,(L"CHeaderCollection::AddUnicodeString\n"));
    OBEX_HEADER *pOBHeader = new OBEX_HEADER;
    
    if(NULL == pOBHeader)
        return E_OUTOFMEMORY;
        
    pOBHeader->bId = Id;
    pOBHeader->value.pszData = SysAllocString(pszData);    
    return InsertHeader(pOBHeader);    
}

HRESULT STDMETHODCALLTYPE 
CHeaderCollection::Remove (byte Id)
{
	CHeaderNode *pTempNode = pHeaders;
	CHeaderNode *pPrevNode = NULL;

	DEBUGMSG(OBEX_OBEXSTREAM_ZONE,(L"CHeaderCollection::Remove() -- op: %x\n", Id));


	while(pTempNode) 
	{
		PREFAST_ASSERT(pTempNode->pItem);

		//if we have found the Id delete the node
		if(pTempNode->pItem->bId == Id)
		{
			//if this is the head of the list
			if(!pPrevNode)
			{
				pHeaders=pHeaders->pNext;
			}
			else
			{
				pPrevNode->pNext = pTempNode->pNext;
			}

			//clean up memory in the header collection
	        if((pTempNode->pItem->bId & OBEX_TYPE_MASK) == OBEX_TYPE_BYTESEQ)
	            delete [] pTempNode->pItem->value.ba.pbaData;     
	        else if((pTempNode->pItem->bId & OBEX_TYPE_MASK) == OBEX_TYPE_UNICODE) {
	            PREFAST_SUPPRESS(307, "This is alloc'ed with SysAllocString");
	            SysFreeString((BSTR)pTempNode->pItem->value.pszData);
	        }

	        delete pTempNode->pItem;	        
	        delete pTempNode;

	        return S_OK;
		}

		//otherwise, advance to the next node
		else
		{
			pPrevNode = pTempNode;
			pTempNode = pTempNode->pNext;			
		}
	}	
   	return E_FAIL;
}


void 
CHeaderCollection::CopyHeader(OBEX_HEADER *pOrig, OBEX_HEADER *pNew)
{
	 PREFAST_ASSERT(pOrig && pNew);
	 DEBUGMSG(OBEX_OBEXSTREAM_ZONE,(L"CHeaderCollection::CopyHeader --- op: %x!\n", pOrig->bId));
    
	
	 pNew->bId = pOrig->bId;

	 if((pOrig->bId & OBEX_TYPE_MASK) == OBEX_TYPE_DWORD)       
	 {			
    	pNew->value.dwData = pOrig->value.dwData;
	 }          
     else if((pOrig->bId & OBEX_TYPE_MASK) == OBEX_TYPE_BYTESEQ)
     {
		pNew->value.ba.pbaData = new BYTE[pOrig->value.ba.dwSize];
		if( pNew->value.ba.pbaData )
		{
	    		memcpy(pNew->value.ba.pbaData, pOrig->value.ba.pbaData, pOrig->value.ba.dwSize);
	    		pNew->value.ba.dwSize = pOrig->value.ba.dwSize;
		}
     }        
     else if((pOrig->bId & OBEX_TYPE_MASK) == OBEX_TYPE_UNICODE)
     {
        pNew->value.pszData = SysAllocString(pOrig->value.pszData);  
     }
     else if((pOrig->bId & OBEX_TYPE_MASK) == OBEX_TYPE_BYTE)
     {
     	pNew->value.bData = pOrig->value.bData;
     }
     else
     {
     	SVSUTIL_ASSERT(FALSE);
     	DEBUGMSG(OBEX_HEADERCOLLECTION_ZONE,(L"CHeaderCollection::CopyHeader --- unknown!\n"));
     }
            
}


HRESULT STDMETHODCALLTYPE 
CHeaderCollection::RemoveAll()
{
 	while(pHeaders)
    {
        CHeaderNode *pTemp = pHeaders;

        //clean up memory in the header collection
        if((pTemp->pItem->bId & OBEX_TYPE_MASK) == OBEX_TYPE_BYTESEQ)
            delete [] pTemp->pItem->value.ba.pbaData;     
        else if((pTemp->pItem->bId & OBEX_TYPE_MASK) == OBEX_TYPE_UNICODE) {
            PREFAST_SUPPRESS(307, "this is BSTR must call SysFreeString");
            SysFreeString((BSTR)pTemp->pItem->value.pszData);
        }

        delete pTemp->pItem;

        pHeaders = pHeaders->pNext;
        delete pTemp;
    }
    
    return S_OK;
}

HRESULT STDMETHODCALLTYPE 
CHeaderCollection::AddCount( unsigned long ulCount)
{
	DEBUGMSG(OBEX_HEADERCOLLECTION_ZONE,(L"CHeaderCollection::AddCount()\n"));
    return AddLong(OBEX_HID_COUNT, ulCount);
}

HRESULT STDMETHODCALLTYPE 
CHeaderCollection::AddName( LPCWSTR pszName)
{
    DEBUGMSG(OBEX_HEADERCOLLECTION_ZONE,(L"CHeaderCollection::AddName\n"));
    return AddUnicodeString(OBEX_HID_NAME, pszName);
}

HRESULT STDMETHODCALLTYPE 
CHeaderCollection::AddType( unsigned long ulSize,  byte *pData)
{   
	DEBUGMSG(OBEX_HEADERCOLLECTION_ZONE,(L"CHeaderCollection::AddType()\n"));
    return AddByteArray(OBEX_HID_TYPE, ulSize, pData);  
}

HRESULT STDMETHODCALLTYPE 
CHeaderCollection::AddLength( unsigned long ulLength)
{
	return AddLong(OBEX_HID_LENGTH, ulLength);  
}


HRESULT STDMETHODCALLTYPE 
CHeaderCollection::AddTimeOld( unsigned long ulTime)
{
	return AddLong(OBEX_HID_TIME_COMPAT, ulTime);  
}

HRESULT STDMETHODCALLTYPE 
CHeaderCollection::AddTime(FILETIME * pFiletime)
{
   SYSTEMTIME st;
   FileTimeToSystemTime( pFiletime, &st );

   char time[17]; //16 bytes plus NULL

   if (FAILED(StringCchPrintfA(time,sizeof(time),"%4.4d%2.2d%2.2dT%2.2d%2.2d%2.2dZ", st.wYear,st.wMonth,st.wDay, st.wHour,st.wMinute,st.wSecond))) {
      SVSUTIL_ASSERT(0);
      return E_INVALIDARG;
   }
   
   return AddByteArray(OBEX_HID_TIME_ISO, 16, (unsigned char *)time);   
}   

HRESULT STDMETHODCALLTYPE 
CHeaderCollection::AddDescription(LPCWSTR pszDescription)
{
    DEBUGMSG(OBEX_HEADERCOLLECTION_ZONE,(L"CHeaderCollection::AddDescription\n"));
    return AddUnicodeString(OBEX_HID_DESCRIPTION, pszDescription);   
}

HRESULT STDMETHODCALLTYPE 
CHeaderCollection::AddTarget( unsigned long ulSize, byte *pData)
{
	DEBUGMSG(OBEX_HEADERCOLLECTION_ZONE,(L"CHeaderCollection::AddTarget()\n"));
    return AddByteArray(OBEX_HID_TARGET, ulSize, pData);   
} 

HRESULT STDMETHODCALLTYPE 
CHeaderCollection::AddHTTP(unsigned long ulSize, byte *pData)
{
    DEBUGMSG(OBEX_HEADERCOLLECTION_ZONE,(L"CHeaderCollection::AddHTTP\n"));
    return AddByteArray(OBEX_HID_HTTP, ulSize, pData);      
}

HRESULT STDMETHODCALLTYPE 
CHeaderCollection::AddBody( unsigned long ulSize, byte *pData)
{
    DEBUGMSG(OBEX_HEADERCOLLECTION_ZONE,(L"CHeaderCollection::AddBody\n"));
    return AddByteArray(OBEX_HID_BODY, ulSize, pData);  
}

HRESULT STDMETHODCALLTYPE 
CHeaderCollection::AddEndOfBody( unsigned long ulSize, byte *pData)
{
    DEBUGMSG(OBEX_HEADERCOLLECTION_ZONE,(L"CHeaderCollection::AddEndOfBody\n"));
    return AddByteArray(OBEX_HID_BODY_END, ulSize, pData);  
}


HRESULT STDMETHODCALLTYPE 
CHeaderCollection::AddWho(unsigned long ulSize, byte *pData)
{        
    DEBUGMSG(OBEX_HEADERCOLLECTION_ZONE,(L"CHeaderCollection::AddWho\n"));
    return AddByteArray(OBEX_HID_WHO, ulSize, pData);     
}

HRESULT STDMETHODCALLTYPE 
CHeaderCollection::AddConnectionId(unsigned long ulConnectionId)
{
    DEBUGMSG(OBEX_HEADERCOLLECTION_ZONE,(L"CHeaderCollection::AddConnectionId\n"));
    //this is a SPECIAL header, it *MUST* be first... (which means 
    //  *LAST* in our list...
    OBEX_HEADER *pOBHeader = new OBEX_HEADER;
    
    if(NULL == pOBHeader)
        return E_OUTOFMEMORY;
        
    pOBHeader->bId = OBEX_HID_CONNECTIONID;
    pOBHeader->value.dwData = ulConnectionId;
    return InsertHeaderEnd(pOBHeader);
}


HRESULT STDMETHODCALLTYPE 
CHeaderCollection::AddAppParams(unsigned long ulSize, byte *pData)
{
    DEBUGMSG(OBEX_HEADERCOLLECTION_ZONE,(L"CHeaderCollection::AddAppParams\n"));
    return AddByteArray(OBEX_HID_APP_PARAMS, ulSize, pData);     
}

HRESULT STDMETHODCALLTYPE 
CHeaderCollection::AddObjectClass(unsigned long ulSize, byte *pData)
{
	DEBUGMSG(OBEX_HEADERCOLLECTION_ZONE,(L"CHeaderCollection::AddObjectClass\n"));
    return AddByteArray(OBEX_HID_CLASS, ulSize, pData);     
}   

HRESULT STDMETHODCALLTYPE 
CHeaderCollection::EnumHeaders(LPHEADERENUM *_pHeaderEnum)
{
    DEBUGMSG(OBEX_HEADERCOLLECTION_ZONE,(L"CHeaderCollection::EnumHeaders\n"));
    CHeaderEnum *pHeaderEnum = new CHeaderEnum();
   
    if(NULL == pHeaderEnum)
        return E_OUTOFMEMORY;
   
    CHeaderNode *pTemp = pHeaders;
    while(pTemp)
    { 
        pHeaderEnum->InsertFront(pTemp->pItem);
        pTemp = pTemp->pNext;
    }

    *_pHeaderEnum = pHeaderEnum;
   
    return S_OK;
}


HRESULT STDMETHODCALLTYPE 
CHeaderCollection::QueryInterface(REFIID riid, void** ppv) 
{
    DEBUGMSG(OBEX_HEADERCOLLECTION_ZONE,(L"CHeaderCollection::QueryInterface\n"));
    if(!ppv) 
        return E_POINTER;
    if(riid == IID_IUnknown) 
        *ppv = this;
    else if(riid == IID_IHeaderCollection)
        *ppv = static_cast<IHeaderCollection*>(this);
    else 
        return *ppv = 0, E_NOINTERFACE;
    
    return AddRef(), S_OK;	
}


ULONG STDMETHODCALLTYPE 
CHeaderCollection::AddRef() 
{
    DEBUGMSG(OBEX_ADDREFREL_ZONE,(L"CHeaderCollection::AddRef\n"));
    return InterlockedIncrement((LONG *)&_refCount);
}

ULONG STDMETHODCALLTYPE 
CHeaderCollection::Release() 
{   
    DEBUGMSG(OBEX_ADDREFREL_ZONE,(L"CHeaderCollection::Release\n"));
    SVSUTIL_ASSERT(_refCount != 0xFFFFFFFF);
    ULONG ret = InterlockedDecrement((LONG *)&_refCount); 
    
    if(!ret) 
        delete this; 
    return ret;
}
