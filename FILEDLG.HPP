#ifndef _FILEDLG_HPP_
#define _FILEDLG_HPP_

#ifndef _WINDOWS_H_INC_
#include <windows.h>
#define _WINDOWS_H_INC_
#endif // _WINDOWS_H_INC_

#ifndef _PVWIN_HPP_INC_
#include "pvwin.hpp"
#define _PVWIN_HPP_INC_
#endif // _PVWIN_HPP_INC_


class FileDlg {

public:

	FileDlg( void )
	{
		m_fileIn1[ 0 ] = 0;
		m_fileIn2[ 0 ] = 0;
		m_fileOutPv[ 0 ] = 0;
		m_fileOut[ 0 ] = 0;
	}

	~FileDlg( void )
	{}

	bool Show( HINSTANCE hInst, HWND hwndOwner )
	{
		s_pThis = this;
		return( DialogBox( hInst, MAKEINTRESOURCE(DLG_FILE), hwndOwner, FileDlg::DlgProcS ) != -1 );
	}

	// nomi dei file
	char m_fileIn1[ 256 ];      // file di input (wave o analisi - estensioni .wav o .fft)
	char m_fileIn2[ 256 ];		// per morph ???
	char m_fileOutPv[ 256 ]; 	// file di analisi da salvare (eventualmente)
	char m_fileOut[ 256 ];		// file di output (wave)
	bool m_SavePv;

protected:

	static BOOL CALLBACK DlgProcS( HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam )
	{
		if( s_pThis != NULL )
			return s_pThis->DlgProc( hwndDlg, uMsg, wParam, lParam );
		else
			return 0;
	}

	BOOL CALLBACK DlgProc( HWND  hwndDlg, UINT  uMsg, WPARAM  wParam, LPARAM  lParam );

	void Init( HWND hwndDlg );

	void FileManage( long Index );

///////////////////////////////////////////////////////

	static FileDlg* s_pThis;	        // "trucco" per la DialogProc

	HWND m_hwndDlg;
	HWND m_hwndEdit[ 4 ];
	HWND m_hwndBut[ 4 ];
};

#endif // _FILEDLG_HPP_
