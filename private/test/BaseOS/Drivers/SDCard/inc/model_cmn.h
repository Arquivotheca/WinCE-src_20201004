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
///////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2002 BSQUARE Corporation. All rights reserved.
// Copyright (c) Microsoft Corporation.  All rights reserved.//
//
// Use of this source code is subject to the terms of the Microsoft shared
// source or premium shared source license agreement under which you licensed
// this source code. If you did not accept the terms of the license agreement,
// you are not authorized to use this source code. For the terms of the license,
// please see the license agreement between you and Microsoft or, if applicable,
// see the SOURCE.RTF on your install media or the root of your tools installation.
// THE SOURCE CODE IS PROVIDED "AS IS", WITH NO WARRANTIES.
//
// --------------------------------------------------------------------
//
// THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF
// ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO
// THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A
// PARTICULAR PURPOSE.
//
// --------------------------------------------------------------------
//
#ifndef MODEL_CMN_H
#define MODEL_CMN_H

#include <windows.h>

#define MAX_PARAMS 100

typedef enum _PARAM_TYPE
{
    in = 1,
    out = 2
}PARAM_TYPE, *PPARAM_TYPE;

typedef struct parameter
{
    PARAM_TYPE type;
    TCHAR* name;
    TCHAR* value;
    parameter *next;
    ~parameter();
    parameter(PARAM_TYPE theType,TCHAR* theName,TCHAR* theValue);
}PARAMETER,*PPARAMETER;

typedef struct state
{
    TCHAR* name;
    TCHAR* value;
    state *next;
    ~state();
    state(TCHAR* theName,TCHAR* theValue);
}STATE,*PSTATE;

typedef struct parameterList
{
    PPARAMETER head;
    PPARAMETER tail;
    void AddTail(PPARAMETER theParam);
    PPARAMETER RemoveFirst(void);
    void RemoveAll(void);
    PPARAMETER FindParameter(TCHAR* ParamToFind);
    parameterList(void){head = NULL;tail = NULL;}
    ~parameterList(); //destroy all parameters
}PARAMETER_LIST;

typedef struct stateList
{
    PSTATE head;
    PSTATE tail;
    void AddTail(PSTATE theState);
    PSTATE RemoveFirst(void);
    void RemoveAll(void);
    stateList(void){head = NULL;tail = NULL;}
    ~stateList(); //destroy all parameters
}STATE_LIST;

typedef class transition
{
    public:
        transition(TCHAR* theName);
        transition();
        ~transition(){Clear();}
        bool AddParam(PPARAMETER theParam);
        void SetActionName(TCHAR* theName);
        PPARAMETER GetNextInParam(void);
        PPARAMETER GetNextOutParam(void);
        bool AddState(PSTATE);
        PSTATE GetNextState(void);
        bool HasAction(void);
        TCHAR* GetName(void){return name;}
        TCHAR* FindInParam(TCHAR* ParamToFind);
        TCHAR* FindOutParam(TCHAR* ParamToFind);
        void Clear(void){delete name; name = NULL;inParamList.RemoveAll();outParamList.RemoveAll();stateList.RemoveAll();}
    private:
        TCHAR* name;

        PARAMETER_LIST inParamList;
        PARAMETER_LIST outParamList;
        STATE_LIST stateList;

    protected:

}TRANSITION, *PTRANSITION;

#endif
