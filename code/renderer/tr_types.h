#ifndef __TR_TYPES_H
#define __TR_TYPES_H

#include "../game/ghoul2_shared.h"

#define	MAX_DLIGHTS		32			// can't be increased, because bit flags are used on surfaces
#define	MAX_ENTITIES	1023		// can't be increased without changing drawsurf bit packing

// renderfx flags
#define	RF_MORELIGHT		0x00001	// allways have some light (viewmodel, some items)
#define	RF_THIRD_PERSON		0x00002	// don't draw through eyes, only mirrors (player bodies, chat sprites)
#define	RF_FIRST_PERSON		0x00004	// only draw through eyes (view weapon, damage blood blob)
#define	RF_DEPTHHACK		0x00008	// for view weapon Z crunching

#define RF_VOLUMETRIC		0x00020	// fake volumetric shading

#define	RF_NOSHADOW			0x00040	// don't add stencil shadows

#define RF_LIGHTING_ORIGIN	0x00080	// use refEntity->lightingOrigin instead of refEntity->origin
									// for lighting.  This allows entities to sink into the floor
									// with their origin going solid, and allows all parts of a
									// player to get the same lighting
#define	RF_SHADOW_PLANE		0x00100	// use refEntity->shadowPlane
#define	RF_WRAP_FRAMES		0x00200	// mod the model frames by the maxframes to allow continuous
										// animation without needing to know the frame count
#define	RF_CAP_FRAMES		0x00400	// cap the model frames by the maxframes for one shot anims

#define RF_ALPHA_FADE		0x00800	// hacks blend mode and uses whatever the set alpha is.
#define RF_PULSATE			0x01000	// for things like a dropped saber, where we want to add an extra visual clue
#define RF_RGB_TINT			0x02000	// overrides ent RGB color to the specified color

#define RF_FORKED			0x04000	// override lightning to have forks
#define RF_TAPERED			0x08000	// lightning tapers
#define RF_GROW				0x10000	// lightning grows from start to end during its life

#define RF_SETANIMINDEX		0x20000	//use backEnd.currentEntity->e.skinNum for R_BindAnimatedImage

#define RF_DISINTEGRATE1	0x40000	// does a procedural hole-ripping thing.
#define RF_DISINTEGRATE2	0x80000	// does a procedural hole-ripping thing with scaling at the ripping point

// refdef flags
#define RDF_NOWORLDMODEL	1		// used for player configuration screen
#define RDF_HYPERSPACE		4		// teleportation effect

typedef byte color4ub_t[4];

typedef struct {
	vec3_t		xyz;
	float		st[2];
	byte		modulate[4];
} polyVert_t;

typedef struct poly_s {
	qhandle_t			hShader;
	int					numVerts;
	polyVert_t			*verts;
} poly_t;

typedef enum 
{
	RT_MODEL,
	RT_POLY,
	RT_SPRITE,
	RT_ORIENTED_QUAD,
	RT_LINE,
	RT_ELECTRICITY,
	RT_CYLINDER,
	RT_LATHE,
	RT_BEAM,
	RT_SABER_GLOW,
	RT_PORTALSURFACE,		// doesn't draw anything, just info for portals
	RT_CLOUDS,

	RT_MAX_REF_ENTITY_TYPE
} refEntityType_t;

typedef struct {
	refEntityType_t	reType;
	int			renderfx;

	qhandle_t	hModel;				// opaque type outside refresh

	// most recent data
	vec3_t		lightingOrigin;		// so multi-part models can be lit identically (RF_LIGHTING_ORIGIN)
	float		shadowPlane;		// projection shadows go here, stencils go slightly lower

	vec3_t		axis[3];			// rotation vectors
	qboolean	nonNormalizedAxes;	// axis are not normalized, i.e. they have scale
	float		origin[3];			// also used as MODEL_BEAM's "from"
	int			frame;				// also used as MODEL_BEAM's diameter

	// previous data for frame interpolation
	float		oldorigin[3];		// also used as MODEL_BEAM's "to"
	int			oldframe;
	float		backlerp;			// 0.0 = current, 1.0 = old

	// texturing
	int			skinNum;			// inline skin index

	qhandle_t	customSkin;			// NULL for default skin
	qhandle_t	customShader;		// use one image for the entire thing

	// misc
	byte		shaderRGBA[4];		// colors used by colorSrc=vertex shaders
	float		shaderTexCoord[2];	// texture coordinates used by tcMod=vertex modifiers
	float		shaderTime;			// subtracted from refdef time to control effect start times

	// extra sprite information
	float		radius;

	// This doesn't have to be unioned, but it does make for more meaningful variable names :)
	union
	{
		float		rotation;
		float		endTime;
		float		saberLength;
	};

/*
Ghoul2 Insert Start
*/
	vec3_t		angles;				// rotation angles - used for Ghoul2

	vec3_t		modelScale;			// axis scale for models
	CGhoul2Info_v	*ghoul2;  		// has to be at the end of the ref-ent in order for it to be created properly
/*
Ghoul2 Insert End
*/

} refEntity_t;


#define	MAX_RENDER_STRINGS			8
#define	MAX_RENDER_STRING_LENGTH	32

typedef struct {
	int			x, y, width, height;
	float		fov_x, fov_y;
	vec3_t		vieworg;
	vec3_t		viewaxis[3];		// transformation matrix

	// time in milliseconds for shader effects and other time dependent rendering issues
	int			time;

	int			rdflags;			// RDF_NOWORLDMODEL, etc

	// 1 bits will prevent the associated area from rendering at all
	byte		areamask[MAX_MAP_AREA_BYTES];

	// text messages for deform text shaders
//	char		text[MAX_RENDER_STRINGS][MAX_RENDER_STRING_LENGTH];
} refdef_t;


typedef enum {
	STEREO_CENTER,
	STEREO_LEFT,
	STEREO_RIGHT
} stereoFrame_t;


/*
** glconfig_t
**
** Contains variables specific to the OpenGL configuration
** being run right now.  These are constant once the OpenGL
** subsystem is initialized.
*/
typedef enum {
	TC_NONE,
	TC_S3TC,
	TC_S3TC_DXT
} textureCompression_t;

typedef struct {
	const char				*renderer_string;
	const char				*vendor_string;
	const char				*version_string;
	const char				*extensions_string;

	int						maxTextureSize;			// queried from GL
	int						maxActiveTextures;		// multitexture ability

	int						colorBits, depthBits, stencilBits;

	qboolean				deviceSupportsGamma;
	textureCompression_t	textureCompression;
	qboolean				textureEnvAddAvailable;
	qboolean				textureFilterAnisotropicAvailable;
	qboolean				clampToEdgeAvailable;

	int						vidWidth, vidHeight;
	// aspect is the screen's physical width / height, which may be different
	// than scrWidth / scrHeight if the pixels are non-square
	// normal screens should be 4/3, but wide aspect monitors may be 16/9
	float					windowAspect;

	int						displayFrequency;

	// synonymous with "does rendering consume the entire screen?", therefore
	// a Voodoo or Voodoo2 will have this set to TRUE, as will a Win32 ICD that
	// used CDS.
	qboolean				isFullscreen;
	qboolean				stereoEnabled;
	qboolean				smpActive;		// dual processor
} glconfig_t;


#if !defined _WIN32

#define _3DFX_DRIVER_NAME	"libMesaVoodooGL.so.3.1"
#ifdef __linux__
#define OPENGL_DRIVER_NAME	"libGL.so.1"
#else
#define OPENGL_DRIVER_NAME	"libGL.so"
#endif

#else

#define OPENGL_DRIVER_NAME	"opengl32"

#endif	// !defined _WIN32


#endif	// __TR_TYPES_H
