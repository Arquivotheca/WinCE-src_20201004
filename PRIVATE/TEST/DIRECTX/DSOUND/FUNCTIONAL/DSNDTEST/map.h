//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// Use of this source code is subject to the terms of your Microsoft Windows CE
// Source Alliance Program license form.  If you did not accept the terms of
// such a license, you are not authorized to use this source code.
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