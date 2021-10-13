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
#ifndef _MAP_H
#define _MAP_H
#include "TUXMAIN.H"

template<class T> 
struct MapEntry {
	T value;
	TCHAR *szName;
};

#define BEGINMAP(TYPE,NAME) MapEntry<TYPE> NAME[]={
#define ENTRY(X) {X,TEXT(#X)},
#define ENDMAP {{0},NULL} };
#define MAPSIZE(NAME) ((sizeof(NAME)/sizeof(NAME[0]))-1)

template<class T>
class Map {
public:
	Map(MapEntry<T> *lpMap,DWORD dwSize) { m_lpMap=lpMap; m_dwSize=dwSize; };
	TCHAR * operator[](T value) {
		for(DWORD i=0;i<m_dwSize;i++) 
			if(m_lpMap[i].value==value) return m_lpMap[i].szName;
		return NULL;
	};
private:
	MapEntry<T> * m_lpMap;
	DWORD m_dwSize;
};

#endif