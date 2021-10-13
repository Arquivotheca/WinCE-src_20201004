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
// The following ifdef block is the standard way of creating macros which make exporting 
// from a DLL simpler. All files within this DLL are compiled with the GUIDGENERATOR_EXPORTS
// symbol defined on the command line. this symbol should not be defined on any project
// that uses this DLL. This way any other project whose source files include this file see 
// GUIDGENERATOR_API functions as being imported from a DLL, whereas this DLL sees symbols
// defined with this macro as being exported.
#ifdef GUIDGENERATOR_EXPORTS
#define GUIDGENERATOR_API __declspec(dllexport)
#else
#define GUIDGENERATOR_API __declspec(dllimport)
#endif

// This class is exported from the GUIDGenerator.dll
class GUIDGENERATOR_API CGUIDGenerator {
public:
    CGUIDGenerator(void);
    // TODO: add your methods here.
};

extern GUIDGENERATOR_API int nGUIDGenerator;

extern "C" {

    GUIDGENERATOR_API BOOL GenerateGUID(TCHAR *ptszFileName,BYTE *ptszGUID,BYTE bRequested);
}
