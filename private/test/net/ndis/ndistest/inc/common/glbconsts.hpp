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
#ifndef _NDT_GLOBAL_CONSTANTS_HPP_
#define _NDT_GLOBAL_CONSTANTS_HPP_

/**
 * \file GlbConsts.hpp
 * 
 * This file contains all the constants that are common to every file
 * in the ndistest branch. One should be able to include this file
 * independent of what is happening anywhere else. This file would be
 * included in all Ndistester components.
 */

//===================================================================
//  Declarations to make code more readable
//===================================================================

#ifndef INLINE
    #define INLINE                   inline
#endif

#ifndef FORCEINLINE
    #define FORCEINLINE         __forceinline
#endif

#ifndef STATIC
    #define STATIC                   static
#endif

#ifndef VIRTUAL
    #define VIRTUAL                 virtual
#endif

#ifndef CDECL
    #define CDECL                     __cdecl
#endif

#ifndef STDCALL
    #define STDCALL                 __stdcall
#endif

#ifndef PRIVATE
   #define PRIVATE
#endif

#ifndef PUBLIC
   #define PUBLIC
#endif

#ifndef NDT_ENUM
#define NDT_ENUM enum
#endif

typedef     long                 WIN32_ERROR;

#ifndef  NTSTATUS
   typedef     long              NTSTATUS;
#endif

#ifndef  BOOL
   typedef  int                  BOOL;
#endif

//
// Some W4 warning disabling
//

//conditional expression is constant
#pragma warning(disable:4127)

//===================================================================
// Some macros that will be used by everyone
//===================================================================

/**
 * Macro sets a particular flag (can set both single & multiple bits)
 * 
 * \param _V      The bitmask into which the flag has to be set
 * \param _F      The bit(s) to be set.
 */
#define SET_FLAG(_V, _F)           ((_V) |= (_F))

/**
 * Macro sets a particular flag (can set both single & multiple bits) in
 * a multiprocessor safe manner
 * 
 * \param _V      The bitmask into which the flag has to be set
 * \param _F      The bit(s) to be set.
 */
#define SET_FLAG_SAFE(_V, _F)      (InterlockedOr((LONG*)&(_V), _F))

/**
 * Macro clear a particular flag (can clear both single & multiple bits)
 * 
 * \param _V      The bitmask from which the flag has to be cleared
 * \param _F      The bit(s) to be cleared.
 */
#define CLEAR_FLAG(_V, _F)         ((_V) &= ~(_F))

/**
 * Macro clear a particular flag (can clear both single & multiple bits) in
 * a multiprocessor safe manner
 * 
 * \param _V      The bitmask from which the flag has to be cleared
 * \param _F      The bit(s) to be cleared.
 */
#define CLEAR_FLAG_SAFE(_V, _F)    (InterlockedAnd((LONG*)&(_V), ~(_F)))

/**
 * Macro checks if a particular bit is set or not in a bitmask.
 * 
 * \param _V      The bitmask in which the flag has to be checked
 * \param _F      The single bit to be checked.
 * \return        True if the bit is set
 */
#define TEST_FLAG(_V, _F)          (((_V) & (_F)) != 0)

/**
 * Macro checks if all of a bunch of bit are set or not in a bitmask.
 *
 * \param _V      The bitmask in which the flag has to be checked
 * \param _F      The bit(s) which have to be tested
 * \return        TRUE if \bALL bits are set in the bitmask
 */
#define TEST_ALL_FLAGS(_V, _F)     (((_V) & (_F)) == (_F))


/**
 * This macro work exactly like other Zero Memory macros except it is
 * safe to be used in both user and kernel mode.
 */
#define NDT_ZERO_MEMORY(_MemToZero, _Length)      memset((void*)_MemToZero, 0, (size_t)_Length)


/**
 * This macro is a stronger delete. If the memory is not already
 * freed, it would free the memory and also assign the pointer to NULL
 * to avoid double free
 * \param   _pMemToDelete  The pointer to memory to be deleted
 */ 
#define NdtFree(_pMemToDelete)       \
   if(_pMemToDelete)                         \
   {                                                      \
      delete _pMemToDelete;               \
      _pMemToDelete = NULL;               \
   }


/**
 * This is an important API and has to be used carefully. This API is needed when
 * exchanging objects between usermode and kernel mode that implement virtual
 * methods. Since virtual methods are essentially function pointers, these pointers
 * can end up pointing to code is User Mode that gets executed from Kernel Mode or
 * vice versa.
 *
 * If there is a need to invoke a method on an object passed from across user mode 
 * boundary or to make a copy of that object, NdtCopyObjectSafe should always be used.
 * 
 * \param pDest   Pointer to the destination object
 * \param pSrc     Pointer to the object whose copy is to be made
 * \param iSize    Size of the object
 */
#define NDTCopyObjectSafe(pDest, pSrc, iSize)       \
   NDTVERIFY(pDest);                                                \
   NDTVERIFY(pSrc);                                                  \
   NDTVERIFY(iSize > 0);                                            \
   NdisMoveMemory((((PCHAR)pDest)+(sizeof(CHAR*))), (((PCHAR)pSrc)+(sizeof(CHAR*))), (iSize - sizeof(CHAR*)));
   //NdisMoveMemory((((PCHAR)pDest)+sizeof(CHAR*)), (((PCHAR)pSrc)+sizeof(CHAR*)), (iSize - sizeof(CHAR*)));   


/**
 * This macro provides a wrapper to invoke a member function using a pointer. It always
 * invokes the fuction within the local object using the this pointer
 * 
 * \param pFunction  Pointer to the function 
 *
 * \code
 * // GET_DATA_LENGTH_FUNCTION points to a member function in Constructor that 
 * // returns a length
 * typedef ULONG (Constructor::*GET_DATA_LENGTH_FUNCTION)(); 
 *
 * // m_pGetDataLength is a a GET_DATA_LENGTH_FUNCTION type and currently 
 * // points to SimpleGetDataLength
 * GET_DATA_LENGTH_FUNCTION m_pGetDataLength = &Constructor::SimpleGetDataLength;
 *
 * // Invoke the function through the pointer using the macro
 * DataLength = CALL_MEMBER_FUNCTION(m_pGetDataLength);
 *
 * \endcode
 */
#define CALL_MEMBER_FUNCTION(pFunction)  ((this).*(pFunction)) 

/**
 * Macro to perform an interlocked increment of a counter. Though macro expects a
 * ulong, the increment is done as a long
 *
 * \param _ulCounter    The counter which has to be incremented.
 * \return        Returns the new incremented value of the counter
 */
#define NDT_INTERLOCKED_INCREMENT(_ulCounter)   NdisInterlockedIncrement((LONG*)&(_ulCounter))


/**
 * Macro to perform an interlocked decrement of a counter. Though macro expects a
 * ulong, the decrement is done as a long
 *
 * \param _ulCounter    The counter which has to be decremented.
 * \return        Returns the new decremented value of the counter
 */
#define NDT_INTERLOCKED_DECREMENT(_ulCounter)   NdisInterlockedDecrement((LONG*)&(_ulCounter))

/**
 * Macro to perform an interlocked exchange of a counter. 
 *
 * \param _ulCounter    The counter which has to be set(exchanged).
 * \param _ulNewValue   The new value to set into the counter
 * \return        Returns the original value of the counter
 */
#define NDT_INTERLOCKED_EXCHANGE(_ulCounter, _ulNewValue)   InterlockedExchange((LONG*)&(_ulCounter), (_ulNewValue))

/**
 * Macro to perform an interlocked increment of a counter
 *
 * \param _ulCounter    The counter which has to be incremented.
 * \param _ulValue      The value to be added to _ulCounter
 * \return        Returns the original value of the counter
 */
#define NDT_INTERLOCKED_ADD(_ulCounter, _ulValue)   InterlockedExchangeAdd((LONG*)&(_ulCounter), (LONG)(_ulValue))

#define NDT_THREAD_SLEEP(_TimeInMs)                \
{                                                  \
   LARGE_INTEGER  _TimeToSleep_;                   \
   _TimeToSleep_.QuadPart = Int32x32To64(_TimeInMs, -10000); \
   KeDelayExecutionThread(KernelMode, TRUE, &_TimeToSleep_); \
}

/**
 * The maximum number of processors we can support
 */
#ifndef NDT_MAX_PROCESSORS
   #define NDT_MAX_PROCESSORS                      32
#endif

/**
 * The maximum number of times we would print an error message (if we opt for controlled message)
 */
#ifndef NDT_MAX_CONTROLLED_ERROR_MSG
   #define NDT_MAX_CONTROLLED_ERROR_MSG            10
#endif


/**
 * \union HANDLE_64
 *
 * This is the handle to objects. It can either be local or remote. We want
 * this to be a 64 bit number, so that we can hold a pointer from a 64 bit machine
 * on an i386. It is used to hold library object handles and also to hold
 * kernel mode object handles.
 *
 */
union HANDLE_64
{
   /** Handle for remote object */
   ULONG64     ul64RemoteHandle;
   /** Handle for local object */
   PVOID       pvLocalHandle;
};

typedef HANDLE_64 *PHANDLE_64;

//
// Define to use 1.65 version of the Native Wi-Fi IHV spec
//
// #define NDT_SPEC_1_65 1


#endif // _NDT_GLOBAL_CONSTANTS_HPP_
