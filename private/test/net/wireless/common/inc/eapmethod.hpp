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
// ----------------------------------------------------------------------------
//
// Use of this source code is subject to the terms of the Microsoft end-user
// license agreement (EULA) under which you licensed this SOFTWARE PRODUCT.
// If you did not accept the terms of the EULA, you are not authorized to use
// this source code. For a copy of the EULA, please see the LICENSE.RTF on your
// install media.
//
// ----------------------------------------------------------------------------
//
// Definition and declaration of basic eapmethod types.
//
// ----------------------------------------------------------------------------

#ifndef _DEFINED_EapMethod_hpp_
#define _DEFINED_EapMethod_hpp_
#pragma once

namespace ce {
namespace qa {

#define LEGACY_METHOD_PATH                _T("\\Comm\\EAP\\Extension")
#define LEGACY_TESTMETHOD_PATH        _T("\\Comm\\TestEap\\Extension")
#define LEGACY_TESTMETHOD_USER        _T("UserName")
#define LEGACY_TESTMETHOD_PASSWD   _T("PassWD")
#define LEGACY_TESTMETHOD_DOMAIN    _T("Domain")
#define LEGACY_TESTMETHOD_UICOUNT   _T("NetuiCount")

   typedef struct _TEST_GENERIC_USER_DATA
   {
      LPWSTR lpwzUserName;
      LPWSTR lpwzPassword;
      LPWSTR lpwzDomain;
   }TEST_GENERIC_USER_DATA;


};
};

#endif /* _DEFINED_EapMethod_hpp_ */
// ----------------------------------------------------------------------------
