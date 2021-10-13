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
//
//	_RTTIBaseClassDescriptor
//
//	TypeDescriptor is declared in ehdata.h
//
#if (defined(_M_IA64) || defined(_M_AMD64)) || defined(VERSP_IA64)	/*IFSTRIP=IGN*/
#pragma pack(push, rttidata, 4)
#endif

#ifndef WANT_NO_TYPES
struct _s_RTTIClassHierarchyDescriptor;
typedef const struct _s_RTTIClassHierarchyDescriptor _RTTIClassHierarchyDescriptor;

typedef const struct	_s_RTTIBaseClassDescriptor	{
#if (defined(_M_IA64) || defined(_M_AMD64)) && !defined(VERSP_IA64) && !defined(_M_CEE_PURE)
	__int32     					pTypeDescriptor;    // Image relative offset of TypeDescriptor
#else
	TypeDescriptor					*pTypeDescriptor;
#endif
	DWORD							numContainedBases;
	PMD								where;
	DWORD							attributes;
#if (defined(_M_IA64) || defined(_M_AMD64)) && !defined(VERSP_IA64) && !defined(_M_CEE_PURE)
	__int32     					pClassDescriptor;	// Image relative offset of _RTTIClassHierarchyDescriptor
#else
	_RTTIClassHierarchyDescriptor	*pClassDescriptor;
#endif
	} _RTTIBaseClassDescriptor;
#endif // WANT_NO_TYPES

#define BCD_NOTVISIBLE				0x00000001
#define BCD_AMBIGUOUS				0x00000002
#define BCD_PRIVORPROTBASE			0x00000004
#define BCD_PRIVORPROTINCOMPOBJ		0x00000008
#define BCD_VBOFCONTOBJ				0x00000010
#define BCD_NONPOLYMORPHIC			0x00000020
#define BCD_HASPCHD					0x00000040			// pClassDescriptor field is present

#define BCD_PTD(bcd)				((bcd).pTypeDescriptor)
#define BCD_NUMCONTBASES(bcd)		((bcd).numContainedBases)
#define BCD_WHERE(bcd)				((bcd).where)
#define BCD_ATTRIBUTES(bcd)			((bcd).attributes)
#define BCD_PCHD(bcd)				((bcd).pClassDescriptor)
#if (defined(_M_IA64) || defined(_M_AMD64)) && !defined(_M_CEE_PURE)
#define BCD_PTD_IB(bcd,ib)			((TypeDescriptor*)((ib) + (bcd).pTypeDescriptor))
#define BCD_PCHD_IB(bcd,ib)			((_RTTIClassHierarchyDescriptor*)((ib) + (bcd).pClassDescriptor))
#endif // _M_IA64 || _M_AMD64


//
//	_RTTIBaseClassArray
//
#pragma warning(disable:4200)		// get rid of obnoxious nonstandard extension warning
#ifndef WANT_NO_TYPES
typedef const struct	_s_RTTIBaseClassArray	{
#if (defined(_M_IA64) || defined(_M_AMD64)) && !defined(VERSP_IA64) && !defined(_M_CEE_PURE)
	__int32                 		arrayOfBaseClassDescriptors[];  // Image relative offset of _RTTIBaseClassDescriptor
#else
	_RTTIBaseClassDescriptor		*arrayOfBaseClassDescriptors[];
#endif
	} _RTTIBaseClassArray;

#endif // WANT_NO_TYPES
#pragma warning(default:4200)

#define BCA_BCDA(bca)				((bca).arrayOfBaseClassDescriptors)

//
//	_RTTIClassHierarchyDescriptor
//
#ifndef WANT_NO_TYPES
typedef const struct	_s_RTTIClassHierarchyDescriptor	{
	DWORD							signature;
	DWORD							attributes;
	DWORD							numBaseClasses;
#if (defined(_M_IA64) || defined(_M_AMD64)) && !defined(VERSP_IA64) && !defined(_M_CEE_PURE)
	__int32         				pBaseClassArray;    // Image relative offset of _RTTIBaseClassArray
#else
	_RTTIBaseClassArray				*pBaseClassArray;
#endif
	} _RTTIClassHierarchyDescriptor;

#endif // WANT_NO_TYPES

#define CHD_MULTINH					0x00000001
#define CHD_VIRTINH					0x00000002
#define CHD_AMBIGUOUS				0x00000004

#define CHD_SIGNATURE(chd)			((chd).signature)
#define CHD_ATTRIBUTES(chd)			((chd).attributes)
#define CHD_NUMBASES(chd)			((chd).numBaseClasses)
#define CHD_PBCA(chd)				((chd).pBaseClassArray)
#define CHD_PBCD(bcd)				(bcd)
#if (defined(_M_IA64) || defined(_M_AMD64)) && !defined(_M_CEE_PURE)
#define CHD_PBCA_IB(chd,ib)			((_RTTIBaseClassArray*)((ib) + (chd).pBaseClassArray))
#define CHD_PBCD_IB(bcd,ib)			((_RTTIBaseClassDescriptor*)((ib) + bcd))
#endif

//
//	_RTTICompleteObjectLocator
//
#ifndef WANT_NO_TYPES
typedef const struct	_s_RTTICompleteObjectLocator	{
	DWORD							signature;
	DWORD							offset;
	DWORD							cdOffset;
#if VERSP_WIN64 && CC_IA64_SOFT25
	TypeDescriptor							*pTypeDescriptor;
	_RTTIClassHierarchyDescriptor			*pClassDescriptor;
	const _s_RTTICompleteObjectLocator		*pSelf;              // Image relative offset of this object
#elif (defined(_M_IA64) || defined(_M_AMD64)) && !defined(_M_CEE_PURE)
	__int32		    			    pTypeDescriptor;    // Image relative offset of TypeDescriptor
	__int32                         pClassDescriptor;   // Image relative offset of _RTTIClassHierarchyDescriptor
	__int32                         pSelf;              // Image relative offset of this object
#else
	TypeDescriptor					*pTypeDescriptor;
	_RTTIClassHierarchyDescriptor	*pClassDescriptor;
#endif
	} _RTTICompleteObjectLocator;

#endif // WANT_NO_TYPES

#define COL_SIGNATURE(col)			((col).signature)
#define COL_OFFSET(col)				((col).offset)
#define COL_CDOFFSET(col)			((col).cdOffset)
#define COL_PTD(col)				((col).pTypeDescriptor)
#define COL_PCHD(col)				((col).pClassDescriptor)
#if (defined(_M_IA64) || defined(_M_AMD64)) && !defined(_M_CEE_PURE)
#define COL_PTD_IB(col,ib)			((TypeDescriptor*)((ib) + (col).pTypeDescriptor))
#define COL_PCHD_IB(col,ib)			((_RTTIClassHierarchyDescriptor*)((ib) + (col).pClassDescriptor))
#define COL_SELF(col)				((col).pSelf)
#endif // _M_IA64 || _M_AMD64

#define COL_SIG_REV0                0
#define COL_SIG_REV1                1

#ifdef BUILDING_TYPESRC_C
//
// Type of the result of __RTtypeid and internal applications of typeid().
// This also introduces the tag "type_info" as an incomplete type.
//

typedef const class type_info &__RTtypeidReturnType;

//
// Declaration of CRT entrypoints, as seen by the compiler.  Types are 
// simplified so as to avoid type matching hassles.
//

#ifndef THROWSPEC
#if _MSC_VER >= 1300
#define THROWSPEC(_ex) throw _ex
#else
#define THROWSPEC(_ex)
#endif
#endif

// Perform a dynamic_cast on obj. of polymorphic type
extern "C" PVOID 
#ifdef prepifdef
        prepifdef _M_CEE_PURE
        __clrcall
        prepelse
        __cdecl
        prependif        // _M_CEE_PURE
#else
        __CLRCALL_OR_CDECL
#endif
        __RTDynamicCast (
            PVOID,				// ptr to vfptr
            LONG,				// offset of vftable
            PVOID,				// src type
            PVOID,				// target type
            BOOL) THROWSPEC((...)); // isReference

// Perform 'typeid' on obj. of polymorphic type
extern "C" PVOID
#ifdef prepifdef
        prepifdef _M_CEE_PURE
        __clrcall
        prepelse
        __cdecl
        prependif        // _M_CEE_PURE
#else
        __CLRCALL_OR_CDECL
#endif
        __RTtypeid (PVOID)  THROWSPEC((...));	// ptr to vfptr

// Perform a dynamic_cast from obj. of polymorphic type to void*
extern "C" PVOID 
#ifdef prepifdef
        prepifdef _M_CEE_PURE
        __clrcall
        prepelse
        __cdecl
        prependif        // _M_CEE_PURE
#else
        __CLRCALL_OR_CDECL
#endif
        __RTCastToVoid (PVOID)  THROWSPEC((...)); // ptr to vfptr
#endif

#if (defined(_M_IA64) || defined(_M_AMD64)) || defined(VERSP_IA64)	/*IFSTRIP=IGN*/
#pragma pack(pop, rttidata)
#endif
