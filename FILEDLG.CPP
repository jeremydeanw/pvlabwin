#include <stdio.h>
#include <stdlib.h>
#include "filedlg.hpp"

FileDlg* FileDlg::s_pThis = NULL;


BOOL CALLBACK FileDlg::DlgProc( HWND  hwndDlg, UINT  uMsg, WPARAM  wParam, LPARAM  lParam )
{
	switch( uMsg )
	{
	case WM_CLOSE:
		EndDialog( hwndDlg, 0 );
		break;
	case WM_INITDIALOG:
		Init( hwndDlg );
		return TRUE;
	case WM_COMMAND:
		{
			WORD wNotifyCode = HIWORD( wParam );
			WORD wID = LOWORD( wParam );

			switch( wID )
			{
			case FILEDLG_CHK1:
				{
					// disabilito/abilito l'edit del file di analisi a seconda dello stato
					// della checkbox
					HWND hwndChk = GetDlgItem( hwndDlg, FILEDLG_CHK1 );
					m_SavePv = ( SendMessage( hwndChk, BM_GETCHECK, 0, 0 ) == BST_CHECKED );

					if( m_SavePv )
					{
						EnableWindow( m_hwndEdit[ 2 ], TRUE );
						EnableWindow( m_hwndBut[ 2 ], TRUE );
					}
					else
					{
						EnableWindow( m_hwndEdit[ 2 ], FALSE );
						EnableWindow( m_hwndBut[ 2 ], FALSE );
					}
				}
				break;
			case FILEDLG_BUT1:
			case FILEDLG_BUT2:
			case FILEDLG_BUT3:
			case FILEDLG_BUT4:
				FileManage( wID - FILEDLG_BUT1 );
				break;
			case FILEDLG_EXIT:
				if( wNotifyCode == BN_CLICKED )
					SendMessage( hwndDlg, WM_CLOSE, 0, 0 );
				break;
			////////////////////////////////////////////////
			// AGGIORNAMENTO NOMI DEI FILE
			////////////////////////////////////////////////
			case FILEDLG_EDIT1:
				if( wNotifyCode == EN_KILLFOCUS )
				{
					SendMessage( m_hwndEdit[ 0 ], WM_GETTEXT, 256, (LPARAM)m_fileIn1 );
				}
				break;
			case FILEDLG_EDIT2:
				if( wNotifyCode == EN_KILLFOCUS )
				{
					SendMessage( m_hwndEdit[ 1 ], WM_GETTEXT, 256, (LPARAM)m_fileIn2 );
				}
				break;
			case FILEDLG_EDIT3:
				if( wNotifyCode == EN_KILLFOCUS )
				{
					SendMessage( m_hwndEdit[ 2 ], WM_GETTEXT, 256, (LPARAM)m_fileOutPv );
				}
				break;
			case FILEDLG_EDIT4:
				if( wNotifyCode == EN_KILLFOCUS )
				{
					SendMessage( m_hwndEdit[ 3 ], WM_GETTEXT, 256, (LPARAM)m_fileOut );
				}
				break;
			default:
				break;
			}
		}
		break;
	default:
		break;
	}
	return 0;
}

void FileDlg::Init( HWND hwndDlg )
{
	// handle della dialog
	m_hwndDlg = hwndDlg;

	// handle degli edit
	m_hwndEdit[ 0 ] = GetDlgItem( hwndDlg, FILEDLG_EDIT1 );
	m_hwndEdit[ 1 ] = GetDlgItem( hwndDlg, FILEDLG_EDIT2 );
	m_hwndEdit[ 2 ] = GetDlgItem( hwndDlg, FILEDLG_EDIT3 );
	m_hwndEdit[ 3 ] = GetDlgItem( hwndDlg, FILEDLG_EDIT4 );

	// handle dei bottoni
	m_hwndBut[ 0 ] = GetDlgItem( hwndDlg, FILEDLG_BUT1 );
	m_hwndBut[ 1 ] = GetDlgItem( hwndDlg, FILEDLG_BUT2 );
	m_hwndBut[ 2 ] = GetDlgItem( hwndDlg, FILEDLG_BUT3 );
	m_hwndBut[ 3 ] = GetDlgItem( hwndDlg, FILEDLG_BUT4 );

	// aggiorna gli edit
	SendMessage( m_hwndEdit[ 0 ], WM_SETTEXT, 0, (LPARAM)m_fileIn1 );
	SendMessage( m_hwndEdit[ 1 ], WM_SETTEXT, 0, (LPARAM)m_fileIn2 );
	SendMessage( m_hwndEdit[ 2 ], WM_SETTEXT, 0, (LPARAM)m_fileOutPv );
	SendMessage( m_hwndEdit[ 3 ], WM_SETTEXT, 0, (LPARAM)m_fileOut );

	// disabilita edit e bottone del file di analisi da salvare
	EnableWindow( m_hwndEdit[ 2 ], FALSE );
	EnableWindow( m_hwndBut[ 2 ], FALSE );

	m_SavePv = false;

	// centratura dialog
	int screenw = GetSystemMetrics( SM_CXSCREEN );
	int screenh = GetSystemMetrics( SM_CYSCREEN );
	RECT rect;
	GetWindowRect( m_hwndDlg, &rect );
	SetWindowPos( m_hwndDlg, HWND_TOP,
		( screenw - rect.right + rect.left ) / 2,
		( screenh - rect.bottom + rect.top ) / 2,
		0, 0, SWP_NOSIZE | SWP_NOZORDER );
}

void FileDlg::FileManage( long Index )
{
	char filter[ 50 ];

	// struttura dati per la dialog standard apri/salva file
	OPENFILENAME of;

	of.lStructSize = sizeof( OPENFILENAME );
    of.hwndOwner = m_hwndDlg;
	of.hInstance = NULL;
    of.lpstrFilter = filter;
    of.lpstrCustomFilter = NULL;
    of.nMaxCustFilter = 0;
    of.nFilterIndex = 0;
    of.nMaxFile = 256;
    of.lpstrFileTitle = NULL;
    of.nMaxFileTitle = 0;
    of.lpstrInitialDir = NULL;
    of.lpstrTitle = NULL;
    of.Flags = 0;
    of.nFileOffset = 0;
    of.nFileExtension = 0;
    of.lpstrDefExt = NULL;
    of.lCustData = 0;
    of.lpfnHook = NULL;
    of.lpTemplateName = NULL;

	switch( Index )
	{
	case 0:
		memcpy( filter, "Wave/Analysis Files\0*.wav;*.fft\0\0", 33 );
	    of.lpstrFile = m_fileIn1;
		break;
	case 1:
		memcpy( filter, "Wave/Analysis Files\0*.wav;*.fft\0\0", 33 );
		of.lpstrFile = m_fileIn2;
		break;
	case 2:
		memcpy( filter, "Analysis Files\0*.fft\0\0", 22 );
		of.lpstrFile = m_fileOutPv;
		break;
	case 3:
		memcpy( filter, "Wave Files\0*.wav\0\0", 18 );
		of.lpstrFile = m_fileOut;
		break;
	}

	if( Index < 2 )
	{
		// file di input
		of.Flags |= OFN_FILEMUSTEXIST;
		if( GetOpenFileName( &of ) == FALSE )
			return;
	}
	else
	{
		// file di output
		of.Flags |= OFN_OVERWRITEPROMPT;
		if( GetSaveFileName( &of ) == FALSE )
			return;
	}

	switch( Index )
	{
	case 0:
		SendMessage( m_hwndEdit[ 0 ], WM_SETTEXT, 0, (LPARAM)m_fileIn1 );
		break;
	case 1:
		SendMessage( m_hwndEdit[ 1 ], WM_SETTEXT, 0, (LPARAM)m_fileIn2 );
		break;
	case 2:
		SendMessage( m_hwndEdit[ 2 ], WM_SETTEXT, 0, (LPARAM)m_fileOutPv );
		break;
	case 3:
		SendMessage( m_hwndEdit[ 3 ], WM_SETTEXT, 0, (LPARAM)m_fileOut );
		break;
	}

}
