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

#include "LiteXml.h"
#include "lxmlLex.h"

namespace litexml
{
class XmlDocument_t;

enum ParseState
{
	PS_UNKNOWN		= -1,	// Invalid

	PS_INITIAL		= 0,	// PS_OPEN_TAG, PS_BEGIN_META
	
	PS_OPEN_TAG,			// PS_TAG_NAME
	PS_TAG_NAME,			// PS_TAG_ATTRIB, PS_END_OPEN_TAG, PS_END_SHORT_TAG
	PS_TAG_ATTRIB,			// PS_TAG_ATTRIB, PS_TAG_EQUALS, PS_END_OPEN_TAG, PS_END_SHORT_TAG
	PS_TAG_EQUALS,			// PS_ATTRIB_VALUE
	PS_ATTRIB_VALUE,		// PS_TAG_ATTRIB, PS_END_OPEN_TAG, PS_END_SHORT_TAG
	PS_END_OPEN_TAG,		// PS_OPEN_TAG, PS_CLOSE_TAG, PS_TEXT
	PS_END_SHORT_TAG,		// PS_OPEN_TAG, PS_CLOSE_TAG, PS_TEXT
	
	PS_CLOSE_TAG,			// PS_CLOSE_TAG_NAME
	PS_CLOSE_TAG_NAME,		// PS_END_CLOSE_TAG
	PS_END_CLOSE_TAG,		// PS_OPEN_TAG, PS_CLOSE_TAG, PS_TEXT

	PS_TEXT,				// PS_OPEN_TAG, PS_CLOSE_TAG

	PS_BEGIN_META,			// PS_META_ATTRIB, PS_END_META
	PS_META_ATTRIB,			// PS_META_EQUALS, PS_META_ATTRIB, PS_END_META
	PS_META_EQUALS,			// PS_META_ATTRIB_VALUE,
	PS_META_ATTRIB_VALUE,	// PS_META_ATTRIB, PS_END_META
	PS_END_META,			// PS_BEGIN_META, PS_OPEN_TAG

	_PS_COUNT_,
};

struct ParseTransition
{
	ParseTransition()
		{ Clear(); }

	void Clear();

	// Table of parse states to transition to given a token
	ParseState		Trans[_TT_COUNT_];

	// An educated guess about what would be most likely expected
	// after this transition if an error were to occur.
	Status::Code	ErrorGuess;

	// Flag specifying whether the state is a final state
	BOOL			fIsFinal;
};

typedef ce::list<XmlElement_t*>	XmlElementStack_t;

#pragma warning (push)
#pragma warning (disable: 4512) // assignment operator could not be generated

struct ParserContext
{
	ParserContext(const XmlLexer_t& _Lexer, LPCXSTR _Text);

	// Clears the data structure without freeing resources
	void ReInit();

	// Aborts the parse, freeing all resources allocated
	// by the calling parse context object
	void Abort();

	const XmlLexer_t&	Lexer;
	LPCXSTR				Text;

	TokenStream_t::const_iterator	Iter;
	ParseState						State;
	XmlElementStack_t				Elems;
	XmlAttribute_t					CurAttrib;

	XmlElement_t*					pRoot;
	XmlMetaElement_t*				pMeta;

	LexParseError					Error;
private:
    ParserContext & operator = (const ParserContext &);
};

typedef void (*PFNPARSEHANDLER)(ParserContext&);

#pragma warning (pop)

class XmlParser_t
{
public:
	XmlParser_t()
		{ }

	void Clear()
		{ m_Error.Clear(); }

	static BOOL Load(XmlDocument_t& _Doc,
		LPCTSTR _lpszFileName,
		LexParseError& _Error,
		DWORD _dwCodePage=(DWORD)-1);

	static BOOL Parse(XmlDocument_t& _Doc,
		const XmlString_t& _Text,
		LexParseError& _Error);

	static BOOL Parse(XmlDocument_t& _Doc,
		const LPBYTE _lpData,
		DWORD _dwSize,
		LexParseError& _Error);

	static BOOL Parse(XmlDocument_t& _Doc,
		const LPBYTE _lpData,
		DWORD _dwSize,
		LexParseError& _Error,
		DWORD _dwCodePage);

private:
	// The parse transition table, and the flag to determine
	// if it's already been initialized
	static ParseTransition s_Transitions[_PS_COUNT_];
	static BOOL s_fInitialized;

	// Initialize the parse transition table. This is only
	// called if the table hasn't already been initialized.
	static void InitializeTransitions();

	// The array of parse state handler procedures
	static const PFNPARSEHANDLER s_fnHandlers[_PS_COUNT_];

private:
	//static void Handle_Initial(ParserContext& _Ps);
	//static void Handle_OpenTag(ParserContext& _Ps);
	static void Handle_TagName(ParserContext& _Ps);
	static void Handle_TagAttrib(ParserContext& _Ps);
	//static void Handle_TagEquals(ParserContext& _Ps);
	static void Handle_AttribValue(ParserContext& _Ps);
	//static void Handle_EndOpenTag(ParserContext& _Ps);
	static void Handle_EndShortTag(ParserContext& _Ps);
	//static void Handle_CloseTag(ParserContext& _Ps);
	static void Handle_CloseTagName(ParserContext& _Ps);
	//static void Handle_EndCloseTag(ParserContext& _Ps);
	static void Handle_Text(ParserContext& _Ps);
	static void Handle_BeginMeta(ParserContext& _Ps);
	static void Handle_MetaAttrib(ParserContext& _Ps);
	//static void Handle_MetaEquals(ParserContext& _Ps);
	static void Handle_MetaAttribValue(ParserContext& _Ps);
	//static void Handle_EndMeta(ParserContext& _Ps);

private:
	static BOOL ParseText(XmlDocument_t& _Doc,
		LexParseError& _Error,
		const XmlLexer_t& _Lexer,
		LPCXSTR _lpText);

private:
	// Execute the parse
	static LexParseError Parse(ParserContext& _Pc);
	
	// When the parse is complete, there are several semantic
	// validations that must be performed. Given the parser
	// context, and information about the previous state,
	// we can both validate the semantics of the document
	// as well as accurately report any possible errors.
	// The ParserContext object is both an input and an output -
	// for example, if it already contains an error, additional 
	// semantic validation is not necessary.
	static BOOL ValidateSemantics(ParserContext& _Pc,
		ParseState _PrevState,
		size_t _PrevPos);

	static LexParseError Analyze(const XmlLexer_t& _Lexer, 
		const XmlChar_t* _lpXml);

private:
	LexParseError	m_Error;
};

}	// namespace litexml