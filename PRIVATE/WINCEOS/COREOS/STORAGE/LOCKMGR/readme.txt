//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//

Lock Manager Object Model
=========================

CFileLockCollection (lockcol)
	|
	+-- CFileLockSet [Exclusive] (lockset)
	|	|	|
	|	|	|
	|	|	+-- CRange (range of set)
	|	|
	|	+-- CFileLockList (locklist)
	|	|	|	|
	|	|	|	|
	|	|	|	+-- CRange (range of list)
	|	|	|
	|	|	+-- CFileLock (lock)
	|	|	|	|
	|	|	|	+-- CRange (range)
	|	|	|
	|	|	+-- CFileLock
	|	|	|	|
	|	|	|	+-- CRange
	|	|	...
	|	|
	|	+-- CFileLockList
	|	|	|
	|	|	...
	|	|
	|	...
	|
	+-- CFileLockSet [Shared]
		|
		...

 - CFileLock (lock) is essentially a range
 - CFileLockList (locklist) is associated with an owner (handle) and is comprised of a list of locks
 - CFileLockSet (lockset) is associated with a lock type (shared, exclusive) and is comprised of multiple lists of locks (an owner can own at most one list in a set)
 - CFileLockCollection (lockcol) is comprised of two lock sets (shared, exclusive)