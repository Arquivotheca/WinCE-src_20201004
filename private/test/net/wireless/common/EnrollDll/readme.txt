//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// Use of this source code is subject to the terms of the Microsoft end-user
// license agreement (EULA) under which you licensed this SOFTWARE PRODUCT.
// If you did not accept the terms of the EULA, you are not authorized to use
// this source code. For a copy of the EULA, please see the LICENSE.RTF on your
// install media.
//



//XXX: NOTE - We must be a UNICODE Image.
//XXX: To use this app WININET.DLL must be available.
//XXX:  To change from DEBUGMSG to printf change the define of OUTPUT* in enroll.h

enrollDll has an Entry Point         EnrollUsingCmdLine 
// Entry point that can be called by other modules to use Enrollment Functionality
extern "C" HRESULT EnrollUsingCmdLine(
    TCHAR* szCmdLine);

szCmdLine is a Text that can take the following values

 -sservername
 	 Causes the Application to use all the default settings, with the passed in servername.
 -ffile.cfg
 	Causes the Application to use file.cfg
	See enroll.cfg for a sample config file.


