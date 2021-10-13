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

namespace litexml
{

struct SearchContext;

class XmlBaseElement_t;
class XmlElement_t;
class XmlTextElement_t;
class XmlCDataElement_t;
class XmlSearch_t;

typedef ce::hash_map<XmlString_t, XmlAttribute_t>	XmlAttributeHash_t;
typedef ce::list<XmlBaseElement_t*>					XmlElementList_t;
typedef ce::list<const XmlBaseElement_t*>			XmlConstElementList_t;

/////////////////////////////////////////////////////////////////////
// class XmlAttributeWalker_t
/////////////////////////////////////////////////////////////////////

#pragma warning (push)
#pragma warning (disable: 4512) // assignment operator could not be generated

class XmlAttributeWalker_t
{
public:
	XmlAttributeWalker_t(const XmlAttributeHash_t& _Attribs) :
		m_Attribs(_Attribs),
		m_Iter(_Attribs.begin())
	{ }

public:
	const XmlAttribute_t* Get() const
		{ return &m_Iter->second; }

	bool Next()
	{
		if( !End() )
			m_Iter++;

		return m_Iter != m_Attribs.end();
	}

	bool End() const
		{ return (m_Iter == m_Attribs.end()); }

protected:
	const XmlAttributeHash_t&			m_Attribs;
	XmlAttributeHash_t::const_iterator	m_Iter;

private:
    XmlAttributeWalker_t & operator = (const XmlAttributeWalker_t &);
};

#pragma warning (pop)

/////////////////////////////////////////////////////////////////////
// class XmlElementTypeWalker_t
/////////////////////////////////////////////////////////////////////

class XmlChildWalker_t
{
public:
	// Searches for all tags of the specified name and type.
	// If no name is specified (i.e. empty string), names are ignored.
	XmlChildWalker_t(const XmlBaseElement_t *_pElem,
		DWORD _dwTypeMask=0xFFFFFFFF,
		const XmlString_t& _strName=_X(""));

	// Searches for all 'standard' tags with the specified name.
	// If no name is specified (i.e. empty string), names are ignored.
	XmlChildWalker_t(const XmlBaseElement_t *_pElem,
		const XmlString_t& _strName);
public:
	bool Next();
	bool End() const;

	const XmlBaseElement_t *Get() const
		{ return *m_Iter; }

private:
	void FindFirst();
	
	bool IsMatch(const XmlBaseElement_t *_pElem);

private:
	const XmlBaseElement_t*				m_pElem;
	DWORD								m_dwTypeMask;
	XmlString_t							m_strName;
	XmlElementList_t::const_iterator	m_Iter;
};

typedef BOOL (*LPXMLTRAVERSALPROC)(const XmlBaseElement_t*,LPVOID);

/////////////////////////////////////////////////////////////////////
// class XmlBaseElement_t
/////////////////////////////////////////////////////////////////////

class XmlBaseElement_t
{
public:
	friend class XmlAttributeWalker_t;
	friend class XmlChildWalker_t;
	friend class XmlSearch_t;

    static const XmlString_t Crlf;

	enum ElementType {
		EtElement	= 0x00000001,	// Normal element
		EtStandard	= 0x00000001,	// Alias for EtElement

		EtMeta		= 0x00000002,	// Meta data element

		EtText		= 0x00000004,	// Text
		EtCData		= 0x00000008,	// CData

		EtAllText		= EtText | EtCData,
		EtAll			= 0xFFFFFFFF,
	};

public:
	XmlBaseElement_t()					{ }
	virtual ~XmlBaseElement_t() = NULL;

public:
	// Determine if the specified element is a direct
	// child of the calling object
	bool IsChild(XmlBaseElement_t *_pChild) const;

	// Retrieve the number of children
	size_t GetChildCount() const
		{ return m_lstChildren.size(); }

	// Remove all child nodes under the calling object
	void RemoveChildren(bool _fDelete=true);

	// Retrieve the name of this tag
	const XmlString_t& GetName() const
		{ return m_strName; }

public:
	// Retrieve the number of attributes
	size_t GetAttributeCount() const
		{ return m_lstAttributes.size(); }

	// Determine if the specified attribute exists
	bool HasAttribute(const XmlString_t& _Name) const
		{ return (m_lstAttributes.find(_Name) != m_lstAttributes.end()); }
	
	// Get at the attributes by name. If the attribute does not
	// exist, a runtime error will be thrown.
	const XmlString_t& GetAttribute(const XmlString_t& _Name) const
		{ return m_lstAttributes.find(_Name)->second.Value(); }

	// Get the text value of an attribute. If the attribute does
	// not exist, the return value will be FALSE.
	BOOL GetAttributeText(const XmlString_t& _Name,
		XmlString_t* _lpOut) const;

	// Get the attribute's numeric value. If the attribute does
	// not exist, or is not of the proper numeric format, the
	// return value will be FALSE.
	BOOL GetAttributeInt(const XmlString_t& _Name, LPINT _lpOut) const
		{ return GetAttributeFmt<INT>(_Name, _lpOut, _X("%d")); }
	BOOL GetAttributeDword(const XmlString_t& _Name, LPDWORD _lpOut) const
		{ return GetAttributeFmt<DWORD>(_Name, _lpOut, _X("%u")); }
	BOOL GetAttributeFloat(const XmlString_t& _Name, PFLOAT _lpOut) const
		{ return GetAttributeFmt<FLOAT>(_Name, _lpOut, _X("%f")); }

	// Reads <major-ver>.<minor-ver>-style version information,
	// storing this information into the least-significant WORD
	// of the output. At the end of a successful call, bytes
	// two and three of the result will be cleared, and byte one
	// will contain the major version and byte zero the minor version.
	// In short, this method converts a value like "2.1" to 0x0201.
	// Returns false if the attribute doesn't exist or is malformatted.
	BOOL GetAttributeVersion(const XmlString_t& _Name,
		LPDWORD _lpOut) const;

	// Use array-indexing to get at the attributes
	const XmlString_t& operator [](const XmlString_t& _Name) const
		{ return GetAttribute(_Name); }

public:

	// Retrieve the raw text representation of the element
	virtual XmlString_t GetText() const;

	virtual void SetText(const XmlString_t& _Text);

public:
	// Retrieve the element type
	virtual ElementType GetType() const = NULL;
	
	// Convert the element to a string
	virtual const XmlString_t& ToString() const = NULL;

	// Expand this tag  as XML onto the end of the specified
	// string, thus reconstructing the XML of this object.
    virtual void ExpandXml(XmlString_t& _strXml) const
        { _strXml += XmlEscape_t::Escape(ToString()); }
    virtual void ExpandXmlFormatted(XmlString_t& _strXml, const XmlString_t& _strPad) const;

	// Perform a iterative depth-first traversal of the XML
	// hierarchy, starting at the calling element. For each
	// node, the specified callback function is called, passing
	// the current node and the specified parameter. The traversal
	// continues until all elements have been reached, or the
	// callback returns FALSE. If the traversal stops due to early
	// completion by the callback, the return value is the success 
	// code Status::TraversalAborted. Optionally, an element type
	// mask may be specified, in which case only elements of
	// a matching type will be included in the traversal.
	Status::Code Traverse(LPXMLTRAVERSALPROC _pfnCallback,
		LPVOID _lpParam,
		DWORD _dwTypeMask=0xFFFFFFFF) const;
	Status::Code Traverse(LPXMLTRAVERSALPROC _pfnCallback,
		LPVOID _lpParam,
		const XmlString_t& _strName,
		DWORD _dwTypeMask=0xFFFFFFFF) const;

	// Walk all attributes of the calling object
	XmlAttributeWalker_t WalkAttributes() const
		{ return XmlAttributeWalker_t(m_lstAttributes); }

protected:

protected:
	XmlBaseElement_t(const XmlString_t& _strName,
		const XmlString_t& _strText = _X("")) :
	m_strName(_strName),
	m_strText(_strText)
		{ }

protected:
	template <typename T>
	BOOL GetAttributeFmt(const XmlString_t& _Name,
		T *_lpOut,
		LPCXSTR _lpFmt) const;

protected:
	XmlString_t			m_strName;
	XmlString_t			m_strText;
	XmlAttributeHash_t	m_lstAttributes;
	XmlElementList_t	m_lstChildren;
};


template <typename T>
BOOL XmlBaseElement_t::GetAttributeFmt(const XmlString_t& _Name,
									   T *_lpOut,
									   LPCXSTR _lpFmt) const
{
	if( _lpOut == NULL )
		return FALSE;

	XmlAttributeHash_t::const_iterator iter;

	if( (iter = m_lstAttributes.find(_Name)) == m_lstAttributes.end() )
		return FALSE;

	return (swscanf_s(iter->second.Value(),
		_lpFmt,
		_lpOut) == 1);
}

}	// namespace litexml

