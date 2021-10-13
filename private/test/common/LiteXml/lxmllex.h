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

#include <strsafe.h>
#include <list.hxx>

#include "lxmlEncoding.h"

namespace litexml
{

    /////////////////////////////////////////////////////////////////////
    // Token types
    /////////////////////////////////////////////////////////////////////
    enum TokenType
    {
        TT_UNKNOWN = -1,

        TT_BEGIN_XML_META = 0,
        TT_END_XML_META,
        TT_BEGIN_COMMENT,
        TT_END_COMMENT,
        TT_BEGIN_CDATA,
        TT_END_CDATA,
        TT_END_SHORT_TAG,
        TT_BEGIN_CLOSE_TAG,
        TT_BEGIN_TAG,
        TT_END_TAG,
        TT_ASSIGN,
        TT_STRING_1,
        TT_STRING_2,

        _TT_FIXED_COUNT_,					// Number of fixed tokens

        TT_IDENTIFIER = _TT_FIXED_COUNT_,
        TT_TEXT,
        TT_RAW_TEXT,

        _TT_COUNT_,							// Number of actual tokens
    };

    struct LexemeInfo
    {
        const XmlChar_t*	lpLexeme;
        size_t				uLength;
    };

    /////////////////////////////////////////////////////////////////////
    // Token datastructure
    /////////////////////////////////////////////////////////////////////

    struct Token
    {
        Token(int _Type=0,
            size_t _Pos=0,
            size_t _Length=0,
            const XmlString_t& _Lexeme=_X(""),
            int _Error=0);

        void Clear();

        int			Type;	// Member of the TokenType enumeration
        size_t		Pos;	// Position of the token's lexeme
        size_t		Length;	// Length of the token's lexeme
        XmlString_t	Lexeme;	// The lexeme
        int			Error;	// Error code (Status::Code)
    };


    /////////////////////////////////////////////////////////////////////
    // Lexical analyzer context
    /////////////////////////////////////////////////////////////////////

    struct LexerContext
    {
        LexerContext(const XmlChar_t *_Text,
            size_t _Pos=0) :
        Text(_Text),
            Pos(_Pos),
            InTag(FALSE),
            TokGuessId(TT_UNKNOWN)
        { 
            StringCchLength(_Text, STRSAFE_MAX_CCH, &Length); 
        }

        const XmlChar_t*	Text;
        size_t				Pos;
        size_t				Length;
        BOOL				InTag;
        Token				CurTok;
        int					TokGuessId;
    };

    struct TextLocation
    {
        TextLocation(int _LineNum=0,
            int _ColumnNum=0) :
        LineNum(_LineNum),
            ColumnNum(_ColumnNum)
        { }

        void Clear()
        {
            LineNum = ColumnNum = 0;
        }

        static TextLocation GetLocationFromPos(const XmlChar_t *_pText, int _Pos);

        int LineNum;
        int ColumnNum;
    };

    struct LexParseError
    {
        LexParseError();	

        LexParseError(const TextLocation& _Loc,
            Status::Code _Code,
            const XmlString_t& _Details=LexParseError::DefaultDetails);

        LexParseError(Status::Code _Code);

        void Clear();

        bool Succeeded() const
        { return LXML_SUCCEEDED(Code); }
        bool Failed() const
        { return LXML_FAILED(Code); }

        TextLocation	Loc;
        Status::Code	Code;
        XmlString_t		Details;

        static const XmlString_t DefaultDetails;
    };


    /////////////////////////////////////////////////////////////////////
    // Token stream type
    /////////////////////////////////////////////////////////////////////

    typedef ce::list<Token> TokenStream_t;


    /////////////////////////////////////////////////////////////////////
    // Build a token stream
    /////////////////////////////////////////////////////////////////////

    class XmlLexer_t
    {
    public:
        // Lex the specified text
        XmlLexer_t(const XmlString_t& _Text);

        // Lex the specified XML file. The fGotFile argument will
        // be set to true iff the file was successfully opened and
        // read. If ppStoreText is non-NULL, it will point to the 
        // location where the file text was stored. This pointer
        // must be freed using the array delete (delete []) when done.
        XmlLexer_t(LPCTSTR _lpszFileName,
            XmlChar_t **ppStoreText,
            DWORD _dwForceCodePage=(DWORD)-1);

        // Lex the specified data, which may optionally contain
        // a byte-order mask. If no mask is present, it is assumed
        // that the data is encoded with ANSI.
        XmlLexer_t(const LPBYTE _lpData,
            DWORD _dwSize,
            XmlChar_t **_ppStoreText);

        XmlLexer_t(const LPBYTE _lpData,
            DWORD _dwSize,
            XmlChar_t **_ppStoreText,
            DWORD _dwCodePage);

        ~XmlLexer_t();

        void Clear();

        const TokenStream_t& GetTokens() const
        { return m_Tokens; }

        bool HasError() const
        { return LXML_FAILED(m_Error.Code); }
        const LexParseError& GetError() const
        { return m_Error; }

        // Returns the Encoding::EncodingType value corresponding
        // to the document's original encoding setting.
        DWORD GetCodePage() const
        { return m_dwCodePage; }

    public:
        static const LexemeInfo		FIXED_TOKENS[];

    private:
        BOOL GetFile(LPCTSTR _lpszFileName,
            XmlChar_t **_lppText,
            DWORD _dwForceCodePage=(DWORD)-1);

        // Loads the lexer using the specified byte stream, which may
        // optionally contain a Unicode BOM. If no BOM is detected,
        // the value of _dwSpecifyBom may be used to specify the
        // BOM that should be used with the data. Finally, if this
        // secondary BOM lookup fails, ANSI encoding is assumed.
        // The resulting data is converted to Utf-16 and a pointer is
        // stored in ppStoreText. On failure, ppStoreText will point to
        // NULL and the return value will be FALSE.
        BOOL GetText(const LPBYTE _lpData,
            DWORD _dwSize,
            XmlChar_t **_ppStoreText,
            DWORD _dwForceCodePage=(DWORD)-1);

        void BuildTokenStream(const XmlString_t& _Text);

    private:
        // Returns the index of the fixed token represented
        // at the current lexer position, or TT_UNKNOWN if
        // no fixed token is found.
        static int IsFixedToken(LexerContext& _Lc);

        static void HandleComment(LexerContext& _Lc);
        static void HandleIdentifier(LexerContext& _Lc);
        static void HandleString(LexerContext& _Lc);
        static void HandleCData(LexerContext& _Lc);
        static void HandleText(LexerContext& _Lc);
        static void HandleFixed(LexerContext& _Lc);
        static void HandleUnknown(LexerContext& _Lc);

        static void AdjustTagFlag(LexerContext& _Lc);

    private:
        TokenStream_t	m_Tokens;
        LexParseError	m_Error;
        DWORD			m_dwCodePage;
    };




}	// namespace litexml