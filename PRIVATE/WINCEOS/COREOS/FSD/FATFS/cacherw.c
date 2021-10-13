/*
		LRU bit definition
		bit 5 (MSB) : Way 1 is used more recently than way 0
		bit 4       : Way 2 is used more recently than way 0
		bit 3       : Way 3 is used more recently than way 0
		bit 2       : Way 2 is used more recently than way 1
		bit 1       : Way 3 is used more recently than way 1
		bit 0 (LSB) : Way 3 is used more recently than way 2
*/


#define	FindLRU(pLRU, dwEntry)	(LRUTable[pLRU[dwEntry]])

#ifdef FindLRU

static const BYTE LRUTable[64] = {
/*	0,	1,	2,	3,	4,	5,	6,	7,	8,	9,	a,	b,	c,	d,	e,	f,	*/
	3  ,2  ,255,2  ,3  ,255,1  ,1  ,255,255,255,2  ,255,255,255,1  ,
	255,255,255,255,3  ,255,1  ,255,255,255,255,255,255,255,1  ,1  ,
	3  ,2  ,255,255,255,255,255,255,255,2  ,255,2  ,255,255,255,255,
	3  ,255,255,255,3  ,255,255,255,0  ,0  ,255,0  ,0  ,255,0  ,0  
};

#else

__inline static int FindLRU (LPBYTE pLRU, DWORD dwEntry)
{
	BYTE LRU=pLRU[dwEntry];
	switch (LRU) {
	// Way 3
	case 0x00:		// 0 - 1 - 2 - 3
	case 0x04:		// 0 - 2 - 1 - 3
	case 0x14:		// 2 - 0 - 1 - 3
	case 0x20:		// 1 - 0 - 2 - 3
	case 0x30:		// 1 - 2 - 0 - 3
	case 0x34:		// 2 - 1 - 0 - 3
		return 3;
	// Way 2
	case 0x01:		// 0 - 1 - 3 - 2
	case 0x03:		// 0 - 3 - 1 - 2
	case 0x0B:		// 3 - 0 - 1 - 2
	case 0x21:		// 1 - 0 - 3 - 2
	case 0x29:		// 1 - 3 - 0 - 2
	case 0x2B:		// 3 - 1 - 0 - 2
		return 2;
	// Way 1
	case 0x06:		// 0 - 2 - 3 - 1
	case 0x07:		// 0 - 3 - 2 - 1
	case 0x0F:		// 3 - 0 - 2 - 1
	case 0x16:		// 2 - 0 - 3 - 1
	case 0x1E:		// 2 - 3 - 0 - 1
	case 0x1F:		// 3 - 2 - 0 - 1
		return 1;
	// Way 0
	case 0x38:		// 1 - 2 - 3 - 0
	case 0x39:		// 1 - 3 - 2 - 0
	case 0x3B:		// 3 - 1 - 2 - 0
	case 0x3C:		// 2 - 1 - 3 - 0
	case 0x3E:		// 2 - 3 - 1 - 0
	case 0x3F:		// 3 - 2 - 1 - 0
		return 0;
	default:
		RETAILMSG(1,(L"Cache LRU Error %x \r\n", LRU));
		return 0;
	}
}
#endif


#define	SetLRU(pLRU, dwEntry, way) \
	pLRU[dwEntry] = (pLRU[dwEntry] \
					| LRUSetTable[way])	\
					& LRUClrTable[way]

#ifdef SetLRU

static const BYTE LRUSetTable[4] = {0x00, 0x20, 0x14, 0x0B};
static const BYTE LRUClrTable[4] = {0x07, 0x39, 0x3E, 0x3F};

#else

__inline static void SetLRU (LPBYTE pLRU, DWORD dwEntry, int way)
{
	BYTE LRU=pLRU[dwEntry];
	switch (way) {
	case 3:
		LRU |= 0x0B;
		// LRU &= 0x3F;
		break;
	case 2:
		LRU |= 0x14;
		LRU &= 0x3E;
		break;
	case 1:
		LRU |= 0x20;
		LRU &= 0x39;
		break;
	case 0:
		// LRU |= 0x00;
		LRU &= 0x07;
		break;
	}
	pLRU[dwEntry]=LRU;
}

#endif

__inline static BOOL IsCacheHit(PVOLUME pvol, DWORD sector, int cSectors)
{
    DWORD	dwSector;
    LPDWORD	pFATCacheLookup		= pvol->v_pFATCacheLookup;
    DWORD	dwCacheEntryMask	= (pvol->v_CacheSize/4)-1;
#ifdef ENABLE_FAT0_CACHE
    DWORD	dwFAT0sec = pvol->v_dwFAT0CacheLookup;
#endif // ENABLE_FAT0_CACHE
#ifdef ENABLE_FAT1_CACHE
    DWORD	dwFAT1sec = pvol->v_dwFAT1CacheLookup;
#endif // ENABLE_FAT1_CACHE

	if ((DWORD)cSectors > pvol->v_CacheSize) {
		// never hit if requested size is bigger than cache size
		return FALSE;
	}
	for (dwSector = sector; dwSector < sector+cSectors; dwSector++) {
		DWORD dwEntry = dwSector & dwCacheEntryMask;
		DWORD dwCacheOffset = dwEntry * 4;

#ifdef ENABLE_FAT0_CACHE
		if (dwFAT0sec == dwSector) {
			continue;
		}
#endif // ENABLE_FAT0_CACHE
#ifdef ENABLE_FAT1_CACHE
		if (dwFAT1sec == dwSector) {
			continue;
		}
#endif // ENABLE_FAT1_CACHE

		if ( (pFATCacheLookup[dwCacheOffset] != dwSector) &&
			 (pFATCacheLookup[dwCacheOffset+1] != dwSector ) &&
			 (pFATCacheLookup[dwCacheOffset+2] != dwSector ) &&
			 (pFATCacheLookup[dwCacheOffset+3] != dwSector ))
			return FALSE;
	}
	return TRUE;
}

__inline static void CacheFill (PVOLUME pvol, DWORD sector, int cSectors, LPVOID pvBuffer)
{
    DWORD	dwSector;
    LPDWORD pFATCacheLookup 	= pvol->v_pFATCacheLookup;
    DWORD 	dwCacheEntryMask 	= (pvol->v_CacheSize/4)-1;
    LPBYTE  pFATCacheBuffer 	= pvol->v_pFATCacheBuffer;
    LPBYTE  pFATCacheLRU 		= pvol->v_pFATCacheLRU;
	LPBYTE	pBuffer = (LPBYTE)pvBuffer; 
#ifdef ENABLE_FAT0_CACHE
    DWORD	dwFAT0sec = pvol->v_secBlkBias;
#endif // ENABLE_FAT0_CACHE
#ifdef ENABLE_FAT1_CACHE
    DWORD	dwFAT1sec = pvol->v_secBlkBias + pvol->v_csecFAT;
#endif // ENABLE_FAT1_CACHE

	if ((DWORD)cSectors > pvol->v_MaxCachedFileSize) {
		return;
	}

	if ((DWORD)cSectors > pvol->v_CacheSize) {
		sector += cSectors - pvol->v_CacheSize;
		pBuffer += 512*(cSectors - pvol->v_CacheSize);
	}
	for (dwSector = sector; dwSector < sector+cSectors; dwSector++) {
		DWORD dwEntry = dwSector & dwCacheEntryMask;
		DWORD dwCacheOffset = dwEntry * 4;
		DWORD way;
#ifdef ENABLE_FAT0_CACHE
		if (dwFAT0sec == dwSector) {
			if (pvol->v_dwFAT0CacheLookup != dwSector) {
				pvol->v_dwFAT0CacheLookup = dwSector;
				memcpy(pvol->v_aFAT0CacheBuffer,pBuffer,512);
			}
			pBuffer += 512;
			continue;
		}
#endif // ENABLE_FAT0_CACHE
#ifdef ENABLE_FAT1_CACHE
		if (dwFAT1sec == dwSector) {
			if (pvol->v_dwFAT1CacheLookup != dwSector) {
				pvol->v_dwFAT1CacheLookup = dwSector;
				memcpy(pvol->v_aFAT1CacheBuffer,pBuffer,512);
			}
			pBuffer += 512;
			continue;
		}
#endif // ENABLE_FAT1_CACHE

		if (pFATCacheLookup[dwCacheOffset] == dwSector) {
			way = 0;
		} else if (pFATCacheLookup[dwCacheOffset+1] == dwSector) {
			way = 1;
		} else if (pFATCacheLookup[dwCacheOffset+2] == dwSector) {
			way = 2;
		} else if (pFATCacheLookup[dwCacheOffset+3] == dwSector) {
			way = 3;
		} else {
			way = FindLRU(pFATCacheLRU, dwEntry);
			pFATCacheLookup[dwCacheOffset + way] = dwSector;
			memcpy(pFATCacheBuffer+(dwCacheOffset+way)*512,pBuffer,512);
		}
		SetLRU(pFATCacheLRU, dwEntry, way);
		pBuffer += 512;
	}
}


__inline static void CacheWrite (PVOLUME pvol, DWORD sector, int cSectors, LPVOID pvBuffer)
{
    DWORD	dwSector;
    LPDWORD pFATCacheLookup 	= pvol->v_pFATCacheLookup;
    DWORD 	dwCacheEntryMask 	= (pvol->v_CacheSize/4)-1;
    LPBYTE  pFATCacheBuffer 	= pvol->v_pFATCacheBuffer;
    LPBYTE  pFATCacheLRU 		= pvol->v_pFATCacheLRU;
	LPBYTE	pBuffer = (LPBYTE)pvBuffer; 
#ifdef ENABLE_FAT0_CACHE
    DWORD	dwFAT0sec = pvol->v_secBlkBias;
#endif // ENABLE_FAT0_CACHE
#ifdef ENABLE_FAT1_CACHE
    DWORD	dwFAT1sec = pvol->v_secBlkBias + pvol->v_csecFAT;
#endif // ENABLE_FAT1_CACHE

	if ((DWORD)cSectors > pvol->v_MaxCachedFileSize) {
		for (dwSector = sector; dwSector < sector+cSectors; dwSector++) {
			DWORD dwEntry = dwSector & dwCacheEntryMask;
			DWORD dwCacheOffset = dwEntry * 4;
			DWORD way;

#ifdef ENABLE_FAT0_CACHE
			if (dwFAT0sec == dwSector) {
				pvol->v_dwFAT0CacheLookup = dwSector;
				memcpy(pvol->v_aFAT0CacheBuffer,pBuffer,512);
				pBuffer += 512;
				continue;
			}
#endif // ENABLE_FAT0_CACHE
#ifdef ENABLE_FAT1_CACHE
			if (dwFAT1sec == dwSector) {
				pvol->v_dwFAT1CacheLookup = dwSector;
				memcpy(pvol->v_aFAT1CacheBuffer,pBuffer,512);
				pBuffer += 512;
				continue;
			}
#endif // ENABLE_FAT1_CACHE

			if (pFATCacheLookup[dwCacheOffset] == dwSector) {
				way = 0;
			} else if (pFATCacheLookup[dwCacheOffset+1] == dwSector) {
				way = 1;
			} else if (pFATCacheLookup[dwCacheOffset+2] == dwSector) {
				way = 2;
			} else if (pFATCacheLookup[dwCacheOffset+3] == dwSector) {
				way = 3;
			} else {
				//way = FindLRU(pFATCacheLRU, dwEntry);
				//pFATCacheLookup[dwCacheOffset + way] = dwSector;
				pBuffer += 512;
				continue;
			}
			// SetLRU(pFATCacheLRU, dwEntry, way);
			memcpy(pFATCacheBuffer+(dwCacheOffset+way)*512,pBuffer,512);
			pBuffer += 512;
		}
		return;
	}

	if ((DWORD)cSectors > pvol->v_CacheSize) {
		sector += cSectors - pvol->v_CacheSize;
		pBuffer += 512*(cSectors - pvol->v_CacheSize);
	}
	for (dwSector = sector; dwSector < sector+cSectors; dwSector++) {
		DWORD dwEntry = dwSector & dwCacheEntryMask;
		DWORD dwCacheOffset = dwEntry * 4;
		DWORD way;

#ifdef ENABLE_FAT0_CACHE
		if (dwFAT0sec == dwSector) {
			pvol->v_dwFAT0CacheLookup = dwSector;
			memcpy(pvol->v_aFAT0CacheBuffer,pBuffer,512);
			pBuffer += 512;
			continue;
		}
#endif // ENABLE_FAT0_CACHE
#ifdef ENABLE_FAT1_CACHE
		if (dwFAT1sec == dwSector) {
			pvol->v_dwFAT1CacheLookup = dwSector;
			memcpy(pvol->v_aFAT1CacheBuffer,pBuffer,512);
			pBuffer += 512;
			continue;
		}
#endif // ENABLE_FAT1_CACHE

		if (pFATCacheLookup[dwCacheOffset] == dwSector) {
			way = 0;
		} else if (pFATCacheLookup[dwCacheOffset+1] == dwSector) {
			way = 1;
		} else if (pFATCacheLookup[dwCacheOffset+2] == dwSector) {
			way = 2;
		} else if (pFATCacheLookup[dwCacheOffset+3] == dwSector) {
			way = 3;
		} else {
			way = FindLRU(pFATCacheLRU, dwEntry);
			pFATCacheLookup[dwCacheOffset + way] = dwSector;
		}
		SetLRU(pFATCacheLRU, dwEntry, way);
		memcpy(pFATCacheBuffer+(dwCacheOffset+way)*512,pBuffer,512);
		pBuffer += 512;
	}
}


__inline static void CacheRead (PVOLUME pvol, DWORD sector, int cSectors, LPVOID pvBuffer)
{
    DWORD	dwSector;
    LPDWORD pFATCacheLookup 	= pvol->v_pFATCacheLookup;
    DWORD 	dwCacheEntryMask 	= (pvol->v_CacheSize/4)-1;
    LPBYTE  pFATCacheBuffer 	= pvol->v_pFATCacheBuffer;
    LPBYTE  pFATCacheLRU 		= pvol->v_pFATCacheLRU;
	LPBYTE	pBuffer = (LPBYTE)pvBuffer; 
#ifdef ENABLE_FAT0_CACHE
    DWORD	dwFAT0sec = pvol->v_dwFAT0CacheLookup;
#endif // ENABLE_FAT0_CACHE
#ifdef ENABLE_FAT1_CACHE
    DWORD	dwFAT1sec = pvol->v_dwFAT1CacheLookup;
#endif // ENABLE_FAT1_CACHE

	for (dwSector = sector; dwSector < sector+cSectors; dwSector++) {
		DWORD dwEntry = dwSector & dwCacheEntryMask;
		DWORD dwCacheOffset = dwEntry * 4;
		DWORD way;

#ifdef ENABLE_FAT0_CACHE
		if (dwFAT0sec == dwSector) {
			memcpy(pBuffer,pvol->v_aFAT0CacheBuffer,512);
			pBuffer += 512;
			continue;
		}
#endif // ENABLE_FAT0_CACHE
#ifdef ENABLE_FAT1_CACHE
		if (dwFAT1sec == dwSector) {
			memcpy(pBuffer,pvol->v_aFAT1CacheBuffer,512);
			pBuffer += 512;
			continue;
		}
#endif // ENABLE_FAT1_CACHE

		if (pFATCacheLookup[dwCacheOffset] == dwSector) {
			way = 0;
		} else if (pFATCacheLookup[dwCacheOffset+1] == dwSector) {
			way = 1;
		} else if (pFATCacheLookup[dwCacheOffset+2] == dwSector) {
			way = 2;
		} else if (pFATCacheLookup[dwCacheOffset+3] == dwSector) {
			way = 3;
		} else {
			RETAILMSG(1,(L"Cache Internal Error \r\n"));
		}
		SetLRU(pFATCacheLRU, dwEntry, way);
		memcpy(pBuffer,pFATCacheBuffer+(dwCacheOffset+way)*512,512);
		pBuffer += 512;
	}
}

