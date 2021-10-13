//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
// Copyright 2001, Cisco Systems, Inc.  All rights reserved.
// No part of this source, or the resulting binary files, may be reproduced,
// transmitted or redistributed in any form or by any means, electronic or
// mechanical, for any purpose, without the express written permission of Cisco.
//
//---------------------------------------------------------------------------
// alloc.h
//---------------------------------------------------------------------------
// Description:
//
// Revision History:
//
// Date        
//---------------------------------------------------------------------------
// 10/26/00    jbeaujon       -Initial Revision
//
//---------------------------------------------------------------------------

#ifndef __ALLOC_H
#define __ALLOC_H

void* __cdecl operator  new         (size_t size);

void __cdecl operator   delete      (void *p);

void*                   reNew       (void *p, size_t size);

size_t                  getSize     (void   *p);

 
 #endif // __ALLOC_H
