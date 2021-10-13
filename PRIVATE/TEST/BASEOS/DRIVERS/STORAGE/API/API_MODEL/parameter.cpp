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

#include "Parameter.h"
#include "main.h"
parameter::~parameter(){
	if(value)
		delete[] value;
	
	value = NULL;
	next = NULL;
}

parameter::parameter(PARAM_TYPE theType,TCHAR* theValue)
{
	// Store the Parameter Type
	paramtype = theType;

	// Store the Parameter Value
	if(theValue == NULL)
		value = theValue;
	else
	{
		value = new TCHAR[_tcslen(theValue) + 1];
		if(value)
			_tcscpy(value,theValue);
	}

	next = NULL;
}

void parameterList::AddTail(PPARAMETER theParam)
{
	if(tail == NULL)
	{
		tail = theParam;
		head = theParam;
	}
	else
	{
		tail->next = theParam;
		tail = theParam;
	}
	Items++;
}
void parameterList::RemoveAll(void)
{
	PPARAMETER ret;
	while(head != NULL)
	{
		ret = head;
		head = head->next;
		delete ret;
		Items--;
	}
	tail = NULL;
}

parameterList::~parameterList()
{
	RemoveAll();
}

PPARAMETER parameterList::RemoveFirst(void)
{
	if(head == NULL)
		return NULL;
	PPARAMETER ret = head;
	head = head->next;
	Items--;
	return ret;
	
}

void parameterList::Print(void)
{
	PPARAMETER cur = head;
	
	
	while(cur != NULL)
	{
		//TCHAR* temp = new TCHAR[_tcslen(cur->name) + _tcslen(cur->value) + 10];
		TCHAR* temp = new TCHAR[_tcslen(cur->value) + 30];
		if(temp)
		{
			switch(cur->paramtype)
				{
				case PARAM_lpInBuffer:
					_stprintf(temp,_T("lpInBuffer = %s"),cur->value);
					break;
				case PARAM_lpOutBuffer:
					_stprintf(temp,_T("lpOutBuffer = %s"),cur->value);
					break;
				case PARAM_BufferType:
					_stprintf(temp,_T("BufferType = %s"),cur->value);
					break;
				case PARAM_IpBytesReturned:
					_stprintf(temp,_T("IpBytesReturned = %s"),cur->value);
					break;
				case PARAM_Alignment:
					_stprintf(temp,_T("Alignment = %s"),cur->value);
					break;
				case PARAM_nInBufferSize:
					_stprintf(temp,_T("nInBufferSize = %s"),cur->value);
					break;
				case PARAM_nOutBufferSize:
					_stprintf(temp,_T("nOutBufferSize = %s"),cur->value);
					break;
				case PARAM_nNumSectors:
					_stprintf(temp,_T("nNumSectors = %s"),cur->value);
					break;
				case PARAM_nStartSector:
					_stprintf(temp,_T("nStartSector = %s"),cur->value);
					break;
				case PARAM_DataBuffer:
					_stprintf(temp,_T("DataBuffer = %s"),cur->value);
					break;
				case PARAM_nNumSGBufs:
					_stprintf(temp,_T("nNumSGBufs = %s"),cur->value);
					break;
				default:
					_stprintf(temp,_T("Unknown = %s"),cur->value);				
				}
			g_pKato->Log(LOG_DETAIL,(_T("%s\n"),temp));
		}
		
		cur = cur->next;
		if(temp)
			delete[] temp;
	}
	
}
	
PPARAMETER parameterList::RemoveLast(void)
{
	PPARAMETER cur = head;
	PPARAMETER ret = tail;

	if(head == NULL || tail == NULL)
		return NULL;

	if(head == tail)
	{
		head = NULL;
		tail = NULL;
		Items--;
		return ret;
	}

	

	for(int i = 1; i < Items - 1; i++)
	{
		cur = cur->next;
	}
	cur->next = NULL;
	tail = cur;

	ret->next = NULL;
	Items--;
	return ret;
	
}

/*TCHAR* parameterList::ValueAt(int index)
{
	PPARAMETER temp = head;
	if(temp == NULL)
		return NULL;
	int counter = 0;
	while(temp != NULL)
	{
		if(counter == index)
		{
			return temp->name;
		}
		temp = temp->next;
		counter++;
	}
	return NULL;
}*/

PPARAMETER parameterList::GetItem(int index)
{
	PPARAMETER temp = head;
	if(temp == NULL)
		return NULL;
	int counter = 0;
	while(temp != NULL)
	{
		if(counter == index)
		{
			return temp;
		}
		temp = temp->next;
		counter++;
	}
	return NULL;
}

PPARAMETER parameterList::FindParameterByValue(TCHAR* ParamToFind)
{
	PPARAMETER temp = head;
	if(temp == NULL)
		return NULL;

	while(temp != NULL)
	{
		if(_tcscmp(temp->value,ParamToFind) == 0)
			return temp;
		temp = temp->next;
	}
	return NULL;
	
}

PPARAMETER parameterList::FindParameterByName(PARAM_TYPE theType)
{
	PPARAMETER temp = head;
	if(temp == NULL)
		return NULL;

	while(temp != NULL)
	{
		if(temp->paramtype == theType)
			return temp;
		temp = temp->next;
	}
	return NULL;	
}
