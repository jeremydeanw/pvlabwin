#ifndef _CURVEDLG_HPP_
#define _CURVEDLG_HPP_

#ifndef _WINDOWS_H_INC_
#include <windows.h>
#define _WINDOWS_H_INC_
#endif // _WINDOWS_H_INC_

#ifndef _PVWIN_HPP_INC_
#include "pvwin.hpp"
#define _PVWIN_HPP_INC_
#endif // _PVWIN_HPP_INC_

#ifndef _CURVE_HPP_INC_
#include "curve.hpp"
#define _CURVE_HPP_INC_
#endif // _CURVE_HPP_INC_

#define USABLE_MARGIN 15

class CurveDlg {

public:

	CurveDlg( void )
	{
		m_drag = false;
		m_manualEdit = false;
		m_ptSelected = 0xFFFFFFFF;

		m_hwndDlg = NULL;
		m_hwndCombo = NULL;
		m_hwndImg = NULL;
		m_hwndLoad = NULL;
		m_hwndSave = NULL;
		m_hwndEditP[ 0 ] = NULL;
		m_hwndEditP[ 1 ] = NULL;
		m_hwndEditXY[ 0 ] = NULL;
		m_hwndEditXY[ 1 ] = NULL;

		m_rectImg.left = 0;
		m_rectImg.top = 0;
		m_rectImg.right = 0;
		m_rectImg.bottom = 0;

		m_rectUsable.left = 0;
		m_rectUsable.top = 0;
		m_rectUsable.right = 0;
		m_rectUsable.bottom = 0;

		m_backbuffer = NULL;
		m_backbmp = NULL;

		m_hpenNorm = NULL;
		m_hpenCurve = NULL;
		m_hpenThick = NULL;
		m_hpenAxis = NULL;
		m_hpenDot = NULL;
		m_hfontAxis = NULL;

		strcpy( m_filename, "*.crv" );
		strcpy( m_LabelX, "xxxx." );
		strcpy( m_LabelY, "yyyy." );
	}

	~CurveDlg( void )
	{}


	bool Show( HINSTANCE hInst, HWND hwndOwner )
	{
		s_pThis = this;
		return( DialogBox( hInst, MAKEINTRESOURCE(DLG_CURVE), hwndOwner, CurveDlg::DlgProcS ) != -1 );
	}

	// curva impostata
	Curve m_Curve;

	// etichette assi x,y
	char m_LabelX[ 50 ];
	char m_LabelY[ 50 ];

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

	void Refresh( void );

	void ChangeCurveSel( void );

	POINT Norm2Dlg( float x, float y )
	{
		LONG width = m_rectUsable.right - m_rectUsable.left;
		LONG height = m_rectUsable.bottom - m_rectUsable.top;

		POINT ret;

		ret.x = (LONG)( x * (float)width ) + m_rectUsable.left;
		ret.y = (LONG)( -y * (float)height ) + m_rectUsable.bottom;

		return ret;
	}

	unsigned long SearchPoint( POINT pt )
	{
		if( ( m_Curve.m_Type != CT_USER ) && ( m_Curve.m_Type != CT_USERCOS ) )
			return 0xFFFFFFFF;

		long i;
		for( i = 0; i < m_Curve.m_nPoints; i++ )
		{
			POINT userpt = Norm2Dlg( m_Curve.m_x[ i ], m_Curve.m_y[ i ] );
			LONG dx = pt.x - userpt.x;
			LONG dy = pt.y - userpt.y;
			if( ( dx * dx ) < 16 && ( dy * dy ) < 16 )
				return i;
		}
		return 0xFFFFFFFF;
	}

	void LoadUserCurve( void );
	void SaveUserCurve( void );
///////////////////////////////////////////////////////

	static CurveDlg* s_pThis;	        // "trucco" per la DialogProc

	HWND m_hwndDlg;
	HWND m_hwndCombo;		// combo-box per selezionare il tipo di curva
	HWND m_hwndEditP[ 2 ];	// edit per i parametri
	HWND m_hwndEditXY[ 2 ];	// edit per le coordinate x,y
	HWND m_hwndImg;			// rettangolo per disegnare
	HWND m_hwndLoad;		// bottone Load
	HWND m_hwndSave;		// bottone Save

	RECT m_rectImg;			// rettangolo immagine (in coordinate dialog !!!)
	RECT m_rectUsable;		// rettangolo immagine meno un certo margine

	HDC m_backbuffer;
	HBITMAP m_backbmp;

	bool m_drag;			// true=modalita' drag, false=modalita' add
	bool m_manualEdit;		// true=edit manuale dei punti per curve USER
	unsigned long m_ptSelected;
	char m_filename[ 256 ];

	HPEN m_hpenNorm;
	HPEN m_hpenCurve;
	HPEN m_hpenThick;
	HPEN m_hpenAxis;
	HPEN m_hpenDot;
	HFONT m_hfontAxis;
};

#endif // _CURVEDLG_HPP_
