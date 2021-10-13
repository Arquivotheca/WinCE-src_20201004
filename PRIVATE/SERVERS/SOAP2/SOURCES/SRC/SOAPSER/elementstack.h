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
// File:    elementstack.h
// 
// Contents:
//
//  Header File 
//
//
//	
//
//-----------------------------------------------------------------------------
#ifndef __ELEMENTSTACK_H_INCLUDED__
	#define __ELEMENTSTACK_H_INCLUDED__

	#include "nspace.h"
    
    class CSoapSerializer; 
    
	// base class to track information about an element or attribute
	class CElement: public CDoubleListEntry
	{
		public: 
			CElement() :	m_pcURI(NULL), 
							m_pcName(NULL),
							m_pcQName(NULL),
							m_pcPrefix(NULL),
							m_pcValue(NULL)
				{};

			virtual ~CElement()
				{ 
					delete[] m_pcURI; 
					delete[] m_pcName;
					delete[] m_pcPrefix;
					delete[] m_pcQName;
					delete[] m_pcValue;
				} 

			HRESULT	Init(void);
			HRESULT setURI(const WCHAR *pcText);
			HRESULT setName(const WCHAR *pcText);
			HRESULT setPrefix(const WCHAR *pcText);
			HRESULT setQName(const WCHAR *pcText);
			HRESULT setValue(const WCHAR *pcText);
			
			WCHAR * getURI(void) const				{ return m_pcURI; };
			WCHAR * getName(void) const				{ return m_pcName; };
			WCHAR * getPrefix(void) const			{ return m_pcPrefix; };
			WCHAR * getQName(void) const			{ return m_pcQName; };
			WCHAR * getValue(void) const			{ return m_pcValue; };

			HRESULT FixNamespace(WCHAR *pDefaultNamespace, CNamespaceHelper * pnsh);
			HRESULT FlushElementContents(CSoapSerializer *pSoapSerializer);
			HRESULT FlushAttributeContents(CSoapSerializer *pSoapSerializer);

		private:	
			WCHAR*		m_pcPrefix;
			WCHAR*  	m_pcURI;
			WCHAR* 		m_pcName;
			WCHAR*		m_pcQName;
			WCHAR*		m_pcValue;
	};


	// this class is used to keep track of the created elements in a document
	// each element keeps track of the attributes associated with it
	class CElementStackEntry : public CElement
	{
		public: 
			CElementStackEntry() :	m_pcDefaultNamespace(NULL),
									m_IsSerialized(FALSE)
				{};
			virtual ~CElementStackEntry();

			HRESULT	Init(void);
			HRESULT setDefaultNamespace(const WCHAR *pcText);
			void setIsSerialized() {m_IsSerialized = TRUE;};
			
			WCHAR * getDefaultNamespace(void) const	{ return m_pcDefaultNamespace; };
			BOOL	getIsSerialized() const 		{ return m_IsSerialized;};


			
			HRESULT AddAttribute(WCHAR * prefix, WCHAR * ns, WCHAR * name, WCHAR * value);

			HRESULT FixNamespace(CNamespaceHelper * pnsh);
			HRESULT FlushElement(CSoapSerializer *pSerializer);


		private:	
			BOOL									m_IsSerialized;
			WCHAR*									m_pcDefaultNamespace;
			CTypedDoubleList<CElement>				m_AttributeList;
	};


	// the stack of elements in an document
	class CElementStack
	{
		public:
			CElementStack();
			~CElementStack(); 

			HRESULT reset(void);
			void Push(CElementStackEntry * pele);
			CElementStackEntry* Pop(void);
			CElementStackEntry* Peek(void) const;

			HRESULT AddAttribute(WCHAR * prefix, WCHAR * ns, WCHAR * name, WCHAR * value);
		private:
			CTypedDoubleList<CElementStackEntry>		m_ElementStack;
	};

#endif


// End of File


