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

class XmlAttribute_t
{
public:
	explicit XmlAttribute_t(const XmlString_t& _strName = _X(""),
		const XmlString_t& _strValue = _X("")) :
	m_strName(_strName),
	m_strValue(_strValue)
		{ }

public:
	// Retrieve the attribute name
	const XmlString_t& Name() const		{ return m_strName; }

	// Retrieve or modify the attribute value
	const XmlString_t& Value() const	{ return m_strValue; }
	XmlString_t& Value()				{ return m_strValue; }

public:
	bool operator ==(const XmlAttribute_t& _Rhs) const
		{ return Name() == _Rhs.Name(); }
	bool operator !=(const XmlAttribute_t& _Rhs) const
		{ return Name() != _Rhs.Name(); }
	bool operator <(const XmlAttribute_t& _Rhs) const
		{ return Name() < _Rhs.Name(); }

private:
	XmlString_t	m_strName;
	XmlString_t	m_strValue;
};

}	// namespace litexml
