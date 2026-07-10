//
// api.cpp -- extern "C" surface of the engine for JavaScript, plus the two
// hooks the engine calls back through. Browser-only glue; no algorithm here.
//

#include "engine.h"
#include <emscripten.h>

static kbEngine g_Engine;
static int g_iSlowMs = 0;

// Decode by hand rather than UTF8ToString: TextDecoder (which it uses) can't
// read views on the resizable ArrayBuffer that ALLOW_MEMORY_GROWTH produces
// in current Chrome. Engine log text is plain ASCII.
EM_JS( void, js_log, ( const char *msg ), {
	var s = '';
	for( var i = msg; HEAPU8[ i ]; i++ ) s += String.fromCharCode( HEAPU8[ i ] );
	if( Module.onLog ) Module.onLog( s );
} );

EM_JS( void, js_draw, ( ), {
	if( Module.onDraw ) Module.onDraw( );
} );

void EngineLog( const char *msg )
{
	js_log( msg );
}

void EngineOnMove( kbEngine *pEngine )
{
	js_draw( );
	// Asyncify: unwind to the browser event loop so the canvas repaints and
	// the Stop button can fire (its handler calls eng_stop -> m_bStop, which
	// the 2008 loops already check).
	emscripten_sleep( pEngine->m_bSlow ? 500 : g_iSlowMs );
}

extern "C" {

EMSCRIPTEN_KEEPALIVE void eng_load_rgba( const unsigned char *rgba, int w, int h )
{
	g_Engine.LoadRGBA( rgba, w, h );
}

EMSCRIPTEN_KEEPALIVE int eng_load_ppm( const char *path )
{
	return g_Engine.LoadPPM( path ) ? 1 : 0;
}

EMSCRIPTEN_KEEPALIVE int eng_save_ppm( const char *path )
{
	return g_Engine.SavePPM( path ) ? 1 : 0;
}

EMSCRIPTEN_KEEPALIVE void eng_scramble( int n, int swirl, int direction )
{
	g_Engine.Scramble( n, swirl != 0, direction != 0 );
}

EMSCRIPTEN_KEEPALIVE void eng_solve( )			{ g_Engine.Solve( ); }
EMSCRIPTEN_KEEPALIVE void eng_flip_solve( )		{ g_Engine.FlipSolve( ); }
EMSCRIPTEN_KEEPALIVE void eng_stupid_solve( )	{ g_Engine.StupidSolve( ); }
EMSCRIPTEN_KEEPALIVE void eng_stop( )			{ g_Engine.Stop( ); }

EMSCRIPTEN_KEEPALIVE int eng_have_image( )		{ return g_Engine.HaveImage( ) ? 1 : 0; }
EMSCRIPTEN_KEEPALIVE int eng_width( )			{ return g_Engine.Width( ); }
EMSCRIPTEN_KEEPALIVE int eng_height( )			{ return g_Engine.Height( ); }
EMSCRIPTEN_KEEPALIVE unsigned char *eng_pixels( ) { return g_Engine.Pixels( ); }

EMSCRIPTEN_KEEPALIVE unsigned int eng_count( )		{ return g_Engine.m_iCount; }
EMSCRIPTEN_KEEPALIVE int eng_pixels_solved( )		{ return g_Engine.m_iPixelsSolved; }
EMSCRIPTEN_KEEPALIVE double eng_avg_distance( )		{ return g_Engine.AvgDistance( ); }
EMSCRIPTEN_KEEPALIVE int eng_max_distance( )		{ return g_Engine.MaxDistance( ); }
EMSCRIPTEN_KEEPALIVE int eng_min_distance( )		{ return g_Engine.MinDistance( ); }

// redraw on/off ("ReDraw"), every N moves, slow ("Slow") and an extra
// per-yield delay in ms for a speed slider.
EMSCRIPTEN_KEEPALIVE void eng_set_draw( int on, int every, int slow, int delayMs )
{
	g_Engine.m_bDraw = ( on != 0 );
	g_Engine.m_iDrawEvery = ( every < 1 ) ? 1 : every;
	g_Engine.m_bSlow = ( slow != 0 );
	g_iSlowMs = ( delayMs < 0 ) ? 0 : delayMs;
}

// Raw addresses, fetched once at startup: JS reads/writes these through the
// heap views while the engine is suspended (Asyncify forbids reentry).
EMSCRIPTEN_KEEPALIVE unsigned char *eng_stop_ptr( )	{ return (unsigned char *) g_Engine.StopFlag( ); }
EMSCRIPTEN_KEEPALIVE unsigned int *eng_count_ptr( )	{ return &g_Engine.m_iCount; }
EMSCRIPTEN_KEEPALIVE int *eng_pixels_solved_ptr( )	{ return &g_Engine.m_iPixelsSolved; }
EMSCRIPTEN_KEEPALIVE int *eng_draw_every_ptr( )		{ return &g_Engine.m_iDrawEvery; }
EMSCRIPTEN_KEEPALIVE int *eng_slow_ms_ptr( )		{ return &g_iSlowMs; }

}
