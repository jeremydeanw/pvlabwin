#include <windows.h>
#include <commctrl.h>
#include "maindlg.hpp"
#include "curvedlg.hpp"
#include "filedlg.hpp"

char g_CurDir[ 256 ];

MainDlg g_MainDlg;
CurveDlg g_CurveDlg;
FileDlg g_FileDlg;

/*int STDCALL */
int WINAPI
WinMain (HINSTANCE hInst, HINSTANCE hPrev, LPSTR lpCmd, int nShow)
{
	if( GetCurrentDirectory( 256, g_CurDir ) == 0 )
		MessageBox( NULL, "Error (GetCurrentDirectory)", "Error", MB_OK );

	InitCommonControls();

	//LoadLibrary( "riched32.dll" ); // se si vogliono usare i RichEdit

	if( !g_MainDlg.Show( hInst ) )
		MessageBox( NULL, "Error (Show main dialog)", "Error", MB_OK );
		
	return 0;	
}
