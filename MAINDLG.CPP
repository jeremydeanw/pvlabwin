#include <stdio.h>
#include <stdlib.h>
#include "pv.hpp"
#include "curvedlg.hpp"
#include "maindlg.hpp"
#include "filedlg.hpp"
#include <commctrl.h>

// macro di utilita'
#define ErrMsg(a)	MessageBox(NULL,(a),"Error",MB_OK)

extern CurveDlg g_CurveDlg;
extern FileDlg g_FileDlg;
extern char g_CurDir[ 256 ];

MainDlg* MainDlg::s_pThis = NULL;

#define NUM_FREQ_XFORM 3
char* szFreqXform[ NUM_FREQ_XFORM ] = {
	"Remapping", "Quantize", "Random Add" };

#define NUM_AMPL_XFORM	4
char* szAmplXform[ NUM_AMPL_XFORM ] = {
	"Transfer function", "Remapping", "Threshold", "Inverse threshold" };

#define NUM_MISC_XFORM	5
char* szMiscXform[ NUM_MISC_XFORM ] = {
	"Delay vs. Frequency", "Stereo-ization", "Morphing (Interpolation)", "Morphing (Cross-synthesis)", "Morphing (Density)" };

#define NUM_FFT_PTS 9
char* szFftPts[ NUM_FFT_PTS ] = {
	"32", "64", "128", "256", "512", "1024", "2048", "4096", "8192" };

#define NUM_WIN_TYPES 4
char* szWinTypes[ NUM_WIN_TYPES ] = {
	"Bartlett", "Hamming", "Hanning", "Blackman" };

#define NUM_FRAME_INCR 6
char* szFrameIncr[ NUM_FRAME_INCR ] = {
	"16", "32", "64", "128", "256", "512" };


// ampiezze massime
float gMaxAmpl;
float gMaxAmpl2;
// RMS massimi
float gMaxRMS;
float gMaxRMS2;

#define MAX_FFT_PTS 8192
float gFftSinTable[ MAX_FFT_PTS ];
float gFftCosTable[ MAX_FFT_PTS ];

//////////////////////////////////
// Funzioni globali di utilita' //
//////////////////////////////////

float QuantizeFreq( float f, float r, float b )
{
	if( r == 1.0f )
	{ // quantizzazione lineare (multipli interi di b)
		float sign = ( ( f < 0.0f ) ? -1.0f : +1.0f );
		float n = fabs( f ) / b;
		n = floor( n + 0.5f );
		return sign * b * n;
	}
	else
	{ // quantizzazione esponenziale (b * r^n)
		// precauzione per evitare overflow nei calcoli
		if( ( fabs( r ) >= 1.0f ) && ( fabs( r ) <= 1.000001f ) ) r = 1.000001f;
		if( ( fabs( r ) < 1.0f ) && ( fabs( r ) >= 0.999999f ) ) r = 0.999999f;

		// N.B.: meglio eseguire i calcoli in double visto che si usa la funzione pow
		double sign = ( ( f < 0.0f ) ? -1.0f : +1.0f );
		double n = log( fabs( f ) / b ) / log( r );
		n = floor( n + 0.5f );
		return (float)( sign * b * pow( r, n ) );
	}
}

void ComputeFft( unsigned long l2p, float* pIn, float* pReal, float* pImg )
{
	long pts = (1L << l2p);
	long i, j, k, l, m, s;
	// copia dati
	for( i = 0; i < pts; i++ )
	{
		pReal[ i ] = pIn[ i ];// / (float)sqrt( (float)pts );
		pImg[ i ] = 0.0f;
	}

	// riordinamento dati "a bit invertiti"
	l = 0;
	for( i = 0; i < pts - 1; i++ )
	{
		if( i < l )
		{
			float tmp = pReal[ l ];
			pReal[ l ] = pReal[ i ];
			pReal[ i ] = tmp;
		}
		k = pts / 2;
		while( k <= l )
		{
			l -= k;
			k /= 2;
		}

		l += k;
	}

	// calcolo FFT
	for( s = 0; s < l2p; s++ )
	{
		long d = 1L << s;
		//float sp = PIGRECO / (float)d;
		long sp = MAX_FFT_PTS / ( d*2 );
		for( i = 0; i < pts; i += d*2 )
		{
			//float ph = 0.0;
			long ph = 0;
			for( m = i; m < i+d; m++ )
			{
				long z = m + d;
				//float rw = cos( ph );
				//float jw = -sin( ph );
				float rw = gFftCosTable[ ph ];
				float jw = -gFftSinTable[ ph ];
				float ra = pReal[ m ];
				float ja = pImg[ m ];
				float rb = pReal[ z ];
				float jb = pImg[ z ];

				float rt = rb * rw - jb * jw;
				float jt = rb * jw + jb * rw;
				pReal[ m ] = ra + rt;
				pImg[ m ] = ja + jt;
				pReal[ z ] = ra - rt;
				pImg[ z ] = ja - jt;
				ph += sp;
			}
		}
	}
}

// finestre
// N.B.: RMS Bartlett = 0.577, RMS Hamming = 0.630, RMS Hanning = 0.612, RMS Blackman = 0.552
float ComputeWindow( long nPts, long i, long wtype )
{
	switch( wtype )
	{
	case WTYPE_BARTLETT:
		return ( 1.0f - (float)abs( i - (nPts-1)/2 ) / (float)((nPts-1)/2) );
	case WTYPE_HAMMING:
		return ( 0.54f - 0.46f * (float)cos( 2.0f * PIGRECO * (float)i / (float)(nPts-1) ) );
	case WTYPE_HANNING:
		return ( 0.5f - 0.5f * (float)cos( 2.0f * PIGRECO * (float)i / (float)(nPts-1) ) );
	case WTYPE_BLACKMAN:
		{
			float ph = 2.0f * PIGRECO * (float)i / (float)(nPts-1);
			return ( 0.42f - 0.5f * (float)cos( ph ) + 0.08f * cos( 2.0f * ph ) );
		}
	default:
		return 0.0f;
	}
}

void ComputePV( long nPts, float* real, float* img, PVBin* pvout, double* oldPh,
	float samplingRate, long frameIncr )
{
	long i,j,n;

	double expDph = 0.0;
	double DphIncr = 2.0 * PIGRECO * (double)frameIncr / (double)nPts;

	for( i = 0; i < (nPts/2 + 1); i++ )
	{
		double modulo = sqrt( (double)real[ i ] * (double)real[ i ] + (double)img[ i ] * (double)img[ i ] );
		double phase = atan2( (double)img[ i ], (double)real[ i ] );
		if ( modulo == 0.0f ) phase = 0.0f;

		pvout[ i ].amplitude = modulo;

		// differenza di fase col frame precedente
		double dPhase = phase - oldPh[ i ];

		// ???
		//MMmaskPhs(dPhase,n,PIGRECO,(1.0f/PIGRECO))
		while( dPhase > PIGRECO )
			dPhase -= 2.0 * PIGRECO;
		while( dPhase < -PIGRECO )
			dPhase += 2.0 * PIGRECO;

		// salvo la fase del frame attuale
		oldPh[ i ] = phase;

		// frequenza del bin
		double binFreq = (double)i * (double)samplingRate / (double)nPts;

		// differenza tra le differenze di fase
		dPhase = dPhase - expDph;

		// ???
		//MMmaskPhs(dPhase,n,PIGRECO,(1.0f/PIGRECO))
		while( dPhase > PIGRECO )
			dPhase -= 2.0 * PIGRECO;
		while( dPhase < -PIGRECO )
			dPhase += 2.0 * PIGRECO;

		// calcolo della frequenza
		pvout[ i ].frequency = (float)( binFreq + dPhase * (double)samplingRate / ( 2.0 * PIGRECO * (double)frameIncr ) );

		// aggiorna diff. di fase aspettata
		expDph += DphIncr;
		//expDph -= 2.0 * PIGRECO * (double)( (int)(expDph/PIGRECO) );
		while( expDph > PIGRECO )
			expDph -= 2.0 * PIGRECO;
		while( expDph < -PIGRECO )
			expDph += 2.0 * PIGRECO;
	}
}

void FindMaxAmpl( char* infile, float* pMaxAmpl, float* pMaxRMS )
{
	char msg[ 256 ];
	FILE* fpi = NULL;

	fpi = fopen( infile, "rb" );

	if( fpi == NULL )
	{
		sprintf( msg, "Can't open file: %s", infile );
		ErrMsg( msg );
	}

	// lettura header
	PVHeader pvhdr;
	fseek( fpi, 0, SEEK_SET );
	fread( &pvhdr, sizeof( PVHeader ), 1, fpi );

	// ricavo il numero totale di frames in ingresso ed in uscita
	unsigned long InFrames = pvhdr.dataBsize / pvhdr.frameBsize;
	// ricavo il numero di bins per frame
	unsigned long NBins = pvhdr.frameSize / 2 + 1;

	// allocazione buffer d'ingresso
	PVBin* pvin = NULL;
	pvin = new PVBin[ NBins ];
	if( pvin == NULL )
	{
		ErrMsg( "Memory allocation error!" );
	}

	unsigned long i, j;
	*pMaxAmpl = 0.0f;
	*pMaxRMS = 0.0f;

	for( i = 0; i < InFrames; i++ )
	{
		// leggo il frame d'ingresso
		fread( pvin, sizeof(PVBin) * NBins, 1, fpi );

		float sum = 0.0f;

		// cerco il massimo dell'ampiezza
		/*for( j = 0; j < NBins; j++ )*/
		for( j = 1; j < NBins - 1; j++ )
		{
			if( pvin[ j ].amplitude > *pMaxAmpl )
				*pMaxAmpl = pvin[ j ].amplitude;

			sum += pvin[ j ].amplitude * pvin[ j ].amplitude;
		}

		sum = sqrt( sum / (NBins - 2 ) );

		if( sum > (*pMaxRMS) )
			*pMaxRMS = sum;
	}

	delete[] pvin;

	fclose( fpi );

// DEBUG
//	char scri[100];
//	sprintf( scri, "max ampl %f max rms %f", *pMaxAmpl, *pMaxRMS );
//	MessageBox( NULL, scri, "X", MB_OK );
}

///////////////////////////////////////////////////////////////////////////////

BOOL CALLBACK MainDlg::DlgProc( HWND  hwndDlg, UINT  uMsg, WPARAM  wParam, LPARAM  lParam )
{
	switch( uMsg )
	{
	case WM_CLOSE:
		EndDialog( hwndDlg, 0 );
		break;
	case WM_INITDIALOG:
		Init( hwndDlg );
		return TRUE;
	case WM_ACTIVATE:
		{
			WORD actFlag = LOWORD( wParam );
			if( actFlag != WA_INACTIVE )
			{
				InvalidateRect( hwndDlg, NULL, TRUE );
			}
		}
		return 0;
	case WM_COMMAND:
		{
			WORD wNotifyCode = HIWORD( wParam );
			WORD wID = LOWORD( wParam );

			switch( wID )
			{
			//***************************************************
			//* Bottoni per la definizione di curve             *
			//***************************************************
			// --- TIME ---
			case BUT_DEFINE2:
				m_curveTime.CopyTo( &g_CurveDlg.m_Curve );
				strcpy( g_CurveDlg.m_LabelX, "Output time" );
				strcpy( g_CurveDlg.m_LabelY, "Input time" );
				g_CurveDlg.Show( m_hInstance, hwndDlg );
				g_CurveDlg.m_Curve.CopyTo( &m_curveTime );
				break;
			// --- FREQUENCY ---
			case BUT_DEFINE3:
				m_curveFreq1.CopyTo( &g_CurveDlg.m_Curve );
				switch( SendMessage( m_hwndCmbFreq, CB_GETCURSEL, 0, 0 ) )
				{
				case 0: // Remapping
				case 1: // Quantize
					strcpy( g_CurveDlg.m_LabelX, "In frequency" );
					strcpy( g_CurveDlg.m_LabelY, "Out frequency" );
					break;
				case 2: // Random Add
					strcpy( g_CurveDlg.m_LabelX, "In frequency" );
					strcpy( g_CurveDlg.m_LabelY, "Random amount" );
					break;
				}
				g_CurveDlg.Show( m_hInstance, hwndDlg );
				g_CurveDlg.m_Curve.CopyTo( &m_curveFreq1 );
				break;
			case BUT_DEFINE3B:
				m_curveFreq2.CopyTo( &g_CurveDlg.m_Curve );
				switch( SendMessage( m_hwndCmbFreq, CB_GETCURSEL, 0, 0 ) )
				{
				case 0: // Remapping
				case 1: // Quantize
					strcpy( g_CurveDlg.m_LabelX, "In frequency" );
					strcpy( g_CurveDlg.m_LabelY, "Out frequency" );
					break;
				case 2: // Random Add
					strcpy( g_CurveDlg.m_LabelX, "In frequency" );
					strcpy( g_CurveDlg.m_LabelY, "Random amount" );
					break;
				}
				g_CurveDlg.Show( m_hInstance, hwndDlg );
				g_CurveDlg.m_Curve.CopyTo( &m_curveFreq2 );
				break;
			case BUT_DEFINE3C:
				m_curveFreqInterp.CopyTo( &g_CurveDlg.m_Curve );
				strcpy( g_CurveDlg.m_LabelX, "time" );
				strcpy( g_CurveDlg.m_LabelY, "Interp. factor" );
				g_CurveDlg.Show( m_hInstance, hwndDlg );
				g_CurveDlg.m_Curve.CopyTo( &m_curveFreqInterp );
				break;
			// --- AMPLITUDE ---
			case BUT_DEFINE4:
				m_curveAmpl1.CopyTo( &g_CurveDlg.m_Curve );
				switch( SendMessage( m_hwndCmbAmpl, CB_GETCURSEL, 0, 0 ) )
				{
				case 0: // Transfer function
					strcpy( g_CurveDlg.m_LabelX, "frequency" );
					strcpy( g_CurveDlg.m_LabelY, "amplitude" );
					break;
				case 1: // Remapping
					strcpy( g_CurveDlg.m_LabelX, "In amplitude" );
					strcpy( g_CurveDlg.m_LabelY, "Out amplitude" );
					break;
				case 2: // Threshold
				case 3: // Inverse Threshold
					strcpy( g_CurveDlg.m_LabelX, "time" );
					strcpy( g_CurveDlg.m_LabelY, "Threshold" );
					break;
				}
				g_CurveDlg.Show( m_hInstance, hwndDlg );
				g_CurveDlg.m_Curve.CopyTo( &m_curveAmpl1 );
				break;
			case BUT_DEFINE4B:
				m_curveAmpl2.CopyTo( &g_CurveDlg.m_Curve );
				strcpy( g_CurveDlg.m_LabelX, "frequency" );
				strcpy( g_CurveDlg.m_LabelY, "amplitude" );
				g_CurveDlg.Show( m_hInstance, hwndDlg );
				g_CurveDlg.m_Curve.CopyTo( &m_curveAmpl2 );
				break;
			case BUT_DEFINE4C:
				m_curveAmplInterp.CopyTo( &g_CurveDlg.m_Curve );
				strcpy( g_CurveDlg.m_LabelX, "time" );
				strcpy( g_CurveDlg.m_LabelY, "Interp. factor" );
				g_CurveDlg.Show( m_hInstance, hwndDlg );
				g_CurveDlg.m_Curve.CopyTo( &m_curveAmplInterp );
				break;
			// --- MISC. ---
			case BUT_DEFINE5:
				m_curveMisc1.CopyTo( &g_CurveDlg.m_Curve );
				switch( SendMessage( m_hwndCmbMisc, CB_GETCURSEL, 0, 0 ) )
				{
				case 0: // BinDelay
					strcpy( g_CurveDlg.m_LabelX, "frequency" );
					strcpy( g_CurveDlg.m_LabelY, "Delay" );
					break;
				case 1: // Stereo
					strcpy( g_CurveDlg.m_LabelX, "frequency" );
					strcpy( g_CurveDlg.m_LabelY, "L/R Balance" );
					break;
				case 2: // Morphing 1 (interpolazione)
				case 3: // Morphing 2 (sintesi incrociata)
				case 4: // Morphing 3 (sintesi "granulare")
					strcpy( g_CurveDlg.m_LabelX, "time" );
					strcpy( g_CurveDlg.m_LabelY, "Morphing factor (A)" );
					break;
				}
				g_CurveDlg.Show( m_hInstance, hwndDlg );
				g_CurveDlg.m_Curve.CopyTo( &m_curveMisc1 );
				break;
			case BUT_DEFINE5B:
				m_curveMisc2.CopyTo( &g_CurveDlg.m_Curve );
				switch( SendMessage( m_hwndCmbMisc, CB_GETCURSEL, 0, 0 ) )
				{
				case 0: // BinDelay
					strcpy( g_CurveDlg.m_LabelX, "frequency" );
					strcpy( g_CurveDlg.m_LabelY, "Delay" );
					break;
				case 1: // Stereo
					strcpy( g_CurveDlg.m_LabelX, "frequency" );
					strcpy( g_CurveDlg.m_LabelY, "L/R Balance" );
					break;
				case 2: // Morphing 1 (interpolazione)
				case 3: // Morphing 2 (sintesi incrociata)
				case 4: // Morphing 3 (sintesi "granulare")
					strcpy( g_CurveDlg.m_LabelX, "time" );
					strcpy( g_CurveDlg.m_LabelY, "Morphing factor (f)" );
					break;
				}
				g_CurveDlg.Show( m_hInstance, hwndDlg );
				g_CurveDlg.m_Curve.CopyTo( &m_curveMisc2 );
				break;
			case BUT_DEFINE5C:
				m_curveMiscInterp.CopyTo( &g_CurveDlg.m_Curve );
				strcpy( g_CurveDlg.m_LabelX, "time" );
				strcpy( g_CurveDlg.m_LabelY, "Interp. factor" );
				g_CurveDlg.Show( m_hInstance, hwndDlg );
				g_CurveDlg.m_Curve.CopyTo( &m_curveMiscInterp );
				break;
			//********************************************
			//* Checkbox di attivazione interpolazione   *
			//********************************************
			case CHK_INTERP3:
				if( wNotifyCode == BN_CLICKED )
				{
					bool status = (bool)( SendMessage( m_hwndIntFreq, BM_GETCHECK, 0 , 0 ) == BST_CHECKED );
					if( status )
					{
						EnableWindow( m_hwndButFreq2, TRUE );
						EnableWindow( m_hwndButFreqInterp, TRUE );
					}
					else
					{
						EnableWindow( m_hwndButFreq2, FALSE );
						EnableWindow( m_hwndButFreqInterp, FALSE );
					}
				}
				break;
			case CHK_INTERP4:
				if( wNotifyCode == BN_CLICKED )
				{
					bool status = (bool)( SendMessage( m_hwndIntAmpl, BM_GETCHECK, 0 , 0 ) == BST_CHECKED );
					if( status )
					{
						EnableWindow( m_hwndButAmpl2, TRUE );
						EnableWindow( m_hwndButAmplInterp, TRUE );
					}
					else
					{
						EnableWindow( m_hwndButAmpl2, FALSE );
						EnableWindow( m_hwndButAmplInterp, FALSE );
					}
				}
				break;
			case CHK_INTERP5:
				if( wNotifyCode == BN_CLICKED )
				{
					bool status = (bool)( SendMessage( m_hwndIntMisc, BM_GETCHECK, 0 , 0 ) == BST_CHECKED );
					if( status )
					{
						EnableWindow( m_hwndButMisc2, TRUE );
						EnableWindow( m_hwndButMiscInterp, TRUE );
					}
					else
					{
						EnableWindow( m_hwndButMisc2, FALSE );
						EnableWindow( m_hwndButMiscInterp, FALSE );
					}
				}
				break;
			//**********************************************
			//* Edit box per la durata del suono in uscita *
			//**********************************************
			case EDIT_DUR:
				if( wNotifyCode == EN_KILLFOCUS )
				{
					char tmp[ 50 ];
					SendMessage( m_hwndEditDur, WM_GETTEXT, 50, (LPARAM)tmp );
					if( sscanf( tmp, "%f", &m_duration ) == EOF )
						m_duration = 0.0f;
				}
				break;
			//*****************
			//* Altri Bottoni *
			//*****************
			case BUT_DUMMY:
				// NON deve essere processato alcun messaggio per questo bottone "finto" !!!
				break;
			case BUT_FILEIO:
				g_FileDlg.Show( m_hInstance, hwndDlg );
				break;
			case BUT_GO:
				if( !Analyze() )
					break;
				if( !Process() )
					break;
				if( !Convert() )
					break;
				break;
			case BUT_EXITMAIN:
				if( wNotifyCode == BN_CLICKED )
					SendMessage( hwndDlg, WM_CLOSE, 0, 0 );
				break;
			//************
			//* Combobox *
			//************
			case CMB_FFT:
			case CMB_WINTYPE:
			case CMB_INCR:
				break;
			case CMB_XFORMTYPE3:
			case CMB_XFORMTYPE4:
			case CMB_XFORMTYPE5:
				if( wNotifyCode == CBN_SELCHANGE )
					OnChangeSel( (HWND)lParam, wID );
				break;
			}
		}
		break;
	default:
		break;
	}
	return 0;
}

///////////////////////////////////////////////////////////////////////////////

void MainDlg::Init( HWND hwndDlg )
{
	// handle della dialog
	m_hwndDlg = hwndDlg;

	// handle dei controlli
	m_hwndCmbFreq 		= GetDlgItem( m_hwndDlg, CMB_XFORMTYPE3 );
	m_hwndCmbAmpl 		= GetDlgItem( m_hwndDlg, CMB_XFORMTYPE4 );
	m_hwndCmbMisc 		= GetDlgItem( m_hwndDlg, CMB_XFORMTYPE5 );
	m_hwndCmbFft 		= GetDlgItem( m_hwndDlg, CMB_FFT );
	m_hwndCmbWin 		= GetDlgItem( m_hwndDlg, CMB_WINTYPE );
	m_hwndCmbIncr 		= GetDlgItem( m_hwndDlg, CMB_INCR );
	m_hwndBypTime 		= GetDlgItem( m_hwndDlg, CHK_BYPASS2 );
	m_hwndBypFreq 		= GetDlgItem( m_hwndDlg, CHK_BYPASS3 );
	m_hwndBypAmpl 		= GetDlgItem( m_hwndDlg, CHK_BYPASS4 );
	m_hwndBypMisc 		= GetDlgItem( m_hwndDlg, CHK_BYPASS5 );
	m_hwndIntFreq 		= GetDlgItem( m_hwndDlg, CHK_INTERP3 );
	m_hwndIntAmpl 		= GetDlgItem( m_hwndDlg, CHK_INTERP4 );
	m_hwndIntMisc 		= GetDlgItem( m_hwndDlg, CHK_INTERP5 );
	m_hwndIntFrames		= GetDlgItem( m_hwndDlg, CHK_TIMEINT );
	m_hwndEditDur 		= GetDlgItem( m_hwndDlg, EDIT_DUR );
	m_hwndEditDly 		= GetDlgItem( m_hwndDlg, EDIT_DLY );
	m_hwndEditRatio		= GetDlgItem( m_hwndDlg, EDIT_RATIO );
	m_hwndEditBase 		= GetDlgItem( m_hwndDlg, EDIT_BASE );
	m_hwndPbar    		= GetDlgItem( m_hwndDlg, PBAR_01 );
	m_hwndStatus  		= GetDlgItem( m_hwndDlg, TXT_STATUS );
	m_hwndButFreq1 		= GetDlgItem( m_hwndDlg, BUT_DEFINE3 );
	m_hwndButFreq2 		= GetDlgItem( m_hwndDlg, BUT_DEFINE3B );
	m_hwndButFreqInterp	= GetDlgItem( m_hwndDlg, BUT_DEFINE3C );
	m_hwndButAmpl1 		= GetDlgItem( m_hwndDlg, BUT_DEFINE4 );
	m_hwndButAmpl2 		= GetDlgItem( m_hwndDlg, BUT_DEFINE4B );
	m_hwndButAmplInterp	= GetDlgItem( m_hwndDlg, BUT_DEFINE4C );
	m_hwndButMisc1 		= GetDlgItem( m_hwndDlg, BUT_DEFINE5 );
	m_hwndButMisc2 		= GetDlgItem( m_hwndDlg, BUT_DEFINE5B );
	m_hwndButMiscInterp	= GetDlgItem( m_hwndDlg, BUT_DEFINE5C );

	// inizializzazione curve
	m_curveTime.Init();
	m_curveFreq1.Init();
	m_curveFreq2.Init();
	m_curveFreqInterp.Init();
	m_curveAmpl1.Init();
	m_curveAmpl2.Init();
	m_curveAmplInterp.Init();
	m_curveMisc1.Init();
	m_curveMisc2.Init();
	m_curveMiscInterp.Init();

	// centratura dialog
	int screenw = GetSystemMetrics( SM_CXSCREEN );
	int screenh = GetSystemMetrics( SM_CYSCREEN );
	RECT rect;
	GetWindowRect( m_hwndDlg, &rect );
	SetWindowPos( m_hwndDlg, HWND_TOP,
		( screenw - rect.right + rect.left ) / 2,
		( screenh - rect.bottom + rect.top ) / 2,
		0, 0, SWP_NOSIZE | SWP_NOZORDER );

	// progress bar
	SendMessage( m_hwndPbar, PBM_SETRANGE, 0, MAKELPARAM(0, 1000) );

	// combo-box
	SendMessage( m_hwndCmbFreq, CB_RESETCONTENT, 0, 0 );
	SendMessage( m_hwndCmbAmpl, CB_RESETCONTENT, 0, 0 );
	SendMessage( m_hwndCmbMisc, CB_RESETCONTENT, 0, 0 );
	SendMessage( m_hwndCmbFft, CB_RESETCONTENT, 0, 0 );
	SendMessage( m_hwndCmbWin, CB_RESETCONTENT, 0, 0 );
	SendMessage( m_hwndCmbIncr, CB_RESETCONTENT, 0, 0 );

	int i;
	for( i = 0; i < NUM_FREQ_XFORM; i++ )
		SendMessage( m_hwndCmbFreq, CB_INSERTSTRING, (WPARAM)i, (LPARAM)szFreqXform[ i ] );
	for( i = 0; i < NUM_AMPL_XFORM; i++ )
		SendMessage( m_hwndCmbAmpl, CB_INSERTSTRING, (WPARAM)i, (LPARAM)szAmplXform[ i ] );
	for( i = 0; i < NUM_MISC_XFORM; i++ )
		SendMessage( m_hwndCmbMisc, CB_INSERTSTRING, (WPARAM)i, (LPARAM)szMiscXform[ i ] );
	for( i = 0; i < NUM_FFT_PTS; i++ )
		SendMessage( m_hwndCmbFft, CB_INSERTSTRING, (WPARAM)i, (LPARAM)szFftPts[ i ] );
	for( i = 0; i < NUM_WIN_TYPES; i++ )
		SendMessage( m_hwndCmbWin, CB_INSERTSTRING, (WPARAM)i, (LPARAM)szWinTypes[ i ] );
	for( i = 0; i < NUM_FRAME_INCR; i++ )
		SendMessage( m_hwndCmbIncr, CB_INSERTSTRING, (WPARAM)i, (LPARAM)szFrameIncr[ i ] );

	SendMessage( m_hwndCmbFreq, CB_SETCURSEL, 0, 0 );
	SendMessage( m_hwndCmbAmpl, CB_SETCURSEL, 0, 0 );
	SendMessage( m_hwndCmbMisc, CB_SETCURSEL, 0, 0 );

	SendMessage( m_hwndCmbFft, CB_SETCURSEL, 5, 0 ); // default 1024 punti, finestra di Blackman, frame di 128 punti
	SendMessage( m_hwndCmbWin, CB_SETCURSEL, 3, 0 );
	SendMessage( m_hwndCmbIncr, CB_SETCURSEL, 3, 0 );

	// checkbox
	SendMessage( m_hwndBypTime, BM_SETCHECK, (WPARAM)BST_CHECKED, 0 );
	SendMessage( m_hwndBypFreq, BM_SETCHECK, (WPARAM)BST_CHECKED, 0 );
	SendMessage( m_hwndBypAmpl, BM_SETCHECK, (WPARAM)BST_CHECKED, 0 );
	SendMessage( m_hwndBypMisc, BM_SETCHECK, (WPARAM)BST_CHECKED, 0 );
	SendMessage( m_hwndIntFreq, BM_SETCHECK, (WPARAM)BST_UNCHECKED, 0 );
	SendMessage( m_hwndIntAmpl, BM_SETCHECK, (WPARAM)BST_UNCHECKED, 0 );
	SendMessage( m_hwndIntMisc, BM_SETCHECK, (WPARAM)BST_UNCHECKED, 0 );

	// controlli da disabilitare
	EnableWindow( m_hwndButFreq2, FALSE );
	EnableWindow( m_hwndButFreqInterp, FALSE );
	EnableWindow( m_hwndEditRatio, FALSE );
	EnableWindow( m_hwndEditBase, FALSE );
	EnableWindow( m_hwndButAmpl2, FALSE );
	EnableWindow( m_hwndButAmplInterp, FALSE );
	EnableWindow( m_hwndButMisc2, FALSE );
	EnableWindow( m_hwndButMiscInterp, FALSE );

	// icona
	HICON hico = LoadIcon( m_hInstance, MAKEINTRESOURCE(ICON_ID) );
	SendMessage( m_hwndDlg, WM_SETICON, (WPARAM)TRUE, (LPARAM)hico );
	SendMessage( m_hwndDlg, WM_SETICON, (WPARAM)FALSE, (LPARAM)hico );

	// tabelle per la FFT
	for( i = 0; i < MAX_FFT_PTS; i++ )
	{
		gFftSinTable[ i ] = (float)sin( i * DUEPIGRECO / (float)MAX_FFT_PTS );
		gFftCosTable[ i ] = (float)cos( i * DUEPIGRECO / (float)MAX_FFT_PTS );;
	}
}

/////////////////////////////
// FUZIONI DI ELABORAZIONE //
/////////////////////////////

#define FRAMES_X_BLOCK	32
//#define __PWD "c:\\progetti\\progc\\pvlabwin\\"

short inbuf[ 8192 ];
float test[ 8192 ];
float real[ 8192 ];
float img[ 8192 ];
double oldph[ 4097 ];
PVBin pvaux[ 4097 ];

bool MainDlg::Analyze( void )
{
	if( strcmp( g_FileDlg.m_fileIn1, "" ) == 0 )
	{
		ErrMsg( "Input file not specified!" );
		return false;
	}

	// controllo l'estensione del file di input
	char* str = &g_FileDlg.m_fileIn1[ strlen( g_FileDlg.m_fileIn1 ) - 3 ];

	char filePv1[ 256 ];
	sprintf( filePv1, "%s\\pv_in1.fft", g_CurDir );

	if( strcmp( strupr( str ), "FFT" ) == 0 )
	{
		if( CopyFile( g_FileDlg.m_fileIn1, filePv1, FALSE ) == FALSE )
		{
			ErrMsg( "Failed to copy file!" );
			return false;
		}

		FindMaxAmpl( filePv1, &gMaxAmpl, &gMaxRMS );
	}
	else if( strcmp( strupr( str ), "WAV" ) == 0 )
	{
		if( !Analyze( g_FileDlg.m_fileIn1, filePv1, &gMaxAmpl, &gMaxRMS ) )
			return false;

		if( g_FileDlg.m_SavePv )
		{
			if( CopyFile( filePv1, g_FileDlg.m_fileOutPv, FALSE ) == FALSE )
			{
				ErrMsg( "Failed to copy file!" );
				return false;
			}
		}
	}
	else
	{
		ErrMsg( "Input file extension not valid!" );
		return false;
	}

	m_bypassMisc = (bool)( SendMessage( m_hwndBypMisc, BM_GETCHECK, 0 , 0 ) == BST_CHECKED );

	// devo usare anche il secondo file di input ?
	if( (!m_bypassMisc) && ( SendMessage( m_hwndCmbMisc, CB_GETCURSEL, 0, 0 ) >= 2 ) )
	{
		str = &g_FileDlg.m_fileIn2[ strlen( g_FileDlg.m_fileIn2 ) - 3 ];

		char filePv2[ 256 ];
		sprintf( filePv2, "%s\\pv_in2.fft", g_CurDir );

		if( strcmp( strupr( str ), "FFT" ) == 0 )
		{
			if( CopyFile( g_FileDlg.m_fileIn2, filePv2, FALSE ) == FALSE )
			{
				ErrMsg( "Failed to copy file!" );
				return false;
			}

			FindMaxAmpl( filePv2, &gMaxAmpl2, &gMaxRMS  );
		}
		else if( strcmp( strupr( str ), "WAV" ) == 0 )
		{
			if( !Analyze( g_FileDlg.m_fileIn2, filePv2, &gMaxAmpl2, &gMaxRMS2 ) )
				return false;
		}
		else
		{
			ErrMsg( "Input #2 file extension not valid!" );
			return false;
		}
	}

	return true;
}

bool MainDlg::Analyze( char* infile, char* outfile, float* pMaxAmpl, float* pMaxRMS )
{
	SendMessage( m_hwndStatus, WM_SETTEXT, 0, (LPARAM)"Analysis..." );

	char msg[ 256 ];

	FILE* fpi = NULL;

	fpi = fopen( infile, "rb" );

	if( fpi == NULL )
	{
		sprintf( msg, "Can't open file: %s", infile );
		ErrMsg( msg );
		return false;
	}

	FILE* fpo = NULL;

	fpo = fopen( outfile, "wb+" );

	if( fpo == NULL )
	{
		sprintf( msg, "Can't open file: %s", outfile );
		ErrMsg( msg );
		return false;
	}

	// lettura header wave
	WaveHdr wh;
	fread( &wh, sizeof( WaveHdr ), 1, fpi );

	long SampleSize = wh.wBitsPerSample / 8;

	if( SampleSize != 2 )
	{
		ErrMsg( "Wave file must be 16 bits!" );
		return false;
	}

	if( wh.nChannels != 1 )
	{
		ErrMsg( "Wave file must be mono!" );
		return false;
	}

	long nSamples = wh.len3 / ( SampleSize * wh.nChannels );

	long i,j,k;
	long remaining = nSamples;
	long frameIncr = 1L << ( SendMessage( m_hwndCmbIncr, CB_GETCURSEL, 0, 0 ) + 4 /*5*/ );
	unsigned long log2pts = SendMessage( m_hwndCmbFft, CB_GETCURSEL, 0, 0 ) + 5 /*7*/;
	long frameSize = 1L << log2pts;
	long WinType = SendMessage( m_hwndCmbWin, CB_GETCURSEL, 0, 0 );

	if( frameIncr > frameSize )
	{
		ErrMsg( "Number of points between frames must be less or equal than the number of FFT points!!!" );
		return false;
	}

	long nFrames = 0;

	if( ( nSamples - frameSize + frameIncr - 1 ) <= 0 )
	{
		ErrMsg( "Not enough samples!!!!" );
		return false;
	}

	for( i = 0; i < ( nSamples - frameSize + frameIncr - 1 ); i += frameIncr )
	{
		nFrames++;
		remaining -= frameIncr;
	}

	// header phase vocoder
	PVHeader pvh;
    pvh.samplingRate = (float)wh.nSamplesPerSec;
    pvh.channels = 1;
    pvh.frameSize = frameSize;
    pvh.frameIncr = frameIncr;
    pvh.frameBsize = sizeof( PVBin ) * (pvh.frameSize / 2 + 1);
	pvh.dataBsize = pvh.frameBsize * nFrames;
    pvh.maxFreq = pvh.samplingRate / 2.0f;

	fwrite( &pvh, sizeof( PVHeader ), 1, fpo );

	long NBins = ( pvh.frameSize / 2 + 1 );
	for( i = 0; i < NBins; i++ )
		oldph[ i ] = 0.0;

	remaining = nSamples;

	long framecnt = 0;

	if( pMaxAmpl != NULL )
		*pMaxAmpl = 0.0f;

	if( pMaxRMS != NULL )
		*pMaxRMS = 0.0f;

	// LOOP PRINCIPALE
	for( i = 0; i < ( nSamples - pvh.frameSize + pvh.frameIncr - 1 ); i += pvh.frameIncr )
	{
		long nRead = ( ( remaining < pvh.frameSize ) ? remaining : pvh.frameSize );

		fseek( fpi, i * sizeof( short ) + sizeof(WaveHdr), SEEK_SET );
		fread( inbuf, nRead * sizeof( short ), 1, fpi );

		for( j = 0; j < nRead; j++ )
			test[ j ] = (float)inbuf[ j ] / 32768.0f;

		for( j = nRead; j < pvh.frameSize; j++ )
			test[ j ] = 0.0f;

		// finestratura
		for( j = 0; j < frameSize; j++ )
			test[ j ] *= ComputeWindow( pvh.frameSize, j, WinType );

		// calcolo FFT
		ComputeFft( log2pts, test, real, img );

		// calcolo phase vocoder (1 frame)
		ComputePV( pvh.frameSize, real, img, pvaux, oldph, pvh.samplingRate, pvh.frameIncr );

		// calcolo (eventuale) dell'ampiezza max
		if( pMaxAmpl != NULL )
		{
			float sum = 0.0f;

			/*for( j = 0; j < NBins; j++ )*/
			for( j = 1; j < NBins - 1; j++ ) // escludiamo il bin della continua e l'ultimo
			{
				if( pvaux[ j ].amplitude > (*pMaxAmpl) )
					*pMaxAmpl = pvaux[ j ].amplitude;

				sum += pvaux[ j ].amplitude * pvaux[ j ].amplitude;
			}

			sum = sqrt( sum / ( NBins - 2 ) );

			if( sum > (*pMaxRMS) )
				*pMaxRMS = sum;
		}

		// scrittura frame
		fwrite( pvaux, sizeof( PVBin ) * NBins, 1, fpo );
		remaining -= frameIncr;

		if( ( framecnt & 0xF ) == 0 )
		{
			SendMessage( m_hwndPbar, PBM_SETPOS, (WPARAM)( ( framecnt * 1000 ) / nFrames ), 0 );
			MSG msg;
			PeekMessage( &msg, m_hwndDlg, 0, 0, PM_NOREMOVE );
			UpdateWindow( m_hwndDlg );
		}

		framecnt++;
	}

	fclose(fpi);
	fclose(fpo);

// DEBUG
//	char scri[100];
//	sprintf( scri, "max ampl %f max rms %f", *pMaxAmpl, *pMaxRMS );
//	MessageBox( NULL, scri, "X", MB_OK );

	return true;
}

bool MainDlg::Process( void )
{
	SendMessage( m_hwndStatus, WM_SETTEXT, 0, (LPARAM)"Transformation..." );

	// acquisizione flag bypass trasformazione temporale
	m_bypassTime = (bool)( SendMessage( m_hwndBypTime, BM_GETCHECK, 0 , 0 ) == BST_CHECKED );

	// acquisizione flag interpolazione frames
	m_interpFrames = (bool)( SendMessage( m_hwndIntFrames, BM_GETCHECK, 0 , 0 ) == BST_CHECKED );

	char msg[ 256 ];
	char filePvIn1[ 256 ];
	char filePvIn2[ 256 ];
	char filePvOut[ 256 ];

	sprintf( filePvIn1, "%s\\pv_in1.fft", g_CurDir );
	sprintf( filePvIn2, "%s\\pv_in2.fft", g_CurDir );
	sprintf( filePvOut, "%s\\pv_out.fft", g_CurDir );

	// apertura file input
	FILE* fpi = NULL;
	fpi = fopen( filePvIn1, "rb" );

	if( fpi == NULL )
	{
		sprintf( msg, "Can't open file: %s", filePvIn1 );
		ErrMsg( msg );
		return false;
	}

	// apertura file output (temporaneo)
	FILE* fpt = NULL;
	fpt = fopen( filePvOut, "wb+" );

	if( fpt == NULL )
	{
		sprintf( msg, "Can't open file: %s", filePvOut );
		ErrMsg( msg );
		return false;
	}

	// lettura header
	PVHeader pvhdr;
	fseek( fpi, 0, SEEK_SET );
	fread( &pvhdr, sizeof( PVHeader ), 1, fpi );

	// ricavo il numero totale di frames in ingresso ed in uscita
	unsigned long InFrames = pvhdr.dataBsize / pvhdr.frameBsize;
	float InDuration = (float)InFrames * (float)pvhdr.frameIncr / pvhdr.samplingRate;
	unsigned long OutFrames = (unsigned long)( (float)InFrames * m_duration / InDuration );

	if( m_bypassTime || ( m_duration == 0.0f ) )
		OutFrames = InFrames;

	// ricavo il numero di bins per frame
	unsigned long NBins = pvhdr.frameSize / 2 + 1;

	// allocazione buffer d'ingresso
	PVBin* pvin = NULL;
	pvin = new PVBin[ ( FRAMES_X_BLOCK + 1 ) * NBins ];
	if( pvin == NULL )
	{
		ErrMsg( "Memory allocation error!" );
		return false;
	}

	// allocazione buffer d'uscita
	PVBin* pvout = NULL;
	pvout = new PVBin[ ( FRAMES_X_BLOCK + 1 ) * NBins ];
	if( pvout == NULL )
	{
		ErrMsg( "Memory allocation error!" );
		return false;
	}

	//////////////////////////////////////
	// *** TRASFORMAZIONE TEMPORALE *** //
	//////////////////////////////////////

	// devo necessariamente salvarla sul file temporaneo

	// scrittura header
	pvhdr.dataBsize = OutFrames * pvhdr.frameBsize;
	fseek( fpt, 0, SEEK_SET );
	fwrite( &pvhdr, sizeof( PVHeader ), 1, fpt );

	unsigned long i, j, k;
	/*gMaxAmpl = 0.0f;*/

	if( m_bypassTime )
	{
		for( i = 0; i < OutFrames; i++ )
		{
			// leggo il frame d'ingresso
			fread( pvin, sizeof(PVBin) * NBins, 1, fpi );

			// cerco il massimo dell'ampiezza
			/*for( j = 0; j < NBins; j++ )
			{
				if( pvin[ j ].amplitude > gMaxAmpl )
					gMaxAmpl = pvin[ j ].amplitude;
			}*/

			// scrivo il frame d'uscita (coincidente con quello d'ingresso)
			fwrite( pvin, sizeof(PVBin) * NBins, 1, fpt );
		}
	}
	else
	{
		for( i = 0; i < OutFrames; i++ )
		{
			// indice temporale (normalizzato tra 0 e 1)
			float t = (float)i / (float)OutFrames;
			// indici di frame
			long inframe1 = (long)floor( (double)m_curveTime.NormValue( t ) * (double)InFrames );
			long inframe2 = inframe1 + 1; //(long)ceil( m_curveTime.NormValue( t ) * (float)InFrames );
			// clip
			if( inframe1 < 0 )
				inframe1 = 0;
			if( inframe1 >= InFrames )
				inframe1 = InFrames - 1;
			if( inframe2 < 0 )
				inframe2 = 0;
			if( inframe2 >= InFrames )
				inframe2 = InFrames - 1;
			// fattore di interpolazione
			double interp = ( (double)m_curveTime.NormValue( t ) * (double)InFrames ) - (double)inframe1;

			// leggo i due frame d'ingresso
			fseek( fpi, inframe1 * sizeof(PVBin) * NBins + sizeof(PVHeader), SEEK_SET );
			fread( pvin, sizeof(PVBin) * NBins, 1, fpi );

			if( m_interpFrames )
			{
				fseek( fpi, inframe2 * sizeof(PVBin) * NBins + sizeof(PVHeader), SEEK_SET );
				fread( &pvin[ NBins ], sizeof(PVBin) * NBins, 1, fpi );
			}

			// ricavo il frame d'uscita (e cerco anche la max ampiezza)
			for( j = 0; j < NBins; j++ )
			{
				if( !m_interpFrames )
				{ // senza interpolazione
					pvout[ j ].amplitude = pvin[ j ].amplitude;
					pvout[ j ].frequency = pvin[ j ].frequency;
				}
				else
				{
					pvout[ j ].amplitude = pvin[ j ].amplitude + (float)( interp * (double)( pvin[ NBins + j ].amplitude  - pvin[ j ].amplitude ) );
					pvout[ j ].frequency = pvin[ j ].frequency + (float)( interp * (double)( pvin[ NBins + j ].frequency  - pvin[ j ].frequency ) );
				}

				/*if( pvout[ j ].amplitude > gMaxAmpl )
					gMaxAmpl = pvout[ j ].amplitude;*/
			}

			// scrittura frame d'uscita
			fwrite( pvout, sizeof(PVBin) * NBins, 1, fpt );
		}
	}

	// chiusura file input
	fflush( fpi );
	fclose( fpi );

	// chiusura file tmp
	fflush( fpt );
	fclose( fpt );

	// cancello il file d'ingresso e rinomino il file d'uscita col nome del file d'ingresso
	DeleteFile( filePvIn1 );
	MoveFile( filePvOut, filePvIn1 );

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

	// riapro il file d'ingresso (che sarebbe l'uscita dello stadio precedente)
	fpi = fopen( filePvIn1, "rb" );

	if( fpi == NULL )
	{
		sprintf( msg, "can't open file: %s", filePvIn1 );
		ErrMsg( msg );
		return false;
	}

	// apertura file d'uscita
	fpt = fopen( filePvOut, "wb+" );

	if( fpt == NULL )
	{
		sprintf( msg, "can't open file: %s", filePvOut );
		ErrMsg( msg );
		return false;
	}

	// rileggo l'header (non servirebbe, pero'...)
	fseek( fpi, 0, SEEK_SET );
	fread( &pvhdr, sizeof( PVHeader ), 1, fpi );

	// ... e lo riscrivo
	fseek( fpt, 0, SEEK_SET );
	fwrite( &pvhdr, sizeof( PVHeader ), 1, fpt );

	// acquisizione flags
	m_bypassFreq = (bool)( SendMessage( m_hwndBypFreq, BM_GETCHECK, 0 , 0 ) == BST_CHECKED );
	m_bypassAmpl = (bool)( SendMessage( m_hwndBypAmpl, BM_GETCHECK, 0 , 0 ) == BST_CHECKED );
	m_bypassMisc = (bool)( SendMessage( m_hwndBypMisc, BM_GETCHECK, 0 , 0 ) == BST_CHECKED );

	m_interpFreq = (bool)( SendMessage( m_hwndIntFreq, BM_GETCHECK, 0 , 0 ) == BST_CHECKED );
	m_interpAmpl = (bool)( SendMessage( m_hwndIntAmpl, BM_GETCHECK, 0 , 0 ) == BST_CHECKED );
	m_interpMisc = (bool)( SendMessage( m_hwndIntMisc, BM_GETCHECK, 0 , 0 ) == BST_CHECKED );

	// acquisizione del tipo di trasformazione
	long FreqXformType = (long)SendMessage( m_hwndCmbFreq, CB_GETCURSEL, 0, 0 );
	long AmplXformType = (long)SendMessage( m_hwndCmbAmpl, CB_GETCURSEL, 0, 0 );
	long MiscXformType = (long)SendMessage( m_hwndCmbMisc, CB_GETCURSEL, 0, 0 );

	// acquisizione dei parametri di quantizzazione
	char szTmp[ 50 ];
	SendMessage( m_hwndEditRatio, WM_GETTEXT, 50, (LPARAM)szTmp );
	if( sscanf( szTmp, "%f", &m_quantizeRatio ) == EOF )
		m_quantizeRatio = 1.0f;
	SendMessage( m_hwndEditBase, WM_GETTEXT, 50, (LPARAM)szTmp );
	if( sscanf( szTmp, "%f", &m_quantizeFreq ) == EOF )
		m_quantizeFreq = 27.5f;

	unsigned long remainingFrames = OutFrames;

	float sr2 = pvhdr.samplingRate / 2.0f;
	float invsr2 = 2.0f / pvhdr.samplingRate;
	float oneonfr = 1.0f / (float)OutFrames;

	m_t = 0.0f;

	// CICLO PRINCIPALE
	for( i = 0; i < OutFrames; i += FRAMES_X_BLOCK )
	{
		unsigned long NFrames = ( remainingFrames < FRAMES_X_BLOCK ? remainingFrames : FRAMES_X_BLOCK );

		// lettura del blocco di frames
		fread( pvin, sizeof(PVBin), NFrames * NBins, fpi );

		// copia dell'ultimo frame ( per trattare le interpolazioni col frame successivo )
		memcpy( &pvin[ FRAMES_X_BLOCK * NBins ], &pvin[ ( FRAMES_X_BLOCK - 1 ) * NBins ], sizeof(PVBin) * NBins );

		// CICLO SECONDARIO
		for( j = 0; j < NFrames; j++ )
		{
			// indice temporale (normalizzato tra 0 e 1)
			m_t += oneonfr;
			
			// annullo tutte le frequenze in uscita
			for( k = 0; k < NBins; k++ )
				pvout[ k + j * NBins ].frequency = 0.0f;
				
			for( k = 0; k < NBins; k++ )
			{
				////////////////////////////
				// *** TRASFORMAZIONI *** //
				////////////////////////////

				// frequenza normalizzata
				float f = pvin[ k + j * NBins ].frequency * invsr2;

				// trasformazione di frequenza
				if( !m_bypassFreq )
				{
					switch( FreqXformType )
					{
					case 0: //Remapping
						{
							float coeff;

							if( m_interpFreq )
							{
								coeff = m_curveFreq1.NormValue( f ) + m_curveFreqInterp.NormValue( m_t ) *
									( m_curveFreq2.NormValue( f ) - m_curveFreq1.NormValue( f ) );
							}
							else
							{
								coeff = m_curveFreq1.NormValue( f );
							}

							pvout[ k + j * NBins ].frequency = coeff * sr2;
						}
						break;
					case 1: // Quantize
						pvout[ k + j * NBins ].frequency = QuantizeFreq( pvin[ k + j * NBins ].frequency,
							m_quantizeRatio, m_quantizeFreq );
						break;
					case 2: // Random add
						{
							float coeff;

							if( m_interpFreq )
							{
								coeff = m_curveFreq1.NormValue( f ) + m_curveFreqInterp.NormValue( m_t ) *
									( m_curveFreq2.NormValue( f ) - m_curveFreq1.NormValue( f ) );
							}
							else
							{
								coeff = m_curveFreq1.NormValue( f );
							}

							float rand_amount = coeff * ( -1.0f + 2.0f * (float)rand() / (float)RAND_MAX );

							// al massimo, viene aggiunto/sottratto il quadruplo della banda di un singolo canale di analisi
							pvout[ k + j * NBins ].frequency = pvin[ k + j * NBins ].frequency + rand_amount * 4.0f * sr2 / (float)NBins;
						}
						break;
					default: // ?!
						pvout[ k + j * NBins ].frequency = pvin[ k + j * NBins ].frequency;
						break;
					}
				}
				else
				{ // bypass
					pvout[ k + j * NBins ].frequency = pvin[ k + j * NBins ].frequency;
				}

				// trasformazione di ampiezza
				if( !m_bypassAmpl )
				{
					switch( AmplXformType )
					{
					case 0: // Transfer function
						{
							float coeff;

							if( m_interpAmpl )
							{
								coeff = m_curveAmpl1.NormValue( f ) + m_curveAmplInterp.NormValue( m_t ) *
									( m_curveAmpl2.NormValue( f ) - m_curveAmpl1.NormValue( f ) );
							}
							else
							{
								coeff = m_curveAmpl1.NormValue( f );
							}

							pvout[ k + j * NBins ].amplitude = coeff * pvin[ k + j * NBins ].amplitude;
						}
						break;
					case 1: // Remapping
						{
							float coeff, namp;

							// ampiezza normalizzata tra 0 e 1
							namp = pvin[ k + j * NBins ].amplitude / gMaxAmpl;

							if( m_interpAmpl )
							{
								coeff = m_curveAmpl1.NormValue( namp ) + m_curveAmplInterp.NormValue( m_t ) *
									( m_curveAmpl2.NormValue( namp ) - m_curveAmpl1.NormValue( namp ) );
							}
							else
							{
								coeff = m_curveAmpl1.NormValue( namp );
							}

							pvout[ k + j * NBins ].amplitude = coeff * gMaxAmpl;
						}
						break;
					case 2: // Threshold
						{
							float thr = m_curveAmpl1.NormValue( m_t ) * gMaxAmpl;

							if( pvin[ k + j * NBins ].amplitude >= thr )
								pvout[ k + j * NBins ].amplitude = pvin[ k + j * NBins ].amplitude;
							else
								pvout[ k + j * NBins ].amplitude = 0.0f;
						}
						break;
					case 3: // Inverse Threshold
						{
							float thr = m_curveAmpl1.NormValue( m_t ) * gMaxAmpl;

							if( pvin[ k + j * NBins ].amplitude <= thr )
								pvout[ k + j * NBins ].amplitude = pvin[ k + j * NBins ].amplitude;
							else
								pvout[ k + j * NBins ].amplitude = 0.0f;
						}
						break;
					default: // ?!
						pvout[ k + j * NBins ].amplitude = pvin[ k + j * NBins ].amplitude;
						break;
					}
				}
				else
				{ // bypass
					pvout[ k + j * NBins ].amplitude = pvin[ k + j * NBins ].amplitude;
				}
			}
		}

		// scrittura del blocco in uscita
		fwrite( pvout, sizeof(PVBin), NFrames * NBins, fpt );

		remainingFrames -= FRAMES_X_BLOCK;

	   	SendMessage( m_hwndPbar, PBM_SETPOS, (WPARAM)( ( i * 1000 ) / NFrames), 0 );
		MSG msg;
		PeekMessage( &msg, m_hwndDlg, 0, 0, PM_NOREMOVE );
		UpdateWindow( m_hwndDlg );
	}

	// chiusura file input
	fclose( fpi );

	// chiusura file tmp
	fclose( fpt );

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

	// per il Delay vs. Frequency, devo usare un altro ciclo a parte
	if( (!m_bypassMisc) && ( SendMessage( m_hwndCmbMisc, CB_GETCURSEL, 0, 0 ) == 0 ) )
	{
		// cancello il file d'ingresso allo stadio precedente...
		DeleteFile( filePvIn1 );
		// ...e rinomino il file d'uscita (dello stadio precedente) col nome del file d'ingresso
		MoveFile( filePvOut, filePvIn1 );

	   	SendMessage( m_hwndPbar, PBM_SETPOS, 0, 0 );
		SendMessage( m_hwndStatus, WM_SETTEXT, 0, (LPARAM)"Delay vs. Frequency..." );

		// acquisizione max delay
		char tmp[ 50 ];
		SendMessage( m_hwndEditDly, WM_GETTEXT, 50, (LPARAM)tmp );
		if( sscanf( tmp, "%f", &m_maxDelay ) == EOF )
			m_maxDelay = 0.0f;

		// riapro il file d'ingresso
		fpi = fopen( filePvIn1, "rb" );

		if( fpi == NULL )
		{
			sprintf( msg, "Can't open file: %s", filePvIn1 );
			ErrMsg( msg );
			return false;
		}

		// apertura file d'uscita
		fpt = fopen( filePvOut, "wb+" );

		if( fpt == NULL )
		{
			sprintf( msg, "Can't open file: %s", filePvOut );
			ErrMsg( msg );
			return false;
		}

		// rileggo l'header
		fseek( fpi, 0, SEEK_SET );
		fread( &pvhdr, sizeof( PVHeader ), 1, fpi );

		InFrames = pvhdr.dataBsize / pvhdr.frameBsize;
		InDuration = (float)InFrames * (float)pvhdr.frameIncr / pvhdr.samplingRate;
		OutFrames = (unsigned long)( (float)InFrames * ( 1.0f + m_maxDelay / InDuration ) );
		NBins = pvhdr.frameSize / 2 + 1;

		// riscrivo l'header
		pvhdr.dataBsize = OutFrames * pvhdr.frameBsize;
		fseek( fpt, 0, SEEK_SET );
		fwrite( &pvhdr, sizeof( PVHeader ), 1, fpt );

		for( i = 0; i < OutFrames; i++ )
		{
			// indice temporale (in secondi)
			float t = (float)i * (float)pvhdr.frameIncr / pvhdr.samplingRate;

			// ricavo il frame d'uscita
			for( j = 0; j < NBins; j++ )
			{
				float f = (float)j / (float)NBins;

				// delay funzione della frequenza
				float delay;
				if( m_interpMisc )
				{
					// indice temporale normalizzato tra 0 e 1 (1 = durata totale in output)
					float tnorm = (float)i / (float)OutFrames;

					delay = m_maxDelay * ( m_curveMisc1.NormValue( f ) + m_curveMiscInterp.NormValue( tnorm ) *
						( m_curveMisc2.NormValue( f ) - m_curveMisc1.NormValue( f ) ) );
				}
				else
				{
					delay = m_curveMisc1.NormValue( f ) * m_maxDelay;
				}

				if( ( t >= delay ) && ( t <= ( InDuration + delay ) ) )
				{
					// indice temporale normalizzato per puntare nel file d'ingresso (1 = durata file input)
					float tn = ( t - delay ) / InDuration;

					// indici di frame
					long inframe1 = (unsigned long)floor( tn * (float)InFrames );
					long inframe2 = (unsigned long)ceil( tn * (float)InFrames );
					// fattore di interpolazione
					float interp = tn * (float)InFrames - (float)inframe1;

					// leggo il bin dei due frame d'ingresso
					fseek( fpi, j * sizeof(PVBin) + inframe1 * sizeof(PVBin) * NBins + sizeof(PVHeader), SEEK_SET );
					fread( &pvin[ 0 ], sizeof(PVBin), 1, fpi );

					fseek( fpi, j * sizeof(PVBin) + inframe2 * sizeof(PVBin) * NBins + sizeof(PVHeader), SEEK_SET );
					fread( &pvin[ 1 ], sizeof(PVBin), 1, fpi );

					pvout[ j ].amplitude = pvin[ 0 ].amplitude + interp * ( pvin[ 1 ].amplitude  - pvin[ 0 ].amplitude );
					pvout[ j ].frequency = pvin[ 0 ].frequency + interp * ( pvin[ 1 ].frequency  - pvin[ 0 ].frequency );
				}
				else
				{
					pvout[ j ].amplitude = 0.0f;
					pvout[ j ].frequency = 0.0f;
				}
			}

			// scrittura frame d'uscita
			fwrite( pvout, sizeof(PVBin) * NBins, 1, fpt );

			if ( ( i & 0x0000003F ) == 0 ) // ogni 64
			{
				SendMessage( m_hwndPbar, PBM_SETPOS, (WPARAM)( ( i * 1000 ) / OutFrames), 0 );
				MSG msg;
				PeekMessage( &msg, m_hwndDlg, 0, 0, PM_NOREMOVE );
				UpdateWindow( m_hwndDlg );
			}
		}

		// chiusura file input
		fclose( fpi );

		// chiusura file tmp
		fclose( fpt );

		// cancella file d'ingresso
		DeleteFile( filePvIn1 );
	}
	else if( (!m_bypassMisc) && ( SendMessage( m_hwndCmbMisc, CB_GETCURSEL, 0, 0 ) >= 2 ) )
	{ //********************** MORPHING **********************************
		// cancello il file d'ingresso allo stadio precedente...
		DeleteFile( filePvIn1 );
		// ...e rinomino il file d'uscita (dello stadio precedente) col nome del file d'ingresso
		MoveFile( filePvOut, filePvIn1 );

	   	SendMessage( m_hwndPbar, PBM_SETPOS, 0, 0 );
		SendMessage( m_hwndStatus, WM_SETTEXT, 0, (LPARAM)"Morphing..." );

		// riapro il file d'ingresso
		fpi = fopen( filePvIn1, "rb" );

		if( fpi == NULL )
		{
			sprintf( msg, "Can't open file: %s", filePvIn1 );
			ErrMsg( msg );
			return false;
		}

		// apro il secondo file
		FILE* fpi2 = NULL;

		fpi2 = fopen( filePvIn2, "rb" );

		if( fpi2 == NULL )
		{
			sprintf( msg, "Can't open file: %s", filePvIn2 );
			ErrMsg( msg );
			return false;
		}

		// apertura file d'uscita
		fpt = fopen( filePvOut, "wb+" );

		if( fpt == NULL )
		{
			sprintf( msg, "Can't open file: %s", filePvOut );
			ErrMsg( msg );
			return false;
		}

		// rileggo l'header dei due file d'ingresso
		fseek( fpi, 0, SEEK_SET );
		fread( &pvhdr, sizeof( PVHeader ), 1, fpi );

		PVHeader pvhdr2;

		fseek( fpi2, 0, SEEK_SET );
		fread( &pvhdr2, sizeof( PVHeader ), 1, fpi2 );

		// check di consistenza
		if( ( pvhdr.frameSize != pvhdr2.frameSize ) ||
			( pvhdr.frameBsize != pvhdr2.frameBsize ) ||
			( pvhdr.frameIncr != pvhdr2.frameIncr ) ||
			( pvhdr.samplingRate != pvhdr2.samplingRate ) )
		{
			ErrMsg( "Analysis files for morphing should have the same format!!!" );
			return false;
		}

		InFrames = pvhdr.dataBsize / pvhdr.frameBsize;
		long InFrames2 = pvhdr2.dataBsize / pvhdr2.frameBsize;
		OutFrames = ( ( InFrames > InFrames2 ) ? InFrames : InFrames2 );
		NBins = pvhdr.frameSize / 2 + 1;

		// riscrivo l'header
		pvhdr.dataBsize = OutFrames * pvhdr.frameBsize;
		fseek( fpt, 0, SEEK_SET );
		fwrite( &pvhdr, sizeof( PVHeader ), 1, fpt );

		long MorphType = SendMessage( m_hwndCmbMisc, CB_GETCURSEL, 0, 0 ) - 2;

		float oneonMaxAmpl = 1.0f / gMaxAmpl;
		float oneonMaxAmpl2 = 1.0f / gMaxAmpl2;
		float oneonMaxRMS = 1.0f / gMaxRMS;
		float oneonMaxRMS2 = 1.0f / gMaxRMS2;

		// ciclo principale
		for( i = 0; i < OutFrames; i++ )
		{
			// indice temporale (normalizzato)
			float t = (float)i / (float)OutFrames;

			// primo fattore di interpolazione
			float interp = m_curveMisc1.NormValue( t );

			// secondo fattore di interpolazione
			float interp2 = m_curveMisc2.NormValue( t );

			// lettura frame d'ingresso: una volta raggiunta la fine del file, il frame rimane l'ultimo letto
			if( i < InFrames )
				fread( pvin, sizeof(PVBin), NBins, fpi );

			if( i < InFrames2 )
				fread( &pvin[ NBins ], sizeof(PVBin), NBins, fpi2 );

			// ricavo il frame d'uscita

			if( MorphType == 0 )
			{ // morphing di tipo 1 (interpolazione "brutale" di ampiezza e frequenza)

				for( j = 0; j < NBins; j++ )
				{
					// primo fattore di interpolazione -> ampiezza, secondo fattore -> frequenza
					pvout[ j ].amplitude = pvin[ j ].amplitude + interp * ( pvin[ j + NBins ].amplitude  - pvin[ j ].amplitude );
					pvout[ j ].frequency = pvin[ j ].frequency + interp2 * ( pvin[ j + NBins ].frequency  - pvin[ j ].frequency );
				}
			}
			else if( MorphType == 1 )
			{ // morphing di tipo 2 (sintesi incrociata)
				// le frequenze sono interpolate come prima, le ampiezze sono interpolate tra quelle del 1o suono
				// e quelle del primo "filtrate" dal secondo (normalizzazione in base a RMS max!)
				for( j = 0; j < NBins; j++ )
				{
					// primo fattore di interpolazione -> ampiezza, secondo fattore -> frequenza
					pvout[ j ].amplitude = pvin[ j ].amplitude * ( 1.0f +
						interp * ( pvin[ j + NBins ].amplitude * oneonMaxRMS2 - 1.0f ) );

					//pvout[ j ].frequency = pvin[ j ].frequency; // vecchia versione
					pvout[ j ].frequency = pvin[ j ].frequency + interp2 * ( pvin[ j + NBins ].frequency  - pvin[ j ].frequency );
				}
			}
			else if( MorphType == 2 )
			{ // morphing di tipo 3 (sintesi "granulare")
				// ogni singolo dato di ampiezza e frequenza puo' appartenere al primo o al secondo suono
				// secondo il fattore di morphing, che in questo caso rappresenta una densita'
				for( j = 0; j < NBins; j++ )
				{
					// primo fattore di interpolazione -> ampiezza, secondo fattore -> frequenza

					// estrazione di un numero casuale tra 0 e 1
					float casuale = (float)rand() / (float)RAND_MAX;

					// interp:  0 - max densita' del primo suono, 1 - max densita' del secondo
					if( casuale > interp )
						pvout[ j ].amplitude = pvin[ j ].amplitude;
					else
						pvout[ j ].amplitude = pvin[ j + NBins ].amplitude;

					if( casuale > interp2 )
						pvout[ j ].frequency = pvin[ j ].frequency;
					else
						pvout[ j ].frequency = pvin[ j + NBins ].frequency;
				}
			}

			// scrittura frame d'uscita
			fwrite( pvout, sizeof(PVBin) * NBins, 1, fpt );

			if ( ( i & 0x0000003F ) == 0 ) // ogni 64
			{
				SendMessage( m_hwndPbar, PBM_SETPOS, (WPARAM)( ( i * 1000 ) / OutFrames), 0 );
				MSG msg;
				PeekMessage( &msg, m_hwndDlg, 0, 0, PM_NOREMOVE );
				UpdateWindow( m_hwndDlg );
			}
		}

		// chiusura file input
		fclose( fpi );
		fclose( fpi2 );

		// chiusura file tmp
		fclose( fpt );

		// cancella file d'ingresso
		DeleteFile( filePvIn1 );
		DeleteFile( filePvIn2 );
	}

	// NOTA: a questo punto, deve esserci solo il file "pv_out.fft" nella directory corrente

	// de-alloco la memoria
	delete[] pvin;
	delete[] pvout;

	return true;
}

bool MainDlg::Convert( void )
{
	// acquisisco i flag e determino il tipo di conversione (stereo o mono)
	m_bypassMisc = ( SendMessage( m_hwndBypMisc, BM_GETCHECK, 0, 0 ) == BST_CHECKED );
	m_interpMisc = ( SendMessage( m_hwndIntMisc, BM_GETCHECK, 0, 0 ) == BST_CHECKED );
	int MiscXform = SendMessage( m_hwndCmbMisc, CB_GETCURSEL, 0, 0 );

	bool stereo = ( ( !m_bypassMisc ) && ( MiscXform == 1 ) );

	// Wave header
	WaveHdr wh;
	wh.nChannels = ( stereo ? 2 : 1 );

	unsigned long i,j,k,n;
	char msg[ 256 ];
	char filePv[ 256 ];
	sprintf( filePv, "%s\\pv_out.fft", g_CurDir );

	// apertura file input (output della sezione precedente)
	FILE* fpi = NULL;
	fpi = fopen( filePv, "rb" );

	if( fpi == NULL )
	{
		sprintf( msg, "Can't open file: %s", filePv );
		ErrMsg( msg );
		return false;
	}

	// apertura file di output
	FILE* fpo = NULL;
	fpo = fopen( g_FileDlg.m_fileOut, "wb+" );

	if( fpo == NULL )
	{
		sprintf( msg, "Can't open file: %s", g_FileDlg.m_fileOut );
		ErrMsg( msg );
		return false;
	}

	// lettura header
	PVHeader pvhdr;
	fseek( fpi, 0, SEEK_SET );
	fread( &pvhdr, sizeof( PVHeader ), 1, fpi );

	// ricavo il numero totale di frames
	unsigned long NFrames = pvhdr.dataBsize / pvhdr.frameBsize;

	// ricavo il numero di bins per frame
	unsigned long NBins = pvhdr.frameSize / 2 + 1;

	// allocazione buffer d'ingresso
	PVBin* pvin = NULL;
	pvin = new PVBin[ ( FRAMES_X_BLOCK + 1 ) * NBins ];
	if( pvin == NULL )
	{
		ErrMsg( "Memory allocation error!" );
		return false;
	}

	// allocazione buffer d'uscita
	short* outbuf = NULL;
	outbuf = new short[ FRAMES_X_BLOCK * pvhdr.frameIncr * wh.nChannels ];
	if( outbuf == NULL )
	{
		ErrMsg( "Memory allocation error!" );
		return false;
	}

	// allocazione buffer fasi
	float* pPhase = NULL;
	pPhase = new float[ NBins ];
	if( pPhase == NULL )
	{
		ErrMsg( "Memory allocation error!" );
		return false;
	}

	for( i = 0; i < NBins; i++ )
		pPhase[ i ] = 0.0f;

	// allocazione buffer ausiliario frequenze e ampiezze
	PVBin* pAuxbin = NULL;
	pAuxbin = new PVBin[ NBins ];
	if( pAuxbin == NULL )
	{
		ErrMsg( "Memory allocation error!" );
		return false;
	}

	for( i = 0; i < NBins; i++ )
	{
		pAuxbin[ i ].amplitude = 0.0f;
		pAuxbin[ i ].frequency = 0.0f;
	}

	// allocazione buffer ausiliario delta freq., amp.
	PVBin* pDeltabin = NULL;
	pDeltabin = new PVBin[ NBins ];
	if( pDeltabin == NULL )
	{
		ErrMsg( "Memory allocation error!" );
		return false;
	}

	for( i = 0; i < NBins; i++ )
	{
		pDeltabin[ i ].amplitude = 0.0f;
		pDeltabin[ i ].frequency = 0.0f;
	}

	// allocazione buffer fattore di "stereofonizzazione"
	float* pRx = NULL;
	pRx = new float[ NBins ];
	if( pRx == NULL )
	{
		ErrMsg( "Memory allocation error!" );
		return false;
	}

	// riempimento campi del wave header
	wh.nSamplesPerSec = (unsigned long)pvhdr.samplingRate;
	wh.nAvgBytesPerSec = wh.nSamplesPerSec * ( wh.wBitsPerSample / 8 ) * wh.nChannels;
	wh.nBlockAlign = ( wh.wBitsPerSample / 8 ) * wh.nChannels;

	long nSamples = NFrames * pvhdr.frameIncr;
	long SamplesSize = nSamples * ( wh.wBitsPerSample / 8 ) * wh.nChannels;
	wh.len1 = SamplesSize + 36;  // SamplesSize + 44 (header) - 8 ("RIFF" + len1)
	wh.len3 = SamplesSize;

	fwrite( &wh, sizeof(WaveHdr), 1, fpo );

///////////////////////////////////////////////////////////////////////////////////////////////////

	float invsr = 1.0f / pvhdr.samplingRate;

#define PIGRECO 	3.1415926535897932384626433832795f
#define DUEPIGRECO 	6.28318530717958647692528676655901f

#ifdef SINTABLESIZE
#undef SINTABLESIZE
#define SINTABLESIZE	65536
#endif

#define NDXCONV		((float)SINTABLESIZE/DUEPIGRECO)

	float SinTable[ SINTABLESIZE ];
	for( long j = 0; j < SINTABLESIZE; j++ )
		SinTable[ j ] = sin( -PIGRECO + DUEPIGRECO * (float)j / (float)SINTABLESIZE );

	float invIncr = 1.0f / (float)pvhdr.frameIncr;
	float TwopiOnSr = DUEPIGRECO / pvhdr.samplingRate;
	float scale = 32768.0f * 2.0f / (float)pvhdr.frameSize;  // ?????????

	unsigned long remainingFrames = NFrames;
	unsigned long fr = 0; // contatore di frame

   	SendMessage( m_hwndPbar, PBM_SETPOS, 0, 0 );
	SendMessage( m_hwndStatus, WM_SETTEXT, 0, (LPARAM)"Resynthesis..." );

	// CICLO PRINCIPALE (per tutti i frame del file di analisi)
	for( i = 0; i < NFrames; i += FRAMES_X_BLOCK )
	{
		unsigned long BFrames = ( remainingFrames < FRAMES_X_BLOCK ? remainingFrames : FRAMES_X_BLOCK );

		// lettura del blocco di frames
		if( remainingFrames <= FRAMES_X_BLOCK )
		{ // ultimo blocco ?
			fread( pvin, sizeof(PVBin), BFrames * NBins, fpi );

			// copia dell'ultimo frame ( per trattare le interpolazioni col frame successivo )
			memcpy( &pvin[ BFrames * NBins ], &pvin[ ( BFrames - 1 ) * NBins ], sizeof(PVBin) * NBins );
		}
		else
		{
			// non e' l'ultimo blocco: leggo un frame in piu' e riposiziono il file un frame indietro
			fread( pvin, sizeof(PVBin), ( BFrames + 1 ) * NBins, fpi );
			fseek( fpi, -( sizeof(PVBin) * NBins ), SEEK_CUR );
		}

		short* pBuffer = outbuf;

		// CICLO SECONDARIO
		for( j = 0; j < BFrames; j++ ) // ciclo per ogni frame del blocco
		{
			// preparo i vettori di ampiezza e frequenza (scalata per NDXCONV * 2 * PIGRECO / SR)
			// e del delta di ampiezza e frequenza (per l'interpolazione all'interno di un frame)
			// + l'eventuale fattore di "stereofonizzazione"
			for( k = 1; k < NBins - 1; k++ )
			{
				pAuxbin[ k ].amplitude = pvin[ j * NBins + k ].amplitude;
				pAuxbin[ k ].frequency = pvin[ j * NBins + k ].frequency * TwopiOnSr * NDXCONV;
				pDeltabin[ k ].amplitude = ( pvin[ ( j + 1 ) * NBins + k ].amplitude - pvin[ j * NBins + k ].amplitude )
					* invIncr;
				pDeltabin[ k ].frequency = ( pvin[ ( j + 1 ) * NBins + k ].frequency - pvin[ j * NBins + k ].frequency )
					* invIncr * TwopiOnSr * NDXCONV;

				if( stereo )
				{
					float t = (float)fr / (float)NFrames;
					float f = (float)k / (float)NBins;
					if( m_interpMisc )
					{
						pRx[ k ] = m_curveMisc1.NormValue( f ) + m_curveMiscInterp.NormValue( t ) *
							( m_curveMisc2.NormValue( f ) - m_curveMisc1.NormValue( f ) );
					}
					else
					{
						pRx[ k ] = m_curveMisc1.NormValue( f );
					}
				}
			}

			for( k = 0; k < pvhdr.frameIncr; k++ ) // ciclo per ogni campione da produrre in uscita
			{
				float sum = 0.0f;
				float sumL = 0.0f;
 				PVBin* pBin = &pAuxbin[ 1 ];
				PVBin* pBinD = &pDeltabin[ 1 ];
				float* ph = &pPhase[ 1 ];

				for( n = 1; n < NBins - 1; n++, ph++, pBin++, pBinD++ ) // per ogni bin (eccetto primo e ultimo)
				{
					float frq, amp, phase, contrib;

					frq = pBin->frequency; // la frequenza e' in realta' scalata per NDXCONV * 2 * PIGRECO / SR
					amp = pBin->amplitude;
					phase = *ph;

					contrib = amp * SinTable[ (long)( phase + PIGRECO * NDXCONV ) ];

					// accumulo il contributo del bin
					if( stereo )
					{
						sumL += ( 1.0f - pRx[ n ] ) * contrib;
						contrib *= pRx[ n ];
					}

					if( amp != 0.0f )
						sum += contrib;

					// aggiorno ampiezza e frequenza
					pBin->amplitude += pBinD->amplitude;
					pBin->frequency += pBinD->frequency;

					// incremento la fase istantanea del bin (scalata per NDXCONV)
					phase += frq;

					// riporto la fase tra 0 e 2 * PIGRECO
					// (ovvero quella scalata tra 0 e 2 * PIGRECO * NDXCONV)
					//while( phase >= DUEPIGRECO * NDXCONV )
					//	phase -= DUEPIGRECO * NDXCONV;
					//while( phase < 0.0f )
					//	phase += DUEPIGRECO * NDXCONV;
					while( phase >= PIGRECO * NDXCONV )
						phase -= DUEPIGRECO * NDXCONV;
					while( phase < (-PIGRECO * NDXCONV) )
						phase += DUEPIGRECO * NDXCONV;

					// riaggiorno la fase
					*ph = phase;

				} // ciclo per ogni bin

				// normalizzazione e copia nel campione
				if( stereo )
				{
					*pBuffer = (short)( sumL * scale );
					pBuffer++;
					*pBuffer = (short)( sum * scale );
					pBuffer++;
				}
				else
				{
					*pBuffer = (short)( sum * scale );
					pBuffer++;
				}

			} // ciclo per ogni campione del frame

			fr++;

		} // ciclo per ogni frame del blocco

		// scrittura del blocco in uscita
		fwrite( outbuf, sizeof(short) * BFrames * pvhdr.frameIncr * wh.nChannels, 1, fpo );

		remainingFrames -= FRAMES_X_BLOCK;

		SendMessage( m_hwndPbar, PBM_SETPOS, (WPARAM)( ( i * 1000 ) / NFrames), 0 );
		MSG msg;
		PeekMessage( &msg, m_hwndDlg, 0, 0, PM_NOREMOVE );
		UpdateWindow( m_hwndDlg );
	}

	// chiusura file
	fclose( fpi );

	fclose( fpo );

	// deallocazione
	delete[] pvin;
	delete[] outbuf;
	delete[] pPhase;
	delete[] pAuxbin;
	delete[] pDeltabin;
	delete[] pRx;

	SendMessage( m_hwndStatus, WM_SETTEXT, 0, (LPARAM)"Done" );
	SendMessage( m_hwndPbar, PBM_SETPOS, 0, 0 );
	MessageBeep( MB_OK );

	return true;
}

void MainDlg::OnChangeSel( HWND hwndCombo, WORD idCombo )
{
	// selezione della combo
	int selection = SendMessage( hwndCombo, CB_GETCURSEL, 0, 0 );

	// stato delle checkbox
	bool chkFreq = (bool)( SendMessage( m_hwndIntFreq, BM_GETCHECK, 0 , 0 ) == BST_CHECKED );
	bool chkAmpl = (bool)( SendMessage( m_hwndIntAmpl, BM_GETCHECK, 0 , 0 ) == BST_CHECKED );
	bool chkMisc = (bool)( SendMessage( m_hwndIntMisc, BM_GETCHECK, 0 , 0 ) == BST_CHECKED );

	switch( idCombo )
	{
	// *** frequency ***
	case CMB_XFORMTYPE3:
		switch( selection )
		{
 		case 0: // Remapping
		case 2: // Random Add
			EnableWindow( m_hwndButFreq1, TRUE );
			EnableWindow( m_hwndIntFreq, TRUE );

			if( chkFreq )
			{
				EnableWindow( m_hwndButFreq2, TRUE );
				EnableWindow( m_hwndButFreqInterp, TRUE );
			}
			else
			{
				EnableWindow( m_hwndButFreq2, FALSE );
				EnableWindow( m_hwndButFreqInterp, FALSE );
			}

			EnableWindow( m_hwndEditRatio, FALSE );
			EnableWindow( m_hwndEditBase, FALSE );
			break;
 		case 1: // Quantize
			EnableWindow( m_hwndButFreq1, FALSE );
			EnableWindow( m_hwndButFreq2, FALSE );
			EnableWindow( m_hwndButFreqInterp, FALSE );
			EnableWindow( m_hwndIntFreq, FALSE );
			EnableWindow( m_hwndEditRatio, TRUE );
			EnableWindow( m_hwndEditBase, TRUE );
			break;
		}
		break;
	// *** amplitude ***
	case CMB_XFORMTYPE4:
		switch( selection )
		{
 		case 0: // Transfer function
		case 1: // Remapping
			EnableWindow( m_hwndButAmpl1, TRUE );
			EnableWindow( m_hwndIntAmpl, TRUE );

			if( chkAmpl )
			{
				EnableWindow( m_hwndButAmpl2, TRUE );
				EnableWindow( m_hwndButAmplInterp, TRUE );
			}
			else
			{
				EnableWindow( m_hwndButAmpl2, FALSE );
				EnableWindow( m_hwndButAmplInterp, FALSE );
			}

			break;
 		case 2: // Threshold
 		case 3: // Inverse Threshold
			EnableWindow( m_hwndButAmpl1, TRUE );
			EnableWindow( m_hwndButAmpl2, FALSE );
			EnableWindow( m_hwndButAmplInterp, FALSE );
			EnableWindow( m_hwndIntAmpl, FALSE );
			break;
		}
		break;
	// *** miscellaneous ***
	case CMB_XFORMTYPE5:
		switch( selection )
		{
 		case 0: // BinDelay
			EnableWindow( m_hwndButMisc1, TRUE );
			EnableWindow( m_hwndIntMisc, TRUE );

			if( chkMisc )
			{
				EnableWindow( m_hwndButMisc2, TRUE );
				EnableWindow( m_hwndButMiscInterp, TRUE );
			}
			else
			{
				EnableWindow( m_hwndButMisc2, FALSE );
				EnableWindow( m_hwndButMiscInterp, FALSE );
			}

			EnableWindow( m_hwndEditDly, TRUE );
			break;
 		case 1: // Stereo
			EnableWindow( m_hwndButMisc1, TRUE );
			EnableWindow( m_hwndIntMisc, TRUE );

			if( chkMisc )
			{
				EnableWindow( m_hwndButMisc2, TRUE );
				EnableWindow( m_hwndButMiscInterp, TRUE );
			}
			else
			{
				EnableWindow( m_hwndButMisc2, FALSE );
				EnableWindow( m_hwndButMiscInterp, FALSE );
			}

			EnableWindow( m_hwndEditDly, FALSE );
			break;
 		case 2: // Morphing 1 (interpolazione)
 		case 3: // Morphing 2 (sintesi incrociata)
 		case 4: // Morphing 3 (sintesi "granulare")
			EnableWindow( m_hwndButMisc1, TRUE );
			EnableWindow( m_hwndButMisc2, TRUE );
			EnableWindow( m_hwndButMiscInterp, FALSE );
			EnableWindow( m_hwndIntMisc, FALSE );
			EnableWindow( m_hwndEditDly, FALSE );
			break;
		}
		break;
	}
}
