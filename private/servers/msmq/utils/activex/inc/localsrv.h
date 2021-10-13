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
//=--------------------------------------------------------------------------=
// LocalServer.H
//=--------------------------------------------------------------------------=
//
// global routines that are specific to the inproc server itself, such as
// registration, object creation, object specification, etc...
//
#ifndef _LOCALSERVER_H_

void      InitializeLibrary(void);
void      UninitializeLibrary(void);
BOOL      RegisterData(void);
BOOL      UnregisterData(void);
BOOL      CheckForLicense();
BOOL      CheckLicenseKey(LPWSTR wszCheckme);
BSTR      GetLicenseKey(void);

// global variables that various people use to get information about the control.
//
extern const WCHAR g_wszLibName [];
extern const CLSID *g_pLibid;

//=--------------------------------------------------------------------------=
// Global object information table
//=--------------------------------------------------------------------------=
// for each object in your application, you have an entry in this table.  they
// do not necessarily have to be CoCreatable, but if they are used, then they
// should reside here.  use the macros to fill in this table.
//
typedef struct tagOBJECTINFO {

    unsigned short usType;
    void          *pInfo;

} OBJECTINFO;

extern OBJECTINFO g_ObjectInfo[];

//=--------------------------------------------------------------------------=
// these things are used to set up our objects in our global object table
//
#define OI_UNKNOWN       0
#define OI_AUTOMATION    1
#define OI_CONTROL       2
#define OI_PROPERTYPAGE  3
#define OI_BOGUS         0xffff

#define OBJECTISCREATABLE(index)  (((UNKNOWNOBJECTINFO *)(g_ObjectInfo[(index)]).pInfo)->rclsid != NULL)
#define ISEMPTYOBJECT(index)      (g_ObjectInfo[index].usType == OI_BOGUS)

// these are the macros you should use to fill in the table.  Note that the name
// must be exactly the same as that used in the global structure you created
// for this object.
//
#define UNKNOWNOBJECT(name)    { OI_UNKNOWN,      (void *)&(name##Object) }
#define AUTOMATIONOBJECT(name) { OI_AUTOMATION,   (void *)&(name##Object) }
#define CONTROLOBJECT(name)    { OI_CONTROL,      (void *)&(name##Control) }
#define PROPERTYPAGE(name)     { OI_PROPERTYPAGE, (void *)&(name##Page) }
#define EMPTYOBJECT            { OI_BOGUS, NULL }

#define _LOCALSERVER_H_
#endif // _LOCALSERVER_H_

