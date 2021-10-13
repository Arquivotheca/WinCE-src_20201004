//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// Use of this source code is subject to the terms of the Microsoft
// premium shared source license agreement under which you licensed
// this source code. If you did not accept the terms of the license
// agreement, you are not authorized to use this source code.
// For the terms of the license, please see the license agreement
// signed by you and Microsoft.
// THE SOURCE CODE IS PROVIDED "AS IS", WITH NO WARRANTIES OR INDEMNITIES.
//
#pragma once

#include "lxmlEncoding.h"
#include "lxmlElement.h"
#include "lxmlParse.h"

namespace litexml
{

class XmlElement_t;
class XmlAttribute_t;
class XmlParser_t;

typedef LexParseError DocumentError_t;

class XmlDocument_t
{
public:
	friend class XmlParser_t;

	// Construct an empty document
	XmlDocument_t();

	// Construct a socument give a specific root and encoding.
	// The encoding is for informational purposes only, because
	// all supported text formats are converted to UTF-16 internally.
	XmlDocument_t(XmlElement_t* _pRoot,
		DWORD _dwSrcCodePage=Encoding::BcpUtf16);

	// Construct a document given a specific root, meta information,
	// and encoding. The encoding is for informational purposes 
	// only, because all supported text formats are converted
	// to UTF-16 internally.
	XmlDocument_t(XmlElement_t* _pRoot,
		XmlMetaElement_t* _pMeta,
		DWORD _dwSrcCodePage=Encoding::BcpUtf16);

	~XmlDocument_t();

	// Clear the contents of this document. You may optionally
	// decide to manage the lifetime of the XML document contents
	// outside the object instance by specifying FALSE for _fDeleteRoot
	// and/or _fDeleteMeta. This is mainly useful when combining multiple
	// documents into a single large one.
	void Clear(BOOL _fDeleteRoot=TRUE, BOOL _fDeleteMeta=TRUE);

	// Load the document from the specified file, returning
	// non-zero on success. On failure, the return value is 
	// FALSE and _Error.Code is set to a Status::Error* value.
	BOOL Load(LPCTSTR _lpszFile,
		DocumentError_t& _Error,
		DWORD _dwForceCodePage=(DWORD)-1);

	// Load the document from the specified text, returning
	// non-zero on success. On failure, the return value is 
	// FALSE and _Error.Code is set to a Status::Error* value.
	BOOL Parse(const XmlString_t& _Text,
		DocumentError_t& _Error);

	// Load the document from the specified data buffer, returning
	// non-zero on success. On failure, the return value is 
	// FALSE and _Error.Code is set to a Status::Error* value.
	// The text encoding is inferred from the BOM at the beginning
	// of the buffer. If no BOM is present, UTF-8 is assumed.
	BOOL Parse(const LPBYTE _lpData,
		DWORD _dwSize,
		DocumentError_t& _Error);

	// Load the document from the specified data buffer, returning
	// non-zero on success. On failure, the return value is 
	// FALSE and _Error.Code is set to a Status::Error* value.
	// The text encoding is specified by _dwCodePage. The parse will
	// fail of the encoding is unsupported or invalid.
	BOOL Parse(const LPBYTE _lpData,
		DWORD _dwSize,
		DocumentError_t& _Error,
		DWORD _dwCodePage);

public:
	// Retrieve the code page
	DWORD GetCodePage() const
		{ return m_dwSrcCodePage; }

	// Update the code page
	VOID SetCodePage(DWORD _dwCodePage)
		{ m_dwSrcCodePage = _dwCodePage; }

	// Retrieve the root element
	XmlElement_t* GetRoot() const
		{ return m_pRoot; }

	// Retrieve the meta data element. If multiple meta data tags
	// were specified in the actual XML text, the attributes will
	// each have been rolled into one element for ease of access.
	XmlMetaElement_t* GetMeta() const
		{ return m_pMeta; }

	// Converts the calling document into an XML text string,
	// storing the converted text into _Text. Any existing
	// string content is overwritten.
	VOID ToString(XmlString_t& _Text) const;

	Status::Code Save(LPCWSTR _lpFileName,
		DWORD _dwReserveSize=(DWORD)-1) const;

public:
	operator XmlElement_t*()
		{ return m_pRoot; }

	operator XmlElement_t*() const
		{ return m_pRoot; }

protected:
	void Swap(XmlDocument_t& _Other);

protected:
	XmlElement_t*		m_pRoot;
	XmlMetaElement_t*	m_pMeta;
	DWORD				m_dwSrcCodePage;

private:
	XmlDocument_t(const XmlDocument_t&);
	XmlDocument_t& operator =(const XmlDocument_t&);
};


}	// namespace litexml
