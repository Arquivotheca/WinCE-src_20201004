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
////////////////////////////////////////////////////////////////////////////////
//
//  TUXTEST TUX DLL
//  Copyright (c) Microsoft Corporation
//
//  Module: StreamLogger.h
//          Contains function declarations, globals, and macros common to all WMT end to end tests.
//
//  Revision History:
//
////////////////////////////////////////////////////////////////////////////////

#ifndef __ASFFILEUTILS_H__
#define __ASFFILEUTILS_H__

////////////////////////////////////////////////////////////////////////////////
// Include files for global declarations

////////////////////////////////////////////////////////////////////////////////
// Type declarations

////////////////////////////////////////////////////////////////////////////////
// Global Constants

////////////////////////////////////////////////////////////////////////////////
// Utility macros
#define SAFE_CLOSEHANDLE(h)		if(NULL != h && INVALID_HANDLE_VALUE != h) \
								{ \
									CloseHandle(h); \
									h = NULL; \
								}

#define SAFE_DELETE(ptr)			if(NULL != ptr) \
								{ \
									delete ptr; \
									ptr = NULL; \
								}

#define SAFE_DELETEARRAY(ptr)	if(NULL != ptr) \
								{ \
									delete[] ptr; \
									ptr = NULL; \
								}

#define SAFE_RELEASE(ptr)		if(NULL != ptr) \
								{ \
									((IUnknown *)(ptr))->Release(); \
									ptr = NULL; \
								}

#define SAFE_CLOSEFILE(p)       if(NULL != p) \
                                { \
                                    fclose(p); \
                                    p = NULL; \
                                }

#define CHK(hRes)               if(FAILED(hRes)) \
                                { \
                                    hr = hRes; \
                                    goto cleanup; \
                                }

#define CHKBOOL(exp, hRes)      if(!exp) \
                                { \
                                    hr = hRes; \
                                    goto cleanup; \
                                }

#define CHKHANDLE(h, hRes)      if(h == INVALID_HANDLE_VALUE) \
                                { \
                                    hr = hRes; \
                                    goto cleanup; \
                                }
                                    
                                    


////////////////////////////////////////////////////////////////////////////////
// Global variables
								   
////////////////////////////////////////////////////////////////////////////////
// Utility func declarations

#endif


