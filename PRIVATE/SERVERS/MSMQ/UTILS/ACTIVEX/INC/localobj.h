//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
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



