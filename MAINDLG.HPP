#ifndef _MAINDLG_HPP_
#define _MAINDLG_HPP_

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

typedef enum { WTYPE_BARTLETT=0, WTYPE_HAMMING=1, WTYPE_HANNING=2, WTYPE_BLACKMAN=3 } FFTWindow;

class MainDlg {

public:

	MainDlg( void )
	{
		m_hwndDlg = NULL;

		m_duration = 0.0f;
	}

	~MainDlg( void )
	{}


	bool Show( HINSTANCE hInst )
	{
		s_pThis = this;
		m_hInstance = hInst;
		return( DialogBox( hInst, MAKEINTRESOURCE(DLG_MAIN), NULL, MainDlg::DlgProcS ) != -1 );
	}

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

	bool Analyze( void );
	bool Analyze( char* infile, char* outfile, float* pMaxAmpl, float* pMaxRMS );
	bool Process( void );
	bool Convert( void );
	void OnChangeSel( HWND hwndCombo, WORD idCombo );

///////////////////////////////////////////////////////

	static MainDlg* s_pThis;	        // "trucco" per la DialogProc

	HINSTANCE m_hInstance;

	HWND m_hwndDlg;
	HWND m_hwndCmbFreq;				// combo-box
	HWND m_hwndCmbAmpl;
	HWND m_hwndCmbMisc;
	HWND m_hwndCmbFft;
	HWND m_hwndCmbWin;
	HWND m_hwndCmbIncr;
	HWND m_hwndBypTime; 			// checkbox di bypass
	HWND m_hwndBypFreq;
	HWND m_hwndBypAmpl;
	HWND m_hwndBypMisc;
	HWND m_hwndEditDur; 			// edit
	HWND m_hwndEditDly;
	HWND m_hwndEditRatio;
	HWND m_hwndEditBase;
	HWND m_hwndIntFreq;				// checkbox interpolazione
	HWND m_hwndIntAmpl;
	HWND m_hwndIntMisc;
	HWND m_hwndIntFrames;
	HWND m_hwndPbar;				// progress bar
	HWND m_hwndStatus;				// status text
	HWND m_hwndButFreq1;            // bottoni
	HWND m_hwndButFreq2;
	HWND m_hwndButFreqInterp;
	HWND m_hwndButAmpl1;
	HWND m_hwndButAmpl2;
	HWND m_hwndButAmplInterp;
	HWND m_hwndButMisc1;
	HWND m_hwndButMisc2;
	HWND m_hwndButMiscInterp;

	// curve di trasformazione
	Curve m_curveTime;

	Curve m_curveFreq1;
	Curve m_curveFreq2;
	Curve m_curveFreqInterp;

	Curve m_curveAmpl1;
	Curve m_curveAmpl2;
	Curve m_curveAmplInterp;

	Curve m_curveMisc1;
	Curve m_curveMisc2;
	Curve m_curveMiscInterp;

	// parametri di trasformazione
	float m_duration;						// durata del file in uscita
	float m_quantizeFreq;					// frequenza base per la quantizzazione
	float m_quantizeRatio;					// rapporto di quantizzazione
	unsigned long m_nThrFrames;				// numero di frames per la "Timed Threshold"
	float m_maxDelay;						// max Delay per la Delay vs. Frequency

	// flags
	bool m_bypassTime;
	bool m_bypassFreq;
	bool m_bypassAmpl;
	bool m_bypassMisc;
	bool m_interpFreq;
	bool m_interpAmpl;
	bool m_interpMisc;
	bool m_interpFrames;

	// parametro per l'interpolazione temporale
	float m_t;
};

#endif // _MAINDLG_HPP_
