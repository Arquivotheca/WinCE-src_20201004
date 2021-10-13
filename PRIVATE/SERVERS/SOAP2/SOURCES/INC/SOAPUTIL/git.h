//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
//+----------------------------------------------------------------------------
//
//
// File:
//      Utility.h
//
// Contents:
//
//      Various utilities
//
//-----------------------------------------------------------------------------

#ifndef __GIT_H_INCLUDED__
#define __GIT_H_INCLUDED__


#ifdef UNDER_CE
extern CRITICAL_SECTION g_csGIT;
extern DWORD		    g_dwGITRefCnt;
#endif 


template <class Itf, const IID* piid>
class GlobalInterfacePointer {
   DWORD m_dwCookie; // the GIT cookie prevent misuse
   Itf *_pItf;
   GlobalInterfacePointer(const GlobalInterfacePointer&);
   void operator =(const GlobalInterfacePointer&);
public:
// start as invalid cookie
   GlobalInterfacePointer(void) : m_dwCookie(0), _pItf(NULL) {   
#ifdef UNDER_CE
		EnterCriticalSection(&g_csGIT);
		
		if(0 == g_dwGITRefCnt)
		{
			//if this fails, set the GIT to NULL 
			if(FAILED(CoCreateInstance(CLSID_StdGlobalInterfaceTable,
                            NULL,
                            CLSCTX_INPROC_SERVER,
                            IID_IGlobalInterfaceTable,
                            (void **)&g_pGIT)))
                            g_pGIT = NULL;
		}
		g_dwGITRefCnt ++;
		LeaveCriticalSection(&g_csGIT);
#endif 
   }

// start with auto-globalized local pointer
   GlobalInterfacePointer(Itf *pItf, HRESULT& hr) 
      : m_dwCookie(0), _pItf(NULL) { 
#ifdef UNDER_CE
		EnterCriticalSection(&g_csGIT);
		
		if(0 == g_dwGITRefCnt)
		{
			//if this fails, set the GIT to NULL 
			if(FAILED(CoCreateInstance(CLSID_StdGlobalInterfaceTable,
                            NULL,
                            CLSCTX_INPROC_SERVER,
                            IID_IGlobalInterfaceTable,
                            (void **)&g_pGIT)))
                            g_pGIT = NULL;
		}	
		g_dwGITRefCnt ++;
		LeaveCriticalSection(&g_csGIT);
#endif 
      hr = Globalize(pItf); 
	}

// auto-unglobalize
   ~GlobalInterfacePointer(void) 
   { 
   Unglobalize(); 

#ifdef UNDER_CE
   	EnterCriticalSection(&g_csGIT);
	g_dwGITRefCnt --;
	if(0 == g_dwGITRefCnt)
	{
		if(g_pGIT)
			g_pGIT->Release();
		g_pGIT = NULL;		
	}	
	LeaveCriticalSection(&g_csGIT);
#endif 
   }

// register an interface pointer in GIT
   HRESULT Globalize(Itf *pItf) {
   	#ifdef UNDER_CE
		ASSERT(0 != g_dwGITRefCnt);
	#endif
	
#ifndef UNDER_CE
      ASSERT(g_pGIT != 0 && m_dwCookie == 0);
      return g_pGIT->RegisterInterfaceInGlobal(pItf,
         *piid, &m_dwCookie);
#else
	  HRESULT hr = E_FAIL;

	  if(g_pGIT)
      	hr = g_pGIT->RegisterInterfaceInGlobal(pItf,
         *piid, &m_dwCookie);

	  //if we dont g_pGIT, we dont have DCOM --
	  //  so we wont be using the FTM.  simply cache the pointer
	  //  we can do this because w/o DCOM there is no such thing
	  //  as a STA
	  else if(!g_pGIT && !m_dwCookie) 
	  {
	  	pItf->AddRef();
	  	_pItf = pItf;	  	
		hr = S_OK;		
	  }	
	  return hr;
#endif 
   }

// revoke an interface pointer in GIT
   HRESULT Unglobalize(void) 
   {
#ifdef UNDER_CE
		ASSERT(0 != g_dwGITRefCnt);
#endif

#ifndef UNDER_CE
        ASSERT(g_pGIT != 0);
        HRESULT hr = S_OK;
        if (m_dwCookie != 0)
        {
            hr=g_pGIT->RevokeInterfaceFromGlobal(m_dwCookie);
        }    
        m_dwCookie = 0;
        
        return hr;
#else
        HRESULT hr = S_OK;
        if (g_pGIT && m_dwCookie != 0)
        {
            hr=g_pGIT->RevokeInterfaceFromGlobal(m_dwCookie);
        }    
        else // <-- we dont have DCOM
    	{
    		if(_pItf)
			{
    			_pItf->Release();
    			_pItf = NULL;
			}
    	}
        m_dwCookie = 0;
        
        return hr;
#endif 
   }


// get a local interface pointer from GIT
   HRESULT Localize(Itf **ppItf) const {
#ifdef UNDER_CE
		ASSERT(0 != g_dwGITRefCnt);
#endif
   
#ifndef UNDER_CE
      ASSERT(g_pGIT != 0 && m_dwCookie != 0);
      return g_pGIT->GetInterfaceFromGlobal(m_dwCookie, 
         *piid,(void**)ppItf);
#else
	 HRESULT hr = E_FAIL;

	 if(g_pGIT)
	 	hr = g_pGIT->GetInterfaceFromGlobal(m_dwCookie, *piid,(void**)ppItf);
	 
	 //if we are here, we dont have DCOM
	 //  so simply addref the contained interface and return it
	 //  directly
	 else if(m_dwCookie == 0)
 	 {
 	 	if(_pItf)
 		{
 		   _pItf->AddRef();
 		   *ppItf = _pItf;
 		   hr = S_OK;
 		}
 	 }
	 return hr;
#endif 
   }
// convenience methods
   bool IsOK(void) const 
   { 
 #ifdef UNDER_CE
	  ASSERT(0 != g_dwGITRefCnt);
 
      if(!g_pGIT && !m_dwCookie && _pItf)
      	return S_OK;
#endif      
      return m_dwCookie != 0; 
   }
   DWORD GetCookie(void) const { return m_dwCookie; }
};


template <class Itf, const IID* piid>
class LocalInterfacePointer {
      Itf *m_pItf; // temp imported pointer
// prevent misuse
      LocalInterfacePointer(const LocalInterfacePointer&);
      operator = (const LocalInterfacePointer&);
public:
   LocalInterfacePointer(const GlobalInterfacePointer<Itf, 
         piid>& rhs,HRESULT& hr) {

#ifdef UNDER_CE
		EnterCriticalSection(&g_csGIT);
		
		if(0 == g_dwGITRefCnt)
		{
			//if this fails, set the GIT to NULL 
			if(FAILED(CoCreateInstance(CLSID_StdGlobalInterfaceTable,
                            NULL,
                            CLSCTX_INPROC_SERVER,
                            IID_IGlobalInterfaceTable,
                            (void **)&g_pGIT)))
                            g_pGIT = NULL;			
		}	
		g_dwGITRefCnt ++;
		LeaveCriticalSection(&g_csGIT);
#endif 
         
      hr = rhs.Localize(&m_pItf);
   }

   LocalInterfacePointer(DWORD dwCookie, HRESULT& hr) {
      ASSERT(g_pGIT != 0);

#ifdef UNDER_CE
		EnterCriticalSection(&g_csGIT);
		
		if(0 == g_dwGITRefCnt)
		{
			//if this fails, set the GIT to NULL 
			if(FAILED(CoCreateInstance(CLSID_StdGlobalInterfaceTable,
                            NULL,
                            CLSCTX_INPROC_SERVER,
                            IID_IGlobalInterfaceTable,
                            (void **)&g_pGIT)))
                            g_pGIT = NULL;			
		}	
		g_dwGITRefCnt ++;
		LeaveCriticalSection(&g_csGIT);
#endif 
      
      hr = g_pGIT->GetInterfaceFromGlobal(dwCookie, *piid, 
         (void**)&m_pItf);
   }
   ~LocalInterfacePointer(void)
    { 
 #ifdef UNDER_CE
		ASSERT(0 != g_dwGITRefCnt);
 #endif
    	if(m_pItf) 
    		m_pItf->Release(); 

    	EnterCriticalSection(&g_csGIT);    	  	
		g_dwGITRefCnt --;
		if(0 == g_dwGITRefCnt)
		{
			if(g_pGIT)
				g_pGIT->Release();
			g_pGIT = NULL;		
		}	
		LeaveCriticalSection(&g_csGIT);
    }

   class SafeItf : public Itf {
      STDMETHOD_(ULONG, AddRef)(void) = 0;  // hide
      STDMETHOD_(ULONG, Release)(void) = 0; // hide
   };

   SafeItf *GetInterface(void) const  
   { 
#ifdef UNDER_CE
		ASSERT(0 != g_dwGITRefCnt);
#endif
   		return (SafeItf*)m_pItf; 
   }

   SafeItf *operator ->(void) const 
   { 
#ifdef UNDER_CE
	   ASSERT(0 != g_dwGITRefCnt);
#endif
	   ASSERT(m_pItf != 0); 
	   return GetInterface();  
   }
};



#define LIP(Itf) LocalInterfacePointer<Itf, &IID_##Itf>
#define GIP(Itf) GlobalInterfacePointer<Itf, &IID_##Itf>

 
#endif //__GIT_H_INCLUDED__
