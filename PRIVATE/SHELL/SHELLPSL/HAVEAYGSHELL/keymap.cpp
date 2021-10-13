#include <windows.h>
#include <winuserm.h>

// reg key defines
TCHAR const c_tszRegValueApp[]       = TEXT("Application");
TCHAR const c_tszRegValueAppPath[]   = TEXT("Path");
TCHAR const c_tszRegValueAppFlags[]  = TEXT("Flags");
TCHAR const c_tszRegPathShellKeys[]  = TEXT("Software\\Microsoft\\Shell\\Keys");

// other defines
#define KEY_ENTRY_COUNT     6
#define NAMESIZE            MAX_PATH

#define SAFE_FREE( x ) if ( x ) { free( x ); x = NULL; }

// keymap entry struct
typedef struct _KEYMAPENTRY
{
    BYTE    key;
    LPTSTR  szAppName;
    LPTSTR  szAppPath;
    LPTSTR  szAppFlags;
    HWND    hwnd;
    LPTSTR  szWndClass;
} KEYMAPENTRY;

// Convenience Functions
//
// Convert hex string to dword.
//
// The string must be uppercase.
//
DWORD HexToDw( LPCTSTR ptszHex )
{
    DWORD dwVal = 0;
    int i, nLen;


    for( i = 0, nLen = _tcslen( ptszHex ); i < nLen; i++ ) {
        if( ptszHex[i] >= TEXT('0') && ptszHex[i] <= TEXT('9') ) {
            dwVal = (dwVal << 4) | (ptszHex[i] - TEXT('0'));
        } else {
            dwVal = (dwVal << 4) | (ptszHex[i] - TEXT('A') + 0xA);
        }
    }

    return dwVal;
}

// class definition
class Keymap
{
private:
    BOOL LoadKeyFromRegistry( int iIndex );
    BOOL FreeKey( int iIndex );

    // MEMBER DATA
    // note: since we are only supporting VK_APP1-VK_APP6, can keep the keymap
    // as an array.  If this assumption changes, will need to change the keymap
    // to be a hash table or some other memory-saving fast-access method
    KEYMAPENTRY m_keys[ KEY_ENTRY_COUNT ];

public:
	Keymap();
	~Keymap();

    BOOL AddToMap( BYTE key, HWND hwnd );
    BYTE GetKeyForApp( LPCTSTR ptszApp );
    BOOL ForwardKey( UINT uiMsg, WPARAM wParam, LPARAM lParam );
};

/* Keymap class methods
 *
 */
Keymap::Keymap()
{
    // initialize keymap
    for ( int i = 0; i < KEY_ENTRY_COUNT; i++ )
    {
        m_keys[ i ].szAppName = NULL;
        m_keys[ i ].szAppPath = NULL;
        m_keys[ i ].szAppFlags = NULL;
        m_keys[ i ].hwnd = NULL;
        m_keys[ i ].szWndClass = NULL;
        m_keys[ i ].key = VK_APP1 + i;
    }
}

Keymap::~Keymap()
{
    // free keymap strings
    for ( int i = 0; i < KEY_ENTRY_COUNT; i++ )
    {
        FreeKey( i );
    }
}

BOOL Keymap::LoadKeyFromRegistry( int iIndex )
{
    TCHAR tszKey[MAX_PATH];
    DWORD dwClassChars = NAMESIZE;
    TCHAR tszClass[NAMESIZE];
    LPTSTR ptszAppCmd = tszClass;
    DWORD dwRes;
    HKEY hkey;
    BOOL bReturn = TRUE;

    if ( ( iIndex < 0 ) || ( iIndex >= KEY_ENTRY_COUNT ) )
    {
        return FALSE;
    }

    // assume that the keymap already has a key for this
    wsprintf(
        tszKey,
        TEXT("%s\\%2.2X"),
        c_tszRegPathShellKeys,
        m_keys[ iIndex ].key );

    if ( ERROR_SUCCESS != ( dwRes = RegOpenKeyEx(
                                        HKEY_LOCAL_MACHINE,
                                        tszKey,
                                        0,
                                        0,
                                        &hkey ) ) )
    {
        SetLastError( dwRes );
        return FALSE;
    }

    dwClassChars = NAMESIZE * sizeof(TCHAR);
    if( ERROR_SUCCESS == RegQueryValueEx(
                                hkey,
                                c_tszRegValueApp,
                                NULL,
                                NULL,
                                (BYTE *)ptszAppCmd,
                                &dwClassChars ) &&
                                ( _tcslen( ptszAppCmd ) > 0 ) )
    {
        // add to map
        if ( !m_keys[ iIndex ].szAppName || ( _tcscmp( m_keys[ iIndex ].szAppName, ptszAppCmd ) != 0 ) )
        {
            if ( m_keys[ iIndex ].szAppName )
            {
                free( m_keys[ iIndex ].szAppName );
            }

            // App name is different, reset path and flags
            SAFE_FREE( m_keys[ iIndex ].szAppPath );
            SAFE_FREE( m_keys[ iIndex ].szAppFlags );

            m_keys[ iIndex ].szAppName = (TCHAR *) malloc( ( _tcslen( ptszAppCmd ) + 1 ) * sizeof( TCHAR ) );
            _tcscpy( m_keys[ iIndex ].szAppName, ptszAppCmd );
        }
    }
    else
    {
        bReturn = FALSE;
        goto exit;
    }


    //
    // get app path
    //
    dwClassChars = NAMESIZE * sizeof(TCHAR);
    if ( ( ERROR_SUCCESS == RegQueryValueEx(
                            hkey,
                            c_tszRegValueAppPath,
                            NULL,
                            NULL,
                            (BYTE *)ptszAppCmd,
                            &dwClassChars ) ) &&
         ( _tcslen( ptszAppCmd ) > 0 ) )
    {
        // still need to check against the path if it exists in case the app name is the same, but
        // the path is different
        if ( !m_keys[ iIndex ].szAppPath || ( _tcscmp( m_keys[ iIndex ].szAppPath, ptszAppCmd ) != 0 ) )
        {
            if ( m_keys[ iIndex ].szAppPath )
            {
                free( m_keys[ iIndex ].szAppPath );
            }
            m_keys[ iIndex ].szAppPath = (TCHAR *) malloc( ( _tcslen( ptszAppCmd ) + 1 ) * sizeof( TCHAR ) );
            _tcscpy( m_keys[ iIndex ].szAppPath, ptszAppCmd );
        }
    }

    //
    // get app flags
    //
    dwClassChars = NAMESIZE * sizeof(TCHAR);
    if ( ( ERROR_SUCCESS == RegQueryValueEx(
                            hkey,
                            c_tszRegValueAppFlags,
                            NULL,
                            NULL,
                            (BYTE *)ptszAppCmd,
                            &dwClassChars ) ) &&
         ( _tcslen( ptszAppCmd ) > 0 ) )
    {
        // still need to check against the flags if it exists in case the app name is the same, but
        // the flags is different
        if ( !m_keys[ iIndex ].szAppFlags || ( _tcscmp( m_keys[ iIndex ].szAppFlags, ptszAppCmd ) != 0 ) )
        {
            if ( m_keys[ iIndex ].szAppFlags )
            {
                free( m_keys[ iIndex ].szAppFlags );
            }

            m_keys[ iIndex ].szAppFlags = (TCHAR *) malloc( ( _tcslen( ptszAppCmd ) + 1 ) * sizeof( TCHAR ) );
            _tcscpy( m_keys[ iIndex ].szAppFlags, ptszAppCmd );
        }
    }

exit:
    RegCloseKey( hkey );

    return bReturn;
}

BOOL Keymap::FreeKey( int iIndex )
{
    if ( ( iIndex < 0 ) || ( iIndex >= KEY_ENTRY_COUNT ) )
    {
        return FALSE;
    }

    SAFE_FREE( m_keys[ iIndex ].szAppName );
    SAFE_FREE( m_keys[ iIndex ].szAppPath );
    SAFE_FREE( m_keys[ iIndex ].szAppFlags );
    SAFE_FREE( m_keys[ iIndex ].szWndClass );
    m_keys[ iIndex ].hwnd = NULL;

    return TRUE;
}

/* Public functions */
BOOL Keymap::AddToMap( BYTE key, HWND hwnd )
{
    // assumes valid keys start at VK_APP1 and run in a range
    int iIndex = key - VK_APP1;
    // REVIEW: don't worry about freeing app info
    // it will get filled in when this window becomes
    // invalid
    if ( !FreeKey( iIndex ) )
    {
        return FALSE;
    }

    m_keys[ iIndex ].hwnd = hwnd;
    TCHAR szClass[MAX_PATH];
    int iLen = GetClassName( hwnd, szClass, MAX_PATH - 1 );
    if ( iLen > 0 )
    {
        m_keys[ iIndex ].szWndClass = (TCHAR *) malloc( (iLen + 1) * sizeof( TCHAR ) );
        _tcscpy( m_keys[ iIndex ].szWndClass, szClass );
    }

    return TRUE;
}

BYTE Keymap::GetKeyForApp( LPCTSTR ptszApp )
{
	if ( !ptszApp )
	{
		return 0;
	}

    for ( int i = 0; i < KEY_ENTRY_COUNT; i++ )
    {
        if ( m_keys[ i ].szAppName &&
             ( _tcsicmp( ptszApp, m_keys[ i ].szAppName ) == 0 ) )
        {
            return m_keys[ i ].key;
        }
    }

    return 0;
}

BOOL Keymap::ForwardKey( UINT uiMsg, WPARAM wParam, LPARAM lParam )
{
    //
    // Ignore key repeats.  We don't get a key repeat on WinCE (reliably), but
    // we see that the transition state is down and that the key was
    // already down.
    //
    if( 0x40000000 == (lParam & 0xC0000000) )
    {
        return TRUE;
    }

    if ( ( uiMsg == WM_SYSKEYDOWN ) || ( uiMsg == WM_KEYDOWN ) || ( uiMsg == WM_SYSKEYUP ) || ( uiMsg == WM_KEYUP ) )
    {
        int iIndex = wParam - VK_APP1;

        if ( ( iIndex >= 0 ) && ( iIndex < KEY_ENTRY_COUNT ) )
        {
/*            KEY_STATE_FLAGS dwShiftState = KeyShiftNoCharacterFlag;
            DWORD dwFlags = KeyStatePrevDownFlag;

            if( WM_KEYDOWN == uiMsg ) {
                dwShiftState |= KeyStateDownFlag;
                dwFlags = 0;
            }
*/
            if ( ( m_keys[ iIndex ].hwnd != NULL ) &&
                 IsWindow( m_keys[ iIndex ].hwnd ) )
            {
                // have a window, make sure the handle hasn't been recycled
                TCHAR szClass[ MAX_PATH ];
                szClass[ 0 ] = _T( '\0' );
                int iCopiedChars = GetClassName( m_keys[ iIndex ].hwnd, szClass, MAX_PATH - 1 );
                BOOL bSameWnd = TRUE;
                if ( m_keys[ iIndex ].szWndClass )
                {
                    bSameWnd = ( szClass && ( _tcsicmp( m_keys[ iIndex ].szWndClass, szClass ) == 0 ) );
                }
                else
                {
                    bSameWnd = ( iCopiedChars == 0 );
                }

                if ( bSameWnd )
                {
                    //PostKeybdMessage( m_keys[ iIndex ].hwnd, wParam, dwFlags, 1, &dwShiftState, &wParam );
                    ::PostMessage( m_keys[ iIndex ].hwnd, uiMsg, wParam, lParam );
                    return TRUE;
                }
                else
                {
                    // clear the key window info and fall through to run the app
                    m_keys[ iIndex ].hwnd = NULL;
                    SAFE_FREE( m_keys[ iIndex ].szWndClass );
                }
            }

            // check to see if the registry has been updated for this key, have to do this
            // in case the OEM has allowed users to change these keys
            // if the window was bad, but the app path is in the key, launch the app instead
            // REVIEW: do we need to check the path?
            if ( LoadKeyFromRegistry( iIndex ) && m_keys[ iIndex ].szAppName && m_keys[ iIndex ].szAppPath )
            {
                TCHAR szCmdLine[ MAX_PATH ];
                _tcscpy( szCmdLine, m_keys[ iIndex ].szAppPath );

                // add a trailing /?
                if ( _tcslen( _tcsrchr( szCmdLine, _T( '\\' ) ) ) > 1 )
                {
                    _tcscat( szCmdLine, _T( "\\" ) );
                }
                
                // append the app
                _tcscat( szCmdLine, m_keys[ iIndex ].szAppName );

                if ( _tcslen( szCmdLine ) > 0 )
                {
                    // execute
                    SHELLEXECUTEINFO info;
                    info.cbSize = sizeof( info );
                    info.fMask = SEE_MASK_NOCLOSEPROCESS;
                    info.hwnd = NULL;
                    info.lpVerb = NULL;
                    info.lpFile = szCmdLine;
                    info.lpParameters = m_keys[ iIndex ].szAppFlags;
                    info.nShow = SW_SHOW;
                    info.hInstApp = NULL;

                    return ShellExecuteEx( &info ); 
                }
            }
        }
    }

    return FALSE;
}

static Keymap *g_pKeymap = NULL;

// SHELL INTERNAL FUNCTIONS
BOOL Keymap_ProcessKey( UINT uiMsg, WPARAM wParam, LPARAM lParam )
{
    if ( !g_pKeymap )
    {
        g_pKeymap = new Keymap();

        if ( !g_pKeymap )
        {
            return FALSE;
        }
    }

    return g_pKeymap->ForwardKey( uiMsg, wParam, lParam );
}


// API FUNCTIONS
/*------------------------------------------------------------------------------
	SHGetAppKeyAssoc

	private\shellw\gserver\wpcshell\thunks.cpp
    thunks to Keymap_GetAppKeyAssoc
    private\shellw\gserver\shell32g\ppc\keymap.cpp
------------------------------------------------------------------------------*/
extern "C" BYTE SHGetAppKeyAssoc(LPCTSTR ptszApp)
{
    if ( !ptszApp || ( _tcslen( ptszApp ) == 0 ) )
    {
        return 0;
    }

    if ( !g_pKeymap )
    {
        g_pKeymap = new Keymap();

        if ( !g_pKeymap )
        {
            return 0;
        }
    }

    return g_pKeymap->GetKeyForApp( ptszApp );
}

/*------------------------------------------------------------------------------
	SHSetAppKeyWndAssoc
------------------------------------------------------------------------------*/
extern "C" BOOL SHSetAppKeyWndAssoc(BYTE bVk, HWND hwnd)
{
    if ( !g_pKeymap )
    {
        g_pKeymap = new Keymap();

        if ( !g_pKeymap )
        {
            return FALSE;
        }
    }

    return g_pKeymap->AddToMap( bVk, hwnd );
}

