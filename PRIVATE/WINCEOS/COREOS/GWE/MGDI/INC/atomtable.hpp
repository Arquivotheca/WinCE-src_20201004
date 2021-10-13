//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
/*


*/
#ifndef __ATOM_TABLE_HPP_INCLUDED__
#define __ATOM_TABLE_HPP_INCLUDED__


class AtomTable_t
{

public:


	static
	void
	Init(
		void
		);

	static
	ATOM
	AddAtom(
		const WCHAR*	pName
		);

	static
	void
	DeleteAtom(
		ATOM	aAtom
		);

	static
	unsigned int
	GetAtomName(
		ATOM	aAtom,
		WCHAR*	pBuffer,
		int		CntCharsBuffer
		);

	static
	ATOM
	FindAtom(
		const WCHAR*	pName
		);

};


#endif

