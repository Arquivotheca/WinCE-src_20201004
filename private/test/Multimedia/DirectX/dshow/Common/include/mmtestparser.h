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
////////////////////////////////////////////////////////////////////////////////

#ifndef _XMLCONFIG_PARSER_H
#define _XMLCONFIG_PARSER_H

#include "DxXML.h"
#include "media.h"

extern HRESULT ParseDWORD( CXMLElement *pEleWord, DWORD* pDWord );
extern HRESULT ParseDWORD( const char* szDWord, DWORD* pDWord );
extern HRESULT ParseINT32( CXMLElement *pEleInt32, INT32* pInt );
extern HRESULT ParseINT32( const char* szInt32, INT32* pInt );
extern HRESULT ParseLONGLONG( CXMLElement *pEleDWord, LONGLONG* pllInt );
extern HRESULT ParseLONGLONG( const char* szLLInt, LONGLONG* pllInt );
extern HRESULT ParseFLOAT( CXMLElement *pEleFloat, FLOAT* pFloat );
extern HRESULT ParseFLOAT( const char* szFloat, FLOAT* pFloat );
extern HRESULT ParseString( CXMLElement *pEleString, TCHAR* szString, int length );
extern HRESULT ParseStringList( CXMLElement *pEleStringList, StringList* pStringList, char delimiter );
extern HRESULT ParseStringListString( const char* szStringList, 
                                      const char delimiter, 
                                      char* szString, 
                                      int len, 
                                      int& pos );

extern HRESULT ParseGUID( CXMLElement *pEleGUID, GUID* pGuid );
extern HRESULT ParseBaseUrls( CXMLElement *pElement, BaseUrl *pBaseUrl );
extern HRESULT ParseMedia( CXMLElement *pEleMedia, Media* pMedia );
extern HRESULT ParseMediaList( CXMLElement *pEleMediaList, MediaList* pMediaList );
extern HRESULT ParseMediaListFile( TCHAR* szMediaListFile, MediaList* pMediaList );
extern HRESULT ParseVideoInfo( CXMLElement *pElement, VideoInfo* pVideoInfo );
extern HRESULT ParseAudioInfo( CXMLElement *pElement, AudioInfo* pAudioInfo );
extern HRESULT ParseCameraProperties(CXMLElement *pElement, CCameraProperty **pProp, int nElementsAllocated, int *nElementsFilled);
extern HRESULT ParseCameraProperty( CXMLElement *pElement, CCameraProperty* pProp );

#endif
