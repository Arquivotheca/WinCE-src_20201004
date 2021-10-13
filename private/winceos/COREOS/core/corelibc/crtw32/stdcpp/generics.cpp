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
*generics.cpp - STL/CLR Generics Definition and Assembly attributes
*
*       Copyright (c) Microsoft Corporation. All rights reserved.
*
*Purpose:
*
*Revision History:
*       05-27-08  HG    Added header.
*
*******************************************************************************/
#include "generics.h"

#using <mscorlib.dll>
#using <System.dll>

using namespace System::Reflection;
using namespace System::Security::Permissions;

[assembly: AssemblyTitle("Microsoft.VisualC.STLCLR")];
[assembly: AssemblyDescription("STLCLR cross assembly library")];
[assembly: AssemblyConfiguration("")];
[assembly: AssemblyCompany("Microsoft")];
[assembly: AssemblyProduct("STLCLR")];
[assembly: AssemblyCopyright("")];
[assembly: AssemblyTrademark("")];
[assembly: AssemblyCulture("")];
[assembly: AssemblyVersion("2.0.0.0")];
[assembly: AssemblyDelaySign(true)];
[assembly: AssemblyKeyName("")];

[assembly: PermissionSet(SecurityAction::RequestOptional,
	Name = "Nothing")];
[assembly: System::CLSCompliant(true)];
[assembly: System::Runtime::InteropServices::ComVisible(false)];

[module:
	System::Diagnostics::CodeAnalysis::SuppressMessage("Microsoft.MSInternal",
	"CA904",
	Scope = "namespace",
	Target = "Microsoft.VisualC.StlClr")];
[module:
	System::Diagnostics::CodeAnalysis::SuppressMessage("Microsoft.MSInternal",
	"CA904",
	Scope = "namespace",
	Target = "Microsoft.VisualC.StlClr.Generic")];
