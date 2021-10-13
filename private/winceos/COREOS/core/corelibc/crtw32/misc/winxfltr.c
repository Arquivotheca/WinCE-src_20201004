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
/***
*winxfltr.c - startup exception filter
*
*       Copyright (c) Microsoft Corporation. All rights reserved.
*
*Purpose:
*       Defines _XcptFilter(), the function called by the exception filter
*       expression in the startup code.
*
*Revision History:
*       10-31-91  GJF   Module created. Copied from the original xcptfltr.c
*                       then extensively revised.
*       11-08-91  GJF   Cleaned up header files usage.
*       12-13-91  GJF   Fixed multi-thread build.
*       01-17-92  GJF   Changed default handling under Win32 - unhandled
*                       exceptions are now passed to UnhandledExceptionFilter.
*                       Dosx32 behavior in unchanged. Also, used a couple of
*                       local macros to simplify handling of single-thread vs
*                       multi-thread code [_Win32_].
*       02-16-93  GJF   Changed for new _getptd().
*       04-06-93  SKS   Replace _CRTAPI* with __cdecl
*       04-19-93  SKS   Move XcptActTabSize under MTHREAD switch
*       04-27-93  GJF   Removed (commented out) entries in _XcptActTab which
*                       corresponded to C RTEs. These will now simply be
*                       passed on through to the system exception handler.
*       07-28-93  GJF   For SIGFPE, must reset the XcptAction field for all
*                       FPE entries to SIG_DFL before calling the user's
*                       handler.
*       09-06-94  CFW   Replace MTHREAD with _MT.
*       08-16-96  GJF   Fixed potential overrun of _XcptActTab. Also, detab-ed.
*       08-21-96  GJF   Fixed _MT part of overrun fix.
*       12-12-01  BWT   Use getptd_noexit. If we can't alloc a new ptd, let the next
*                       guy in the chain deal with it.
*       05-30-02  GB    Added __CppXcptFilter which is calls _XcptFilter only
*                       for C++ exceptions.
*       01-12-05  DJT   Use OS-encoded global function pointers
*       05-05-05  JL    Don't encode function pointers in ptd (VSW:470622)
*       03-01-06  SSM   return EXCEPTION_CONTINUE_SEARCH from _XcptFilter 
*                       instead of calling UnhandledExcpetionFilter
*       07-18-07  ATC   ddb#125490.  Making _xcptActTabl const.
*
*******************************************************************************/

#ifndef _POSIX_

#include <cruntime.h>
#include <float.h>
#include <mtdll.h>
#include <oscalls.h>
#include <signal.h>
#include <stddef.h>
#include <internal.h>


/*
 * special code denoting no signal.
 */
#define NOSIG   -1


const struct _XCPT_ACTION _XcptActTab[] = {

/*
 * Exceptions corresponding to the same signal (e.g., SIGFPE) must be grouped
 * together.
 *
 * If any XcptAction field is changed in this table, that field must be
 * initialized with an encoded function pointer at CRT initialization time.
 *
 *        XcptNum                                        SigNum    XcptAction
 *        -------------------------------------------------------------------
 */
        { (unsigned long)STATUS_ACCESS_VIOLATION,         SIGSEGV, SIG_DFL },

        { (unsigned long)STATUS_ILLEGAL_INSTRUCTION,      SIGILL,  SIG_DFL },

        { (unsigned long)STATUS_PRIVILEGED_INSTRUCTION,   SIGILL,  SIG_DFL },

/*      { (unsigned long)STATUS_NONCONTINUABLE_EXCEPTION, NOSIG,   SIG_DIE },
 */
/*      { (unsigned long)STATUS_INVALID_DISPOSITION,      NOSIG,   SIG_DIE },
 */
        { (unsigned long)STATUS_FLOAT_DENORMAL_OPERAND,   SIGFPE,  SIG_DFL },

        { (unsigned long)STATUS_FLOAT_DIVIDE_BY_ZERO,     SIGFPE,  SIG_DFL },

        { (unsigned long)STATUS_FLOAT_INEXACT_RESULT,     SIGFPE,  SIG_DFL },

        { (unsigned long)STATUS_FLOAT_INVALID_OPERATION,  SIGFPE,  SIG_DFL },

        { (unsigned long)STATUS_FLOAT_OVERFLOW,           SIGFPE,  SIG_DFL },

        { (unsigned long)STATUS_FLOAT_STACK_CHECK,        SIGFPE,  SIG_DFL },

        { (unsigned long)STATUS_FLOAT_UNDERFLOW,          SIGFPE,  SIG_DFL },
        
        { (unsigned long)STATUS_FLOAT_MULTIPLE_FAULTS,    SIGFPE,  SIG_DFL },

        { (unsigned long)STATUS_FLOAT_MULTIPLE_TRAPS,     SIGFPE,  SIG_DFL },

/*      { (unsigned long)STATUS_INTEGER_DIVIDE_BY_ZERO,   NOSIG,   SIG_DIE },
 */
/*      { (unsigned long)STATUS_STACK_OVERFLOW,           NOSIG,   SIG_DIE }
 */
};

/*
 * WARNING!!!! The definition below amounts to defining that:
 *
 *                  XcptActTab[ _First_FPE_Indx ]
 *
 * is the very FIRST entry in the table corresponding to a floating point
 * exception. Whenever the definition of the XcptActTab[] table is changed,
 * this #define must be review to ensure correctness.
 */
const int _First_FPE_Indx = 3;

/*
 * There are _Num_FPE (currently, 9) entries in XcptActTab corresponding to
 * floating point exceptions.
 */
const int _Num_FPE = 9;

/*
 * size of the exception-action table (in bytes)
 */
const int _XcptActTabSize = sizeof _XcptActTab;

/*
 * number of entries in the exception-action table
 */
const int _XcptActTabCount = (sizeof _XcptActTab)/sizeof(_XcptActTab[0]);


/*
 * the FPECODE and PXCPTINFOPTRS macros are intended to simplify some of
 * single vs multi-thread code in the filter function. basically, each macro
 * is conditionally defined to be a global variable or the corresponding
 * field in the per-thread data structure. NOTE THE ASSUMPTION THAT THE
 * _ptiddata VARIABLE IS ALWAYS NAMED ptd!!!!
 */

#define FPECODE         ptd->_tfpecode

#define PXCPTINFOPTRS   ptd->_tpxcptinfoptrs

/*
 * function to look up the exception action table (_XcptActTab[]) corresponding
 * to the given exception
 */

static struct _XCPT_ACTION * __cdecl xcptlookup(
        unsigned long,
        struct _XCPT_ACTION *
        );

#ifdef  DEBUG

/*
 * prototypes for debugging routines
 */
void prXcptActTabEntry(const struct _XCPT_ACTION *);
void prXcptActTab(void);

#endif  /* DEBUG */
/***
*int _CppXcptFilter(xcptnum, pxcptptrs) - Wrapper over _XcptFilter so that 
*       _XcptFilter only gets called for C++ Exceptions.
*
*******************************************************************************/

int __cdecl __CppXcptFilter (
    unsigned long xcptnum,
    PEXCEPTION_POINTERS pxcptinfoptrs
    )
{
#if 0
    Note that here we are checking for EH_EXCEPTION_NUMBER defined in ehdata.h
    in langapi\include\ehdata.h
#endif
    if (xcptnum==('msc'|0xE0000000)) {
        return _XcptFilter(xcptnum,pxcptinfoptrs);
    } else {
        return EXCEPTION_CONTINUE_SEARCH;
    }
}

/***
*int _XcptFilter(xcptnum, pxcptptrs) - Identify exception and the action to
*       be taken with it
*
*Purpose:
*       _XcptFilter() is called by the exception filter expression of the
*       _try - _except statement, in the startup code, which guards the call
*       to the user's main(). _XcptFilter() consults the _XcptActTab[] table
*       to identify the exception and determine its disposition. The
*       is disposition of an exception corresponding to a C signal may be
*       modified by a call to signal(). There are three broad cases:
*
*       (1) Unrecognized exceptions and exceptions for which the XcptAction
*           value is SIG_DFL.
*
*           In both of these cases, EXCEPTION_CONTINUE_SEARCH is returned to
*           cause the OS exception dispatcher to pass the exception onto the
*           next exception handler in the chain (usually a system default
*           handler).
*
*       (2) Exceptions corresponding to C signals with an XcptAction value
*           NOT equal to SIG_DFL.
*
*           These are the C signals whose disposition has been affected by a
*           call to signal() or whose default semantics differ slightly from
*           from the corresponding OS exception. In all cases, the appropriate
*           disposition of the C signal is made by the function (e.g., calling
*           a user-specified signal handler). Then, EXCEPTION_CONTINUE_EXECU-
*           TION is returned to cause the OS exception dispatcher to dismiss
*           the exception and resume execution at the point where the
*           exception occurred.
*
*       (3) Exceptions for which the XcptAction value is SIG_DIE.
*
*           These are the exceptions corresponding to fatal C runtime errors.
*           _XCPT_HANDLE is returned to cause control to pass into the
*           _except-block of the _try - _except statement. There, the runtime
*           error is identified, an appropriate error message is printed out
*           and the program is terminated.
*
*Entry:
*
*Exit:
*
*Exceptions:
*       That's what it's all about!
*
*******************************************************************************/

int __cdecl _XcptFilter (
        unsigned long xcptnum,
        PEXCEPTION_POINTERS pxcptinfoptrs
        )
{
        struct _XCPT_ACTION * pxcptact;
        _PHNDLR phandler;
        void *oldpxcptinfoptrs;
        int oldfpecode;
        int indx;

        _ptiddata ptd = _getptd_noexit();
        if (!ptd) {
            // we can't deal with it - pass it on.
            return(EXCEPTION_CONTINUE_SEARCH);
        }

        pxcptact = xcptlookup(xcptnum, ptd->_pxcptacttab);

        if (pxcptact == NULL)
        {
            phandler = SIG_DFL;
        }
        else
        {
            phandler = pxcptact->XcptAction;
        }

        /*
         * first, take care of all unrecognized exceptions and exceptions with
         * XcptAction values of SIG_DFL.
         */
        if ( phandler == SIG_DFL )
                return(EXCEPTION_CONTINUE_SEARCH);


#ifdef  DEBUG
        prXcptActTabEntry(pxcptact);
#endif  /* DEBUG */

        /*
         * next, weed out all of the exceptions that need to be handled by
         * dying, perhaps with a runtime error message
         */
        if ( phandler == SIG_DIE ) {
                /*
                 * reset XcptAction (in case of recursion) and drop into the
                 * except-clause.
                 */
                pxcptact->XcptAction = SIG_DFL;
                return(EXCEPTION_EXECUTE_HANDLER);
        }

        /*
         * next, weed out all of the exceptions that are simply ignored
         */
        if ( phandler == SIG_IGN )
                /*
                 * resume execution
                 */
                return(EXCEPTION_CONTINUE_EXECUTION);

        /*
         * the remaining exceptions all correspond to C signals which have
         * signal handlers associated with them. for some, special setup
         * is required before the signal handler is called. in all cases,
         * if the signal handler returns, -1 is returned by this function
         * to resume execution at the point where the exception occurred.
         */

        /*
         * save the old value of _pxcptinfoptrs (in case this is a nested
         * exception/signal) and store the current one.
         */
        oldpxcptinfoptrs = PXCPTINFOPTRS;
        PXCPTINFOPTRS = pxcptinfoptrs;

        /*
         * call the user-supplied signal handler
         *
         * floating point exceptions must be handled specially since, from
         * the C point-of-view, there is only one signal. the exact identity
         * of the exception is passed in the global variable _fpecode.
         */
        if ( pxcptact->SigNum == SIGFPE ) {

                /*
                 * reset the XcptAction field to the default for all entries
                 * corresponding to SIGFPE.
                 */
                for ( indx = _First_FPE_Indx ;
                      indx < _First_FPE_Indx + _Num_FPE ;
                      indx++ )
                {
                        ( (struct _XCPT_ACTION *)(ptd->_pxcptacttab) +
                          indx )->XcptAction = SIG_DFL;
                }

                /*
                 * Save the current _fpecode in case it is a nested floating
                 * point exception (not clear that we need to support this,
                 * but it's easy).
                 */
                oldfpecode = FPECODE;

                /*
                 * there are no exceptions corresponding to
                 * following _FPE_xxx codes:
                 *
                 *      _FPE_UNEMULATED
                 *      _FPE_SQRTNEG
                 *
                 * futhermore, STATUS_FLOATING_STACK_CHECK is
                 * raised for both floating point stack under-
                 * flow and overflow. thus, the exception does
                 * not distinguish between _FPE_STACKOVERLOW
                 * and _FPE_STACKUNDERFLOW. arbitrarily, _fpecode
                 * is set to the former value.
                 *
                 * the following should be a switch statement but, alas, the
                 * compiler doesn't like switching on unsigned longs...
                 */
                if ( pxcptact->XcptNum == STATUS_FLOAT_DIVIDE_BY_ZERO )

                        FPECODE = _FPE_ZERODIVIDE;

                else if ( pxcptact->XcptNum == STATUS_FLOAT_INVALID_OPERATION )

                        FPECODE = _FPE_INVALID;

                else if ( pxcptact->XcptNum == STATUS_FLOAT_OVERFLOW )

                        FPECODE = _FPE_OVERFLOW;

                else if ( pxcptact->XcptNum == STATUS_FLOAT_UNDERFLOW )

                        FPECODE = _FPE_UNDERFLOW;

                else if ( pxcptact->XcptNum == STATUS_FLOAT_DENORMAL_OPERAND )

                        FPECODE = _FPE_DENORMAL;

                else if ( pxcptact->XcptNum == STATUS_FLOAT_INEXACT_RESULT )

                        FPECODE = _FPE_INEXACT;

                else if ( pxcptact->XcptNum == STATUS_FLOAT_STACK_CHECK )

                        FPECODE = _FPE_STACKOVERFLOW;

                else if ( pxcptact->XcptNum == STATUS_FLOAT_MULTIPLE_TRAPS )

                        FPECODE = _FPE_MULTIPLE_TRAPS;

                else if ( pxcptact->XcptNum == STATUS_FLOAT_MULTIPLE_FAULTS )

                        FPECODE = _FPE_MULTIPLE_FAULTS;

                /*
                 * call the SIGFPE handler. note the special code to support
                 * old MS-C programs whose SIGFPE handlers expect two args.
                 *
                 * NOTE: THE CAST AND CALL BELOW DEPEND ON __cdecl BEING
                 * CALLER CLEANUP!
                 */
                (*(void (__cdecl *)(int, int))phandler)(SIGFPE, FPECODE);

                /*
                 * restore the old value of _fpecode
                 */
                FPECODE = oldfpecode;
        }
        else {
                /*
                 * reset the XcptAction field to the default, then call the
                 * user-supplied handler
                 */
                pxcptact->XcptAction = SIG_DFL;
                (*phandler)(pxcptact->SigNum);
        }

        /*
         * restore the old value of _pxcptinfoptrs
         */
        PXCPTINFOPTRS = oldpxcptinfoptrs;

        return(EXCEPTION_CONTINUE_EXECUTION);

}


/***
*struct _XCPT_ACTION * xcptlookup(xcptnum, pxcptrec) - look up exception-action
*       table entry for xcptnum
*
*Purpose:
*       Find the in _XcptActTab[] whose Xcptnum field is xcptnum.
*
*Entry:
*       unsigned long xcptnum            - exception type
*
*       _PEXCEPTIONREPORTRECORD pxcptrec - pointer to exception report record
*       (used only to distinguish different types of XCPT_SIGNAL)
*
*Exit:
*       If successful, pointer to the table entry. If no such entry, NULL is
*       returned.
*
*Exceptions:
*
*******************************************************************************/

static struct _XCPT_ACTION * __cdecl xcptlookup (
        unsigned long xcptnum,
        struct _XCPT_ACTION * pxcptacttab
        )

{
        struct _XCPT_ACTION *pxcptact = pxcptacttab;

        /*
         * walk thru the _xcptactab table looking for the proper entry
         */

        while ( (pxcptact->XcptNum != xcptnum) && 
                (++pxcptact < pxcptacttab + _XcptActTabCount) ) ;

        /*
         * if no table entry was found corresponding to xcptnum, return NULL
         */
        if ( (pxcptact >= pxcptacttab + _XcptActTabCount) ||
             (pxcptact->XcptNum != xcptnum) )
                return(NULL);

        return(pxcptact);
}

#ifdef DEBUG

/*
 * DEBUGGING TOOLS!
 */
struct xcptnumstr {
        unsigned long num;
        char *str;
};

struct xcptnumstr XcptNumStr[] = {

        { (unsigned long)STATUS_DATATYPE_MISALIGNMENT,
            "STATUS_DATATYPE_MISALIGNMENT" },

        { (unsigned long)STATUS_ACCESS_VIOLATION,
            "STATUS_ACCESS_VIOLATION" },

        { (unsigned long)STATUS_ILLEGAL_INSTRUCTION,
            "STATUS_ILLEGAL_INSTRUCTION" },

        { (unsigned long)STATUS_NONCONTINUABLE_EXCEPTION,
            "STATUS_NONCONTINUABLE_EXCEPTION" },

        { (unsigned long)STATUS_INVALID_DISPOSITION,
            "STATUS_INVALID_DISPOSITION" },

        { (unsigned long)STATUS_FLOAT_DENORMAL_OPERAND,
            "STATUS_FLOAT_DENORMAL_OPERAND" },

        { (unsigned long)STATUS_FLOAT_DIVIDE_BY_ZERO,
            "STATUS_FLOAT_DIVIDE_BY_ZERO" },

        { (unsigned long)STATUS_FLOAT_INEXACT_RESULT,
            "STATUS_FLOAT_INEXACT_RESULT" },

        { (unsigned long)STATUS_FLOAT_INVALID_OPERATION,
            "STATUS_FLOAT_INVALID_OPERATION" },

        { (unsigned long)STATUS_FLOAT_OVERFLOW,
            "STATUS_FLOAT_OVERFLOW" },

        { (unsigned long)STATUS_FLOAT_STACK_CHECK,
            "STATUS_FLOAT_STACK_CHECK" },

        { (unsigned long)STATUS_FLOAT_UNDERFLOW,
            "STATUS_FLOAT_UNDERFLOW" },

        { (unsigned long)STATUS_FLOAT_MULTIPLE_FAULTS,
            "STATUS_FLOAT_MULTIPLE_FAULTS" },

        { (unsigned long)STATUS_FLOAT_MULTIPLE_TRAPS,
            "STATUS_FLOAT_MULTIPLE_TRAPS" },

        { (unsigned long)STATUS_INTEGER_DIVIDE_BY_ZERO,
            "STATUS_INTEGER_DIVIDE_BY_ZERO" },

        { (unsigned long)STATUS_PRIVILEGED_INSTRUCTION,
            "STATUS_PRIVILEGED_INSTRUCTION" },
#ifndef _WIN32_WCE
        { (unsigned long)_STATUS_STACK_OVERFLOW,
            "_STATUS_STACK_OVERFLOW" }
#endif 
};

#define XCPTNUMSTR_SZ   ( sizeof XcptNumStr / sizeof XcptNumStr[0] )

/*
 * return string mnemonic for exception
 */
char * XcptNumToStr (
        unsigned long xcptnum
        )
{
        int indx;

        for ( indx = 0 ; indx < XCPTNUMSTR_SZ ; indx++ )
                if ( XcptNumStr[indx].num == xcptnum )
                        return(XcptNumStr[indx].str);

        return(NULL);
}

struct signumstr {
        int num;
        char *str;
};

struct signumstr SigNumStr[] = {
        { SIGINT,       "SIGINT" },
        { SIGILL,       "SIGILL" },
        { SIGFPE,       "SIGFPE" },
        { SIGSEGV,      "SIGSEGV" },
        { SIGTERM,      "SIGTERM" },
        { SIGBREAK,     "SIGBREAK" },
        { SIGABRT,      "SIGABRT" }
};

#define SIGNUMSTR_SZ   ( sizeof SigNumStr / sizeof SigNumStr[0] )

/*
 * return string mnemonic for signal
 */
char * SigNumToStr (
        int signum
        )
{
        int indx;

        for ( indx = 0 ; indx < SIGNUMSTR_SZ ; indx++ )
                if ( SigNumStr[indx].num == signum )
                        return(SigNumStr[indx].str);

        return(NULL);
}

struct actcodestr {
        _PHNDLR code;
        char *str;
};

struct actcodestr ActCodeStr[] = {
        { SIG_DFL,      "SIG_DFL" },
        { SIG_IGN,      "SIG_IGN" },
        { SIG_DIE,      "SIG_DIE" }
};

#define ACTCODESTR_SZ   ( sizeof ActCodeStr / sizeof ActCodeStr[0] )

/*
 * return string mnemonic for action code
 */
char * ActCodeToStr (
        _PHNDLR action
        )
{
        int indx;

        for ( indx = 0 ; indx < ACTCODESTR_SZ ; indx++ )
                if ( ActCodeStr[indx].code == action)
                        return(ActCodeStr[indx].str);

        return("FUNCTION ADDRESS");
}

/*
 * print out exception-action table entry
 */
void prXcptActTabEntry (
        const struct _XCPT_ACTION *pxcptact
        )
{

#ifdef _WCE_FULLCRT
        printf("XcptNum    = %s\n", XcptNumToStr(pxcptact->XcptNum));
        printf("SigNum     = %s\n", SigNumToStr(pxcptact->SigNum));
        printf("XcptAction = %s\n", ActCodeToStr(pxcptact->XcptAction));
#endif
}

/*
 * print out all entries in the exception-action table
 */
void prXcptActTab (
        void
        )
{
#ifdef _WCE_FULLCRT
        int indx;

        for ( indx = 0 ; indx < _XcptActTabCount ; indx++ ) {
                printf("\n_XcptActTab[%d] = \n", indx);
                prXcptActTabEntry(&_XcptActTab[indx]);
        }
#endif
}

#endif  /* DEBUG */

#endif  /* _POSIX_ */
