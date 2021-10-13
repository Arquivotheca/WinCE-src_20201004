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

#ifndef __CONVERTBASE64_h__
#define __CONVERTBASE64_h__

static byte char_to_base64 [128];
static char base64_to_char [64];
static BOOL tables_initialised = FALSE;

/*  -------------------------------------------------------------------------
    init_conversion_tables function -- internal
    initialise the tables conversion for BASE64 coding
    -----------------------------------------------------------------------*/

static void
init_conversion_tables (void)
{
    byte
        value,                          /*  Value to store in table          */
        offset,
        index;                          /*  Index in table                   */

    /*  Reset the tables                                                     */
    memset (char_to_base64, 0, sizeof (char_to_base64));
    memset (base64_to_char, 0, sizeof (base64_to_char));

    value  = 'A';
    offset = 0;

    for (index = 0; index < 62; index++)
      {
        if (index == 26)
          {
            value  = 'a';
            offset = 26;
          }
        else
        if (index == 52)
          {
            value  = '0';
            offset = 52;
          }
        base64_to_char [index] = value + index - offset;
        char_to_base64 [value + index - offset] = index;
      }
    base64_to_char [62]  = '+';
    base64_to_char [63]  = '/';
    char_to_base64 ['+'] = 62;
    char_to_base64 ['/'] = 63;

    tables_initialised = TRUE;
}




// http://www.imatix.com/html/sfl/sfl209.htm

// Encodes a source buffer in Base 64 and stores the result in the target buffer. 
// The target buffer must be at least 1/3rd longer than the amount of data in the source buffer. 
// The base64 data consists of portable printable characters as defined in RFC 1521. 
// Returns the number of bytes output into the target buffer. 

inline size_t encode_base64 (const byte *source, byte *target, size_t source_size)
{
    size_t
        target_size = 0;                /*  Length of target buffer          */
    int
        nb_block;                       /*  Total number of blocks           */
    byte
        *p_source,                      /*  Pointer to source buffer         */
        *p_target,                      /*  Pointer to target buffer         */
        value;                          /*  Value of Base64 byte             */

    _ASSERT (source);
    _ASSERT (target);

    if (source_size == 0)
        return (0);

    if (!tables_initialised)
        init_conversion_tables ();

    /*    Bit positions
                  | byte 1 | byte 2 | byte 3 |
    source block   87654321 87654321 87654321         -> 3 bytes of 8 bits

                  | byte 1 | byte 2 | byte 3 | byte 4 |
    Encoded block  876543   218765   432187   654321  -> 4 bytes of 6 bits
    */

    nb_block = (int) (source_size / 3);

    /*  Check if we have a partially-filled block                            */
    if (nb_block * 3 != (int) source_size)
        nb_block++;
    target_size = (size_t) nb_block * 4;
    target [target_size] = '\0';

    p_source = (byte *) source;         /*  Point to start of buffers        */
    p_target = target;

    while (nb_block--)
      {
        /*  Byte 1                                                           */
        value       = *p_source >> 2;
        *p_target++ = base64_to_char [value];

        /*  Byte 2                                                           */
        value = (*p_source++ & 0x03) << 4;
        if ((size_t) (p_source - source) < source_size)
            value |= (*p_source & 0xF0) >> 4;
        *p_target++ = base64_to_char [value];

        /*  Byte 3 - pad the buffer with '=' if block not completed          */
        if ((size_t) (p_source - source) < source_size)
          {
            value = (*p_source++ & 0x0F) << 2;
            if ((size_t) (p_source - source) < source_size)
                value |= (*p_source & 0xC0) >> 6;
            *p_target++ = base64_to_char [value];
          }
        else
            *p_target++ = '=';

        /*  Byte 4 - pad the buffer with '=' if block not completed          */
        if ((size_t) (p_source - source) < source_size)
          {
            value       = *p_source++ & 0x3F;
            *p_target++ = base64_to_char [value];
          }
        else
            *p_target++ = '=';
     }
   return (target_size);
}


// http://www.imatix.com/html/sfl/sfl210.htm

// Decodes a block of Base 64 data and stores the resulting binary data in a target buffer. 
// The target buffer must be at least 3/4 the size of the base 64 data. 
// Returns the number of characters output into the target buffer. 

inline size_t decode_base64 (const byte *source, byte *target, size_t source_size)
{
    size_t
        target_size = 0;                /*  Length of target buffer          */
    int
        nb_block;                       /*  Total number of block            */
    byte
        value,                          /*  Value of Base64 byte             */
        *p_source,                      /*  Pointer in source buffer         */
        *p_target;                      /*  Pointer in target buffer         */

    _ASSERT (source);
    _ASSERT (target);

    if (source_size == 0)
        return (0);

    if (!tables_initialised)
        init_conversion_tables ();

    /*  Bit positions
                  | byte 1 | byte 2 | byte 3 | byte 4 |
    Encoded block  654321   654321   654321   654321  -> 4 bytes of 6 bits
                  | byte 1 | byte 2 | byte 3 |
    Decoded block  65432165 43216543 21654321         -> 3 bytes of 8 bits
    */

    nb_block    = (int)source_size / 4;
    target_size = (size_t) nb_block * 3;
    target [target_size] = '\0';

    p_source = (byte *) source;         /*  Point to start of buffers        */
    p_target = target;

    while (nb_block--)
      {
        /*  Byte 1                                                           */
        *p_target    = char_to_base64 [(byte) *p_source++] << 2;
        value        = char_to_base64 [(byte) *p_source++];
        *p_target++ += ((value & 0x30) >> 4);

        /*  Byte 2                                                           */
        *p_target    = ((value & 0x0F) << 4);
        value        = char_to_base64 [(byte) *p_source++];
        *p_target++ += ((value & 0x3C) >> 2);

        /*  Byte 3                                                           */
        *p_target    = (value & 0x03) << 6;
        value        = char_to_base64 [(byte) *p_source++];
        *p_target++ += value;
      }
   return (target_size);
}

/*inline HRESULT Base64EncodeBinary(BYTE* pByte, DWORD cbSize, BSTR* pbstrEncoded)
{
   char* pszTarget = new char[(2*cbSize) + 4];
   if(!pszTarget)
      return E_OUTOFMEMORY;

   ZeroMemory(pszTarget, (2*cbSize) + 4);

   encode_base64((const unsigned char*)pByte, (unsigned char*)pszTarget, cbSize);

   *pbstrEncoded = SysAllocStringLen(NULL, (2*cbSize) + 2);
   if(!pbstrEncoded)
      return E_OUTOFMEMORY;

   MultiByteToWideChar(CP_ACP, 0, pszTarget, -1, *pbstrEncoded, (2*cbSize) + 2);

   delete[] pszTarget;

   return S_OK;
}
*/
inline HRESULT Base64DecodeBinary(BSTR bstrEncoded, DWORD* pcbSize, BYTE** ppDecoded)
{
   // *ppDecoded is allocated with "new" operator ** MUST FREE **

   int nLen = ocslen(bstrEncoded);
   char* pszBase64 = new char[nLen+1];
   if(!pszBase64)
      return E_OUTOFMEMORY;

   int nConvertedLen = nLen * 2;

   *ppDecoded = new BYTE[nConvertedLen+1];
   if(!*ppDecoded)
     return E_OUTOFMEMORY;

   ZeroMemory(pszBase64, nLen+1);
   ZeroMemory(*ppDecoded, nConvertedLen+1);

   WideCharToMultiByte(CP_ACP, 0, bstrEncoded, -1, pszBase64, nLen, NULL, NULL);
   *pcbSize = (DWORD)decode_base64((const unsigned char*)pszBase64, (unsigned char*)*ppDecoded, nLen);

   delete[] pszBase64;

   return S_OK;
}

inline HRESULT Base64Encode(BSTR bstrText, BSTR* pbstrEncoded)
{
   int nLen = ocslen(bstrText);
   char* pszSource = new char[nLen+1];
   if(!pszSource)
      return E_OUTOFMEMORY;

   int nConvertedLen = 2*nLen + 2;

   char* pszTarget = new char[nConvertedLen+1];
   if(!pszTarget)
      return E_OUTOFMEMORY;

   ZeroMemory(pszSource, nLen+1);
   ZeroMemory(pszTarget, nConvertedLen+1);

   WideCharToMultiByte(CP_ACP, 0, bstrText, -1, pszSource, nLen, NULL, NULL);
   encode_base64((const unsigned char*)pszSource, (unsigned char*)pszTarget, nLen);

   *pbstrEncoded = SysAllocStringLen(NULL, nConvertedLen);
   if(!pbstrEncoded)
      return E_OUTOFMEMORY;

   MultiByteToWideChar(CP_ACP, 0, pszTarget, -1, *pbstrEncoded, nConvertedLen);

   delete[] pszSource;
   delete[] pszTarget;

   return S_OK;
}

inline HRESULT Base64Decode(BSTR bstrEncoded, BSTR* pbstrText)
{
   int nLen = ocslen(bstrEncoded);
   char* pszBase64 = new char[nLen+1];
   if(!pszBase64)
      return E_OUTOFMEMORY;

   int nConvertedLen = nLen * 2;

   char* pszTarget = new char[nConvertedLen+1];
   if(!pszTarget)
      return E_OUTOFMEMORY;

   ZeroMemory(pszBase64, nLen+1);
   ZeroMemory(pszTarget, nConvertedLen+1);

   WideCharToMultiByte(CP_ACP, 0, bstrEncoded, -1, pszBase64, nLen, NULL, NULL);
   decode_base64((const unsigned char*)pszBase64, (unsigned char*)pszTarget, nLen);

   *pbstrText = SysAllocStringLen(NULL, nConvertedLen);
   if(!pbstrText)
      return E_OUTOFMEMORY;

   MultiByteToWideChar(CP_ACP, 0, pszTarget, -1, *pbstrText, nConvertedLen);

   delete[] pszBase64;
   delete[] pszTarget;

   return S_OK;
}

inline HRESULT CreateStreamOnBase64(BSTR bstrBase64, IStream** ppStream)
{
   HRESULT hr = 0;

   int nLen = ocslen(bstrBase64);
   char* pszBase64 = new char[nLen+1];
   if(!pszBase64)
      return E_OUTOFMEMORY;

   ZeroMemory(pszBase64, nLen+1);

   WideCharToMultiByte(CP_ACP, 0, bstrBase64, -1, pszBase64, nLen, NULL, NULL);

   int nNewLen = 3*(nLen/4);
   HGLOBAL hGlobal = GlobalAlloc(GMEM_MOVEABLE, nNewLen+1);

   if(!hGlobal)
   {
      delete[] pszBase64;
      return E_OUTOFMEMORY;
   }

   unsigned char* pszTarget = (unsigned char*)GlobalLock(hGlobal);
   ZeroMemory(pszTarget, nNewLen);
   int nRetSize = (int)decode_base64((const unsigned char*)pszBase64, (unsigned char*)pszTarget, nLen);
   _ASSERT(nRetSize == nNewLen);

   BOOL bOK = GlobalUnlock(hGlobal);
   if(!bOK)
   {
      DWORD dwErr = GetLastError();
   }

   hr = CreateStreamOnHGlobal(hGlobal, TRUE, ppStream);
   _ASSERT(SUCCEEDED(hr));

   ULARGE_INTEGER liSize;
   liSize.QuadPart = nRetSize;
   hr = (*ppStream)->SetSize(liSize);
   _ASSERT(SUCCEEDED(hr));

   delete[] pszBase64;

   return S_OK;
}

#endif // __CONVERTBASE64_h__