#include <windows.h>
#include <tchar.h>

#ifndef SKIPTABLE_H
#define SKIPTABLE_H

#define EMPTYSKIPTABLE TEXT("empty")
#define MAX_FONTLINKSTRINGLENGTH 256

typedef class cSkipTable {
    public:
        cSkipTable(): m_pnSkipTable(NULL), m_nSkipTableEntryCount(0), m_tszSkipString(EMPTYSKIPTABLE) {}
        ~cSkipTable();

        bool InitializeSkipTable(const TCHAR *tszSkipString);
        bool IsInSkipTable(int nSkipEntry);
        bool OutputSkipTable();
        TCHAR* SkipString();

        private:
            int *m_pnSkipTable;
            int m_nSkipTableEntryCount;
            TCHAR *m_tszSkipString;
} SKIPTABLE;
#endif

