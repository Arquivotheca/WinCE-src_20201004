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

#include <hash_map.hxx>

namespace litexml
{

struct CodePageInfo;

typedef Status::Code (*LPDECODEPROC)(const CodePageInfo*,const LPBYTE,DWORD,LPXSTR*);
typedef Status::Code (*LPENCODEPROC)(const CodePageInfo*,LPCXSTR,LPBYTE*,LPDWORD);

// Details about a specific text encoding
struct CodePageInfo
{
	CodePageInfo() 
		{ ZeroMemory(this, sizeof(CodePageInfo)); }

	DWORD			CodePage;	// Code page
	BYTE			Bom[8];		// Byte order mark
	DWORD			BomSize;	// Size of the byte order mark
	LPDECODEPROC	fnDecode;	// Decoder routine
	LPENCODEPROC	fnEncode;	// Encoder routine
};

// Map of code page -> CodePageInfo
typedef ce::hash_map<DWORD,CodePageInfo>		CodePageTable_t;

// Text encoding helper
class Encoding
{
public:
	enum BaseCodePage
	{
		BcpUtf16	= 1200,		// UTF-16
		BcpUtf8		= 65001,	// UTF-8
		BcpUtf7		= 65000,	// UTF-7
	};

	// Retrieve the encoding info associated with the specified
	// codepage. Custom encodings take priority over the base
	// codepage enumeration members.
	static BOOL LookupCodePage(DWORD _dwCodePage,
		CodePageInfo *_pCpi);

	// Register or unregister custom code pages.
	static BOOL RegisterCodePage(const CodePageInfo* _pCpi);
	static BOOL UnregisterCodePage(DWORD _dwCodePage);

	// Detect the encodnig specified by the given data buffer.
	// If no encoding is specified, or if an unsupported encoding
	// is used, the system will fall back to ANSI.
	static BOOL DetectCodePage(const LPBYTE _lpData,
		DWORD _dwSize,
		CodePageInfo *_pCpi);

	// Decodes the data specified by _lpData. The decoded string
	// is stored at the address pointed to by _ppStrDecoded.
	static Status::Code Decode(LPBYTE _lpData,
		DWORD _dwSize,
		LPXSTR *_ppStrDecoded);

	// Decode the data specified by _lpData. The data cannot contain
	// a BOM, as the encoding is specified explicitly by _dwEncoding.
	static Status::Code Decode(LPBYTE _lpData,
		DWORD _dwSize,
		LPXSTR *_ppStrDecoded,
		DWORD _dwCodePage);

public:
	// Decodes a BOM-less in-memory buffer from UTF-16 to the 
	// internal LiteXml format
	static Status::Code Decode_Utf16(const CodePageInfo* _pCpi,
		const LPBYTE _lpData,
		DWORD _dwSize,
		LPXSTR *_ppStrDecoded);

	// Decodes a BOM-less in-memory buffer from UTF-8 to the 
	// internal LiteXml format
	static Status::Code Decode_Utf8(const CodePageInfo* _pCpi,
		const LPBYTE _lpData,
		DWORD _dwSize,
		LPXSTR *_ppStrDecoded);

	// Decodes a BOM-less in-memory buffer from UTF-7 to the 
	// internal LiteXml format
	static Status::Code Decode_Utf7(const CodePageInfo* _pCpi,
		const LPBYTE _lpData,
		DWORD _dwSize,
		LPXSTR *_ppStrDecoded);

private:
	static CodePageTable_t	CodePageTable;
	static CodePageTable_t	CustomCodePageTable;
	static CodePageInfo		DefaultCodePage;
	static BOOL s_fCodePageTableInitialized;

private:
	static VOID InitializeCodePageTable();
};

}	// litexml