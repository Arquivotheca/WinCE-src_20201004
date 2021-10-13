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

#include "lxmlAttribute.h"
#include "lxmlBaseElement.h"

namespace litexml
{

/////////////////////////////////////////////////////////////////////
// class XmlAttributeElement_t
/////////////////////////////////////////////////////////////////////

class XmlAttributeElement_t : public XmlBaseElement_t
{
public:
	XmlAttributeElement_t(const XmlString_t& _strName) :
	  XmlBaseElement_t(_strName)
	{ }

public:

	// Set the value of an attribute
	bool SetAttribute(const XmlAttribute_t& _Attrib);
	bool SetAttribute(const XmlString_t& _Name, const XmlString_t& _Value);

	// Remove an attribute
	void RemoveAttribute(const XmlString_t& _Name);

protected:
	virtual void ExpandAttributes(XmlString_t& _strXml) const;
};

/////////////////////////////////////////////////////////////////////
// class XmlElement_t
/////////////////////////////////////////////////////////////////////

class XmlElement_t : public XmlAttributeElement_t
{
public:
	XmlElement_t(const XmlString_t& _strName) :
		XmlAttributeElement_t(_strName)
	{ }

	virtual ~XmlElement_t() { }

public:
	// Concatenates the text of all direct children and
	// returns the resulting string
	virtual XmlString_t GetText() const;

	// Removes all child elements and sets the element's
	// child text to the specified value
	virtual void SetText(const XmlString_t& _Text);

	// Retrieve the element type
	virtual ElementType GetType() const
		{ return XmlBaseElement_t::EtStandard; }
	
	// Convert the element to a string
	virtual const XmlString_t& ToString() const
		{ return GetName(); }

	// Expand this tag and all children to an XML string
    virtual void ExpandXml(XmlString_t& _strXml) const;
    virtual void ExpandXmlFormatted(XmlString_t& _strXml, const XmlString_t& _strPad) const;

public:
	bool PrependChild(XmlBaseElement_t *_pNew);
	bool AppendChild(XmlBaseElement_t *_pNew);

	bool InsertBefore(const XmlBaseElement_t *_pChild,
		XmlBaseElement_t *_pNew);
	bool InsertAfter(const XmlBaseElement_t *_pChild,
		XmlBaseElement_t *_pNew);

	bool RemoveChild(const XmlBaseElement_t *_pChild, bool _fDelete=true);
    
};

/////////////////////////////////////////////////////////////////////
// class XmlMetaElement_t
/////////////////////////////////////////////////////////////////////

class XmlMetaElement_t : public XmlAttributeElement_t
{
public:
	XmlMetaElement_t() :
		XmlAttributeElement_t(XmlMetaElement_t::TagName)
	{
	}

	virtual ElementType GetType() const
		{ return XmlBaseElement_t::EtMeta; }

	virtual const XmlString_t& ToString() const
		{ return GetName(); }
	
	// Expand this tag to an XML string
    virtual void ExpandXml(XmlString_t& _strXml) const;
    virtual void ExpandXmlFormatted(XmlString_t& _strXml, const XmlString_t& _strPad) const;

private:
	static const XmlChar_t	TagName[];
};

/////////////////////////////////////////////////////////////////////
// class XmlTextElement_t
/////////////////////////////////////////////////////////////////////

class XmlTextElement_t : public XmlBaseElement_t
{
public:
    XmlTextElement_t(const XmlString_t& _strText) :
      XmlBaseElement_t(XmlTextElement_t::TagName, _strText)
      {
          m_strText.trim(_X(" \t\r\f\n"));
      }

	virtual ElementType GetType() const
		{ return XmlBaseElement_t::EtText; }

	virtual const XmlString_t& ToString() const
		{ return m_strText; }

	virtual void SetText(const XmlString_t& _Text);

protected:
	XmlTextElement_t(const XmlString_t& _strName, const XmlString_t& _strText) :
		XmlBaseElement_t(_strName, _strText)
	{ }

private:
	static const XmlChar_t	TagName[];
};

/////////////////////////////////////////////////////////////////////
// class XmlCDataElement_t
/////////////////////////////////////////////////////////////////////

class XmlCDataElement_t : public XmlTextElement_t
{
public:
	XmlCDataElement_t(const XmlString_t& _strText);

	virtual ~XmlCDataElement_t()
		{ }

public:
	virtual void SetText(const XmlString_t& _strText);

    virtual void ExpandXml(XmlString_t& _strXml) const
        { _strXml += ToString(); }

public:
	virtual ElementType GetType() const
		{ return XmlBaseElement_t::EtCData; }
	
	virtual const XmlString_t& ToString() const
		{ return GetCData(); }

	const XmlString_t& GetCData() const
		{ return m_strCData; }

public:
	static const XmlString_t CDataOpen;
	static const XmlString_t CDataClose;

private:
	static const XmlChar_t	TagName[];

private:
	// The CDATA-style text used by ToString
	XmlString_t m_strCData;
};




}	// namespace litexml