//
// msvc_compat.h -- shims for the MSVC-only "_s" CRT functions used by the
// 2008 sources (kbPPMImage.h, kbImageMap.cpp), so those files compile with
// clang/Emscripten completely unmodified. Injected via `emcc -include`.
//

#ifndef MSVC_COMPAT_H_
#define MSVC_COMPAT_H_

#ifndef _MSC_VER

#include <stdio.h>
#include <stdarg.h>
#include <ctype.h>

#define sprintf_s snprintf

static inline int fopen_s( FILE **f, const char *name, const char *mode )
{
	*f = fopen( name, mode );
	return ( *f == NULL ) ? 1 : 0;
}

// Minimal fscanf_s covering the directives the 2008 code uses:
// literal text, whitespace, %d, and %c (which in MSVC's fscanf_s is
// followed by a buffer-size vararg that we consume and ignore).
static inline int fscanf_s( FILE *fp, const char *fmt, ... )
{
	va_list ap;
	va_start( ap, fmt );
	int assigned = 0;

	for( const char *p = fmt; *p; p++ )
	{
		if( isspace( (unsigned char) *p ) )
		{
			int ch;
			while( ( ch = fgetc( fp ) ) != EOF && isspace( ch ) );
			if( ch != EOF ) ungetc( ch, fp );
		}
		else if( *p == '%' )
		{
			p++;
			if( *p == 'd' )
			{
				int *out = va_arg( ap, int * );
				if( fscanf( fp, "%d", out ) != 1 ) break;
				assigned++;
			}
			else if( *p == 'c' )
			{
				unsigned char *out = va_arg( ap, unsigned char * );
				(void) va_arg( ap, unsigned int );	// MSVC buffer-size arg
				int ch = fgetc( fp );
				if( ch == EOF ) break;
				*out = (unsigned char) ch;
				assigned++;
			}
			else break;	// directive we don't support
		}
		else
		{
			int ch = fgetc( fp );
			if( ch != *p )
			{
				if( ch != EOF ) ungetc( ch, fp );
				break;
			}
		}
	}

	va_end( ap );
	return assigned;
}

#endif // !_MSC_VER

#endif // MSVC_COMPAT_H_
