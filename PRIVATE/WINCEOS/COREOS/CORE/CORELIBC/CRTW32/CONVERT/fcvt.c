//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
#include <corecrt.h>
#include <corecrtstorage.h>

void __cdecl _fptostr(char *buf, int digits, STRFLT pflt) {
	char *pbuf = buf;
	char *mantissa = pflt->mantissa;
	/* initialize the first digit in the buffer to '0' (NOTE - NOT '\0')
	 * and set the pointer to the second digit of the buffer.  The first
	 * digit is used to handle overflow on rounding (e.g. 9.9999...
	 * becomes 10.000...) which requires a carry into the first digit. */
	*pbuf++ = '0';
	/* Copy the digits of the value into the buffer (with 0 padding)
	 * and insert the terminating null character. */
	while (digits > 0) {
		*pbuf++ = (*mantissa) ? *mantissa++ : (char)'0';
		digits--;
	}
    *pbuf = '\0';
	/* do any rounding which may be needed.  Note - if digits < 0 don't
	 * do any rounding since in this case, the rounding occurs in  a digit
	 * which will not be output beause of the precision requested */
	if (digits >= 0 && *mantissa >= '5') {
		pbuf--;
		while (*pbuf == '9')
			*pbuf-- = '0';
		*pbuf += 1;
	}
	if (*buf == '1') {
		/* the rounding caused overflow into the leading digit (e.g.
		 * 9.999.. went to 10.000...), so increment the decpt position
		 * by 1 */
		pflt->decpt++;
	} else {
		/* move the entire string to the left one digit to remove the
		 * unused overflow digit.
		 */
		strcpy(buf,buf+1);
	}
}

char * __cdecl _fpcvt(STRFLT pflt, int digits, int *decpt, int *sign) {
	crtGlob_t *pcrtg;
	if (!(pcrtg = GetCRTStorage()))
		return 0;
	_fptostr(pcrtg->fpcvtbuf, (digits > CVTBUFSIZE - 2) ? CVTBUFSIZE - 2 : digits, pflt);
	*sign = (pflt->sign == '-') ? 1 : 0;
	*decpt = pflt->decpt;
	return pcrtg->fpcvtbuf;
}

char * __cdecl _fcvt(double value, int ndec, int *decpt, int *sign) {
	struct _strflt strfltstruct;
    _LDOUBLE ld;
    FOS autofos;
    __dtold(&ld, &value);
    strfltstruct.flag =  $I10_OUTPUT(ld,&autofos);
    strfltstruct.sign = autofos.sign;
    strfltstruct.decpt = autofos.exp;
    strcpy(strfltstruct.mantissa,autofos.man);
    return(_fpcvt(&strfltstruct, strfltstruct.decpt + ndec, decpt, sign));
}

char * __cdecl _ecvt(double value, int ndigit, int *decpt, int *sign) {
	char *retbuf;
	struct _strflt strfltstruct;
    _LDOUBLE ld;
    FOS autofos;
    __dtold(&ld, &value);
    strfltstruct.flag =  $I10_OUTPUT(ld,&autofos);
    strfltstruct.sign = autofos.sign;
    strfltstruct.decpt = autofos.exp;
    strcpy(strfltstruct.mantissa,autofos.man);
    retbuf = _fpcvt(&strfltstruct, ndigit, decpt, sign);
	return(retbuf);
}

