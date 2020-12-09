#ifndef _CURVE_HPP_
#define _CURVE_HPP_

#ifndef _MATH_H_INC_
#include <math.h>
#define _MATH_H_INC_
#endif // _MATH_H_INC_

#define MAX_CURVE_PARAMS	10
#define MAX_CURVE_PTS		200
#define PIGRECO		3.1415926535897932384626433832795f

typedef enum
{
	CT_LINEAR		= 0,
	CT_EXP			= 1,
	CT_POW			= 2,
	CT_CONDENS		= 3,
	CT_DISPERS		= 4,
	CT_USER			= 5, // user curves
	CT_USERCOS		= 6,
	CT_EXPREV		= 7, // reversed curves
	CT_POWREV		= 8,
	CT_CONDREV		= 9,
	CT_DISPREV		= 10,
	CT_SINUS		= 11,
	NUM_CURVE_TYPES	= 12
} CurveType;

class Curve {
public:
	Curve( void )
	{
		// default: retta y = mx + q con m = 1, q = 0 (cioe' y = x)
		m_Type = CT_LINEAR;

		m_p[ 0 ] = 1.0f;
		m_p[ 1 ] = 0.0f;

		// per avere un settaggio di default anche in caso di CT_USER
		m_nPoints = 2;
		m_x[ 0 ] = 0.0f;	m_y[ 0 ] = 0.0f;
		m_x[ 1 ] = 1.0f;	m_y[ 1 ] = 1.0f;
	}

	void Init( void )
	{
		m_Type = CT_LINEAR;

		m_p[ 0 ] = 1.0f;
		m_p[ 1 ] = 0.0f;

		// per avere un settaggio di default anche in caso di CT_USER
		m_nPoints = 2;
		m_x[ 0 ] = 0.0f;	m_y[ 0 ] = 0.0f;
		m_x[ 1 ] = 1.0f;	m_y[ 1 ] = 1.0f;
	}

	// valore normalizzato tra 0 e 1 (sia per x che per y)
	float NormValue( float x )
	{
		// clip (non dovrebbe succedere, pero'...)
		if( x < 0.0f ) x = 0.0f;
		if( x > 1.0f ) x = 1.0f;

		// curva reversed ?
		if( ( m_Type >= CT_EXPREV ) && ( m_Type <= CT_DISPREV ) )
			x = 1.0f - x;

		switch( m_Type )
		{
		case CT_LINEAR:
			{
				float y = m_p[ 0 ] * x + m_p[ 1 ];
				// clip in uscita
				if( y < 0.0f )
					return 0.0f;
				else if( y > 1.0f )
					return 1.0f;
				else
					return y;
			}
			break;
		case CT_EXP:
		case CT_EXPREV:
			// gia' normalizzata in uscita
			return (float)( ( exp( (double)x * (double)m_p[ 0 ] ) - 1.0f ) / ( exp( (double)m_p[ 0 ] ) - 1.0f ) );
			break;
		case CT_POW:
		case CT_POWREV:
			// gia' normalizzata in uscita
			//return (float)( ( pow( (double)m_p[ 0 ], (double)x ) - 1.0f ) / ( (double)m_p[ 0 ] - 1.0f ) );
			return (float)pow( (double)x, (double)m_p[ 0 ] );
			break;
		case CT_CONDENS:
		case CT_CONDREV:
			// gia' normalizzata in uscita
			{
				double a = (double)m_p[ 0 ];
				double b = (double)m_p[ 1 ];
				double prod = a * b;
				double denom1 = exp( -b * ( 1.0f - a ) ) - exp( prod );
				double denom2 = exp( b * ( 1.0f - a ) ) - exp( -prod );
				double num1 = exp( -b * ( x - a ) ) - exp( prod );
				double num2 = exp( b * ( x - a ) ) - exp( -prod );

				return (float)( a * ( num1 / denom1 ) + ( 1.0f - a ) * ( num2 / denom2 ) );
			}
			break;
		case CT_DISPERS:
		case CT_DISPREV:
			// gia' normalizzata in uscita
			{
				double a = ( m_Type == CT_DISPERS ) ? (double)m_p[ 0 ] : (double)( 1.0 - m_p[ 0 ] );
				double b = (double)m_p[ 1 ];
				double prod = a * b;
				double denom = atan( b * ( a - 1.0f ) ) - atan( prod );
				double num = atan( b * ( a - x ) ) - atan( prod );

				return (float)( num / denom );
			}
			break;
		case CT_USER:
		case CT_USERCOS:
			if( m_nPoints == 0 )
				return 0.0f;
			else
			{
				long i;

				// N.B.: i punti si suppongono ordinati per ascissa (e normalizzati)!!!
				for( i = 0; i < m_nPoints - 1; i++ )
				{
					if( ( x >= m_x[ i ] ) && x < m_x[ i + 1 ] )
						break;
				}

				long next = ( ( i == m_nPoints - 1 ) ? i : i + 1 );
				float t;
				if( m_x[ next ] == m_x[ i ] )
					t = 0.0f;
				else
					t = ( x - m_x[ i ] ) / ( m_x[ next ] - m_x[ i ] );

				if( m_Type == CT_USER )
					return ( m_y[ i ] + t * ( m_y[ next ] - m_y[ i ] ) );
				else
					return ( m_y[ i ] + 0.5f * ( 1.0f - cos ( PIGRECO * t ) ) * ( m_y[ next ] - m_y[ i ] ) );
			}
			break;
		case CT_SINUS:
			return (float)( 0.5f * sin( 2.0 * PIGRECO * (double)x * (double)m_p[ 0 ] + (double)m_p[ 1 ] ) + 0.5f );
			break;
		default:
			return 0.0f;
		}
	}

	// curva con un range di x = 0..m_xScale e y = 0..m_yScale
	float ScaledValue( float x )
	{
		return NormValue( x / m_xScale ) * m_yScale;
	}

	void AddPoint( float x, float y )
	{
		if( ( m_Type != CT_USER ) && ( m_Type != CT_USERCOS ) )
			return;

		if( m_nPoints < MAX_CURVE_PTS )
		{
			m_x[ m_nPoints ] = x;
			m_y[ m_nPoints ] = y;
			m_nPoints++;
			Sort();
		}
	}

	void RemovePoint( unsigned long index )
	{
		if( ( m_Type != CT_USER ) && ( m_Type != CT_USERCOS ) )
			return;

		if( ( index < m_nPoints - 1 ) && ( index > 0 ) )
		{
			unsigned long i;
			for( i = index + 1; i < m_nPoints; i++ )
			{
				m_x[ i - 1 ] = m_x[ i ];
				m_y[ i - 1 ] = m_y[ i ];
			}
			m_nPoints--;
		}
	}

	void SetPoint( unsigned long i, float x, float y )
	{
		if( ( ( m_Type == CT_USER ) || ( m_Type == CT_USERCOS ) ) &&
			i < m_nPoints )
		{
			// clip
			if( x < 0.0f ) x = 0.0f;
			if( x > 1.0f ) x = 1.0f;
			if( y < 0.0f ) y = 0.0f;
			if( y > 1.0f ) y = 1.0f;

			m_x[ i ] = x;
			m_y[ i ] = y;
			Sort();
		}
	}

	void ChangeType( CurveType type )
	{
		m_Type = type;
		ParamCheck();
	}

	void SetParam( unsigned long i, float value )
	{
		if( i < MAX_CURVE_PARAMS )
		{
			m_p[ i ] = value;
			ParamCheck();
		}
	}

	float GetParam( unsigned long i )
	{
		if( i < MAX_CURVE_PARAMS )
			return m_p[ i ];
		else
			return 0.0f;
	}

	void CopyTo( Curve* pCurve )
	{
		pCurve->m_Type = m_Type;

		unsigned long i;
		for( i = 0; i < MAX_CURVE_PARAMS; i++ )
			pCurve->m_p[ i ] = m_p[ i ];

		pCurve->m_nPoints = m_nPoints;

		for( i = 0; i < m_nPoints; i++ )
		{
			pCurve->m_x[ i ] = m_x[ i ];
			pCurve->m_y[ i ] = m_y[ i ];
		}
		pCurve->m_xScale = m_xScale;
		pCurve->m_yScale = m_yScale;
	}

	CurveType m_Type;

	// per curve USER:
	unsigned long m_nPoints;
	float m_x[ MAX_CURVE_PTS ];
	float m_y[ MAX_CURVE_PTS ];
	float m_xScale;
	float m_yScale;

protected:
	void Sort( void )
	{
		if( ( m_Type != CT_USER ) && ( m_Type != CT_USERCOS ) )
			return;

		long i, j;

		for( i = 0; i < m_nPoints; i++ )
		{
			for( j = i; j < m_nPoints; j++ )
			{
				if( m_x[ i ] > m_x[ j ] )
				{
					float tmpx = m_x[ j ];
					float tmpy = m_y[ j ];

					m_x[ j ] = m_x[ i ];
					m_y[ j ] = m_y[ i ];

					m_x[ i ] = tmpx;
					m_y[ i ] = tmpy;
				}
			}
		}
	}

	void ParamCheck( void )
	{
		float sign;
		// clip dei parametri
		switch( m_Type )
		{
		case CT_LINEAR:
			break;
		case CT_EXP:
		case CT_EXPREV:
			if( fabs( m_p[ 0 ] ) <= 0.000001f )
			{
				sign = ( m_p[ 0 ] < 0.0f ? -1.0f : 1.0f );
				m_p[ 0 ] = 0.000001f * sign;
			}
			break;
		case CT_POW:
		case CT_POWREV:
			if( m_p[ 0 ] < 0.0f )
				m_p[ 0 ] = 0.0f;
			//if( m_p[ 0 ] <= 0.000001f )
			//	m_p[ 0 ] = 0.000001f;
			//if( fabs( 1.0f - m_p[ 0 ] ) <= 0.000001f )
			//{
			//	sign = ( m_p[ 0 ] < 1.0f ? -1.0f : 1.0f );
			//	m_p[ 0 ] = 1.0 + 0.000001f * sign;
			//}
			break;
		case CT_CONDENS:
		case CT_CONDREV:
		case CT_DISPERS:
		case CT_DISPREV:
			if( m_p[ 0 ] <= 0.000001f )
				m_p[ 0 ] = 0.000001f;
			if( m_p[ 0 ] >= 0.999999f )
				m_p[ 0 ] = 0.999999f;
			if( m_p[ 1 ] <= 0.1f )
				m_p[ 1 ] = 0.1f;
			break;
		case CT_USER:
		case CT_USERCOS:
			break;
		case CT_SINUS:
			break;
		default:
			break;
		}
	}

	// per curve parametriche:
	float m_p[ MAX_CURVE_PARAMS ];
};

#endif // _CURVE_HPP_
