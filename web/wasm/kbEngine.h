//=============================================================================
// Kevin Brandon
// January 2008 / July 2026
//
// kbEngine.h -- The Image Scramble engine, extracted from MainScreen.cpp of the
// original 2008 Win32 program so it can run anywhere (here: WebAssembly).
//
// The scramble/solve algorithm methods are transplanted from MainScreen.cpp
// with their bodies intact. Everything that belonged to the MriFramework UI
// layer (Mark Resources, Inc code -- not ported) is replaced by two hooks:
//
//   EngineLog(msg)     <- was mcLogBox::Log
//   EngineOnMove()     <- was m_ImageWindow.Draw / m_Lock / Sleep inside
//                         MoveHoleLowLevel; yields to the browser event loop
//
// The MRI primitive typedefs are reproduced below so the transplanted bodies
// compile verbatim.
//=============================================================================

#ifndef KB_ENGINE_H_
#define KB_ENGINE_H_

#include "kbPPMImage.h"
#include "kbImageMap.h"

typedef bool			mBool;
typedef int				mInt;
typedef unsigned int	mUInt;
typedef double			mDouble;

void EngineLog( const char *msg );
void EngineOnMove( class kbEngine *pEngine );

class kbEngine
{
	kbPPMImage			m_Image;
	kbImageMap			m_ScramblerMap;
	kbImageMap			m_ImageMap;

	mBool				m_bStop;
	mBool				m_bHaveImage;
	mBool				m_bFlip;

	mBool				m_bNewScramble;		// "Swirl" checkbox
	mBool				m_bDirection;		// "Toggle Direction" checkbox

	kbPoint m_HoleLocation;

	public:
	kbEngine( );

	mUInt				m_iCount;
	mInt				m_iPixelsSolved;

	mBool				m_bDraw;			// "ReDraw" checkbox
	mBool				m_bSlow;			// "Slow" checkbox
	mInt				m_iDrawEvery;		// 2026: draw every N moves (1 = original behavior)

	void LoadRGBA( const unsigned char *rgba, int w, int h );	// was OpenNewFile (GDI+ branch)
	bool LoadPPM( const char *filename );						// was OpenNewFile (.ppm branch)
	bool SavePPM( const char *filename );

	// nTimes is double (as the 2008 GetDigitBoxValue was) so counts beyond
	// int32 -- up to 1e10 from the UI -- work.
	void Scramble( double nTimes, bool bSwirl, bool bDirection );
	void Solve( );
	void FlipSolve( );
	void StupidSolve( );

	// 2026: one interactive move (raw MoveHoleLowLevel direction code),
	// bypassing the draw/yield hook so it stays synchronous for UI clicks.
	void MoveHoleManual( int iDirection );

	void Stop( )	{ m_bStop = true; }
	bool HaveImage( )	{ return m_bHaveImage; }
	// Address of the stop flag: lets JS request a stop by writing memory
	// directly, since Asyncify forbids calling exports while suspended.
	bool *StopFlag( )	{ return &m_bStop; }

	int Width( )	{ return m_Image.Width( ); }
	int Height( )	{ return m_Image.Height( ); }
	unsigned char *Pixels( )	{ return m_Image.GetRawPixels( ); }

	// 2026: home position of the pixel currently at (x,y) -- its tile number.
	kbPoint Home( int x, int y )	{ return m_ScramblerMap.GetPoint( x, y ); }

	double AvgDistance( )	{ return m_ScramblerMap.GetAvgDistance( ); }
	int MaxDistance( )		{ return m_ScramblerMap.GetMaxDistance( ); }
	int MinDistance( )		{ return m_ScramblerMap.GetMinDistance( ); }

	private:
	void ResetMaps( );

	void MoveHoleLowLevel( int iDirection );

	void MoveHoleUp( );
	void MoveHoleDown( );
	void MoveHoleLeft( );
	void MoveHoleRight( );
	void MoveHole( kbPoint point, mBool bXFirst );

	void MovePointUp( kbPoint *point );
	void MovePointDown( kbPoint *point );
	void MovePointLeft( kbPoint *point );
	void MovePointRight( kbPoint *point );
	void MovePoint( kbPoint start, kbPoint end );
};

#endif
