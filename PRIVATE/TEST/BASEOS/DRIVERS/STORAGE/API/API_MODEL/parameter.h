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
// THE SOURCE CODE IS PROVIDED "AS IS", WITH NO WARRANTIES.
//

#include <tchar.h>
#include <windows.h>
#ifndef _PARAMETER_H
#define _PARAMETER_H

typedef enum _PARAM_TYPE
{
	PARAM_Unknown = -1,
	PARAM_lpInBuffer,
	PARAM_lpOutBuffer,
	PARAM_BufferType,
	PARAM_IpBytesReturned,
	PARAM_Alignment,
	PARAM_nInBufferSize,
	PARAM_nOutBufferSize,
	PARAM_nNumSectors,
	PARAM_nStartSector,
	PARAM_DataBuffer,
	PARAM_nNumSGBufs
} PARAM_TYPE;

typedef struct parameter
{
	PARAM_TYPE paramtype;
	TCHAR* value;
	parameter *next;
	~parameter();
	parameter(PARAM_TYPE theType, TCHAR* theValue);
}PARAMETER,*PPARAMETER;

typedef struct parameterList
{
	PPARAMETER head;
	PPARAMETER tail;
	void AddTail(PPARAMETER theParam);
	PPARAMETER RemoveFirst(void);
	PPARAMETER RemoveLast(void);
	void RemoveAll(void);
	PPARAMETER FindParameterByValue(TCHAR* ParamToFind);
	PPARAMETER FindParameterByName(PARAM_TYPE theType);
	void Print(void);
	//TCHAR* parameterList::ValueAt(int index);
	PPARAMETER parameterList::GetItem(int index);
	parameterList(void){head = NULL;tail = NULL;Items = 0;}
	~parameterList(); //destroy all parameters
	int Items;
}PARAMETER_LIST;
#endif
