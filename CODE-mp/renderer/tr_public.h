#ifndef __TR_PUBLIC_H
#define __TR_PUBLIC_H

#include "../cgame/tr_types.h"

#define	REF_API_VERSION		8

//
// these are the functions exported by the refresh module
//
typedef struct {
	// called before the library is unloaded
	// if the system is just reconfiguring, pass destroyWindow = qfalse,
	// which will keep the screen from flashing to the desktop.
	void	(*Shutdown)( qboolean destroyWindow );

	// All data that will be used in a level should be
	// registered before rendering any frames to prevent disk hits,
	// but they can still be registered at a later time
	// if necessary.
	//
	// BeginRegistration makes any existing media pointers invalid
	// and returns the current gl configuration, including screen width
	// and height, which can be used by the client to intelligently
	// size display elements
	void	(*BeginRegistration)( glconfig_t *config );
	qhandle_t (*RegisterModel)( const char *name );
	qhandle_t (*RegisterSkin)( const char *name );
	qhandle_t (*RegisterShader)( const char *name );
	qhandle_t (*RegisterShaderNoMip)( const char *name );
	void	(*LoadWorld)( const char *name );

	// the vis data is a large enough block of data that we go to the trouble
	// of sharing it with the clipmodel subsystem
	void	(*SetWorldVisData)( const byte *vis );

	// EndRegistration will draw a tiny polygon with each texture, forcing
	// them to be loaded into card memory
	void	(*EndRegistration)( void );

	// a scene is built up by calls to R_ClearScene and the various R_Add functions.
	// Nothing is drawn until R_RenderScene is called.
	void	(*ClearScene)( void );
	void	(*AddRefEntityToScene)( const refEntity_t *re );
	void	(*AddMiniRefEntityToScene)( const miniRefEntity_t *re );
	void	(*AddPolyToScene)( qhandle_t hShader , int numVerts, const polyVert_t *verts, int num );
	int		(*LightForPoint)( vec3_t point, vec3_t ambientLight, vec3_t directedLight, vec3_t lightDir );
	void	(*AddLightToScene)( const vec3_t org, float intensity, float r, float g, float b );
	void	(*AddAdditiveLightToScene)( const vec3_t org, float intensity, float r, float g, float b );
	void	(*RenderScene)( const refdef_t *fd );

	void	(*SetColor)( const float *rgba );	// NULL = 1,1,1,1
	void	(*DrawStretchPic) ( float x, float y, float w, float h, 
		float s1, float t1, float s2, float t2, qhandle_t hShader );	// 0 = white
	void	(*DrawRotatePic) ( float x, float y, float w, float h, 
		float s1, float t1, float s2, float t2, float a1, qhandle_t hShader );	// 0 = white
	void	(*DrawRotatePic2) ( float x, float y, float w, float h, 
		float s1, float t1, float s2, float t2, float a1, qhandle_t hShader );	// 0 = white

	// Draw images for cinematic rendering, pass as 32 bit rgba
	void	(*DrawStretchRaw) (int x, int y, int w, int h, int cols, int rows, const byte *data, int client, qboolean dirty);
	void	(*UploadCinematic) (int cols, int rows, const byte *data, int client, qboolean dirty);

	void	(*BeginFrame)( stereoFrame_t stereoFrame );

	// if the pointers are not NULL, timing info will be returned
	void	(*EndFrame)( int *frontEndMsec, int *backEndMsec );


	int		(*MarkFragments)( int numPoints, const vec3_t *points, const vec3_t projection,
				   int maxPoints, vec3_t pointBuffer, int maxFragments, markFragment_t *fragmentBuffer );

	int		(*LerpTag)( orientation_t *tag,  qhandle_t model, int startFrame, int endFrame, 
					 float frac, const char *tagName );
	void	(*ModelBounds)( qhandle_t model, vec3_t mins, vec3_t maxs );

#ifdef __USEA3D
	void    (*A3D_RenderGeometry) (void *pVoidA3D, void *pVoidGeom, void *pVoidMat, void *pVoidGeomStatus);
#endif

	qhandle_t (*RegisterFont)( const char *fontName );
	int		(*Font_StrLenPixels) (const char *text, const int iFontIndex, const float scale);
	int		(*Font_StrLenChars) (const char *text);
	int		(*Font_HeightPixels)(const int iFontIndex, const float scale);
	void	(*Font_DrawString)(int ox, int oy, const char *text, const float *rgba, const int setIndex, int iCharLimit, const float scale);
	qboolean (*Language_IsAsian)(void);
	qboolean (*Language_UsesSpaces)(void);
	unsigned int (*AnyLanguage_ReadCharFromString)( const char *psText, int *piAdvanceCount, qboolean *pbIsTrailingPunctuation/* = NULL*/ );

	void	(*RemapShader)(const char *oldShader, const char *newShader, const char *offsetTime);
	qboolean (*GetEntityToken)( char *buffer, int size );
	qboolean (*inPVS)( const vec3_t p1, const vec3_t p2 );

	void (*GetLightStyle)(int style, color4ub_t color);
	void (*SetLightStyle)(int style, int color);

	void	(*GetBModelVerts)( int bmodelIndex, vec3_t *vec, vec3_t normal );
} refexport_t;

//
// these are the functions imported by the refresh module
//
typedef struct {
	// print message on the local console
	void	(QDECL *Printf)( int printLevel, const char *fmt, ...);

	// abort the game
	void	(QDECL *Error)( int errorLevel, const char *fmt, ...);

	// milliseconds should only be used for profiling, never
	// for anything game related.  Get time from the refdef
	int		(*Milliseconds)( void );

	// stack based memory allocation for per-level things that
	// won't be freed
#ifdef HUNK_DEBUG
	void	*(*Hunk_AllocDebug)( int size, ha_pref pref, char *label, char *file, int line );
#else
	void	*(*Hunk_Alloc)( int size, ha_pref pref );
#endif
	void	*(*Hunk_AllocateTempMemory)( int size );
	void	(*Hunk_FreeTempMemory)( void *block );

	// dynamic memory allocator for things that need to be freed
	void	*(*Malloc)( int iSize, memtag_t eTag, qboolean bZeroIt );
	void	(*Free)( void *buf );

	cvar_t	*(*Cvar_Get)( const char *name, const char *value, int flags );
	void	(*Cvar_Set)( const char *name, const char *value );

	void	(*Cmd_AddCommand)( const char *name, void(*cmd)(void) );
	void	(*Cmd_RemoveCommand)( const char *name );
	void (*Cmd_ArgsBuffer)( char *buffer, int bufferLength );

	int		(*Cmd_Argc) (void);
	char	*(*Cmd_Argv) (int i);

	void	(*Cmd_ExecuteText) (int exec_when, const char *text);

	// visualization for debugging collision detection
	void	(*CM_DrawDebugSurface)( void (*drawPoly)(int color, int numPoints, float *points) );

	// a -1 return means the file does not exist
	// NULL can be passed for buf to just determine existance
	int		(*FS_FileIsInPAK)( const char *name, int *pCheckSum );
	int		(*FS_ReadFile)( const char *name, void **buf );
	void	(*FS_FreeFile)( void *buf );
	char **	(*FS_ListFiles)( const char *name, const char *extension, int *numfilesfound );
	void	(*FS_FreeFileList)( char **filelist );
	void	(*FS_WriteFile)( const char *qpath, const void *buffer, int size );
	qboolean (*FS_FileExists)( const char *file );

	// cinematic stuff
	void	(*CIN_UploadCinematic)(int handle);
	int		(*CIN_PlayCinematic)( const char *arg0, int xpos, int ypos, int width, int height, int bits);
	e_status (*CIN_RunCinematic) (int handle);

	int (*CM_PointContents)( const vec3_t p, clipHandle_t model );

} refimport_t;


// this is the only function actually exported at the linker level
// If the module can't init to a valid rendering state, NULL will be
// returned.
refexport_t*GetRefAPI( int apiVersion, refimport_t *rimp );

#endif	// __TR_PUBLIC_H
