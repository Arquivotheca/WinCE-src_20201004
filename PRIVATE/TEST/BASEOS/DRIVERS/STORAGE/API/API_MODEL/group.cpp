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

#include "Group.h"


void GROUP_LIST::AddTail(PGROUP theGroup)
{
	if(tail == NULL)
	{
		tail = theGroup;
		head = theGroup;
	}
	else
	{
		tail->next = theGroup;
		tail = theGroup;
	}
}
void GROUP_LIST::RemoveAll(void)
{
	PGROUP ret;
	while(head != NULL)
	{
		ret = head;
		head = head->next;
		delete ret;
	}
	tail = NULL;
}
PGROUP GROUP_LIST::RemoveFirst(void)
{
	if(head == NULL)
		return NULL;
	PGROUP ret = head;
	head = head->next;
	return ret;
}

GROUP_LIST::~GROUP_LIST()
{
	RemoveAll();
}

PGROUP GROUP_LIST::FindGroup(PARAM_TYPE GroupToFind)
{
	PGROUP temp = head;
	if(temp == NULL)
		return NULL;

	while(temp != NULL)
	{
		if (temp->paramtype == GroupToFind)
			return temp;
		temp = temp->next;
	}
	return NULL;
}

GROUP::~GROUP()
{
	next = NULL;
}

GROUP::Group(PARAM_TYPE theType)
{
	paramtype = theType;
	next = NULL;
}


GROUP::Group(void)
{

}


