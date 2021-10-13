//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
#include <corecrt.h>

/* flag values */
#define FL_UNSIGNED   1       /* wcstoul called */
#define FL_NEG	      2       /* negative sign found */
#define FL_OVERFLOW   4       /* overflow occured */
#define FL_READDIGIT  8       /* we've read at least one correct digit */

static unsigned long __cdecl xstrtoxl(const char *nptr, const char **endptr, int ibase, int flags, BOOL bUnicode) {
	const unsigned char *p;
	wchar_t c;
	unsigned long number;
	unsigned digval;
	unsigned long maxval;
	if (ibase < 0 || ibase == 1 || ibase > 36) {
		if (endptr)
			*endptr = nptr;
		return 0;
	}
	p = nptr;			/* p is our scanning pointer */
	number = 0;			/* start with zero */
	if (!bUnicode)
		c = (WCHAR) (BYTE) *p++;
	else
		c = *((LPWSTR)p)++;			/* read char */
	while (iswspace(c)) {
		if (!bUnicode)
			c = (WCHAR)(BYTE)*p++;
		else
			c = *((LPWSTR)p)++;		/* skip whitespace */
	}
	if ((c == L'-') || (c == L'+')) {
		if (c == L'-')
			flags |= FL_NEG;	/* remember minus sign */
		if (!bUnicode)
			c = (WCHAR)(BYTE)*p++;
		else
			c = *((LPWSTR)p)++;			/* read char */
	}
	if (!ibase)
		ibase = (c != L'0') ? 10 : ((*p == 'x' || *p == 'X') && (!bUnicode || !p[1])) ? 16 : 8;
	if ((ibase == 16) && (c == L'0') && (*p == 'x' || *p == 'X') && (!bUnicode || !p[1])) {
		if (!bUnicode) {
			++p;
			c = *p++;	/* advance past prefix */
		} else {
			++p;
			++p;
			c = *((LPWSTR)p)++;	/* advance past prefix */
		}
	}
	/* if our number exceeds this, we will overflow on multiply */
	maxval = ULONG_MAX / ibase;
	for (;;) {	/* exit in middle of loop */
		/* convert c to value */
		if (isdigit(c))
			digval = c - '0';
		else if ((c>='A') && (c<='Z'))
			digval = c - 'A' + 10;
		else if ((c>='a') && (c<='z'))
			digval = c - 'a' + 10;
		else
			break;
		if (digval >= (unsigned)ibase)
			break;		/* exit loop if bad digit found */
		/* record the fact we have read one digit */
		flags |= FL_READDIGIT;
		/* we now need to compute number = number * base + digval, but we
		   need to know if overflow occured.  This requires a tricky pre-check. */
		if (number < maxval || (number == maxval && (unsigned long)digval <= ULONG_MAX % ibase))
			number = number * ibase + digval;
		else
			flags |= FL_OVERFLOW;
		if (!bUnicode)
			c = (WCHAR)(BYTE)*p++;
		else
			c = *((LPWSTR)p)++;			/* read char */
	}
	--p;				/* point to place that stopped scan */
	if (bUnicode)
		--p;
	if (!(flags & FL_READDIGIT)) {
		p = nptr;
		number = 0;
	} else if ((flags & FL_OVERFLOW) ||
			   (!(flags & FL_UNSIGNED) && (((flags & FL_NEG) && (number > -LONG_MIN)) ||
									       (!(flags & FL_NEG) && (number > LONG_MAX))))) {
		number = (flags & FL_UNSIGNED) ? ULONG_MAX : (flags & FL_NEG) ? (unsigned long)(-LONG_MIN) : LONG_MAX;
	}
	if (endptr)
		*endptr = p;
	if (flags & FL_NEG)
		number = (unsigned long)(-(long)number);
	return number;
}

long __cdecl wcstol(const wchar_t *nptr, wchar_t **endptr, int ibase) {
	return (long) xstrtoxl((const char *)nptr, (const char **)endptr, ibase, 0,1);
}

unsigned long __cdecl wcstoul(const wchar_t *nptr, wchar_t **endptr, int ibase) {
	return xstrtoxl((const char *)nptr, (const char **)endptr, ibase, FL_UNSIGNED,1);
}

long __cdecl strtol(const char *nptr, char **endptr, int ibase) {
	return (long) xstrtoxl(nptr, endptr, ibase, 0,0);
}

unsigned long __cdecl strtoul(const char *nptr, char **endptr, int ibase) {
	return xstrtoxl(nptr, endptr, ibase, FL_UNSIGNED,0);
}

