#ifndef INCLUDED_BINPARSE_H
#define INCLUDED_BINPARSE_H

#include <pehdr.h>
#include <romldr.h>

#include "traverse.h"

bool process_image(const ROMHDR *promhdr, DWORD _rom_offset, SIGNING_CALLBACK _call_back);
bool process_image(const TCHAR *_file_name, SIGNING_CALLBACK _call_back);
bool read_image(void *pDestBuffer, DWORD dwAddress, DWORD dwLength, bool use_rom_offset = true);

#endif
