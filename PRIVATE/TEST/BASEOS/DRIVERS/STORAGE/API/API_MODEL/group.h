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
#ifndef _GROUP_H
#define _GROUP_H
#include "Parameter.h"


typedef struct Group
{
	PARAM_TYPE paramtype;
	Group *next;
	int items;
	PARAMETER_LIST m_ParameterList;
	~Group();
	Group(PARAM_TYPE theType);
	Group();
}GROUP,*PGROUP;

typedef struct GroupList
{
	PGROUP head;
	PGROUP tail;
	void AddTail(PGROUP theGroup);
	PGROUP RemoveFirst(void);
	void RemoveAll(void);
	PGROUP FindGroup(PARAM_TYPE GroupToFind);
	GroupList(void){head = NULL;tail = NULL;}
	~GroupList(); //destroy all parameters
}GROUP_LIST,*PGROUP_LIST;
#endif
