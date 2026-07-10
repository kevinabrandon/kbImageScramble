/**
 *	\file kbPPMImage.h
 *	\brief Defines the kbPPMImage class.
 *	\author Kevin Brandon, brandonk@uci.edu
 *	\date April 15, 2006
 */

#ifndef _KBPPMIMAGE_H_
#define _KBPPMIMAGE_H_

#include <stdio.h>

/// A simple pixel.
struct kbPixel
{
	
    kbPixel() { r = 0; g = 0; b = 0; }	///< Default constructor.
    kbPixel( unsigned char r_, unsigned char g_, unsigned char b_ ) { r = r_; g = g_; b = b_; }	///< Constructor.
	
	unsigned char r;	///< Red channel.
	unsigned char g;	///< Green channel.
	unsigned char b;	///< Blue channel.
};

/// A simple image class using the PPM format.
class kbPPMImage
{
public:
	
    inline kbPPMImage( int x_res, int y_res );	///< Builds an empty image.
	inline kbPPMImage( ) { m_iWidth = 0; m_iHeight = 0; m_Pixels = NULL; }	///< Default constructor.
    inline ~kbPPMImage()						///< Defualt destructor.
	{ 
		delete[] m_Pixels; 
	}

	inline bool OpenPPM( const char *file_name );		///< Opens a PPM image.
    inline bool WritePPM( const char *file_name );		///< Writes a PPM image.
	inline bool CreatePPM( int w, int h );				///< Create an empty PPM image.
	inline kbPixel GetPixel( int x, int y );			///< Get a specific Pixel.
	inline kbPixel GetPixel( int i );					///< Get a specific Pixel.
	inline void	SetPixel( int x, int y, kbPixel p  );	///< Sets a specific Pixel.
	inline void SetPixel( int i, kbPixel p );			///< Sets a specific Pixel.
	int Width( ) { return m_iWidth; };
	int Height( ) { return m_iHeight; };
	unsigned char * GetRawPixels( ) { return (unsigned char *) m_Pixels; };
	
private:
    kbPixel *m_Pixels;		///< The pixel array (the image data).
    int    m_iWidth;		///< Width of the image.
    int    m_iHeight;		///< Height of the image.
};

inline kbPPMImage::kbPPMImage( int x_res, int y_res )
{
    m_iWidth  = x_res;
    m_iHeight = y_res;
    m_Pixels = new kbPixel[ m_iWidth * m_iHeight ];
	if( m_Pixels == NULL ) 
	{
		printf( "error creating ppm image!\n" );
		return;
	}
    for( int i = 0; i < m_iWidth * m_iHeight; i++ ) m_Pixels[ i ] = kbPixel( 0, 0, 0 );
}

inline bool kbPPMImage::OpenPPM( const char *file_name )
{
	FILE *fp;
	fopen_s( &fp, file_name, "rb" );
	if( fp == NULL ) 
	{	
		printf( "error couldn't open file: %s\n", file_name );
		return false;
	}

	fscanf_s( fp, "P6\n" );
	
	long position = ftell( fp );

	if( fgetc( fp ) == '#' )	while( fgetc( fp ) != '\n' );	
	else						fseek( fp, position, 0 );
		
	int value = fscanf_s( fp, "%d %d\n255\n", &m_iWidth, &m_iHeight );
	if( value != 2 )
	{
		printf( "error reading image file: %s\n", file_name );
		return false;
	}
    
	if( m_iWidth <= 0 || m_iHeight <= 0 )		return false;
	
	kbPixel *p = m_Pixels;
	if( p != NULL ) 
	{
		delete [] m_Pixels;
		m_Pixels = NULL;
	}
	p = m_Pixels = new kbPixel[ m_iWidth * m_iHeight ];
	if( p == NULL ) 
	{
		printf( "Error creating image: %s\n", file_name );
		return false;
	}
	for( int i = 0; i < m_iWidth * m_iHeight; i++ )
	{
		if( value = fscanf_s( fp, "%c%c%c", &p->r, 1, &p->g, 1, &p->b, 1 ) != 3 )
		{
			printf( "error reading ppm image: %i, %i\n", i, value );
			break;
		}
		p++;
	}
	fclose( fp );
	return true;
}

inline bool kbPPMImage::CreatePPM( int w, int h )
{
	m_iWidth = w;
	m_iHeight = h;

	if( m_iWidth <= 0 || m_iHeight <= 0 )		return false;
	
	kbPixel *p = m_Pixels;
	if( p != NULL ) 
	{
		delete [] m_Pixels;
		m_Pixels = NULL;
	}
	p = m_Pixels = new kbPixel[ m_iWidth * m_iHeight ];
	if( p == NULL ) 
	{
		printf( "Error creating image: (%i,%i)\n", w, h );
		return false;
	}
	return true;
}

inline kbPixel kbPPMImage::GetPixel( int x, int y )
{
	if( x < 0 || x > m_iWidth ) return kbPixel( 0, 0, 0 );
	if( y < 0 || y > m_iHeight ) return kbPixel( 255, 255, 255 );	

	int index = x + m_iWidth * y;
	return kbPixel(	m_Pixels[ index ].r, 
					m_Pixels[ index ].g, 
					m_Pixels[ index ].b );
}

inline kbPixel kbPPMImage::GetPixel( int i )
{
	if( i < 0 ) return kbPixel( 0, 0, 0 );
	if( i > m_iWidth * m_iHeight ) return kbPixel( 255, 255, 255 );	

	return kbPixel(	m_Pixels[ i ].r, 
					m_Pixels[ i ].g, 
					m_Pixels[ i ].b );
}

inline void	kbPPMImage::SetPixel( int x, int y, kbPixel p )
{
	if( x < 0 || x > m_iWidth ) return;
	if( y < 0 || y > m_iHeight ) return;

	int index = x + m_iWidth * y;
	m_Pixels[ index ].r = p.r;
	m_Pixels[ index ].g = p.g;
	m_Pixels[ index ].b = p.b;
}

inline void	kbPPMImage::SetPixel( int i, kbPixel p )
{
	if( i < 0 ) return;
	if( i > m_iWidth * m_iHeight ) return;	

	m_Pixels[ i ].r = p.r;
	m_Pixels[ i ].g = p.g;
	m_Pixels[ i ].b = p.b;
}

inline bool kbPPMImage::WritePPM( const char *file_name )
    {
    kbPixel *p = m_Pixels;
    FILE  *fp;
	fopen_s( &fp, file_name, "w+b" );
    if( fp == NULL ) return false; 
    fprintf( fp, "P6\n%d %d\n255\n", m_iWidth, m_iHeight );
    for( int i = 0; i < m_iWidth * m_iHeight; i++ )
        {
        fprintf( fp, "%c%c%c", p->r, p->g, p->b );
        p++;
        }
    fclose( fp );
    return true;
    }


#endif
