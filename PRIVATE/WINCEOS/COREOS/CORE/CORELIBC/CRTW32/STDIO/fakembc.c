//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//

#include <cruntime.h>
#include <crtdbg.h>

int change_to_widechar(wchar_t *pwc, const char *s, size_t n)
{
        if ( !s || n == 0 )
            /* indicate do not have state-dependent encodings,
               handle zero length string */
            return 0;

        if ( !*s )
        {
            /* handle NULL char */
            if (pwc)
                *pwc = 0;
            return 0;
        }

        if (pwc)
            *pwc = (wchar_t)(unsigned char)*s;
        return sizeof(char);
}

int change_to_multibyte(char *s, wchar_t wchar)
{
        if ( !s )
            /* indicate do not have state-dependent encodings */
            return 0;

        if ( wchar > 255 )  /* validate high byte */
        {
            //errno = EILSEQ;
            return -1;
        }

        *s = (char) wchar;
        return sizeof(char);
}


