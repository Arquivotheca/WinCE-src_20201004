//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
#ifndef _INC_FLTINTRN
#define _INC_FLTINTRN

#ifdef __cplusplus
extern "C" {
#endif

#include <cruntime.h>


/*
 * structs used to fool the compiler into not generating floating point
 * instructions when copying and pushing [long] double values
 */



#ifndef DOUBLE
//typedef struct {
//	double x;
//} DOUBLE;
typedef double DOUBLE;
#endif

#ifndef LONGDOUBLE

typedef struct {
	long double x;
} LONGDOUBLE;

#endif

typedef struct _strflt
{
	int sign;	      /* zero if positive otherwise negative */
	int decpt;	      /* exponent of floating point number */
	int flag;	      /* zero if okay otherwise IEEE overflow */
	char *mantissa;       /* pointer to mantissa in string form */
} *STRFLT;

/*
 * typedef for _fltin
 */

typedef struct _flt
{
	int flags;
	int nbytes;	     /* number of characters read */
	long lval;
	double dval;	     /* the returned floating point number */
}
	*FLT;


/* floating point conversion routines, keep in sync with mrt32\include\convert.h */

void _fltout2( double, STRFLT);

/*
 * table of pointers to floating point helper routines
 *
 * We can't specify the prototypes for the entries of the table accurately,
 * since different functions in the table have different arglists.
 * So we declare the functions to take and return void (which is the
 * correct prototype for _fptrap(), which is what the entries are all
 * initialized to if no floating point is loaded) and cast appropriately
 * on every usage.
 */

typedef void (* PFV)(void);
extern PFV _cfltcvt_tab[6];

typedef void (* PF0)(DOUBLE*, char*, int, int, int);
//#define _cfltcvt(a,b,c,d,e) (*((PF0)_cfltcvt_tab[0]))(a,b,c,d,e)
void __cdecl _cfltcvt( double * arg, char * buffer, int format, int precision, int caps);

typedef void (* PF1)(char*);
//#define _cropzeros(a)	    (*((PF1)_cfltcvt_tab[1]))(a)
void __cdecl _cropzeros(char * buf);

typedef void (* PF2)(int, char*, char*);
//#define _fassign(a,b,c)     (*((PF2)_cfltcvt_tab[2]))(a,b,c)
void __cdecl _fassign(int flag, char * argument, char * number);

typedef void (* PF3)(char*);
//#define _forcdecpt(a)	    (*((PF3)_cfltcvt_tab[3]))(a)
void __cdecl _forcdecpt(char *buffer);

typedef int (* PF4)(DOUBLE*);
//#define _positive(a)	    (*((PF4)_cfltcvt_tab[4]))(a)

typedef void (* PF5)(LONGDOUBLE*, char*, int, int, int);
//#define _cldcvt(a,b,c,d,e)  (*((PF5)_cfltcvt_tab[5]))(a,b,c,d,e)




#ifdef __cplusplus
}
#endif

#endif	/* _INC_FLTINTRN */
