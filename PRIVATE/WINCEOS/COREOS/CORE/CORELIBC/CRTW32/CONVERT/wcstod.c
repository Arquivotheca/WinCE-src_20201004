//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
#include <corecrt.h>

double __cdecl wcstod(const wchar_t *nptr, wchar_t **endptr) {
	double tmp;
	const wchar_t *ptr = nptr;
	char *cptr;
	int len;
    _LDBL12 ld12;
    const char *EndPtr;
    DWORD flags;
	INTRNCVT_STATUS intrncvt;
	/* scan past leading space/tab characters */
	while (isspace((char)*ptr))
		ptr++;
	if (!(cptr = (char *)LocalAlloc(LPTR,(wcslen(ptr)+1)*sizeof(wchar_t))))
		return 0.0;
	for (len = 0; ptr[len]; len++)
	    cptr[len] = (char)ptr[len];
	cptr[len] = 0;
    flags = __strgtold12(&ld12, &EndPtr, cptr);
	LocalFree(cptr);
    if (flags & SLD_NODIGITS) {
		if (endptr)
			*endptr = (wchar_t *)nptr;
		return 0.0;
    }
	if (endptr)
		*endptr = (wchar_t *)ptr + (EndPtr - cptr);
	intrncvt = _ld12tod(&ld12, &tmp);
	if (flags & SLD_OVERFLOW  || intrncvt == INTRNCVT_OVERFLOW)
		return (*ptr == '-') ? -HUGE_VAL : HUGE_VAL; /* negative or positive overflow */
	if (flags & SLD_UNDERFLOW || intrncvt == INTRNCVT_UNDERFLOW)
		return 0.0; /* underflow */
	return tmp;
}

_CRTIMP double __cdecl strtod(const char *nptr, char **endptr) {
	double tmp;
	const char *ptr = nptr;
    _LDBL12 ld12;
    const char *EndPtr;
    DWORD flags;
	INTRNCVT_STATUS intrncvt;
	/* scan past leading space/tab characters */
	while (isspace(*ptr))
		ptr++;
    flags = __strgtold12(&ld12, &EndPtr, ptr);
    if (flags & SLD_NODIGITS) {
		if (endptr)
			*endptr = (char *)nptr;
		return 0.0;
    }
	if (endptr)
		*endptr = (char *)EndPtr;
	intrncvt = _ld12tod(&ld12, &tmp);
	if (flags & SLD_OVERFLOW  || intrncvt == INTRNCVT_OVERFLOW)
		return (*ptr == '-') ? -HUGE_VAL : HUGE_VAL; /* negative or positive overflow */
	if (flags & SLD_UNDERFLOW || intrncvt == INTRNCVT_UNDERFLOW)
		return 0.0; /* underflow */
	return tmp;
}

