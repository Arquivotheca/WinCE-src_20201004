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
#include "SAXParser.h"
#include "tuxmain.h"

// -------------------------------------------- //
//                                              //
// SAXParser                                   //
//                                              //
// -------------------------------------------- //
SAXParser::SAXParser()
	: m_pszName(NULL),
	  m_cchName(0),
	  m_pszAttributeName(NULL),
	  m_cchAttributeName(0),
	  m_pszAttributeValue(NULL),
	  m_cchAttributeValue(0),
	  m_pszCharacters(NULL),
	  m_cchCharacters(0)
{
}

// -------------------------------------------- //
//                                              //
// ~SAXParser                                  //
//                                              //
// -------------------------------------------- //
SAXParser::~SAXParser()
{
	delete [] m_pszName;
	delete [] m_pszAttributeName;
	delete [] m_pszAttributeValue;
	delete [] m_pszCharacters;
}

// -------------------------------------------- //
//                                              //
// Parse the xml file provided                                      //
//                                              //
// -------------------------------------------- //
bool SAXParser::Parse(const char *szFilename)
{
	if (szFilename == NULL || *szFilename == '\0')
	{
		return false;
	}

	if (!m_scanner.Init(szFilename))
	{
		return false;
	}

	if (!Document())
	{
		LOG(TEXT("%S: ERROR %d@%S - Failed to parse %S\n"), __FUNCTION__, __LINE__, __FILE__, szFilename);
		return false;
	}

	return true;
}

// ------------------------------------ //
//                                      //
// Document := '<' Element              //
//                                      //
// ------------------------------------ //
bool SAXParser::Document()
{
	if (!StartDocument())
	{
		return false;
	}

	m_scanner.SkipWhiteSpace();

	if (!m_scanner.Match('<'))
	{
		return false;
	}

	m_scanner.Advance();

	if (!Element())
	{
		return false;
	}

	if (!EndDocument())
	{
		return false;
	}
	
	return true;
}

// --------------------------------------- //
//                                         //
// Element := Name Atributes ElementCont   //
//                                         //
// --------------------------------------- //
bool SAXParser::Element()
{
	if (!Name())
	{
		return false;
	}

	//skip comments before getting to the actual text of the file
	//primarily here to skip copyright tags
	SkipComment();

	if (!StartElement(m_pszName, m_cchName))
	{
		return false;
	}

	if (!Attributes())
	{
		return false;
	}

	return ElementCont();
}

// --------------------------------------- //
//                                         //
// SkipComment :=  Will skip all tags begining with !-- and ?xml
//                                         //
// --------------------------------------- //
bool SAXParser::SkipComment()
{
	char szComment [4] = {'!','-', '-', '\0'};
	char szInfo [5] = {'?', 'x', 'm', 'l', '\0'};

	while(!strncmp(m_pszName, szComment, 3) || !strncmp(m_pszName, szInfo, 4)) {
		// read/advance till end of the tag
		if (!Characters())
		{
			// Report failure
			return false;
		}

		//Advance 1 character so skip the '<' of the next tag
		m_scanner.Advance();	

		//put the next tag name in m_pszName
		if(!Name())
		{
			return false;
		}
	}
	
	return true;
}

// ------------------------------------------- //
//                                             //
// ElementCont := '/' '>' |                    //
//                '>' Content EndElement       //
//                                             //
// ------------------------------------------- //
bool SAXParser::ElementCont()
{
	m_scanner.SkipWhiteSpace();

	bool fMatchFS = m_scanner.Match('/');
	bool fMatchGT = m_scanner.Match('>');

	if (!fMatchFS && !fMatchGT)
	{
		return false;
	}

	m_scanner.Advance();

	if (fMatchFS)
	{
		m_scanner.SkipWhiteSpace();

		if (!m_scanner.Match('>'))
		{
			return false;
		}

		if (!EndElement(m_pszName, m_cchName))
		{
			return false;
		}

		return m_scanner.Advance();
	}

	if (!Content())
	{
		return false;
	}

	return EndElement();
}

// -------------------------------------------- //
//                                              //
// Content := Characters '<' Elements           //
//                                              //
// -------------------------------------------- //
bool SAXParser::Content()
{
	if (!Characters())
	{
		return false;
	}

	if (!Characters(m_pszCharacters, m_cchCharacters))
	{
		return false;
	}

	m_scanner.SkipWhiteSpace();

	if (!m_scanner.Match('<'))
	{
		return false;
	}

	m_scanner.Advance();

	return Elements();
}

// -------------------------------------------- //
//                                              //
// Elements := '/' |                            //
//             Element Characters '<' Elements  //
//                                              //
// -------------------------------------------- //
bool SAXParser::Elements()
{
	m_scanner.SkipWhiteSpace();

	if (m_scanner.Match('/'))
	{
		return m_scanner.Advance();
	}

	if (!Element())
	{
		return false;
	}

	if (!Characters())
	{
		return false;
	}

	if (!Characters(m_pszCharacters, m_cchCharacters))
	{
		return false;
	}

	m_scanner.SkipWhiteSpace();

	if (!m_scanner.Match('<'))
	{
		return false;
	}

	m_scanner.Advance();

	return Elements();
}

// -------------------------------------------- //
//                                              //
// EndElement := Name '>'                       //
//                                              //
// -------------------------------------------- //
bool SAXParser::EndElement()
{
	if (!Name())
	{
		return false;
	}

	m_scanner.SkipWhiteSpace();

	if (!m_scanner.Match('>'))
	{
		return false;
	}

	if (!EndElement(m_pszName, m_cchName))
	{
		return false;
	}

	return m_scanner.Advance();
}

// -------------------------------------------- //
//                                              //
// Name                                         //
//                                              //
// -------------------------------------------- //
bool SAXParser::Name()
{
	m_scanner.SkipWhiteSpace();

	delete [] m_pszName;
	m_pszName = NULL;

	m_cchName = 0;

	return m_scanner.GetName(&m_pszName, &m_cchName) && m_pszName && strlen(m_pszName);
}

// ---------------------------------------- //
//                                          //
// Attributes := empty |                    //
//               Attribute Attributes       //
//                                          //
// ---------------------------------------- //
bool SAXParser::Attributes()
{
	m_scanner.SkipWhiteSpace();

	if (m_scanner.Match('/') || m_scanner.Match('>'))
	{
		return true;
	}

	if (!Attribute())
	{
		return false;
	}

	return Attributes();
}

// --------------------------------------------- //
//                                               //
// Attribute := AttributeName '=' AttributeValue //
//                                               //
// --------------------------------------------- //
bool SAXParser::Attribute()
{
	if (!AttributeName())
	{
		return false;
	}

	if (!Attribute(m_pszAttributeName, m_cchAttributeName))
	{
		return false;
	}

	m_scanner.SkipWhiteSpace();

	if (!m_scanner.Match('='))
	{
		return false;
	}

	m_scanner.Advance();

	if (!AttributeValue())
	{
		return false;
	}

	if (!AttributeValue(m_pszAttributeValue, m_cchAttributeValue))
	{
		return false;
	}

	return true;
}

// -------------------------------------------- //
//                                              //
// AttributeName                                //
//                                              //
// -------------------------------------------- //
bool SAXParser::AttributeName()
{
	m_scanner.SkipWhiteSpace();

	delete [] m_pszAttributeName;
	m_pszAttributeName = NULL;

	m_cchAttributeName = 0;

	return m_scanner.GetAttributeName(&m_pszAttributeName, &m_cchAttributeName);
}

// -------------------------------------------- //
//                                              //
// AttributeValue := ''' Value ''' |            //
//                   '"' Value '"'              //
//                                              //
// -------------------------------------------- //
bool SAXParser::AttributeValue()
{
	m_scanner.SkipWhiteSpace();

	bool fMatchSQ = m_scanner.Match('\'');
	bool fMatchDQ = m_scanner.Match('\"');
	if (!fMatchSQ && !fMatchDQ)
	{
		return false;
	}

	m_scanner.Advance();

	delete [] m_pszAttributeValue;
	m_pszAttributeValue = NULL;

	m_cchAttributeValue = 0;

	if (!m_scanner.GetAttributeValue(&m_pszAttributeValue, &m_cchAttributeValue))
	{
		return false;
	}

	if (fMatchSQ)
	{
		if (!m_scanner.Match('\''))
		{
			return false;
		}

		return m_scanner.Advance();
	}

	if (!m_scanner.Match('\"'))
	{
		return false;
	}

	return m_scanner.Advance();
}

// -------------------------------------------- //
//                                              //
// Characters                                   //
//                                              //
// -------------------------------------------- //
bool SAXParser::Characters()
{
	delete [] m_pszCharacters;
	m_pszCharacters = NULL;

	m_cchCharacters = 0;

	return m_scanner.GetCharacters(&m_pszCharacters, &m_cchCharacters);
}

