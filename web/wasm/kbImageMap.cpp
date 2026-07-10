//
//	kbImageMap.cpp
//

#include "kbImageMap.h"

kbImageMap::kbImageMap( )
{
	m_pMap = NULL;
	m_iWidth = 0;
	m_iHeight = 0;
}

kbImageMap::kbImageMap( int w, int h )
{
	m_pMap = new kbPoint[ w * h ];
	if( m_pMap == NULL )
	{
		m_iWidth = 0;
		m_iHeight = 0;
	}
	else
	{
		m_iWidth = w;
		m_iHeight = h;
		InitMap( );
	}
}

void kbImageMap::Create( int w, int h )
{
	if( m_pMap != NULL ) delete [] m_pMap;
	m_pMap = new kbPoint[ w * h ];
	if( m_pMap == NULL )
	{
		m_iWidth = 0;
		m_iHeight = 0;
	}
	else
	{
		m_iWidth = w;
		m_iHeight = h;
		InitMap( );
	}
}

void kbImageMap::InitMap( )
{
	if( m_pMap == NULL ) return;
	for( int j = 0; j < m_iHeight; j++ )
	{
		for( int i = 0; i < m_iWidth; i++ )
		{
			SetPoint( i, j, kbPoint( i, j ) );
		}
	}
}

kbPoint kbImageMap::GetPoint( int i )
{
	if( i < 0 )						return kbPoint( 0, 0 );
	if( i >= m_iHeight * m_iWidth )	return kbPoint( m_iWidth - 1, m_iHeight - 1 );
	
	return m_pMap[ i ];
}

kbPoint kbImageMap::GetPoint( int i, int j )
{
	kbPoint ret;
	if( i < 0 )				return kbPoint( 0, 0 );
	if( i >= m_iWidth )		return kbPoint( m_iWidth - 1, m_iHeight - 1 );
	if( j < 0 )				return kbPoint( 0, 0 );
	if( j >= m_iHeight )	return kbPoint( m_iWidth - 1, m_iHeight - 1 );

	return m_pMap[ i + j * m_iWidth ];
}


void kbImageMap::SetPoint( int i, kbPoint p )
{
	if( i < 0 )						return;
	if( i >= m_iWidth * m_iHeight ) return; 

	m_pMap[ i ] = p;
}

void kbImageMap::SetPoint( int i, int j, kbPoint p )
{
	if( i < 0 )				return;
	if( i >= m_iWidth )		return; 
	if( j < 0 )				return;
	if( j >= m_iHeight )	return;

	m_pMap[ i + j * m_iWidth ] = p;
}

void kbImageMap::PrintOut( char *filename )
{
	if( m_pMap == NULL ) return;
	FILE *out;
	fopen_s( &out, filename, "w" );
	if( out == NULL ) return;

	kbPoint Max = GetMaxPoint( );
	kbPoint Min = GetMinPoint( );

	fprintf( out, "%ix%i\n", m_iWidth, m_iHeight );
	fprintf( out, "Max Point: (%i, %i), Min Point: (%i, %i)\n\n", Max.x, Max.y, Min.x, Min.y );
	fprintf( out, "         " );
	for( int k = 0; k < m_iWidth; k++ )
	{
		fprintf( out, "%4.1i         ", k );
	}
	fprintf( out, "\n" );
	for( int j = 0; j < m_iHeight; j++ )
	{
		fprintf( out, "%4.1i    ", j );
		for( int i = 0; i < m_iWidth; i++ )
		{
			kbPoint p;
			p = GetPoint( i, j );
			fprintf( out, "(%4.1i, %4.1i) ", p.x, p.y );
		}
		fprintf( out, "\n" );
	}
	fclose( out );

}

double kbImageMap::GetAvgDistance( )
{
	double dSum = 0;
	for( int j = 0; j < m_iHeight; j++ )
	{
		for( int i = 0; i < m_iWidth; i++ )
		{
			kbPoint p;
			p = GetPoint( i, j );

			if( p.x == m_iWidth - 1 && p.y == m_iHeight - 1 ) continue;

			int xDistance = abs( i - p.x );
			int yDistance = abs( j - p.y );
			int iDistance = xDistance + yDistance;
			dSum += iDistance;
		}
	}
	return dSum / ( m_iHeight * m_iWidth - 1);
}

int kbImageMap::GetMaxDistance( )
{
	int iMax = 0;
	for( int j = 0; j < m_iHeight; j++ )
	{
		for( int i = 0; i < m_iWidth; i++ )
		{
			kbPoint p;
			p = GetPoint( i, j );

			if( p.x == m_iWidth - 1 && p.y == m_iHeight - 1 ) continue;

			int xDistance = abs( i - p.x );
			int yDistance = abs( j - p.y );
			int iDistance = xDistance + yDistance;
			if( iDistance > iMax ) iMax = iDistance;
		}
	}
	return iMax;
}

int kbImageMap::GetMinDistance( )
{
	int iMin = 100000000;
	for( int j = 0; j < m_iHeight; j++ )
	{
		for( int i = 0; i < m_iWidth; i++ )
		{
			kbPoint p;
			p = GetPoint( i, j );

			int xDistance = abs( i - p.x );
			int yDistance = abs( j - p.y );
			int iDistance = xDistance + yDistance;

			if( iDistance < iMin ) iMin = iDistance;
		}
	}
	return iMin;
}

kbPoint kbImageMap::GetMaxPoint( )
{
	int iMax = 0;
	kbPoint MaxPoint;
	for( int j = 0; j < m_iHeight; j++ )
	{
		for( int i = 0; i < m_iWidth; i++ )
		{
			kbPoint p;
			p = GetPoint( i, j );

			if( p.x == 0 && p.y == 0 ) continue;

			int xDistance = abs( i - p.x );
			int yDistance = abs( j - p.y );
			int iDistance = xDistance + yDistance;
			if( iDistance > iMax ) 
			{
				iMax = iDistance;
				MaxPoint = p;
			}
		}
	}
	return MaxPoint;
}

kbPoint	kbImageMap::GetMinPoint( )
{
	int iMin = 100000000;
	kbPoint MinPoint;
	for( int j = 0; j < m_iHeight; j++ )
	{
		for( int i = 0; i < m_iWidth; i++ )
		{
			kbPoint p;
			p = GetPoint( i, j );

			int xDistance = abs( i - p.x );
			int yDistance = abs( j - p.y );
			int iDistance = xDistance + yDistance;

			if( iDistance < iMin ) 
			{
				iMin = iDistance;
				MinPoint = p;
			}
		}
	}
	return MinPoint;
}

kbPoint kbImageMap::FindPoint( kbPoint p )
{
	for( int j = 0; j < m_iHeight; j++ )
	{
		for( int i = 0; i < m_iWidth; i++ )
		{
			kbPoint CurrentPoint = GetPoint( i, j );
			if( CurrentPoint.x == p.x && CurrentPoint.y == p.y )
			{
				return kbPoint( i, j );	// return where it is
			}
		}
	}
	return kbPoint( 0, 0 );
}


kbPoint kbImageMap::FindPoint( kbPoint p, int x, int y )
{
	if( x < 0 ) x = 0;
	if( y < 0 ) y = 0;

	int index = x + y * m_iWidth;

	for( int i = index; i < m_iWidth * m_iHeight; i++ )
	{
		int iX = i % m_iWidth;
		int iY = i / m_iWidth;

		kbPoint CurrentPoint = GetPoint( i );
		if( CurrentPoint.x == p.x && CurrentPoint.y == p.y )
		{
			return kbPoint( iX, iY );	// return where it is
		}
	}
	return kbPoint( 0, 0 );
}
	
