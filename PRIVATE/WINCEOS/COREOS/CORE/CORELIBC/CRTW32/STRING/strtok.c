//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
#include <corecrtstorage.h>

char * __cdecl strtok(char * string, const char * control) {
	crtGlob_t *pcrtg;
	unsigned char *str;
	const unsigned char *ctrl = control;
	unsigned char map[32];
	int count;
	if (!(pcrtg = GetCRTStorage()))
		return 0;
	/* Clear control map */
	for (count = 0; count < 32; count++)
		map[count] = 0;
	/* Set bits in delimiter table */
	do {
		map[*ctrl >> 3] |= (1 << (*ctrl & 7));
	} while (*ctrl++);
	/* Initialize str. If string is NULL, set str to the saved
	 * pointer (i.e., continue breaking tokens out of the string
	 * from the last strtok call) */
	if (string)
		str = string;
	else
		str = pcrtg->nexttoken;
	/* Find beginning of token (skip over leading delimiters). Note that
	 * there is no token iff this loop sets str to point to the terminal
	 * null (*str == '\0') */
	while ((map[*str >> 3] & (1 << (*str & 7))) && *str)
		str++;
	string = str;
	/* Find the end of the token. If it is not the end of the string,
	 * put a null there. */
	for (; *str ; str++)
		if ( map[*str >> 3] & (1 << (*str & 7)) ) {
			*str++ = '\0';
			break;
		}
	pcrtg->nexttoken = str;
	/* Determine if a token has been found. */
	if (string == str)
		return NULL;
	else
		return string;
}

