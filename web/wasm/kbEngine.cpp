//=============================================================================
// Kevin Brandon
// January 2008 / July 2026
//
// kbEngine.cpp -- The Image Scramble engine.
//
// The functions below are transplanted from the 2008 MainScreen.cpp
// (original sources preserved offline). Algorithm bodies are unchanged; the
// only substitutions are:
//   - QueryPerformanceCounter        -> EngineNow() (milliseconds)
//   - mcLogBox / sprintf_s           -> EngineLog / snprintf
//   - m_ImageWindow.Draw + m_Lock    -> EngineOnMove() hook in MoveHoleLowLevel
//   - GetDigitBoxValue(eN) etc.      -> plain parameters
//=============================================================================

#include "kbEngine.h"
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
static double EngineNow( ) { return emscripten_get_now( ); }
#else
#include <time.h>
static double EngineNow( )
{
	struct timespec ts;
	clock_gettime( CLOCK_MONOTONIC, &ts );
	return ts.tv_sec * 1000.0 + ts.tv_nsec / 1000000.0;
}
#endif

// 2026 deviation from the original: the hole was black in 2008; bright
// purple-pink makes it visible against nearly any photograph.
static const kbPixel gHolePixel( 230, 40, 255 );

// GetNewPixelLocationWeighted / GetNewPixelLocation: verbatim from
// MainScreen.cpp (they were already free functions with no framework ties).

int GetNewPixelLocationWeighted( int *x, int *y, int w, int h, int iSuggestion )
{
	static int iLastMove = 0;
	int iRand;
	while( 1 )
	{
		iRand =( int ) ( ( ( double)rand( ) / (double)RAND_MAX ) * 5 )  ;
		switch( iRand )
		{
		case 0:
			// up
			if( *y > 0 && iLastMove != 1 )
			{
				*y = *y - 1;
				iLastMove = iRand;
				return iRand;
			}
			break;
		case 1:
			// down
			if( *y < h - 1 && iLastMove != 0 )
			{
				*y = *y + 1;
				iLastMove = iRand;
				return iRand;
			}
			break;
		case 2:
			// left
			if( *x > 0 && iLastMove != 3 )
			{
				*x = *x - 1;
				iLastMove = iRand;
				return iRand;
			}
			break;
		case 3:
			// right
			if( *x < w - 1 && iLastMove != 2)
			{
				*x = *x + 1;
				iLastMove = iRand;
				return iRand;
			}
			break;
		case 4:
			// iSuggestion
			switch( iSuggestion )
			{
			case 0:
				// up
				if( *y > 0 && iLastMove != 1 )
				{
					*y = *y - 1;
					iLastMove = iSuggestion;
					return iSuggestion;
				}
				break;
			case 1:
				// down
				if( *y < h - 1 && iLastMove != 0 )
				{
					*y = *y + 1;
					iLastMove = iSuggestion;
					return iSuggestion;
				}
				break;
			case 2:
				// left
				if( *x > 0 && iLastMove != 3 )
				{
					*x = *x - 1;
					iLastMove = iSuggestion;
					return iSuggestion;
				}
				break;
			case 3:
				// right
				if( *x < w - 1 && iLastMove != 2)
				{
					*x = *x + 1;
					iLastMove = iSuggestion;
					return iSuggestion;
				}
				break;
			}
			break;
		}
	}

	iLastMove = iRand;
	return iRand;
}

int GetNewPixelLocation( int *x, int *y, int w, int h )
{
	static int iLastMove = 0;
	int iRand;
	while( 1 )
	{
		iRand =( int ) ( ( ( double)rand( ) / (double)RAND_MAX ) * 4 )  ;
		switch( iRand )
		{
		case 0:
			// up
			if( *y > 0 && iLastMove != 1 )
			{
				*y = *y - 1;
				iLastMove = iRand;
				return iRand;
			}
			break;
		case 1:
			// down
			if( *y < h - 1 && iLastMove != 0 )
			{
				*y = *y + 1;
				iLastMove = iRand;
				return iRand;
			}
			break;
		case 2:
			// left
			if( *x > 0 && iLastMove != 3 )
			{
				*x = *x - 1;
				iLastMove = iRand;
				return iRand;
			}
			break;
		case 3:
			// right
			if( *x < w - 1 && iLastMove != 2)
			{
				*x = *x + 1;
				iLastMove = iRand;
				return iRand;
			}
			break;
		}
	}

	iLastMove = iRand;
	return iRand;
}

kbEngine::kbEngine( )
{
	m_bStop = false;
	m_bHaveImage = false;
	m_bFlip = false;
	m_bNewScramble = false;
	m_bDirection = false;
	m_bDraw = true;
	m_bSlow = false;
	m_iDrawEvery = 1;
	m_iCount = 0;
	m_iPixelsSolved = 0;
}

// ResetMaps: the tail of the 2008 OpenNewFile -- fresh identity maps, hole in
// the bottom-right corner.
void kbEngine::ResetMaps( )
{
	m_ScramblerMap.Create( m_Image.Width( ), m_Image.Height( ) );
	m_ImageMap.Create( m_Image.Width( ), m_Image.Height( ) );
	m_HoleLocation.x = m_Image.Width( ) - 1;
	m_HoleLocation.y = m_Image.Height( ) - 1;

	m_Image.SetPixel( m_HoleLocation.x, m_HoleLocation.y, gHolePixel );

	m_bHaveImage = true;

	char msg[ 1024 ];
	snprintf( msg, 1024, "Opened Image: %ix%i", m_Image.Width( ), m_Image.Height( ) );
	EngineLog( msg );
	EngineLog( " " );
}

// LoadRGBA: was GDIToPPM + OpenNewFile; the browser hands us decoded RGBA
// where GDI+ handed the original a Bitmap.
void kbEngine::LoadRGBA( const unsigned char *rgba, int w, int h )
{
	m_bStop = true;
	m_Image.CreatePPM( w, h );

	for( mInt j = 0; j < h; j++ )
	{
		for( mInt i = 0; i < w; i++ )
		{
			const unsigned char *c = rgba + 4 * ( i + j * w );
			m_Image.SetPixel( i, j, kbPixel( c[ 0 ], c[ 1 ], c[ 2 ] ) );
		}
	}

	ResetMaps( );
}

bool kbEngine::LoadPPM( const char *filename )
{
	m_bStop = true;
	if( !m_Image.OpenPPM( filename ) )
	{
		m_bHaveImage = false;
		return false;
	}
	ResetMaps( );
	return true;
}

bool kbEngine::SavePPM( const char *filename )
{
	if( !m_bHaveImage ) return false;
	return m_Image.WritePPM( filename );
}

void kbEngine::Scramble( int nTimes, bool bSwirl, bool bDirection )
{
	if( !m_bHaveImage ) return;

	m_bNewScramble = bSwirl;
	m_bDirection = bDirection;

	int w = m_Image.Width( );
	int h = m_Image.Height( );

	m_bStop = false;

	m_iCount = 0;

	int iSuggestion = 0;
	double dStart = EngineNow( );
	for( int i = 0; i < nTimes; i++ )
	{
		if( m_bStop ) break;

		int newX = m_HoleLocation.x;
		int newY = m_HoleLocation.y;
		if( m_bNewScramble )
		{
			if( 0 == m_iCount % ( m_Image.Height( ) + m_Image.Width( ) ) )
			{
				switch( iSuggestion )
				{
				case 0:
					if( m_bDirection )	iSuggestion = 2;
					else				iSuggestion = 3;
					break;
				case 1:
					if( m_bDirection )	iSuggestion = 3;
					else				iSuggestion = 2;
					break;
				case 2:
					if( m_bDirection )	iSuggestion = 1;
					else				iSuggestion = 0;
					break;
				case 3:
					if( m_bDirection )	iSuggestion = 0;
					else				iSuggestion = 1;
					break;
				}
			}
			MoveHoleLowLevel( GetNewPixelLocationWeighted( &newX, &newY, w, h, iSuggestion ) );

		}
		else
		{
			MoveHoleLowLevel( GetNewPixelLocation( &newX, &newY, w, h ) );
		}
	}
	double dStop = EngineNow( );

	mDouble dTotalSeconds = ( dStop - dStart ) / 1000.0;
	mInt iHours = (mInt) ( dTotalSeconds / 3600 );
	mInt iMin = (mInt) ( ( dTotalSeconds - ( 3600 * iHours ) ) / 60 );
	mDouble dSeconds = ( dTotalSeconds - ( 3600 * iHours ) - 60 * iMin );

	mDouble dAvgDistance = m_ScramblerMap.GetAvgDistance( );
	mInt iMinDistance = m_ScramblerMap.GetMinDistance( );
	mInt iMaxDistance = m_ScramblerMap.GetMaxDistance( );
	char Message[ 1024 ];

	snprintf( Message, 1024, "%i Scrambles in %i:%i:%.3f", m_iCount, iHours, iMin, dSeconds );
	EngineLog( Message );
	snprintf( Message, 1024, "Pixel Distance: Avg: %.3f, Max: %i, Min: %i", dAvgDistance, iMaxDistance, iMinDistance );
	EngineLog( Message );
	EngineLog( " " );
}

void kbEngine::MoveHole( kbPoint point, mBool bXFirst )
{
	kbPoint HoleStart;
	HoleStart.x = m_HoleLocation.x;
	HoleStart.y = m_HoleLocation.y;

	if( bXFirst )
	{
		for( int i = 0; i < abs(HoleStart.x - point.x); i++ )
		{
			if( HoleStart.x > point.x )	MoveHoleLeft( );
			else						MoveHoleRight( );
		}

		for( int j = 0; j < abs( HoleStart.y - point.y); j++ )
		{
			if( HoleStart.y > point.y )	MoveHoleDown( );
			else						MoveHoleUp( );
		}
	}
	else
	{
		for( int j = 0; j < abs( HoleStart.y - point.y); j++ )
		{
			if( HoleStart.y > point.y )	MoveHoleDown( );
			else						MoveHoleUp( );
		}
		for( int i = 0; i < abs(HoleStart.x - point.x); i++ )
		{
			if( HoleStart.x > point.x )	MoveHoleLeft( );
			else						MoveHoleRight( );
		}
	}
}

void kbEngine::MoveHoleUp( )
{
	MoveHoleLowLevel( 1 );
}

void kbEngine::MoveHoleDown( )
{
	MoveHoleLowLevel( 0 );
}

void kbEngine::MoveHoleLeft( )
{
	MoveHoleLowLevel( 2 );
}

void kbEngine::MoveHoleRight( )
{
	MoveHoleLowLevel( 3 );
}

void kbEngine::MovePointUp( kbPoint *point )
{
	if( point->y == m_Image.Height( ) - 1 ) return;
	if( point->x == m_HoleLocation.x )
	{
		if( !m_bFlip )
		{
			if( m_HoleLocation.x == m_Image.Width( ) - 1 )	MoveHoleLeft( );
			else											MoveHoleRight( );
		}
		else
		{
			if( m_HoleLocation.x == 0 )	MoveHoleRight( );
			else						MoveHoleLeft( );
		}
	}
	MoveHole( kbPoint( point->x, point->y + 1 ), false );
	MoveHoleDown( );
	point->y++;
}

void kbEngine::MovePointDown( kbPoint *point )
{
	if( point->y == 0 ) return;
	if( point->x == m_HoleLocation.x )
	{
		if( !m_bFlip )
		{
			if( m_HoleLocation.x == m_Image.Width( ) - 1 )	MoveHoleLeft( );
			else											MoveHoleRight( );
		}
		else
		{
			if( m_HoleLocation.x == 0 )	MoveHoleRight( );
			else						MoveHoleLeft( );
		}
	}
	MoveHole( kbPoint( point->x, point->y - 1 ), false );
	MoveHoleUp( );
	point->y--;
}

void kbEngine::MovePointLeft( kbPoint *point )
{
	if( point->x == 0 ) return;
	if( point->y == m_HoleLocation.y )
	{
		if( !m_bFlip )
		{
			if( m_HoleLocation.y == m_Image.Height( ) - 1 )	MoveHoleDown( );
			else											MoveHoleUp( );
		}
		else
		{
			if( m_HoleLocation.y == 0 )	MoveHoleUp( );
			else						MoveHoleDown( );
		}
	}
	MoveHole( kbPoint( point->x - 1, point->y ), true );
	MoveHoleRight( );
	point->x--;
}

void kbEngine::MovePointRight( kbPoint *point )
{
	if( point->x == m_Image.Width( ) - 1 ) return;
	if( point->y == m_HoleLocation.y )
	{
		if( !m_bFlip )
		{
			if( m_HoleLocation.y == m_Image.Height( ) - 1 )	MoveHoleDown( );
			else											MoveHoleUp( );
		}
		else
		{
			if( m_HoleLocation.y == 0 )	MoveHoleUp( );
			else						MoveHoleDown( );
		}
	}
	MoveHole( kbPoint( point->x + 1, point->y ), true );
	MoveHoleLeft( );
	point->x++;
}

void kbEngine::MovePoint( kbPoint start, kbPoint end )
{
	if( start.x < 0 || start.x > m_Image.Width( ) - 1 ) return;
	if( start.y < 0 || start.y > m_Image.Height( ) - 1 ) return;
	if( end.x < 0 || end.x > m_Image.Width( ) - 1 ) return;
	if( end.y < 0 || end.y > m_Image.Height( ) - 1 ) return;

	kbPoint currentPoint = start;

	for( int i = 0; i < abs(start.x - end.x); i++ )
	{
		if( start.x > end.x )	MovePointLeft( &currentPoint );
		else					MovePointRight( &currentPoint );
	}

	for( int j = 0; j < abs( start.y - end.y); j++ )
	{
		if( start.y > end.y )	MovePointDown( &currentPoint );
		else					MovePointUp( &currentPoint );
	}
}

void kbEngine::Solve( )
{
	kbPoint point, rightPoint, leftPoint, topPoint, bottomPoint;

	if( !m_bHaveImage ) return;

	m_bFlip = false;

	m_bStop = false;
	m_iCount = 0;
	m_iPixelsSolved = 0;

	double dStart = EngineNow( );

	for( mInt j = 0; j < m_Image.Height( ) - 2; j++ )
	{
		if( m_bStop ) break;
		for( mInt i = 0; i < m_Image.Width( ) - 2; i++ )
		{
			if( m_bStop ) break;
			point = m_ImageMap.GetPoint( i, j );
			if( point.x == i && point.y == j ) { m_iPixelsSolved++; continue; };
			if( point.y < j + 2 )
			{
				MovePointUp( &point );
				MovePointUp( &point );
			}
			MovePoint( point, kbPoint( i, j ) );

			m_iPixelsSolved++;
		}

		// move right pixel out of the way
		rightPoint = m_ImageMap.GetPoint( m_Image.Width( ) - 1, j );
		MovePointUp( &rightPoint );
		MovePointUp( &rightPoint );

		// move left pixel out of the way
		leftPoint = m_ImageMap.GetPoint( m_Image.Width( ) - 2, j );
		MovePointUp( &leftPoint );
		MovePointUp( &leftPoint );

		// move left pixel into position
		MovePoint( leftPoint, kbPoint( m_Image.Width( ) - 1, j ) );

		// move right pixel out of the way
		rightPoint = m_ImageMap.GetPoint( m_Image.Width( ) - 1, j );
		MovePointUp( &rightPoint );
		MovePointUp( &rightPoint );

		// move right pixel into position
		MovePoint( rightPoint, kbPoint( m_Image.Width( ) - 1, j + 1 ) );

		// place both of them...
		MoveHoleLeft( );
		MoveHoleDown( );
		MoveHoleDown( );
		MoveHoleRight( );
		MoveHoleUp( );

		m_iPixelsSolved += 2;
	}


	for( mInt i = 0; i < m_Image.Width( ) - 1; i++ )
	{
		if( m_bStop ) break;

		// move top pixel out of the way
		topPoint = m_ImageMap.GetPoint( i, m_Image.Height( ) - 2 );
		MovePointRight( &topPoint );
		MovePointRight( &topPoint );
		MovePointRight( &topPoint );

		// move bottom pixel into postion
		bottomPoint = m_ImageMap.GetPoint( i, m_Image.Height( ) - 1 );
		MovePoint( bottomPoint, kbPoint( i, m_Image.Height( ) - 2 ) );

		MoveHoleRight( );

		// move top pixel into postion
		topPoint = m_ImageMap.GetPoint( i, m_Image.Height( ) - 2 );
		MovePoint( topPoint, kbPoint( i + 1, m_Image.Height( ) - 2 ) );

		MoveHoleUp( );
		if( m_HoleLocation.x == i + 2 )		MoveHoleLeft( );
		MoveHoleLeft( );
		MoveHoleDown( );
		MoveHoleRight( );

		m_iPixelsSolved += 2;
	}

	MoveHoleUp( );

	m_iPixelsSolved++;

	double dStop = EngineNow( );

	mDouble dTotalSeconds = ( dStop - dStart ) / 1000.0;
	mInt iHours = (mInt) ( dTotalSeconds / 3600 );
	mInt iMin = (mInt) ( ( dTotalSeconds - ( 3600 * iHours ) ) / 60 );
	mDouble dSeconds = ( dTotalSeconds - ( 3600 * iHours ) - 60 * iMin );

	char Message[ 1024 ];

	snprintf( Message, 1024, "Solved %i pixels in %i:%i:%.3f with %i moves", m_iPixelsSolved, iHours, iMin, dSeconds, m_iCount );
	EngineLog( Message );
	EngineLog( " " );

}

void kbEngine::FlipSolve( )
{
	kbPoint point, rightPoint, leftPoint, topPoint, bottomPoint;

	if( !m_bHaveImage ) return;

	m_bFlip = true;

	m_bStop = false;
	m_iCount = 0;
	m_iPixelsSolved = 0;

	double dStart = EngineNow( );

	for( mInt j = 0; j < m_Image.Height( ) - 2; j++ )
	{
		if( m_bStop ) break;
		for( mInt i = 0; i < m_Image.Width( ) - 2; i++ )
		{
			if( m_bStop ) break;
			point = m_ImageMap.GetPoint( i, j );
			if( point.x == m_Image.Width( ) - i - 1 && point.y == m_Image.Height( ) - j - 1 ) { m_iPixelsSolved++; continue; };

			if( point.y > m_Image.Height( ) - j - 3 )
			{
				MovePointDown( &point );
				MovePointDown( &point );
			}

			MovePoint( point, kbPoint( m_Image.Width( ) - i - 1, m_Image.Height( ) - j - 1 ) );

			m_iPixelsSolved++;
		}

		// move right pixel out of the way
		rightPoint = m_ImageMap.GetPoint( m_Image.Width( ) - 1, j );
		MovePointDown( &rightPoint );
		MovePointDown( &rightPoint );

		// move left pixel out of the way
		leftPoint = m_ImageMap.GetPoint( m_Image.Width( ) - 2, j );
		MovePointDown( &leftPoint );
		MovePointDown( &leftPoint );

		// move left pixel into position
		MovePoint( leftPoint, kbPoint( 0, m_Image.Height( ) - j - 1 ) );

		// move right pixel out of the way
		rightPoint = m_ImageMap.GetPoint( m_Image.Width( ) - 1, j );
		MovePointDown( &rightPoint );
		MovePointDown( &rightPoint );

		// move right pixel into position
		MovePoint( rightPoint, kbPoint( 0, m_Image.Height( ) - j - 2 ) );

		// place both of them...
		MoveHoleRight( );
		MoveHoleUp( );
		MoveHoleUp( );
		MoveHoleLeft( );
		MoveHoleDown( );

		m_iPixelsSolved += 2;
	}

	for( mInt i = 0; i < m_Image.Width( ) - 1; i++ )
	{
		if( m_bStop ) break;

		// move top pixel out of the way
		topPoint = m_ImageMap.GetPoint( i, m_Image.Height( ) - 2 );
		MovePointLeft( &topPoint );
		MovePointLeft( &topPoint );
		MovePointLeft( &topPoint );

		// move bottom pixel into postion
		bottomPoint = m_ImageMap.GetPoint( i, m_Image.Height( ) - 1 );
		MovePoint( bottomPoint, kbPoint( m_Image.Width( ) - i - 1, 1 ) );

		MoveHoleLeft( );

		// move top pixel into postion
		topPoint = m_ImageMap.GetPoint( i, m_Image.Height( ) - 2 );
		MovePoint( topPoint, kbPoint( m_Image.Width( ) - i - 2, 1 ) );

		MoveHoleDown( );
		if( m_HoleLocation.x == m_Image.Width( ) - 1 - i - 2 )		MoveHoleRight( );
		MoveHoleRight( );
		MoveHoleUp( );
		MoveHoleLeft( );

		m_iPixelsSolved += 2;
	}

	MoveHoleDown( );

	m_iPixelsSolved++;

	double dStop = EngineNow( );

	mDouble dTotalSeconds = ( dStop - dStart ) / 1000.0;
	mInt iHours = (mInt) ( dTotalSeconds / 3600 );
	mInt iMin = (mInt) ( ( dTotalSeconds - ( 3600 * iHours ) ) / 60 );
	mDouble dSeconds = ( dTotalSeconds - ( 3600 * iHours ) - 60 * iMin );

	char Message[ 1024 ];

	snprintf( Message, 1024, "Flip Solved %i pixels in %i:%i:%.3f with %i moves", m_iPixelsSolved, iHours, iMin, dSeconds, m_iCount );
	EngineLog( Message );
	EngineLog( " " );
}

void kbEngine::MoveHoleLowLevel( int iDirection )
{
	kbPoint newHoleLocation = m_HoleLocation;

	switch( iDirection )
	{
	case 0:
		// down
		if( m_HoleLocation.y == 0 ) return;
		newHoleLocation.y--;
		break;
	case 1:
		// up
		if( m_HoleLocation.y == m_Image.Height( ) - 1 ) return;
		newHoleLocation.y++;
		break;
	case 2:
		// left
		if( m_HoleLocation.x == 0 ) return;
		newHoleLocation.x--;
		break;
	case 3:
		// right
		if( m_HoleLocation.x == m_Image.Width( ) - 1 ) return;
		newHoleLocation.x++;
		break;
	}

	kbPoint newPoint = m_ScramblerMap.GetPoint( newHoleLocation.x, newHoleLocation.y );
	kbPoint oldPoint = m_ScramblerMap.GetPoint( m_HoleLocation.x, m_HoleLocation.y );
	m_ScramblerMap.SetPoint( m_HoleLocation.x, m_HoleLocation.y, newPoint );
	m_ScramblerMap.SetPoint( newHoleLocation.x, newHoleLocation.y, oldPoint );

	m_ImageMap.SetPoint( newPoint.x, newPoint.y, m_HoleLocation );
	m_ImageMap.SetPoint( oldPoint.x, oldPoint.y, newHoleLocation );

	kbPixel newPixel = m_Image.GetPixel( newHoleLocation.x, newHoleLocation.y );

	m_Image.SetPixel( m_HoleLocation.x, m_HoleLocation.y, newPixel );
	m_Image.SetPixel( newHoleLocation.x, newHoleLocation.y, gHolePixel );
	m_HoleLocation = newHoleLocation;

	if( m_bDraw && ( m_iCount % (mUInt) m_iDrawEvery ) == 0 )
	{
		// 2026: EngineOnMove repaints the canvas and yields to the browser
		// event loop -- standing in for m_ImageWindow.Draw( ) under m_Lock,
		// and for Sleep( 500 ) when Slow is checked.
		EngineOnMove( this );
	}
	m_iCount++;
}

void kbEngine::StupidSolve( )
{
	kbPoint point;

	if( !m_bHaveImage ) return;

	m_bFlip = false;

	m_bStop = false;
	m_iCount = 0;
	m_iPixelsSolved = 0;

	double dStart = EngineNow( );

	for( mInt j = 0; j < m_Image.Height( ); j++ )
	{
		if( m_bStop ) break;
		for( mInt i = 0; i < m_Image.Width( ); i++ )
		{
			if( m_bStop ) break;
			if( j == m_Image.Height( ) - 1 && i == m_Image.Width( ) - 1 ) break;
			point = m_ImageMap.GetPoint( i, j );

			MovePoint( point, kbPoint( i, j ) );

			m_iPixelsSolved++;
		}
	}

	double dStop = EngineNow( );

	mDouble dTotalSeconds = ( dStop - dStart ) / 1000.0;
	mInt iHours = (mInt) ( dTotalSeconds / 3600 );
	mInt iMin = (mInt) ( ( dTotalSeconds - ( 3600 * iHours ) ) / 60 );
	mDouble dSeconds = ( dTotalSeconds - ( 3600 * iHours ) - 60 * iMin );

	char Message[ 1024 ];

	snprintf( Message, 1024, "Stupid Solved %i pixels in %i:%i:%.3f with %i moves", m_iPixelsSolved, iHours, iMin, dSeconds, m_iCount );
	EngineLog( Message );
	EngineLog( " " );
}
