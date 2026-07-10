//
//	kbImageMap.h
//

#ifndef KB_IMAGE_MAP_H_
#define KB_IMAGE_MAP_H_

//#include "windows.h"
#include "stdio.h"
#include "math.h"

struct kbPoint
{
    kbPoint() { x = 0; y = 0; };
    kbPoint( int x_, int y_ ) { x = x_; y = y_; };

	int x;
	int y;
};

class kbImageMap
{
	int m_iWidth;
	int m_iHeight;

	kbPoint *m_pMap;

public:
	kbImageMap( );
	kbImageMap( int w, int h );

	void Create( int w, int h );

	void InitMap( );

	void PrintOut( char *filename );

	kbPoint GetPoint( int i );
	kbPoint GetPoint( int i, int j );
	void SetPoint( int i, kbPoint p );
	void SetPoint( int i, int j, kbPoint p );
	kbPoint FindPoint( kbPoint p );
	kbPoint FindPoint( kbPoint p, int x, int y );
	double GetAvgDistance( );
	int GetMaxDistance( );
	int GetMinDistance( );

	kbPoint GetMaxPoint( );
	kbPoint	GetMinPoint( );

};

#endif