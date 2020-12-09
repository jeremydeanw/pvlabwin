#ifndef __PV_HPP__
#define __PV_HPP__

#ifndef __STDIO_H_INC__
#include <STDIO.H>
#define __STDIO_H_INC__
#endif

#ifndef __MATH_H_INC__
#include <MATH.H>
#define __MATH_H_INC__
#endif

#ifndef __MEMORY_H_INC__
#include <MEMORY.H>
#define __MEMORY_H_INC__
#endif


struct PVHeader
{
	PVHeader( void ) :
	magic( 0x7E662 ),
	headBsize( sizeof( PVHeader ) ),
	dataFormat( 36 ),
	frameFormat( 7 ),
	minFreq( 0.0f ),
	freqFormat( 1 )
	{}

    long    magic;           // 517730 (0x7E662) for Csound files, 'Erbe' for SoundHack files
    long    headBsize;       // byte offset from start to data
                             // (usually sizeof(SpectHeader))
    long    dataBsize;       // number of bytes of data not including the header
    long    dataFormat;      // (short) format specifier
                             // always 36 for floating point
    float   samplingRate;
    long    channels;
    long    frameSize;       // number of points in FFT (number of bands * 2)
    long    frameIncr;       // number of new samples each frame (frames overlap)
    long    frameBsize;      // bytes in each file frame
                             // frameBsize = sizeof(float) * (frameSize >> 1 + 1) << 1;
    long    frameFormat;     // this is either 3 for SoundHack files (amplitude & phase)
                             // or 7 for Csound files (amplitude & frequency)
    float   minFreq;         // 0.0
    float   maxFreq;         // maxFreq = samplingRate/2.0;
    long    freqFormat;      // flag for log/lin frequency (always 1 for linear)
    char    info[4];         // extendable byte area
};

struct PVBin
{
	PVBin( void ):
	amplitude( 0.0f ),
	frequency( 0.0f )
	{}

    float    amplitude;    // from 0.0 to 1.0
    float    frequency;    // from 0.0 to samplingRate/2.0
};

struct WaveHdr {

	WaveHdr( void )
	{
		riff[ 0 ] = 'R'; riff[ 1 ] = 'I'; riff[ 2 ] = 'F'; riff[ 3 ] = 'F';
		wave[ 0 ] = 'W'; wave[ 1 ] = 'A'; wave[ 2 ] = 'V'; wave[ 3 ] = 'E';
		fmt_[ 0 ] = 'f'; fmt_[ 1 ] = 'm'; fmt_[ 2 ] = 't'; fmt_[ 3 ] = ' ';
		data[ 0 ] = 'd'; data[ 1 ] = 'a'; data[ 2 ] = 't'; data[ 3 ] = 'a';
		len2 = 16;
		wFormatTag = 1;
		wBitsPerSample = 16;
		nChannels = 1;
		nBlockAlign = ( wBitsPerSample / 8 ) * nChannels;
	}

	char riff[ 4 ];
	long len1;	// lunghezza file - 8
	char wave[ 4 ];
	char fmt_[ 4 ];
	long len2;	// = 16 (lunghezza della sezione successiva)
	unsigned short wFormatTag;	// = 1 (PCM)
	unsigned short nChannels;
	unsigned long nSamplesPerSec;
	unsigned long nAvgBytesPerSec;
	unsigned short nBlockAlign;	// ( wBitsPerSample / 8 ) * nChannels
	unsigned short wBitsPerSample;
	char data[ 4 ];
	long len3;	// lunghezza file - 44
};

class PhaseVocoder {
public:
	PhaseVocoder( void )
	{
		m_nFrames = 0;
		m_ppBins = NULL;
	}

	~PhaseVocoder( void )
	{
		if ( m_ppBins != NULL )
		{
			for ( long i = 0; i < m_nFrames; i++ )
			{
				if ( m_ppBins[ i ] != NULL )
				{
					delete[] m_ppBins[ i ];
					m_ppBins[ i ] = NULL;
				}
			}

			delete[] m_ppBins;
			m_ppBins = NULL;
		}
	}

	bool Create( char* filename )
	{
		FILE* fp = NULL;

		fp = fopen( filename, "rb+" );

		if ( fp == NULL )
			return false;

		// leggo l'header
		fread( &m_hdr, sizeof(PVHeader), 1, fp );

		// ricavo il numero totale di frames
		m_nFrames = m_hdr.dataBsize / m_hdr.frameBsize;

		if ( m_nFrames == 0 )
			return false;

		m_ppBins = new PVBin*[ m_nFrames ];

		for ( long i = 0; i < m_nFrames; i++ ) {

			// alloco i bin di un frame...
			m_ppBins[ i ] = new PVBin[ m_hdr.frameSize / 2 + 1 ];

			if( m_ppBins[ i ] == NULL )
				return false;

			// ...e li leggo
			fread( &m_ppBins[ i ][ 0 ], ( m_hdr.frameSize / 2 + 1 )*sizeof(PVBin), 1, fp );
		}

		fclose( fp );

		return true;
	}

	void Destroy( void )
	{
		if ( m_ppBins != NULL )
		{
			for ( long i = 0; i < m_nFrames; i++ )
			{
				if ( m_ppBins[ i ] != NULL )
				{
					delete[] m_ppBins[ i ];
					m_ppBins[ i ] = NULL;
				}
			}

			delete[] m_ppBins;
			m_ppBins = NULL;
		}

		m_nFrames = 0;

		memset( &m_hdr, 0, sizeof( PVHeader ) );
	}

	bool Save( char* filename )
	{
		FILE* fp = NULL;

		fp = fopen( filename, "wb+" );

		if ( fp == NULL )
			return false;

		// scrivo l'header
		fwrite( &m_hdr, sizeof(PVHeader), 1, fp );

		// scrivo i frames
		if ( m_nFrames == 0 )
			return false;

		for ( long i = 0; i < m_nFrames; i++ ) {
			fwrite( &m_ppBins[ i ][ 0 ], ( m_hdr.frameSize / 2 + 1 )*sizeof(PVBin), 1, fp );
		}

		fclose( fp );

		return true;
	}

	void DumpHeader( void )
	{
		printf( "sampling rate %f\n", m_hdr.samplingRate );
		printf( "channels %d\n", m_hdr.channels );
		printf( "frame size %d\n", m_hdr.frameSize );
		printf( "frame incr. %d\n", m_hdr.frameIncr );
		printf( "Total num. of frames %d\n", m_nFrames );
	}

	float MaxAmpl( int frame )
	{
		if ( frame >= m_nFrames )
			return -1.0f;

		float max = 0.0f;

		for ( int i = 0; i < ( m_hdr.frameSize / 2 + 1 ); i++ )
		{
			if( m_ppBins[ frame ][ i ].amplitude >= max )
				max = m_ppBins[ frame ][ i ].amplitude;
		}

		return max;
	}

	float RMS( int frame )
	{
		if ( frame >= m_nFrames )
			return 0.0f;

		float sum = 0.0f;
		float amp = 0.0f;
		for ( int i = 0; i < ( m_hdr.frameSize / 2 + 1 ); i++ )
		{
			amp = m_ppBins[ frame ][ i ].amplitude;
			sum += amp * amp;
		}

		return (float)sqrt( sum / ( m_hdr.frameSize / 2 + 1 ) );
	}

	bool SaveWave( char* filename )
	{
		FILE* fp = NULL;

		fp = fopen( filename, "wb+" );

		if ( fp == NULL )
			return false;
		// Wave header
		WaveHdr wh;
		wh.nSamplesPerSec = (unsigned long)m_hdr.samplingRate;
		wh.nAvgBytesPerSec = wh.nSamplesPerSec * ( wh.wBitsPerSample / 8 ) * wh.nChannels;

		long nSamples = m_nFrames * m_hdr.frameIncr;
		long SamplesSize = nSamples * ( wh.wBitsPerSample / 8 ) * wh.nChannels;
		wh.len1 = SamplesSize + 36;  // SamplesSize + 44 (header) - 8 ("RIFF" + len1)
		wh.len3 = SamplesSize;

		fwrite( &wh, sizeof(WaveHdr), 1, fp );

		short* pBuf = new short[ m_hdr.frameIncr ];

		if ( pBuf == NULL )
			return false;

		float* pPhase = new float[ ( m_hdr.frameSize / 2 + 1 ) ];

		if ( pPhase == NULL )
			return false;

		for( long i = 0; i < ( m_hdr.frameSize / 2 + 1 ); i++ )
			pPhase[ i ] = 0.0f;

		float invsr = 1.0f / m_hdr.samplingRate;

#define PIGRECO 	3.1415926535897932384626433832795f
#define DUEPIGRECO 	6.28318530717958647692528676655901f
#define SINTABLESIZE	8192

		float SinTable[ SINTABLESIZE ];
		for( long j = 0; j < SINTABLESIZE; j++ )
			SinTable[ j ] = sin( -PIGRECO + DUEPIGRECO * (float)j / (float)SINTABLESIZE );

		float ndxconv = (float)SINTABLESIZE / DUEPIGRECO;

		char hb[] = "|/-\\";
		int heartbeat = 0;

		float invIncr = 1.0f / (float)m_hdr.frameIncr;
		float TwopiOnSr = DUEPIGRECO / m_hdr.samplingRate;
		float t = 0.0f;
		float scale = 32768.0f * 2.0f / (float)m_hdr.frameSize;  // ?????????

		for ( long f = 0; f < m_nFrames; f++ )
		{
			if( ( f & 0xF ) == 0 )
			{
				printf( "%c\b", hb[ heartbeat & 0x3 ] );
				heartbeat++;
			}

			long nextf = ( f < m_nFrames - 1 ) ? f + 1 : f;
			float interp = 0.0f;

			for ( long s = 0; s < m_hdr.frameIncr; s++ )
			{
				float sum = 0.0f;

 				PVBin* pBin = &m_ppBins[ f ][ 0 ];
				PVBin* pNextBin = &m_ppBins[ nextf ][ 0 ];
				float * ph = &pPhase[ 0 ];

				//for( long b = 0; b < ( m_hdr.frameSize / 2 + 1 ); b++ )
				for( long b = 1; b < m_hdr.frameSize / 2; b++, ph++ )
				{
					float frq, amp;

					// interpolo sia la frequenza che l'ampiezza col frame successivo
					frq = ( pNextBin->frequency - pBin->frequency ) * interp + pBin->frequency;
					amp = ( pNextBin->amplitude - pBin->amplitude ) * interp + pBin->amplitude;

					//frq = pBin->frequency;
					//amp = pBin->amplitude;

					pBin++;
					pNextBin++;

					// accumulo il contributo del bin
					if( amp != 0.0f )
						//sum += amp * sin( *ph );  // versione senza tabella (piu' lenta!!!)
						sum += amp * SinTable[ (long)( ( *ph + PIGRECO ) * ndxconv ) ];

					// incremento la fase istantanea del bin
					*ph += TwopiOnSr * frq;

					// riporto la fase tra -PIGRECO e PIGRECO
					while( *ph >= PIGRECO )
						*ph -= DUEPIGRECO;
					while( *ph < -PIGRECO )
						*ph += DUEPIGRECO;
				}

				// normalizzazione
				pBuf[ s ] = (short)( sum * scale );

				t += invsr;
				interp += invIncr;
 			}

			fwrite( pBuf, sizeof(short)*m_hdr.frameIncr, 1, fp );
		}

		delete[] pPhase;
		delete[] pBuf;

		fclose( fp );

		return true;
	}

	PVHeader	m_hdr;
	PVBin**		m_ppBins;
	long		m_nFrames;	            // numero totale di frames (comprese le sovrapposizioni)
};

#endif // __PV_HPP__
