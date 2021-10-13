// reloc.h
#pragma once

BOOL
RelocateImageFile(CModuleData *pOldMod, CModuleData *pNewMod, LPCSTR szFilename);

HRESULT
ApplyReverseFixups(CImageData *pimgOld, CImageData *pimgNew);
