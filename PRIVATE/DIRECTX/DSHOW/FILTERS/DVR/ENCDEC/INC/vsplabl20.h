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
// THE SOURCE CODE IS PROVIDED "AS IS", WITH NO WARRANTIES.
//
#ifndef	_VSPLABL_H
#define	_VSPLABL_H
//
// VSPWeb labels and macros
//
// 11/30/99
//

// Ignore unreferenced labels.
#pragma warning (disable : 4102)


//
// Macros for defining arrays of pointers to protected data.
//
// NOTE:
//	Vulcan will not allow enumeration of relocations with a data
//  unless it has been accessed. The macros TOUCH_SCP_DATA()
//  and TOUCH_DATA can be used to access the data without
//  altering it.
//

#define SCP_DATA(SegNum1, SegNum2, CryptMethod, Significance) \
void const *ScpProtectedData_##SegNum1##_##SegNum2##_##CryptMethod##_##Significance##_00_00

#define SCP_DATA_ARRAY(SegNum1, SegNum2, CryptMethod, Significance) \
ScpProtectedData_##SegNum1##_##SegNum2##_##CryptMethod##_##Significance##_00_00

#define	SCP_PROTECTED_DATA_LIST	"ScpProtectedData"

#define TOUCH_SCP_DATA(SegNum1, SegNum2, CryptMethod, Significance) { \
_asm	push	EAX \
_asm	lea		EAX, ScpProtectedData_##SegNum1##_##SegNum2##_##CryptMethod##_##Significance##_00_00 \
_asm	pop		EAX }

#define	TOUCH_DATA( x ) {	\
_asm	push	EAX			\
_asm	lea		EAX,x		\
_asm	pop		EAX	}


//
// Macros for defining arrays of pointers to encrypted data.
//
// If the values for Significance, Proximity, Redundance are
// all set to zero the SCP tool will treat a segment as an
// encrypted segment and will not insert random verification
// calls and instead use manually inserted labels for locations
// where calls for encryption and decryption should be inserted.
//

#define SCP_ENCRYPTED_DATA(SegNum1, SegNum2, CryptMethod) \
void const *ScpProtectedData_##SegNum1##_##SegNum2##_##CryptMethod##_0xffffffff_00_00

#define TOUCH_SCP_ENCRYPTED_DATA(SegNum1, SegNum2, CryptMethod) TOUCH_SCP_DATA(SegNum1, SegNum2, CryptMethod, 0xffffffff) 

#define	CRYPTO_PREFIX		"CRYPTO_SEGMENT_HERE"
#define ENCRYPT_DATA_PREFIX "CRYPTO_SEGMENT_HERE_ENCRYT_"
#define DECRYPT_DATA_PREFIX "CRYPTO_SEGMENT_HERE_DECRYPT_"

#define ENCRYPT_DATA( SegNum1, SegNum2, FUNCTION_UNIQUE )	CRYPTO_SEGMENT_HERE_ENCRYT_##SegNum1##_##SegNum2##_##FUNCTION_UNIQUE##:
#define DECRYPT_DATA( SegNum1, SegNum2, FUNCTION_UNIQUE )	CRYPTO_SEGMENT_HERE_DECRYPT_##SegNum1##_##SegNum2##_##FUNCTION_UNIQUE##:

#define	HARD_CODED_CLEANUP_CALL_PREFIX	"CRYPTO_CLEANUP_HERE_"
#define CRYPTO_CLEANUP( FunctionUnique ) CRYPTO_CLEANUP_HERE_##FunctionUnique: { _asm mov eax,eax }
#define HARD_CODED_CLEANUP_CALL_PREFIX_LEN 20


//
// Macros for defining verification call placement.
//

#define SCP_VERIFY_CALL_PLACEMENT(MarkerId) \
SCP_VERIFY_CALL_PLACEMENT_##MarkerId##:

//
// VSPWeb macros for verified segments outside functions
//
// New issues introduced by VC7 as used in the Whistler build environment:
//
// 1. Unlike VC6, VC7 removes all "dead" code even when compiling with /Zi
// (to produce PDBs).
//
// 2. VC7 removes duplicate functions (i.e., functions containing code
// VC7 considers identical).  Such functions need not actually be
// identical; for example, VC7 considers the following equivalent and
// throws one out:
//
// _declspec(naked) void foo() {
//   __asm lea eax, FOO_LABEL;
//   FOO_LABEL:__asm ret;
// }
//
// _declspec(naked) void bar() {
//   __asm lea eax, BAR_LABEL;
//   BAR_LABEL:__asm ret;
// }
//
// VC7 eliminates one of these despite attempts to reference them
// separately in live code; e.g., the following two instructions
// will be identical:
//  __asm cmp eax, foo;
//  __asm cmp eax, bar;
//
// The macros below were changed to handle this.  Each generated function
// now contains different code, created using the unique segment IDs of
// each segment.  Additionally, using the TOUCH_SCP_SEG_FUN macro in
// live code is now necessary to ensure VC7 keeps these functions.
//
// NOTE: This makes it easier for the cracker to locate segments by their
// IDs.  We should replace this temporary solution with something better.
//

#define BEGIN_FUNC_PREFIX       "Begin_Vspweb_Scp_Segment_"
#define BEGIN_FUNC_PREFIX_LEN   25

#define END_FUNC_PREFIX         "End_Vspweb_Scp_Segment_"
#define END_FUNC_PREFIX_LEN     23

#define BEGIN_VSPWEB_SCP_SEGMENT(SegNum1, SegNum2, CryptMethod, Significance, Proximity, Redundance) \
_declspec(naked) void Begin_Vspweb_Scp_Segment_##SegNum1##_##SegNum2() \
{ \
__asm { \
	__asm mov eax, SegNum1 \
	} \
BEGIN_SCP_SEGMENT_##SegNum1##_##SegNum2##_##CryptMethod##_##Significance##_##Proximity##_##Redundance: \
__asm { \
    __asm mov ebx, SegNum2 \
    __asm ret \
	} \
} 

#define END_VSPWEB_SCP_SEGMENT(SegNum1, SegNum2) \
_declspec(naked) void End_Vspweb_Scp_Segment_##SegNum1##_##SegNum2() \
{ \
__asm { \
	__asm mov ecx, SegNum1 \
	} \
END_SCP_SEGMENT_##SegNum1##_##SegNum2: \
__asm { \
    __asm mov edx, SegNum2 \
    __asm ret \
	} \
} 

// This macro "touches" the functions generated by the above macros,
// and must be placed somewhere within live code for each verified
// segment defined by the above.
#define TOUCH_SCP_SEG_FUN(SegNum1, SegNum2) \
  void End_Vspweb_Scp_Segment_##SegNum1##_##SegNum2(); \
  void Begin_Vspweb_Scp_Segment_##SegNum1##_##SegNum2(); \
  __asm { \
    __asm cmp eax, Begin_Vspweb_Scp_Segment_##SegNum1##_##SegNum2 \
    __asm cmp eax, End_Vspweb_Scp_Segment_##SegNum1##_##SegNum2 \
} 

#if 0
//
// some hacks to make sure some VSP library functions are included in the
// application to be protected (to avoid these functions from being dynamically
// linked)
//
// Note: These are no longer necessary because the relevant code is injected
// automatically.
//

// Put this in some source file.
#define VSPWEB_DECL_SETUP \
extern "C" { \
extern void __cdecl DoComparison01(void *, void *, int); \
extern void __cdecl DoVerification01(void *, void *, void *, void *, BYTE, BYTE, BYTE, int);  \
extern void __cdecl DoVerification02(BYTE, void *, void *, void *, void *, BYTE, BYTE, int); \
extern void __cdecl DoVerification03(void *, void *, void *, BYTE, BYTE, BYTE, int, void *); \
extern void __cdecl DoVerification04(void *, void *, void *, BYTE, BYTE, BYTE, int, void *); \
extern void __cdecl DoVerification05(void *, void *, void *, void *, BYTE, BYTE, BYTE, int); \
}

// Put this in one live function.
#define VSPWEB_SETUP \
{ \
	__asm cmp eax, DoComparison01 \
	__asm cmp ebx, DoVerification01 \
	__asm cmp ebx, DoVerification02 \
	__asm cmp ebx, DoVerification03 \
	__asm cmp ebx, DoVerification04 \
	__asm cmp ebx, DoVerification05 \
}
#endif

#define	CRYPT_METHOD_AUTO	0
#define	SCP_AUTO_MAC_LIST	"AutoCheckSums"
#define	SCP_TOTAL_AUTO_MACS	"TotalAutoCheckSums"


#endif // #indef	_VSPLABL_H
