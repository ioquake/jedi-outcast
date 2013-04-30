/*
===========================================================================
Copyright (C) 1999-2005 Id Software, Inc.

This file is part of Quake III Arena source code.

Quake III Arena source code is free software; you can redistribute it
and/or modify it under the terms of the GNU General Public License as
published by the Free Software Foundation; either version 2 of the License,
or (at your option) any later version.

Quake III Arena source code is distributed in the hope that it will be
useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Quake III Arena source code; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
===========================================================================
*/

#ifdef USE_LOCAL_HEADERS
#	include "SDL.h"
#else
#	include <SDL.h>
#endif

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#include "../renderer/tr_local.h"
#include "sdl_local.h"
#ifdef notyet
#include "sdl_icon.h"
#endif

#define CLIENT_WINDOW_TITLE	"Jedi Outcast"
#define CLIENT_WINDOW_MIN_TITLE	"JO"

/* Just hack it for now. */
#ifdef MACOS_X
#include <OpenGL/OpenGL.h>
typedef CGLContextObj QGLContext;
#define GLimp_GetCurrentContext() CGLGetCurrentContext()
#define GLimp_SetCurrentContext(ctx) CGLSetCurrentContext(ctx)
#else
typedef void *QGLContext;
#define GLimp_GetCurrentContext() (NULL)
#define GLimp_SetCurrentContext(ctx)
#endif

static QGLContext opengl_context;
static float displayAspect;

typedef enum
{
	RSERR_OK,

	RSERR_INVALID_FULLSCREEN,
	RSERR_INVALID_MODE,

	RSERR_UNKNOWN
} rserr_t;

static SDL_Surface *screen = NULL;
static const SDL_VideoInfo *videoInfo = NULL;

cvar_t *r_allowSoftwareGL; // Don't abort out if a hardware visual can't be obtained
cvar_t *r_allowResize; // make window resizable
cvar_t *r_centerWindow;
cvar_t *r_sdlDriver;
cvar_t *r_noborder;

/*
===============
GLimp_Shutdown
===============
*/
void GLimp_Shutdown( void )
{
	IN_Shutdown();

	SDL_QuitSubSystem( SDL_INIT_VIDEO );
	screen = NULL;
}

/*
===============
GLimp_Minimize

Minimize the game so that user is back at the desktop
===============
*/
void GLimp_Minimize(void)
{
	SDL_WM_IconifyWindow();
}


/*
===============
GLimp_LogComment
===============
*/
void GLimp_LogComment( char *comment )
{
}

/*
===============
GLimp_CompareModes
===============
*/
static int GLimp_CompareModes( const void *a, const void *b )
{
	const float ASPECT_EPSILON = 0.001f;
	SDL_Rect *modeA = *(SDL_Rect **)a;
	SDL_Rect *modeB = *(SDL_Rect **)b;
	float aspectA = (float)modeA->w / (float)modeA->h;
	float aspectB = (float)modeB->w / (float)modeB->h;
	int areaA = modeA->w * modeA->h;
	int areaB = modeB->w * modeB->h;
	float aspectDiffA = fabs( aspectA - displayAspect );
	float aspectDiffB = fabs( aspectB - displayAspect );
	float aspectDiffsDiff = aspectDiffA - aspectDiffB;

	if( aspectDiffsDiff > ASPECT_EPSILON )
		return 1;
	else if( aspectDiffsDiff < -ASPECT_EPSILON )
		return -1;
	else
		return areaA - areaB;
}


/*
===============
GLimp_DetectAvailableModes
===============
*/
static void GLimp_DetectAvailableModes(void)
{
	char buf[ MAX_STRING_CHARS ] = { 0 };
	SDL_Rect **modes;
	int numModes;
	int i;

	modes = SDL_ListModes( videoInfo->vfmt, SDL_OPENGL | SDL_FULLSCREEN );

	if( !modes )
	{
		ri.Printf( PRINT_WARNING, "Can't get list of available modes\n" );
		return;
	}

	if( modes == (SDL_Rect **)-1 )
	{
		ri.Printf( PRINT_ALL, "Display supports any resolution\n" );
		return; // can set any resolution
	}

	for( numModes = 0; modes[ numModes ]; numModes++ );

	if( numModes > 1 )
		qsort( modes, numModes, sizeof( SDL_Rect* ), GLimp_CompareModes );

	for( i = 0; i < numModes; i++ )
	{
		const char *newModeString = va( "%ux%u ", modes[ i ]->w, modes[ i ]->h );

		if( strlen( newModeString ) < (int)sizeof( buf ) - strlen( buf ) )
			Q_strcat( buf, sizeof( buf ), newModeString );
		else
			ri.Printf( PRINT_WARNING, "Skipping mode %ux%x, buffer too small\n", modes[i]->w, modes[i]->h );
	}

	if( *buf )
	{
		buf[ strlen( buf ) - 1 ] = 0;
		ri.Printf( PRINT_ALL, "Available modes: '%s'\n", buf );
		ri.Cvar_Set( "r_availableModes", buf );
	}
}

/*
===============
GLimp_SetMode
===============
*/
static int GLimp_SetMode(int mode, qboolean fullscreen, qboolean noborder)
{
	const char*   glstring;
	int sdlcolorbits;
	int colorbits, depthbits, stencilbits;
	int tcolorbits, tdepthbits, tstencilbits;
	int samples;
	int i = 0;
	SDL_Surface *vidscreen = NULL;
	Uint32 flags = SDL_OPENGL;

	ri.Printf( PRINT_ALL, "Initializing OpenGL display\n");

	if ( r_allowResize->integer )
		flags |= SDL_RESIZABLE;

	if( videoInfo == NULL )
	{
		static SDL_VideoInfo sVideoInfo;
		static SDL_PixelFormat sPixelFormat;

		videoInfo = SDL_GetVideoInfo( );

		// Take a copy of the videoInfo
		memcpy( &sPixelFormat, videoInfo->vfmt, sizeof( SDL_PixelFormat ) );
		sPixelFormat.palette = NULL; // Should already be the case
		memcpy( &sVideoInfo, videoInfo, sizeof( SDL_VideoInfo ) );
		sVideoInfo.vfmt = &sPixelFormat;
		videoInfo = &sVideoInfo;

		if( videoInfo->current_h > 0 )
		{
			// Guess the display aspect ratio through the desktop resolution
			// by assuming (relatively safely) that it is set at or close to
			// the display's native aspect ratio
			displayAspect = (float)videoInfo->current_w / (float)videoInfo->current_h;

			ri.Printf( PRINT_ALL, "Estimated display aspect: %.3f\n", displayAspect );
		}
		else
		{
			ri.Printf( PRINT_ALL,
					"Cannot estimate display aspect, assuming 1.333\n" );
		}
	}

	ri.Printf (PRINT_ALL, "...setting mode %d:", mode );

	if (mode == -2)
	{
		// use desktop video resolution
		if( videoInfo->current_h > 0 )
		{
			glConfig.vidWidth = videoInfo->current_w;
			glConfig.vidHeight = videoInfo->current_h;
		}
		else
		{
			glConfig.vidWidth = 640;
			glConfig.vidHeight = 480;
			ri.Printf( PRINT_ALL,
					"Cannot determine display resolution, assuming 640x480\n" );
		}

		glConfig.windowAspect = (float)glConfig.vidWidth / (float)glConfig.vidHeight;
	}
	else if ( !R_GetModeInfo( &glConfig.vidWidth, &glConfig.vidHeight, &glConfig.windowAspect, mode ) )
	{
		ri.Printf( PRINT_ALL, " invalid mode\n" );
		return RSERR_INVALID_MODE;
	}
	ri.Printf( PRINT_ALL, " %d %d\n", glConfig.vidWidth, glConfig.vidHeight);

	if (fullscreen)
	{
		flags |= SDL_FULLSCREEN;
		glConfig.isFullscreen = qtrue;
	}
	else
	{
		if (noborder)
			flags |= SDL_NOFRAME;

		glConfig.isFullscreen = qfalse;
	}

	colorbits = r_colorbits->value;
	if ((!colorbits) || (colorbits >= 32))
		colorbits = 24;

	if (!r_depthbits->value)
		depthbits = 24;
	else
		depthbits = r_depthbits->value;
	stencilbits = r_stencilbits->value;
#ifdef notyet
	samples = r_ext_multisample->value;
#else
	samples = 0;
#endif

	for (i = 0; i < 16; i++)
	{
		// 0 - default
		// 1 - minus colorbits
		// 2 - minus depthbits
		// 3 - minus stencil
		if ((i % 4) == 0 && i)
		{
			// one pass, reduce
			switch (i / 4)
			{
				case 2 :
					if (colorbits == 24)
						colorbits = 16;
					break;
				case 1 :
					if (depthbits == 24)
						depthbits = 16;
					else if (depthbits == 16)
						depthbits = 8;
				case 3 :
					if (stencilbits == 24)
						stencilbits = 16;
					else if (stencilbits == 16)
						stencilbits = 8;
			}
		}

		tcolorbits = colorbits;
		tdepthbits = depthbits;
		tstencilbits = stencilbits;

		if ((i % 4) == 3)
		{ // reduce colorbits
			if (tcolorbits == 24)
				tcolorbits = 16;
		}

		if ((i % 4) == 2)
		{ // reduce depthbits
			if (tdepthbits == 24)
				tdepthbits = 16;
			else if (tdepthbits == 16)
				tdepthbits = 8;
		}

		if ((i % 4) == 1)
		{ // reduce stencilbits
			if (tstencilbits == 24)
				tstencilbits = 16;
			else if (tstencilbits == 16)
				tstencilbits = 8;
			else
				tstencilbits = 0;
		}

		sdlcolorbits = 4;
		if (tcolorbits == 24)
			sdlcolorbits = 8;

#ifdef __sgi /* Fix for SGIs grabbing too many bits of color */
		if (sdlcolorbits == 4)
			sdlcolorbits = 0; /* Use minimum size for 16-bit color */

		/* Need alpha or else SGIs choose 36+ bit RGB mode */
		SDL_GL_SetAttribute( SDL_GL_ALPHA_SIZE, 1);
#endif

		SDL_GL_SetAttribute( SDL_GL_RED_SIZE, sdlcolorbits );
		SDL_GL_SetAttribute( SDL_GL_GREEN_SIZE, sdlcolorbits );
		SDL_GL_SetAttribute( SDL_GL_BLUE_SIZE, sdlcolorbits );
		SDL_GL_SetAttribute( SDL_GL_DEPTH_SIZE, tdepthbits );
		SDL_GL_SetAttribute( SDL_GL_STENCIL_SIZE, tstencilbits );

		SDL_GL_SetAttribute( SDL_GL_MULTISAMPLEBUFFERS, samples ? 1 : 0 );
		SDL_GL_SetAttribute( SDL_GL_MULTISAMPLESAMPLES, samples );

#ifdef notyet
		if(r_stereoEnabled->integer)
		{
			glConfig.stereoEnabled = qtrue;
			SDL_GL_SetAttribute(SDL_GL_STEREO, 1);
		}
		else
		{
			glConfig.stereoEnabled = qfalse;
			SDL_GL_SetAttribute(SDL_GL_STEREO, 0);
		}
#endif
		
		SDL_GL_SetAttribute( SDL_GL_DOUBLEBUFFER, 1 );

#if 0 // See http://bugzilla.icculus.org/show_bug.cgi?id=3526
		// If not allowing software GL, demand accelerated
		if( !r_allowSoftwareGL->integer )
		{
			if( SDL_GL_SetAttribute( SDL_GL_ACCELERATED_VISUAL, 1 ) < 0 )
			{
				ri.Printf( PRINT_ALL, "Unable to guarantee accelerated "
						"visual with libSDL < 1.2.10\n" );
			}
		}
#endif

		if( SDL_GL_SetAttribute( SDL_GL_SWAP_CONTROL, r_swapInterval->integer ) < 0 )
			ri.Printf( PRINT_ALL, "r_swapInterval requires libSDL >= 1.2.10\n" );

#ifdef USE_ICON
		{
			SDL_Surface *icon = SDL_CreateRGBSurfaceFrom(
					(void *)CLIENT_WINDOW_ICON.pixel_data,
					CLIENT_WINDOW_ICON.width,
					CLIENT_WINDOW_ICON.height,
					CLIENT_WINDOW_ICON.bytes_per_pixel * 8,
					CLIENT_WINDOW_ICON.bytes_per_pixel * CLIENT_WINDOW_ICON.width,
#ifdef Q3_LITTLE_ENDIAN
					0x000000FF, 0x0000FF00, 0x00FF0000, 0xFF000000
#else
					0xFF000000, 0x00FF0000, 0x0000FF00, 0x000000FF
#endif
					);

			SDL_WM_SetIcon( icon, NULL );
			SDL_FreeSurface( icon );
		}
#endif

		SDL_WM_SetCaption(CLIENT_WINDOW_TITLE, CLIENT_WINDOW_MIN_TITLE);
		SDL_ShowCursor(0);

		if (!(vidscreen = SDL_SetVideoMode(glConfig.vidWidth, glConfig.vidHeight, colorbits, flags)))
		{
			ri.Printf( PRINT_DEVELOPER, "SDL_SetVideoMode failed: %s\n", SDL_GetError( ) );
			continue;
		}

		opengl_context = GLimp_GetCurrentContext();

		ri.Printf( PRINT_ALL, "Using %d/%d/%d Color bits, %d depth, %d stencil display.\n",
				sdlcolorbits, sdlcolorbits, sdlcolorbits, tdepthbits, tstencilbits);

		glConfig.colorBits = tcolorbits;
		glConfig.depthBits = tdepthbits;
		glConfig.stencilBits = tstencilbits;
		break;
	}

	GLimp_DetectAvailableModes();

	if (!vidscreen)
	{
		ri.Printf( PRINT_ALL, "Couldn't get a visual\n" );
		return RSERR_INVALID_MODE;
	}

	screen = vidscreen;

	glstring = (char *) qglGetString (GL_RENDERER);
	ri.Printf( PRINT_ALL, "GL_RENDERER: %s\n", glstring );

	return RSERR_OK;
}

/*
===============
GLimp_StartDriverAndSetMode
===============
*/
static qboolean GLimp_StartDriverAndSetMode(int mode, qboolean fullscreen, qboolean noborder)
{
	rserr_t err;

	if (!SDL_WasInit(SDL_INIT_VIDEO))
	{
		char driverName[ 64 ];

		if (SDL_Init(SDL_INIT_VIDEO) == -1)
		{
			ri.Printf( PRINT_ALL, "SDL_Init( SDL_INIT_VIDEO ) FAILED (%s)\n",
					SDL_GetError());
			return qfalse;
		}

		SDL_VideoDriverName( driverName, sizeof( driverName ) - 1 );
		ri.Printf( PRINT_ALL, "SDL using driver \"%s\"\n", driverName );
		ri.Cvar_Set( "r_sdlDriver", driverName );
	}

	if (fullscreen && Cvar_VariableIntegerValue( "in_nograb" ) )
	{
		ri.Printf( PRINT_ALL, "Fullscreen not allowed with in_nograb 1\n");
		ri.Cvar_Set( "r_fullscreen", "0" );
		r_fullscreen->modified = qfalse;
		fullscreen = qfalse;
	}
	
	err = (rserr_t) GLimp_SetMode(mode, fullscreen, noborder);

	switch ( err )
	{
		case RSERR_INVALID_FULLSCREEN:
			ri.Printf( PRINT_ALL, "...WARNING: fullscreen unavailable in this mode\n" );
			return qfalse;
		case RSERR_INVALID_MODE:
			ri.Printf( PRINT_ALL, "...WARNING: could not set the given mode (%d)\n", mode );
			return qfalse;
		default:
			break;
	}

	return qtrue;
}

static void GLW_InitTextureCompression( void )
{
	qboolean newer_tc, old_tc;

	// Check for available tc methods.
	newer_tc = ( strstr( glConfig.extensions_string, "ARB_texture_compression" )
		&& strstr( glConfig.extensions_string, "EXT_texture_compression_s3tc" )) ? qtrue : qfalse;
	old_tc = ( strstr( glConfig.extensions_string, "GL_S3_s3tc" )) ? qtrue : qfalse;

	if ( old_tc )
	{
		ri.Printf( PRINT_ALL, "...GL_S3_s3tc available\n" );
	}

	if ( newer_tc )
	{
		ri.Printf( PRINT_ALL, "...GL_EXT_texture_compression_s3tc available\n" );
	}

	if ( !r_ext_compressed_textures->value )
	{
		// Compressed textures are off
		glConfig.textureCompression = TC_NONE;
		ri.Printf( PRINT_ALL, "...ignoring texture compression\n" );
	}
	else if ( !old_tc && !newer_tc )
	{
		// Requesting texture compression, but no method found
		glConfig.textureCompression = TC_NONE;
		ri.Printf( PRINT_ALL, "...no supported texture compression method found\n" );
		ri.Printf( PRINT_ALL, ".....ignoring texture compression\n" );
	}
	else
	{
		// some form of supported texture compression is avaiable, so see if the user has a preference
		if ( r_ext_preferred_tc_method->integer == TC_NONE )
		{
			// No preference, so pick the best
			if ( newer_tc )
			{
				ri.Printf( PRINT_ALL, "...no tc preference specified\n" );
				ri.Printf( PRINT_ALL, ".....using GL_EXT_texture_compression_s3tc\n" );
				glConfig.textureCompression = TC_S3TC_DXT;
			}
			else
			{
				ri.Printf( PRINT_ALL, "...no tc preference specified\n" );
				ri.Printf( PRINT_ALL, ".....using GL_S3_s3tc\n" );
				glConfig.textureCompression = TC_S3TC;
			}
		}
		else
		{
			// User has specified a preference, now see if this request can be honored
			if ( old_tc && newer_tc )
			{
				// both are avaiable, so we can use the desired tc method
				if ( r_ext_preferred_tc_method->integer == TC_S3TC )
				{
					ri.Printf( PRINT_ALL, "...using preferred tc method, GL_S3_s3tc\n" );
					glConfig.textureCompression = TC_S3TC;
				}
				else
				{
					ri.Printf( PRINT_ALL, "...using preferred tc method, GL_EXT_texture_compression_s3tc\n" );
					glConfig.textureCompression = TC_S3TC_DXT;
				}
			}
			else
			{
				// Both methods are not available, so this gets trickier
				if ( r_ext_preferred_tc_method->integer == TC_S3TC )
				{
					// Preferring to user older compression
					if ( old_tc )
					{
						ri.Printf( PRINT_ALL, "...using GL_S3_s3tc\n" );
						glConfig.textureCompression = TC_S3TC;
					}
					else
					{
						// Drat, preference can't be honored 
						ri.Printf( PRINT_ALL, "...preferred tc method, GL_S3_s3tc not available\n" );
						ri.Printf( PRINT_ALL, ".....falling back to GL_EXT_texture_compression_s3tc\n" );
						glConfig.textureCompression = TC_S3TC_DXT;
					}
				}
				else
				{
					// Preferring to user newer compression
					if ( newer_tc )
					{
						ri.Printf( PRINT_ALL, "...using GL_EXT_texture_compression_s3tc\n" );
						glConfig.textureCompression = TC_S3TC_DXT;
					}
					else
					{
						// Drat, preference can't be honored 
						ri.Printf( PRINT_ALL, "...preferred tc method, GL_EXT_texture_compression_s3tc not available\n" );
						ri.Printf( PRINT_ALL, ".....falling back to GL_S3_s3tc\n" );
						glConfig.textureCompression = TC_S3TC;
					}
				}
			}
		}
	}
}

/*
** GLW_InitExtensions
*/
cvar_t *r_ATI_NPATCH_available = NULL;
static void GLimp_InitExtensions( void )
{
	if ( !r_allowExtensions->integer )
	{
		ri.Printf( PRINT_ALL, "*** IGNORING OPENGL EXTENSIONS ***\n" );
		return;
	}

	ri.Printf( PRINT_ALL, "Initializing OpenGL extensions\n" );

	// Select our tc scheme
	GLW_InitTextureCompression();

	// GL_EXT_texture_env_add
	glConfig.textureEnvAddAvailable = qfalse;
	if ( strstr( glConfig.extensions_string, "EXT_texture_env_add" ) )
	{
		if ( r_ext_texture_env_add->integer )
		{
			glConfig.textureEnvAddAvailable = qtrue;
			ri.Printf( PRINT_ALL, "...using GL_EXT_texture_env_add\n" );
		}
		else
		{
			glConfig.textureEnvAddAvailable = qfalse;
			ri.Printf( PRINT_ALL, "...ignoring GL_EXT_texture_env_add\n" );
		}
	}
	else
	{
		ri.Printf( PRINT_ALL, "...GL_EXT_texture_env_add not found\n" );
	}

	// GL_EXT_texture_filter_anisotropic
	glConfig.textureFilterAnisotropicAvailable = qfalse;
	if ( strstr( glConfig.extensions_string, "EXT_texture_filter_anisotropic" ) )
	{
		glConfig.textureFilterAnisotropicAvailable = qtrue;
		ri.Printf( PRINT_ALL, "...GL_EXT_texture_filter_anisotropic available\n" );

		if ( r_ext_texture_filter_anisotropic->integer )
		{
			ri.Printf( PRINT_ALL, "...using GL_EXT_texture_filter_anisotropic\n" );
		}
		else
		{
			ri.Printf( PRINT_ALL, "...ignoring GL_EXT_texture_filter_anisotropic\n" );
		}
		ri.Cvar_Set( "r_ext_texture_filter_anisotropic_avail", "1" );
	}
	else
	{
		ri.Printf( PRINT_ALL, "...GL_EXT_texture_filter_anisotropic not found\n" );
		ri.Cvar_Set( "r_ext_texture_filter_anisotropic_avail", "0" );
	}

	// GL_EXT_clamp_to_edge
	glConfig.clampToEdgeAvailable = qfalse;
	if ( strstr( glConfig.extensions_string, "GL_EXT_texture_edge_clamp" ) )
	{
		glConfig.clampToEdgeAvailable = qtrue;
		ri.Printf( PRINT_ALL, "...Using GL_EXT_texture_edge_clamp\n" );
	}

	// GL_ARB_multitexture
	qglMultiTexCoord2fARB = NULL;
	qglActiveTextureARB = NULL;
	qglClientActiveTextureARB = NULL;
	if ( strstr( glConfig.extensions_string, "GL_ARB_multitexture" )  )
	{
		if ( r_ext_multitexture->integer )
		{
			qglMultiTexCoord2fARB = ( PFNGLMULTITEXCOORD2FARBPROC ) SDL_GL_GetProcAddress( "glMultiTexCoord2fARB" );
			qglActiveTextureARB = ( PFNGLACTIVETEXTUREARBPROC ) SDL_GL_GetProcAddress( "glActiveTextureARB" );
			qglClientActiveTextureARB = ( PFNGLCLIENTACTIVETEXTUREARBPROC ) SDL_GL_GetProcAddress( "glClientActiveTextureARB" );

			if ( qglActiveTextureARB )
			{
				qglGetIntegerv( GL_MAX_ACTIVE_TEXTURES_ARB, &glConfig.maxActiveTextures );

				if ( glConfig.maxActiveTextures > 1 )
				{
					ri.Printf( PRINT_ALL, "...using GL_ARB_multitexture\n" );
				}
				else
				{
					qglMultiTexCoord2fARB = NULL;
					qglActiveTextureARB = NULL;
					qglClientActiveTextureARB = NULL;
					ri.Printf( PRINT_ALL, "...not using GL_ARB_multitexture, < 2 texture units\n" );
				}
			}
		}
		else
		{
			ri.Printf( PRINT_ALL, "...ignoring GL_ARB_multitexture\n" );
		}
	}
	else
	{
		ri.Printf( PRINT_ALL, "...GL_ARB_multitexture not found\n" );
	}

	// GL_EXT_compiled_vertex_array
	qglLockArraysEXT = NULL;
	qglUnlockArraysEXT = NULL;
	if ( strstr( glConfig.extensions_string, "GL_EXT_compiled_vertex_array" ) )
	{
		if ( r_ext_compiled_vertex_array->integer )
		{
			ri.Printf( PRINT_ALL, "...using GL_EXT_compiled_vertex_array\n" );
			qglLockArraysEXT = ( void ( APIENTRY * )( int, int ) ) SDL_GL_GetProcAddress( "glLockArraysEXT" );
			qglUnlockArraysEXT = ( void ( APIENTRY * )( void ) ) SDL_GL_GetProcAddress( "glUnlockArraysEXT" );
			if (!qglLockArraysEXT || !qglUnlockArraysEXT) {
				ri.Error (ERR_FATAL, "bad getprocaddress");
			}
		}
		else
		{
			ri.Printf( PRINT_ALL, "...ignoring GL_EXT_compiled_vertex_array\n" );
		}
	}
	else
	{
		ri.Printf( PRINT_ALL, "...GL_EXT_compiled_vertex_array not found\n" );
	}

	qglPointParameterfEXT = NULL;
	qglPointParameterfvEXT = NULL;
	if ( strstr( glConfig.extensions_string, "GL_EXT_point_parameters" ) )
	{
		if ( r_ext_compiled_vertex_array->integer || 1)
		{
			ri.Printf( PRINT_ALL, "...using GL_EXT_point_parameters\n" );
			qglPointParameterfEXT = ( void ( APIENTRY * )( GLenum, GLfloat) ) SDL_GL_GetProcAddress( "glPointParameterfEXT" );
			qglPointParameterfvEXT = ( void ( APIENTRY * )( GLenum, GLfloat *) ) SDL_GL_GetProcAddress( "glPointParameterfvEXT" );
			if (!qglPointParameterfEXT || !qglPointParameterfvEXT) 
			{
				ri.Error (ERR_FATAL, "bad getprocaddress");
			}
		}
		else
		{
			ri.Printf( PRINT_ALL, "...ignoring GL_EXT_point_parameters\n" );
		}
	}
	else
	{
		ri.Printf( PRINT_ALL, "...GL_EXT_point_parameters not found\n" );
	}

#ifdef _NPATCH
	// GL_ATI_pn_triangles
	qglPNTrianglesiATI = NULL;
	r_ATI_NPATCH_available = ri.Cvar_Get( "r_ATI_NPATCH_available", "0",CVAR_ROM );
/*	if ( strstr( glConfig.extensions_string, "GL_ATI_pn_triangles" ) )
	{
		ri.Cvar_Set( "r_ATI_NPATCH_available", "1" );
		if ( r_ati_pn_triangles->integer )
		{	
			ri.Printf( PRINT_ALL, "...using GL_ATI_pn_triangles\n" );
			qglPNTrianglesiATI = ( void ( APIENTRY * )( GLenum, GLint ) ) SDL_GL_GetProcAddress( "glPNTrianglesiATI" );
			if (!qglPNTrianglesiATI) {
				ri.Error (ERR_FATAL, "bad getprocaddress");
			}
		}
		else
		{
			ri.Printf( PRINT_ALL, "...ignoring GL_ATI_pn_triangles\n" );
		}
	}
	else
	{
		ri.Printf( PRINT_ALL, "...GL_ATI_pn_triangles not found\n" );
	}
*/
#endif // _NPATCH
}

#define R_MODE_FALLBACK 3 // 640 * 480

/*
===============
GLimp_Init

This routine is responsible for initializing the OS specific portions
of OpenGL
===============
*/
void GLimp_Init( void )
{
	r_allowSoftwareGL = ri.Cvar_Get( "r_allowSoftwareGL", "0", CVAR_LATCH );
	r_sdlDriver = ri.Cvar_Get( "r_sdlDriver", "", CVAR_ROM );
	r_allowResize = ri.Cvar_Get( "r_allowResize", "0", CVAR_ARCHIVE );
	r_centerWindow = ri.Cvar_Get( "r_centerWindow", "0", CVAR_ARCHIVE );
	r_noborder = Cvar_Get( "r_noborder", "0", CVAR_ARCHIVE );
	qboolean fullscreen, noborder;

	if( Cvar_VariableIntegerValue( "com_abnormalExit" ) )
	{
		ri.Cvar_Set( "r_mode", va( "%d", R_MODE_FALLBACK ) );
		ri.Cvar_Set( "r_fullscreen", "0" );
		ri.Cvar_Set( "r_centerWindow", "0" );
		ri.Cvar_Set( "com_abnormalExit", "0" );
	}

#ifdef notyet
	Sys_SetEnv( "SDL_VIDEO_CENTERED", r_centerWindow->integer ? "1" : "" );
#endif

	Sys_GLimpInit( );

	fullscreen = (r_fullscreen->integer) ? qtrue : qfalse;
	noborder = (r_noborder->integer) ? qtrue : qfalse;

	// Create the window and set up the context
	if(GLimp_StartDriverAndSetMode(r_mode->integer, fullscreen, noborder))
		goto success;

	// Try again, this time in a platform specific "safe mode"
	Sys_GLimpSafeInit( );

	if(GLimp_StartDriverAndSetMode(r_mode->integer, fullscreen, qfalse))
		goto success;

	// Finally, try the default screen resolution
	if( r_mode->integer != R_MODE_FALLBACK )
	{
		ri.Printf( PRINT_ALL, "Setting r_mode %d failed, falling back on r_mode %d\n",
				r_mode->integer, R_MODE_FALLBACK );

		if(GLimp_StartDriverAndSetMode(R_MODE_FALLBACK, qfalse, qfalse))
			goto success;
	}

	// Nothing worked, give up
	ri.Error( ERR_FATAL, "GLimp_Init() - could not load OpenGL subsystem" );

success:
#if 0
	// This values force the UI to disable driver selection
	glConfig.driverType = GLDRV_ICD;
	glConfig.hardwareType = GLHW_GENERIC;
#endif
	glConfig.deviceSupportsGamma = (SDL_SetGamma( 1.0f, 1.0f, 1.0f ) >= 0) ? qtrue : qfalse;

	// Mysteriously, if you use an NVidia graphics card and multiple monitors,
	// SDL_SetGamma will incorrectly return false... the first time; ask
	// again and you get the correct answer. This is a suspected driver bug, see
	// http://bugzilla.icculus.org/show_bug.cgi?id=4316
	glConfig.deviceSupportsGamma = (SDL_SetGamma( 1.0f, 1.0f, 1.0f ) >= 0) ? qtrue : qfalse;

	if ( -1 == r_ignorehwgamma->integer)
		glConfig.deviceSupportsGamma = qtrue;

	if ( 1 == r_ignorehwgamma->integer)
		glConfig.deviceSupportsGamma = qfalse;

	// get our config strings
	const char* glstring;
	glstring = (const char *)qglGetString (GL_VENDOR);
	if (!glstring) {
		glstring = "invalid driver";
	}
	Q_strncpyz( glConfig.vendor_string, glstring, sizeof( glConfig.vendor_string ) );
	glstring = (const char *)qglGetString (GL_RENDERER);
	if (!glstring) {
		glstring = "invalid driver";
	}
	Q_strncpyz( glConfig.renderer_string, glstring, sizeof( glConfig.renderer_string ) );
	glstring = (const char *)qglGetString (GL_VERSION);
	if (!glstring) {
		glstring = "invalid driver";
	}
	Q_strncpyz( glConfig.version_string, glstring, sizeof( glConfig.version_string ) );
	glstring = (const char *)qglGetString (GL_EXTENSIONS);
	if (!glstring) {
		glstring = "invalid driver";
	}
	Q_strncpyz( glConfig.extensions_string, glstring, sizeof( glConfig.extensions_string ) );

	// OpenGL driver constants
	qglGetIntegerv( GL_MAX_TEXTURE_SIZE, &glConfig.maxTextureSize );
	// stubbed or broken drivers may have reported 0...
	if ( glConfig.maxTextureSize <= 0 )
	{
		glConfig.maxTextureSize = 0;
	}

	// initialize extensions
	GLimp_InitExtensions( );

	ri.Cvar_Get( "r_availableModes", "", CVAR_ROM );

	// This depends on SDL_INIT_VIDEO, hence having it here
	IN_Init( );
}


/*
===============
GLimp_EndFrame

Responsible for doing a swapbuffers
===============
*/
void GLimp_EndFrame( void )
{
	// don't flip if drawing to front buffer
	if ( Q_stricmp( r_drawBuffer->string, "GL_FRONT" ) != 0 )
	{
		SDL_GL_SwapBuffers();
	}

	if( r_fullscreen->modified )
	{
		qboolean    fullscreen;
		qboolean    needToToggle = qtrue;
		qboolean    sdlToggled = qfalse;
		SDL_Surface *s = SDL_GetVideoSurface( );

		if( s )
		{
			// Find out the current state
			fullscreen = (!!( s->flags & SDL_FULLSCREEN )) ? qtrue : qfalse;
				
			if( r_fullscreen->integer && Cvar_VariableIntegerValue( "in_nograb" ) )
			{
				ri.Printf( PRINT_ALL, "Fullscreen not allowed with in_nograb 1\n");
				ri.Cvar_Set( "r_fullscreen", "0" );
				r_fullscreen->modified = qfalse;
			}

			// Is the state we want different from the current state?
			needToToggle = (!!r_fullscreen->integer != fullscreen) ? qtrue : qfalse;

			if( needToToggle )
				sdlToggled = (SDL_WM_ToggleFullScreen( s )) ? qtrue : qfalse;
		}

		if( needToToggle )
		{
			// SDL_WM_ToggleFullScreen didn't work, so do it the slow way
			if( !sdlToggled )
				ri.Cmd_ExecuteText(EXEC_APPEND, "vid_restart");

			IN_Restart( );
		}

		r_fullscreen->modified = qfalse;
	}
}
