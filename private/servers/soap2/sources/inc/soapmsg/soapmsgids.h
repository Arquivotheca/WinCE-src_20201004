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
#ifndef __SOAPMSGIDS_H_INCLUDED__
#define __SOAPMSGIDS_H_INCLUDED__

#define DISPID_SOAPCLIENT_INIT              1
#define DISPID_SOAPCLIENT_FAULTCODE         2
#define DISPID_SOAPCLIENT_FAULTSTRING       3
#define DISPID_SOAPCLIENT_FAULTACTOR        4
#define DISPID_SOAPCLIENT_FAULTDETAIL       5
#define DISPID_SOAPCLIENT_UNKNOWNSTART      10  // MAKE SURE this is HIGHER than the last USED one in client

#define DISPID_SOAPSERVER_INIT              1
#define DISPID_SOAPSERVER_SOAPINVOKE        2
#define DISPID_SOAPSERVER_TRACEDIRECTORY    3

#define DISPID_SOAPERROR_CODE               1
#define DISPID_SOAPERROR_STRING             2
#define DISPID_SOAPERROR_ACTOR              3
#define DISPID_SOAPERROR_DETAIL             4
#define DISPID_SOAPERROR_NAMESPACE          5

#define UUID_ISOAPCLIENT                4BDFD94B-415C-46f2-95E1-D145AB080F9E
#define UUID_ISOAPSERVER                E8685095-8543-4771-B2EE-E17C58379E47
#define UUID_ISOAPERROR                 EDABBFA8-E126-402D-B65D-4EFAC1405F6E

#define LIBID_SOAPMESSAGE_TYPELIB       6E0038DE-883C-4ff6-8B69-5D6FD7398476

#define CLSID_SOAPSERVER                EBB2FF12-861A-42b6-B815-B1AF4D944916
#define CLSID_SOAPCLIENT                86D54F3D-652D-4ab3-A1A6-14D403F6C813

#endif //__SOAPMSGIDS_H_INCLUDED__
