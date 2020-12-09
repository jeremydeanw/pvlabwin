#include <stdio.h>
#include <stdlib.h>
#include "curvedlg.hpp"

CurveDlg* CurveDlg::s_pThis = NULL;

// stringhe coi tipi di curva
char* szCurveTypes[ NUM_CURVE_TYPES ] =
{
	"Linear", "Exponential", "Power", "Condensation", "Dispersion", "User (linear segments)", "User (cosine segments)",
	"Exponential (Reversed)", "Power (Reversed)", "Condensation (Reversed)", "Dispersion (Reversed)", "Sinusoidal"
};

BOOL CALLBACK CurveDlg::DlgProc( HWND  hwndDlg, UINT  uMsg, WPARAM  wParam, LPARAM  lParam )
{
	switch( uMsg )
	{
	case WM_CLOSE:
		EndDialog( hwndDlg, 0 );
		break;
	case WM_INITDIALOG:
		Init( hwndDlg );
		return TRUE;
	case WM_CTLCOLORSTATIC:
		{
			Refresh();
			return 0; // handle della brush da usare per il repaint del controllo
		}
		break;
	case WM_COMMAND:
		{
			WORD wNotifyCode = HIWORD( wParam );
			WORD wID = LOWORD( wParam );

			switch( wID )
			{
			case BUT_LOAD:
				{
					if( ( m_Curve.m_Type != CT_USER ) && ( m_Curve.m_Type != CT_USERCOS ) )
						break; // non dovrebbe succedere, pero'...

					LoadUserCurve();
				}
				break;
			case BUT_SAVE:
				{
					if( ( m_Curve.m_Type != CT_USER ) && ( m_Curve.m_Type != CT_USERCOS ) )
						break; // non dovrebbe succedere, pero'...

					SaveUserCurve();
				}
				break;
			case BUT_TEST:
				// NON deve essere processato alcun messaggio per questo bottone "finto" !!!
				break;
			case BUT_EXIT:
				if( wNotifyCode == BN_CLICKED )
					SendMessage( hwndDlg, WM_CLOSE, 0, 0 );
				break;
			case CMB_CURVETYPE:
				if( wNotifyCode == CBN_SELCHANGE )
					ChangeCurveSel();
				break;
			case EDIT_PAR_0:
			case EDIT_PAR_1:
				if( wNotifyCode == EN_KILLFOCUS )
				{
					// acquisizione handle
					HWND hwndEdit = GetDlgItem( hwndDlg, wID );
					WORD ndx = wID - (WORD)EDIT_PAR_0;

					// acquisizione contenuto dell'edit
					char tmp[ 50 ];
					SendMessage( hwndEdit, WM_GETTEXT, 50, (LPARAM)tmp );

					if( tmp[ 0 ] == '\0' )
						break;

					float value = atof( tmp );

					m_Curve.SetParam( ndx, value );

					// nel caso in cui il parametro sia stato clippato...
					if( m_Curve.GetParam( ndx ) != value )
					{
						sprintf( tmp, "%f", m_Curve.GetParam( ndx ) );
						SendMessage( hwndEdit, WM_SETTEXT, 0, (LPARAM)tmp );
					}

					Refresh();
				}
				break;
			case EDIT_X:
			case EDIT_Y:
				if( wNotifyCode == EN_KILLFOCUS )
				{
					if( m_manualEdit == false )
						break;

					if( ( m_Curve.m_Type != CT_USER ) && ( m_Curve.m_Type != CT_USERCOS ) )
						break; // non dovrebbe succedere, pero'...

					if( m_ptSelected == 0xFFFFFFFF )
						break; // non dovrebbe succedere, pero'...

					// acquisizione handle
					HWND hwndEdit = GetDlgItem( hwndDlg, wID );
					WORD ndx = wID - (WORD)EDIT_X;

					// acquisizione contenuto dell'edit
					char tmp[ 50 ];
					SendMessage( hwndEdit, WM_GETTEXT, 50, (LPARAM)tmp );

					if( tmp[ 0 ] == '\0' )
						break;

					float value = atof( tmp );

					float x = m_Curve.m_x[ m_ptSelected ];
					float y = m_Curve.m_y[ m_ptSelected ];

					if( ndx == 0 )
						x = value;
					else if ( ndx == 1 )
						y = value;

					// clip
					if ( x < 0.0f ) x = 0.0f;
					if ( x > 1.0f ) x = 1.0f;
					if ( y < 0.0f ) y = 0.0f;
					if ( y > 1.0f ) y = 1.0f;
					if ( m_ptSelected == 0 )
						x = 0.0f;
					else if ( m_ptSelected == m_Curve.m_nPoints - 1 )
						x = 1.0f;
					else if ( x < m_Curve.m_x[ m_ptSelected - 1 ] )
						x = m_Curve.m_x[ m_ptSelected - 1 ];
					else if ( x > m_Curve.m_x[ m_ptSelected + 1 ] )
						x = m_Curve.m_x[ m_ptSelected + 1 ];

					m_Curve.SetPoint( m_ptSelected, x, y );

					sprintf( tmp, "%f", x );
					SendMessage( m_hwndEditXY[ 0 ], WM_SETTEXT, 0, (LPARAM)tmp );
					sprintf( tmp, "%f", y );
					SendMessage( m_hwndEditXY[ 1 ], WM_SETTEXT, 0, (LPARAM)tmp );

					Refresh();
				}
				break;
			default:
				break;
			}
		}
		break;
	case WM_RBUTTONDOWN:
		{
			if( ( m_Curve.m_Type != CT_USER ) && ( m_Curve.m_Type != CT_USERCOS ) )
				break;

			POINT pt;
			pt.x = LOWORD( lParam );
			pt.y = HIWORD( lParam );

			if( ( m_ptSelected = SearchPoint( pt ) ) != 0xFFFFFFFF )
			{
				m_Curve.RemovePoint( m_ptSelected );
				Refresh();
			}
		}
		break;
	case WM_LBUTTONUP:
			m_drag = false;
		break;
	case WM_LBUTTONDOWN:
		{
			if( ( m_Curve.m_Type != CT_USER ) && ( m_Curve.m_Type != CT_USERCOS ) )
				break;

			WORD flags = wParam;
			// coordinate mouse (relative alla dialog)
			POINT pt;
			pt.x = LOWORD( lParam );
			pt.y = HIWORD( lParam );

			// se il mouse non e' nell'area utile, esco subito
			if( !PtInRect( &m_rectUsable, pt ) )
				break;

			// se ero in modalita' "manual edit", il click serve ad uscirne
			if( m_manualEdit == true )
			{
				m_manualEdit = false;
				EnableWindow( m_hwndEditXY[ 0 ], FALSE );
				EnableWindow( m_hwndEditXY[ 1 ], FALSE );
				break;
			}

			m_drag = false;

			// ho cliccato abbastanza vicino ad un punto per selezionarlo ?
			if( ( m_ptSelected = SearchPoint( pt ) ) != 0xFFFFFFFF )
			{ // ...SI

				// se premo SHIFT passo in modalita' edit manuale delle coordinate
				if( ( flags & MK_SHIFT ) != 0 )
				{
					m_manualEdit = true;
					EnableWindow( m_hwndEditXY[ 0 ], TRUE );
					EnableWindow( m_hwndEditXY[ 1 ], TRUE );
					char tmp[ 50 ];
					sprintf( tmp, "%f", m_Curve.m_x[ m_ptSelected ] );
					SendMessage( m_hwndEditXY[ 0 ], WM_SETTEXT, 0, (LPARAM)tmp );
					sprintf( tmp, "%f", m_Curve.m_y[ m_ptSelected ] );
					SendMessage( m_hwndEditXY[ 1 ], WM_SETTEXT, 0, (LPARAM)tmp );
				}
				else
					m_drag = true;
			}

			// se non devo trascinare ne' editare, aggiungo il punto alla curva
			if( m_drag == false && m_manualEdit == false )
			{
				LONG dx = pt.x - m_rectUsable.left;
				LONG dy = m_rectUsable.bottom - pt.y;
				LONG w = m_rectUsable.right - m_rectUsable.left;
				LONG h = m_rectUsable.bottom - m_rectUsable.top;
				float x = (float)dx / (float)w;
				float y = (float)dy / (float)h;

				m_Curve.AddPoint( x, y );
				Refresh();
			}
		}
	case WM_MOUSEMOVE:
		{
			// sono in modalita' trascinamento ?
			if( m_drag == false )
				break;

			WORD flags = wParam;
			// coordinate mouse
			POINT pt;
			pt.x = LOWORD( lParam );
			pt.y = HIWORD( lParam );

			LONG dx = pt.x - m_rectUsable.left;
			LONG dy = m_rectUsable.bottom - pt.y;
			LONG w = m_rectUsable.right - m_rectUsable.left;
			LONG h = m_rectUsable.bottom - m_rectUsable.top;
			float x = (float)dx / (float)w;
			float y = (float)dy / (float)h;

			// clip
			if ( x < 0.0f ) x = 0.0f;
			if ( x > 1.0f ) x = 1.0f;
			if ( y < 0.0f ) y = 0.0f;
			if ( y > 1.0f ) y = 1.0f;
			if ( m_ptSelected == 0 )
				x = 0.0f;
			else if ( m_ptSelected == m_Curve.m_nPoints - 1 )
				x = 1.0f;
			else if ( x < m_Curve.m_x[ m_ptSelected - 1 ] )
				x = m_Curve.m_x[ m_ptSelected - 1 ];
			else if ( x > m_Curve.m_x[ m_ptSelected + 1 ] )
				x = m_Curve.m_x[ m_ptSelected + 1 ];

			m_Curve.SetPoint( m_ptSelected, x, y );

			// aggiorno il display delle coordinate (tanto per indicazione...)
			char tmp[ 50 ];
			sprintf( tmp, "%f", x );
			SendMessage( m_hwndEditXY[ 0 ], WM_SETTEXT, 0, (LPARAM)tmp );
			sprintf( tmp, "%f", y );
			SendMessage( m_hwndEditXY[ 1 ], WM_SETTEXT, 0, (LPARAM)tmp );
			UpdateWindow( m_hwndEditXY[ 0 ] );
			UpdateWindow( m_hwndEditXY[ 1 ] );

			Refresh();
		}
		break;
	default:
		break;
	}
	return 0;

}

void CurveDlg::Init( HWND hwndDlg )
{
	// handle della dialog
	m_hwndDlg = hwndDlg;

	// handle dei controlli
	m_hwndCombo = GetDlgItem( m_hwndDlg, CMB_CURVETYPE );
	m_hwndEditP[ 0 ] = GetDlgItem( m_hwndDlg, EDIT_PAR_0 );
	m_hwndEditP[ 1 ] = GetDlgItem( m_hwndDlg, EDIT_PAR_1 );
	m_hwndEditXY[ 0 ] = GetDlgItem( m_hwndDlg, EDIT_X );
	m_hwndEditXY[ 1 ] = GetDlgItem( m_hwndDlg, EDIT_Y );
	m_hwndImg = GetDlgItem( m_hwndDlg, IMG_01 );
	m_hwndLoad = GetDlgItem( m_hwndDlg, BUT_LOAD );
	m_hwndSave = GetDlgItem( m_hwndDlg, BUT_SAVE );

	// inizializzazione curva (NO! viene impostata dall'esterno)
	//m_Curve.Init();

	// acquisisco il rettangolo della finestra immagine...
	GetClientRect( m_hwndImg, &m_rectImg );

	// ... e lo converto in coordinate relative alla dialog
	MapWindowPoints( m_hwndImg, m_hwndDlg, (LPPOINT)&m_rectImg, 2 );

	// calcolo il rettangolo utile
	m_rectUsable.left = m_rectImg.left + USABLE_MARGIN;
	m_rectUsable.right = m_rectImg.right - USABLE_MARGIN;
	m_rectUsable.top = m_rectImg.top + USABLE_MARGIN;
	m_rectUsable.bottom = m_rectImg.bottom - USABLE_MARGIN;

	// creo le penne:normale, spessa (per disegnare i punti nelle curve user), i tick mediani, e i tick intermedi
	m_hpenNorm = CreatePen( PS_SOLID, 1, 0x00000000 );
	m_hpenCurve = CreatePen( PS_SOLID, 1, 0x00FF0000 );
	m_hpenThick = CreatePen( PS_SOLID, 5, 0x00000000 );
	m_hpenAxis = CreatePen( PS_SOLID, 1, 0x0020D000 );
	m_hpenDot = CreatePen( PS_DOT, 1, 0x0020D000 );

	// creo un font per l'etichetta assi
	m_hfontAxis = CreateFont(
			6,						// logical height of font
    		6,						// logical average character width
    		0,						// angle of escapement
    		0,						// base-line orientation angle
    		FW_NORMAL,				// font weight
    		FALSE,					// italic attribute flag
    		FALSE,					// underline attribute flag
    		FALSE,					// strikeout attribute flag
    		DEFAULT_CHARSET,		// character set identifier
    		OUT_DEFAULT_PRECIS,		// output precision
    		CLIP_DEFAULT_PRECIS,	// clipping precision
    		DEFAULT_QUALITY	,		// output quality
    		DEFAULT_PITCH,			// pitch and family
    		"MS Sans Serif" );		// address of typeface name string

	// inizializzazione COMBO-BOX
	SendMessage( m_hwndCombo, CB_RESETCONTENT, 0, 0 );
	long i;
	for( i = 0; i < NUM_CURVE_TYPES; i++ )
	{
		SendMessage( m_hwndCombo, CB_ADDSTRING, 0, (LPARAM)szCurveTypes[ i ] );
	}
	SendMessage( m_hwndCombo, CB_SETCURSEL, (WPARAM)m_Curve.m_Type, 0 );

	// inizializzazione dei controlli in base alla curva selezionata
	ChangeCurveSel();

	// disabilito gli edit delle coordinate x,y
	EnableWindow( m_hwndEditXY[ 0 ], FALSE );
	EnableWindow( m_hwndEditXY[ 1 ], FALSE );

	m_drag = false;
	m_manualEdit = false;
	m_ptSelected = 0xFFFFFFFF;

	// Creo il backbuffer:
	// 1) acquisisco il DC della finestra static
	HDC hdc = GetDC( m_hwndImg );
	// 2) creo un DC in memoria
	m_backbuffer = CreateCompatibleDC( hdc );
	// 3) creo un bitmap compatibile con la finestra static e con le dimensioni desiderate
	m_backbmp = CreateCompatibleBitmap( hdc, m_rectImg.right - m_rectImg.left, m_rectImg.bottom - m_rectImg.top );
	// 4) rilascio il DC della finestra static
	ReleaseDC( m_hwndImg, hdc );
	// 5) seleziono il bitmap nel DC ( in modo che venga allocata sufficiente memoria )
	SelectObject( m_backbuffer, (HGDIOBJ)m_backbmp );
}

void CurveDlg::Refresh( void )
{
	// **************************
	// Disegno curva
	// **************************

	// acquisizione rettangolo immagine (coord. NON relative alla dialog!)
	RECT bbox;
	GetClientRect( m_hwndImg, &bbox );

	// acquisizione DC immagine
	HRGN hrgn = CreateRectRgn( bbox.left, bbox.top, bbox.right, bbox.bottom );
	HDC hdc = GetDCEx( m_hwndImg, hrgn, DCX_CACHE );
	/* anziche' GetDC( hwndImg ) - cosi' il clipping sembra funzionare! */

	if( hdc == NULL )
		return;

	unsigned long width = bbox.right - bbox.left;
	unsigned long height = bbox.bottom - bbox.top;

	RECT usable;
	usable.left = bbox.left + USABLE_MARGIN;
	usable.right = bbox.right - USABLE_MARGIN;
	usable.top = bbox.top + USABLE_MARGIN;
	usable.bottom = bbox.bottom - USABLE_MARGIN;

	unsigned long Uwidth = usable.right - usable.left;
	unsigned long Uheight = usable.bottom - usable.top;

	// pulizia sfondo
	BitBlt( m_backbuffer, 0, 0, width, height, NULL, 0, 0, WHITENESS );

	// etichette assi
	HFONT hfontPrev = (HFONT)SelectObject( m_backbuffer, m_hfontAxis );
	UINT alignPrev = GetTextAlign( m_backbuffer );
	SetTextAlign( m_backbuffer, TA_LEFT | TA_TOP );
	POINT pt;
	pt.x = usable.right - 50;
	pt.y = usable.bottom + 3;
	TextOut( m_backbuffer, pt.x, pt.y, m_LabelX, strlen( m_LabelX ) );
	pt.x = usable.left + 3;
	pt.y = usable.top - 14;
	TextOut( m_backbuffer, pt.x, pt.y, m_LabelY, strlen( m_LabelY ) );
	SetTextAlign( m_backbuffer, alignPrev ); // ripristino allineamento precedente
	SelectObject( m_backbuffer, hfontPrev ); // ripristino font precedente

	// disegno assi
	HPEN hpenPrev = (HPEN)SelectObject( m_backbuffer, m_hpenNorm );

	MoveToEx( m_backbuffer, bbox.left, usable.bottom, NULL );
	LineTo( m_backbuffer, bbox.right, usable.bottom );
	MoveToEx( m_backbuffer, usable.left, bbox.bottom, NULL );
	LineTo( m_backbuffer, usable.left, bbox.top );

	// disegno ticks mediani
	SelectObject( m_backbuffer, m_hpenAxis );
	MoveToEx( m_backbuffer, usable.left, ( usable.bottom + usable.top ) / 2, NULL );
	LineTo( m_backbuffer, usable.right, ( usable.bottom + usable.top ) / 2 );

	MoveToEx( m_backbuffer, ( usable.left + usable.right ) / 2, usable.bottom, NULL );
	LineTo( m_backbuffer, ( usable.left + usable.right ) / 2, usable.top );

	// disegno ticks orizzontali
	unsigned long i;
	SelectObject( m_backbuffer, m_hpenDot );
	for( i = 0; i < 10; i++ )
	{
		if( i == 5 )
			continue;

		int y = usable.top + i * ( usable.bottom - usable.top ) / 10;
		MoveToEx( m_backbuffer, usable.left, y, NULL );
		LineTo( m_backbuffer, usable.right, y );
	}
	// disegno ticks verticali
	for( i = 0; i < 10; i++ )
	{
		if( i == 4 )
			continue;

		int x = usable.left + ( i + 1 ) * ( usable.right - usable.left ) / 10;
		MoveToEx( m_backbuffer, x, usable.bottom, NULL );
		LineTo( m_backbuffer, x, usable.top );
	}

	SelectObject( m_backbuffer, m_hpenCurve );

	if( ( m_Curve.m_Type == CT_USER ) || ( m_Curve.m_Type == CT_USERCOS ) )
	{
		//hpenPrev = (HPEN)SelectObject( m_backbuffer, m_hpenThick );

		float xpr, ypr;

		// disegno i punti della curva USER con una penna leggermente piu' spessa
		for( i = 0; i < m_Curve.m_nPoints; i++ )
		{
			float x = m_Curve.m_x[ i ] * (float)Uwidth + usable.left;
			float y = ( 1.0f - m_Curve.m_y[ i ] ) * (float)Uheight + usable.top;

			SelectObject( m_backbuffer, m_hpenCurve );

			if( ( i > 0 ) && ( m_Curve.m_Type == CT_USER ) )
			{
				MoveToEx( m_backbuffer, (int)xpr, (int)ypr, NULL );
				LineTo( m_backbuffer, (int)x, (int)y );
			}

			xpr = x;
			ypr = y;

			SelectObject( m_backbuffer, m_hpenThick );

			MoveToEx( m_backbuffer, (int)x, (int)y, NULL );
			LineTo( m_backbuffer, (int)x, (int)y );

		}


	}

	if( ( m_Curve.m_Type != CT_USER ) )
	{
		SelectObject( m_backbuffer, m_hpenCurve ); // penna per disegno curva

		for( i = 0; i <= Uwidth; i++ )
		{
			float x = (float)i / (float)Uwidth;
			float y = ( 1.0f - m_Curve.NormValue( x ) ) * (float)Uheight + usable.top;

			// disegno a singolo pixel
			//SetPixel( hdc, i, (int)y, 0x00000000 );

			// disegno a pixel uniti
			if( i == 0 )
				MoveToEx( m_backbuffer, i + usable.left, (int)y, NULL );

			LineTo( m_backbuffer, i + usable.left, (int)y );
		}
	}

	SelectObject( m_backbuffer, hpenPrev ); // ripristino penna precedente all'inizio del disegno

	// copia nel front buffer
	BitBlt( hdc, 0, 0, width, height, m_backbuffer, 0, 0, SRCCOPY );

	// rilascio DC immagine
	ReleaseDC( m_hwndImg, hdc );
}

void CurveDlg::ChangeCurveSel( void )
{
	// selezione corrente
	LONG sel = (LONG)SendMessage( m_hwndCombo, CB_GETCURSEL, 0, 0 );
	if( sel != CB_ERR )
	{
		LONG NewType = (LONG)CT_LINEAR + sel;
		// cambia tipo di curva
		m_Curve.ChangeType( (CurveType)NewType );

		// aggiorna parametri
		if( ( NewType != CT_USER ) && ( NewType != CT_USERCOS ) )
		{
			char tmp[ 50 ];

			// abilitazione e aggiornamento dell'edit relativo al primo parametro
			EnableWindow( m_hwndEditP[ 0 ], TRUE );
			sprintf( tmp, "%f", m_Curve.GetParam( 0 ) );
			SendMessage( m_hwndEditP[ 0 ], WM_SETTEXT, 0, (LPARAM)tmp );

			if( NewType == CT_LINEAR || NewType == CT_CONDENS || NewType == CT_DISPERS
				|| NewType == CT_CONDREV || NewType == CT_DISPREV || NewType == CT_SINUS )
			{ // abilitazione e aggiornamento dell'edit relativo al secondo parametro
				EnableWindow( m_hwndEditP[ 1 ], TRUE );
				sprintf( tmp, "%f", m_Curve.GetParam( 1 ) );
				SendMessage( m_hwndEditP[ 1 ], WM_SETTEXT, 0, (LPARAM)tmp );
			}
			else
			{ // disabilitazione dell'edit relativo al secondo parametro
				EnableWindow( m_hwndEditP[ 1 ], FALSE );
			}

			// disabiitazione degli edit relativi alle coordinate x,y
			m_manualEdit = false;
			EnableWindow( m_hwndEditXY[ 0 ], FALSE );
			EnableWindow( m_hwndEditXY[ 1 ], FALSE );

			// disabilitazione dei bottoni Load, Save
			EnableWindow( m_hwndLoad, FALSE );
			EnableWindow( m_hwndSave, FALSE );
		}
		else
		{
			// disabilitazione degli edit relativi ai parametri
			EnableWindow( m_hwndEditP[ 0 ], FALSE );
			EnableWindow( m_hwndEditP[ 1 ], FALSE );

			// abilitazione dei bottoni Load, Save
			EnableWindow( m_hwndLoad, TRUE );
			EnableWindow( m_hwndSave, TRUE );
		}
		// rinfresca l'immagine
		Refresh();
	}
}

/********************************************************
 * FUNZIONI DI CARICAMENTO/SALVATAGGIO DELLA CURVA USER *
 ********************************************************/

// FORMATO DEL FILE (di testo):
// <numero punti>
// per ogni punto:
//     <x,y>
void CurveDlg::LoadUserCurve( void )
{
	char filter[ 24 ] = "User Curve Files|*.crv|";
	filter[ 16 ] = 0;
	filter[ 22 ] = 0;
	filter[ 23 ] = 0;

	// apre una finestra di tipo "apri file"
	OPENFILENAME of;
	of.lStructSize = sizeof( OPENFILENAME );
    of.hwndOwner = m_hwndDlg;//NULL;
	of.hInstance = NULL;
    of.lpstrFilter = filter;
    of.lpstrCustomFilter = NULL;
    of.nMaxCustFilter = 0;
    of.nFilterIndex = 0;
    of.lpstrFile = m_filename;
    of.nMaxFile = 256;
    of.lpstrFileTitle = NULL;
    of.nMaxFileTitle = 0;
    of.lpstrInitialDir = NULL;
    of.lpstrTitle = NULL;
    of.Flags = OFN_FILEMUSTEXIST;
    of.nFileOffset = 0;
    of.nFileExtension = 0;
    of.lpstrDefExt = NULL;
    of.lCustData = 0;
    of.lpfnHook = NULL;
    of.lpTemplateName = NULL;

	if( GetOpenFileName( &of ) == FALSE )
		return;

	if( strcmp( m_filename, "" ) == 0 )
		return;

	FILE* fp = NULL;
	fp = fopen( m_filename, "r+" );
	if( fp == NULL )
		MessageBox( NULL, "Errore", "Errore", MB_OK );

	fscanf( fp, "%d", &m_Curve.m_nPoints );

	if( m_Curve.m_nPoints == 0 || m_Curve.m_nPoints > MAX_CURVE_PTS )
		MessageBox( NULL, "Errore", "Errore", MB_OK );

	unsigned long i;
	for( i = 0; i < m_Curve.m_nPoints; i++ )
	{
		fscanf( fp, "%f", &m_Curve.m_x[ i ] );

		if( m_Curve.m_x[ i ] < 0.0f || m_Curve.m_x[ i ] > 1.0f )
			MessageBox( NULL, "Errore", "Errore", MB_OK );

		fscanf( fp, "%f", &m_Curve.m_y[ i ] );

		if( m_Curve.m_y[ i ] < 0.0f || m_Curve.m_y[ i ] > 1.0f )
			MessageBox( NULL, "Errore", "Errore", MB_OK );
	}

	fclose( fp );
}

void CurveDlg::SaveUserCurve( void )
{
	char filter[ 24 ] = "User Curve Files|*.crv|";
	filter[ 16 ] = 0;
	filter[ 22 ] = 0;
	filter[ 23 ] = 0;

	// apre una finestra di tipo "salva file"
	OPENFILENAME of;
	of.lStructSize = sizeof( OPENFILENAME );
    of.hwndOwner = m_hwndDlg;
	of.hInstance = NULL;
    of.lpstrFilter = filter;
    of.lpstrCustomFilter = NULL;
    of.nMaxCustFilter = 0;
    of.nFilterIndex = 0;
    of.lpstrFile = m_filename;
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

	if( GetSaveFileName( &of ) == FALSE )
		return;

	if( strcmp( m_filename, "" ) == 0 )
		return;

	FILE* fp = NULL;
	fp = fopen( m_filename, "w" );
	if( fp == NULL )
		MessageBox( NULL, "Errore", "Errore", MB_OK );

	fprintf( fp, "%d\n", m_Curve.m_nPoints );

	unsigned long i;
	for( i = 0; i < m_Curve.m_nPoints; i++ )
	{
		fprintf( fp, "%f\t\t", m_Curve.m_x[ i ] );
		fprintf( fp, "%f\n", m_Curve.m_y[ i ] );
	}

	fclose( fp );
}
