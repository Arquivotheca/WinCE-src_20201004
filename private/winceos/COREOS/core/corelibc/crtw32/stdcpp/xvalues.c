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
/* values used by math functions -- IEEE 754 version */
#if defined(_M_CEE_PURE)
#if defined(MRTDLL)
#undef MRTDLL
#endif
#if defined(MRTDLL)
#undef CRTDLL
#endif
#endif

#include "xmath.h"
_C_STD_BEGIN

		/* macros */
#define NBITS	(48 + _DOFF)

 #if _D0 == 0
  #define INIT(w0)		{w0, 0, 0, 0}
  #define INIT2(w0, w1)	{w0, 0, 0, w1}

 #else /* _D0 == 0 */
  #define INIT(w0)		{0, 0, 0, w0}
  #define INIT2(w0, w1)	{w1, 0, 0, w0}
 #endif /* _D0 == 0 */

		/* static data */
extern /* const */ _Dconst _Denorm = {INIT2(0, 1)};
extern /* const */ _Dconst _Eps = {
	INIT((_DBIAS - NBITS - 1) << _DOFF)};
extern /* const */ _Dconst _Hugeval = {INIT(_DMAX << _DOFF)};
extern /* const */ _Dconst _Inf = {INIT(_DMAX << _DOFF)};
extern /* const */ _Dconst _Nan = {INIT((_DMAX << _DOFF)
	| (1 << (_DOFF - 1)))};
extern /* const */ _Dconst _Snan = {INIT2(_DMAX << _DOFF, 1)};
extern /* const */ _Dconst _Rteps = {
	INIT((_DBIAS - NBITS / 2) << _DOFF)};

extern /* const */ double _Xbig = (NBITS + 1) * 347L / 1000;
extern /* const */ double _Zero = 0.0;
_C_STD_END

/*
 * Copyright (c) 1992-2007 by P.J. Plauger.  ALL RIGHTS RESERVED.
 * Consult your license regarding permissions and restrictions.
 V5.03:0009 */
