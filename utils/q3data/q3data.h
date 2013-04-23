// q3data.h


#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <string.h>
#include <sys/stat.h>

#include "cmdlib.h"
#include "scriplib.h"
#include "mathlib.h"
#include "polyset.h"
#include "trilib.h"
#include "imagelib.h"
#include "threads.h"
#include "l3dslib.h"
#include "bspfile.h"
#include "p3dlib.h"
#include "3dslib.h"
#include "aselib.h"
#include "md3lib.h"

void Cmd_ASEConvert( qboolean grabAnims, qboolean bIsGhoul2);
void Cmd_ASEInitAnimGrab(void);
void Cmd_ASEFinalizeAnimGrab(void);
void Cmd_ASEAnimGrab(void);
void Cmd_3DSConvert( void );
void Cmd_Modelname (void);
void Cmd_SpriteBase (void);
void Cmd_Base (void);
void Cmd_Cd (void);
void Cmd_Origin (void);
void Cmd_ScaleUp (void);
void Cmd_Frame (void);
void Cmd_Modelname (void);
void Cmd_SpriteShader(void);
void Cmd_Skin(void);
void Cmd_Skinsize (void);
void FinishModel (int type);

void Cmd_Grab (void);
void Cmd_Raw (void);
void Cmd_Mip (void);
void Cmd_Environment (void);
void Cmd_Colormap (void);

void Cmd_File (void);
void Cmd_Dir (void);
void Cmd_StartWad (void);
void Cmd_EndWad (void);
void Cmd_Mippal (void);
void Cmd_Mipdir (void);

void Cmd_Video (void);

void ReleaseFile (char *filename);
void ReleaseShader( char *filename );

void Convert3DStoMD3( const char *filename );

void OrderMesh( int input[][3], int output[][3], int numTris, int maxVerts );
void CleanUpAfterGrabbing();

extern	byte		*byteimage, *lbmpalette;
extern	int			byteimagewidth, byteimageheight;

extern	qboolean	g_release;			// don't grab, copy output data to new tree
extern	char		g_releasedir[1024];	// c:\quake2\baseq2, etc
extern	qboolean	g_archive;			// don't grab, copy source data to new tree
extern	qboolean	do3ds;
extern	char		g_only[256];		// if set, only grab this cd
extern	qboolean	g_skipmodel;		// set true when a cd is not g_only
extern	qboolean	g_verbose;
extern  qboolean	gbKeyPress;

extern	char		*trifileext;

#define TYPE_ITEM		0
#define TYPE_PLAYER		1
#define TYPE_WEAPON		2
#define TYPE_HAND		3
#define TYPE_UNKNOWN	4
#define TYPE_GHOUL2_1FRAME 5
