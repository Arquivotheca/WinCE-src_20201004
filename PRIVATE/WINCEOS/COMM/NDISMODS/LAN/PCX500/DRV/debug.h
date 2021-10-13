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
//  debug.h
#include "Card.h"
USHORT	X500ReadWord(PCARD card, USHORT offset);
void	X500WriteWord(PCARD card, USHORT offset, USHORT val);
void	X500ReadBytes(PCARD card, void *pbuf, UINT offset, USHORT len);
void	X500WriteBytes(PCARD card, void *pbuf, UINT offset, USHORT len);
USHORT	X500ReadAuxWord(PCARD card, UINT offset);
void	X500WriteAuxWord(PCARD card, UINT offset, USHORT val);
void	X500ReadAuxBytes(PCARD card, void *pbuf, UINT offset, USHORT len);
void	X500WriteAuxBytes(PCARD card, void *pbuf, UINT offset, USHORT len);

