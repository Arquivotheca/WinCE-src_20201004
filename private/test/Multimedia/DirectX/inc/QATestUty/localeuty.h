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


#pragma once

#ifndef __LOCALEUTY_H__
#define __LOCALEUTY_H__

// external dependencies
// ¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯
#include <tchar.h>
#include <string>

namespace std
{

#   if defined(UNICODE) || defined(_UNICODE)
        typedef wstring tstring;
#   else
        typedef string tstring;
#   endif

#   ifndef _stprintf
#       if defined(UNICODE) || defined(_UNICODE)
#           define _stprintf wsprintf
#       else
#           define _stprintf sprintf
#       endif
#   endif

#   ifndef _tcspbrk
#       if defined(UNICODE) || defined(_UNICODE)
#           define _tcspbrk wcspbrk
#       else
#           define _tcspbrk strpbrk
#       endif
#   endif

    inline tstring to_tstring(DWORD dw)
    {
        TCHAR rgtcBuffer[17];     
        swprintf_s(rgtcBuffer,TEXT("0x%lX"),dw);
        return (LPCTSTR)rgtcBuffer;
    }

}



#endif