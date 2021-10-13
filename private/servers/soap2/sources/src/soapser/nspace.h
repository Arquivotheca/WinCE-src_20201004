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
//+----------------------------------------------------------------------------
//
// 
// File:    nspace.h
// 
// Contents:
//
//  Header File 
//
//		ISoapSerializer Interface describtion
//	
//
//-----------------------------------------------------------------------------
#ifndef __NSPACE_H_INCLUDED__
	#define __NSPACE_H_INCLUDED__


	HRESULT allocateAndCopy(WCHAR ** target, const WCHAR * source);


	class CNamespaceListEntry : public CDoubleListEntry
	{
		public: 
			CNamespaceListEntry() : m_pcPrefix(NULL), 
									m_pcURI(NULL), 
									lCurrentLevel(0),
									fSerialized(FALSE)
				{};
				
			~CNamespaceListEntry()
				{
				delete[] m_pcPrefix;
				delete[] m_pcURI;
				};

			HRESULT set(const WCHAR *pcPrefix, const WCHAR * pcURI, long lLevel);
			
			WCHAR * getPrefix(void) const
				{ return m_pcPrefix; };
				
			WCHAR * getURI(void) const
				{ return m_pcURI; };
				
			long getLevel(void) const
				{ return lCurrentLevel; };

			BOOL getSerialized(void) const
				{ return fSerialized; };
				
			void setSerialized(BOOL flag) 
				{ fSerialized = flag; };

			
		private:	
			WCHAR *  	m_pcPrefix;
			WCHAR *  	m_pcURI;
			long	 	lCurrentLevel;
			BOOL		fSerialized;
	};				


	class CNamespaceHelper
	{
		public:
			CNamespaceHelper() 
            {
                uNamespaceNumber = 1;
     			lNamespaceLevel = 0;
            };
			~CNamespaceHelper(); 

			HRESULT reset(void);
			HRESULT AddNamespace(const WCHAR * pcPrefix, const WCHAR * pcURI);
			CNamespaceListEntry * FindNamespace(const WCHAR * pcURI, CNamespaceListEntry * pPos);
			CNamespaceListEntry * FindURI(const WCHAR * pcPrefix);
			CNamespaceListEntry * NonSerialized(CNamespaceListEntry * pPos);
			
			void PushLevel(void)
				{ lNamespaceLevel ++; };

			HRESULT PopLevel(void);
			
			long 	getLevel(void) 
				{return lNamespaceLevel;};

		private:
			CTypedDoubleList<CNamespaceListEntry>		dlNamespaceList;
			unsigned long 	uNamespaceNumber;
			long 			lNamespaceLevel;
	};

#endif


// End of File


