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
// LocalObjects.H
//=--------------------------------------------------------------------------=
//
// this file is used by automation servers to delcare things that their objects
// need other parts of the server to see.
//
#ifndef _LOCALOBJECTS_H_

//=--------------------------------------------------------------------------=
// these constants are used in conjunction with the g_ObjectInfo table that
// each inproc server defines.  they are used to identify a given  object
// within the server.
//
// **** ADD ALL NEW OBJECTS TO THIS LIST ****
//

#define OBJECT_TYPE_OBJMQQUERY                      0
#define OBJECT_TYPE_OBJMQQUEUEINFO                  1
#define OBJECT_TYPE_OBJMQQUEUE                      2
#define OBJECT_TYPE_OBJMQMESSAGE                    3
#define OBJECT_TYPE_OBJMQQUEUEINFOS                 4
#define OBJECT_TYPE_OBJMQEVENT                      5
#define OBJECT_TYPE_OBJMQAPPLICATION                6


#define _LOCALOBJECTS_H_
#endif // _LOCALOBJECTS_H_



