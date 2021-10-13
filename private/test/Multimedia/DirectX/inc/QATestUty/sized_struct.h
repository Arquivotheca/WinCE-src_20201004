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
// ============================================================================
// Library sized_struct
//      
//      Provides a template class for structures with a member called dwSize
//      Adds construction, assignment and comparison semanitcs to the simple struct.
//      Since the sized_struct is derived from the template parameter type, 
//          it is semantically synonymous with the parameter type.
//      Also provides Preset semantics (see PresetUty.h>
//
// History:
//      01/26/2000 · ericflee   · created
// ============================================================================

// External dependencies
// ¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯
#include "PresetUty.h"

template <class TBase>
struct sized_struct : public TBase
{
    typedef sized_struct<TBase> this_type;

    // constructors
    inline sized_struct() { Clear(); }
    inline explicit sized_struct(const this_type& dsbd) { Copy(dsbd); }
    inline explicit sized_struct(const TBase& dsbd) { Copy(dsbd); }
    
    // assignment operators
    inline this_type& operator = (const TBase& dsbd) { Copy(dsbd); return *this; }
    inline this_type& operator = (const this_type& dsbd) { Copy(dsbd); return *this; }

    // predicate operators
    inline bool operator == (const TBase& dsbd) const { return 0==memcmp(this, &dsbd,sizeof(TBase)); }
    inline bool operator == (const this_type& dsbd) const { return 0==memcmp(this, &dsbd,sizeof(TBase)); }
    inline bool operator != (const TBase& dsbd) const { return 0!=memcmp(this, &dsbd,sizeof(TBase)); }
    inline bool operator != (const this_type& dsbd) const { return 0!=memcmp(this, &dsbd,sizeof(TBase)); }

    // utilities
    inline void Clear() { memset(this, 0, sizeof(TBase)); dwSize=sizeof(TBase); }
    inline void Copy(const TBase& dsbd) { memcpy(this, &dsbd, sizeof(TBase)); }
    inline void Copy(const this_type& dsbd) { memcpy(this, &dsbd, sizeof(TBase)); }

    inline void Preset() 
    { 
        using namespace PresetUty;
        PresetStruct(*this); 
        dwSize=sizeof(TBase); 
    }

    inline bool IsPreset() 
    { 
        using namespace PresetUty;
        // inefficient, but since this is only for test purposes, it doesn't really matter
        this_type tmp; 
        tmp.Preset();
        return *this == tmp;
    }
};
