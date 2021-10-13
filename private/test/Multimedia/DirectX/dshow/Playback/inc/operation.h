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

#ifndef OPERATION_H
#define OPERATION_H

#include <windows.h>
#include <vector>

using namespace std;

#define MAX_OP_LEN		512
#define MAX_OP_NAME_LEN 64

enum OperationType
{
	OpWait,
	OpSwitchGDI,
	OpSwitchDDraw,
	OpRun,
	OpStop,
	OpPause
};

class Operation
{
public:
	OperationType type;

public:
	static HRESULT EnableLogging();
	static bool IsLoggingEnabled();

public:
	virtual ~Operation() {}
	virtual HRESULT Execute() = 0;
};

typedef vector<Operation*> OpList;

class WaitOp : public Operation
{
public:
	enum Units
	{
		MediaDuration,
		Msec
	};

	Units units;
	DWORD time;

public:
	HRESULT Execute();
};

class SwitchGDIOp : public Operation
{
public:
	virtual HRESULT Execute();
};

class SwitchDDrawOp : public Operation
{
public:
	virtual HRESULT Execute();
};

#endif //OPERATION_H