//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
#include <corecrt.h>

static void __cdecl xtoa(unsigned long val, char *buf, unsigned radix, int is_neg) {
	char *p;		/* pointer to traverse string */
	char *firstdig; 	/* pointer to first digit */
	char temp;		/* temp char */
	unsigned digval;	/* value of digit */
	p = buf;
	if (is_neg) {
		*p++ = '-';
		val = (unsigned long)(-(long)val);
	}
	firstdig = p;		/* save pointer to first digit */
	do {
		digval = (unsigned) (val % radix);
		val /= radix;	/* get next digit */
		/* convert to ascii and store */
		if (digval > 9)
			*p++ = (char)(digval - 10 + 'a');	/* a letter */
		else
			*p++ = (char)(digval + '0');		/* a digit */
	} while (val > 0);
	/* We now have the digit of the number in the buffer, but in reverse
	   order.  Thus we reverse them now. */
	*p-- = '\0';		/* terminate string; p points to last digit */
	do {
		temp = *p;
		*p = *firstdig;
		*firstdig = temp;	/* swap *p and *firstdig */
	} while (++firstdig < --p); /* repeat until halfway */
}

char * __cdecl _itoa(int val, char *buf, int radix) {
	xtoa((unsigned long)val, buf, radix, (radix == 10 && val < 0));
	return buf;
}

char * __cdecl _ltoa(long val, char *buf, int radix) {
	xtoa((unsigned long)val, buf, radix, (radix == 10 && val < 0));
	return buf;
}

char * __cdecl _ultoa(unsigned long val, char *buf, int radix) {
	xtoa(val, buf, radix, 0);
	return buf;
}

