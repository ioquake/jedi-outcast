
#include "stdafx.h"
#include "qe3.h"

#define	PAGEFLIPS	2

z_t		z;

/*
============
Z_Init
============
*/
void Z_Init (void)
{
	z.origin[0] = 0;
	z.origin[1] = 20;
	z.origin[2] = 46;

	z.scale = 1;
}



/*
============================================================================

  MOUSE ACTIONS

============================================================================
*/

static	int	cursorx, cursory;

/*
==============
Z_MouseDown
==============
*/
void Z_MouseDown (int x, int y, int buttons)
{
	vec3_t	org, dir, vup, vright;
	brush_t	*b;

	Sys_GetCursorPos (&cursorx, &cursory);

	vup[0] = 0; vup[1] = 0; vup[2] = 1/z.scale;

	VectorCopy (z.origin, org);
	org[2] += (y - (z.height/2))/z.scale;
	org[1] = MIN_WORLD_COORD;

	b = selected_brushes.next;
	if (b != &selected_brushes)
	{
		org[0] = (b->mins[0] + b->maxs[0])/2;
	}

	dir[0] = 0; dir[1] = 1; dir[2] = 0;

	vright[0] = 0; vright[1] = 0; vright[2] = 0;

	// new mouse code for ZClip, I'll do this stuff before falling through into the standard ZWindow mouse code...
	//
	if (g_pParentWnd->GetZWnd()->m_pZClip)	// should always be the case I think, but this is safer
	{
		bool bToggle = false;
		bool bSetTop = false;
		bool bSetBot = false;
		bool bReset  = false;

		if (g_PrefsDlg.m_nMouseButtons == 2)
		{
			// 2 button mice...
			//
			bToggle = (GetKeyState(VK_F1) & 0x8000);
			bSetTop = (GetKeyState(VK_F2) & 0x8000);
			bSetBot = (GetKeyState(VK_F3) & 0x8000);
			bReset  = (GetKeyState(VK_F4) & 0x8000);
		}
		else
		{	
			// 3 button mice...
			//
			bToggle = (buttons == (MK_RBUTTON|MK_SHIFT|MK_CONTROL));
			bSetTop = (buttons == (MK_RBUTTON|MK_SHIFT));
			bSetBot = (buttons == (MK_RBUTTON|MK_CONTROL));
			bReset  = (GetKeyState(VK_F4) & 0x8000);
		}				
		
		if (bToggle)
		{
			g_pParentWnd->GetZWnd()->m_pZClip->Enable(!(g_pParentWnd->GetZWnd()->m_pZClip->IsEnabled()));
		    Sys_UpdateWindows (W_ALL);
			return;
		}

		if (bSetTop)
		{
			g_pParentWnd->GetZWnd()->m_pZClip->SetTop(org[2]);
		    Sys_UpdateWindows (W_ALL);
			return;
		}
		
		if (bSetBot)
		{
			g_pParentWnd->GetZWnd()->m_pZClip->SetBottom(org[2]);
		    Sys_UpdateWindows (W_ALL);
			return;
		}

		if (bReset)
		{
			g_pParentWnd->GetZWnd()->m_pZClip->Reset();
		    Sys_UpdateWindows (W_ALL);
			return;
		}
	}

	// LBUTTON = manipulate selection
	// shift-LBUTTON = select
	// middle button = grab texture
	// ctrl-middle button = set entire brush to texture
	// ctrl-shift-middle button = set single face to texture
	//
	// see code above for these next 3, I just commented them here as well for clarity...
	//
	// ctrl-shift-RIGHT button = toggle ZClip on/off
	//      shift-RIGHT button = set ZClip top marker
	//       ctrl-RIGHT button = set ZClip bottom marker

  int nMouseButton = g_PrefsDlg.m_nMouseButtons == 2 ? MK_RBUTTON : MK_MBUTTON;
	if ( (buttons == MK_LBUTTON)
		|| (buttons == (MK_LBUTTON | MK_SHIFT))
		|| (buttons == MK_MBUTTON)
//		|| (buttons == (MK_MBUTTON|MK_CONTROL))
		|| (buttons == (nMouseButton|MK_SHIFT|MK_CONTROL)) )
	{
		Drag_Begin (x, y, buttons,
			vright, vup,
			org, dir);
		return;
	}

	// control mbutton = move camera
	if ((buttons == (MK_CONTROL|nMouseButton) ) || (buttons == (MK_CONTROL|MK_LBUTTON)))
	{	
		g_pParentWnd->GetCamera()->Camera().origin[2] = org[2] ;
		Sys_UpdateWindows (W_CAMERA|W_XY_OVERLAY|W_Z);
	}


}

/*
==============
Z_MouseUp
==============
*/
void Z_MouseUp (int x, int y, int buttons)
{
	Drag_MouseUp ();
}

/*
==============
Z_MouseMoved
==============
*/
void Z_MouseMoved (int x, int y, int buttons)
{
	if (!buttons)
		return;
	if (buttons == MK_LBUTTON)
	{
		Drag_MouseMoved (x, y, buttons);
		Sys_UpdateWindows (W_Z|W_CAMERA_IFON|W_XY);
		return;
	}
	// rbutton = drag z origin
	if (buttons == MK_RBUTTON)
	{
		Sys_GetCursorPos (&x, &y);
		if ( y != cursory)
		{
			z.origin[2] += y-cursory;
			Sys_SetCursorPos (cursorx, cursory);
			Sys_UpdateWindows (W_Z);
		}
		return;
	}
		// control mbutton = move camera
  int nMouseButton = g_PrefsDlg.m_nMouseButtons == 2 ? MK_RBUTTON : MK_MBUTTON;
	if ((buttons == (MK_CONTROL|nMouseButton) ) || (buttons == (MK_CONTROL|MK_LBUTTON)))
	{	
		g_pParentWnd->GetCamera()->Camera().origin[2] = (y - (z.height/2))/z.scale;
		Sys_UpdateWindows (W_CAMERA|W_XY_OVERLAY|W_Z);
	}

}


/*
============================================================================

DRAWING

============================================================================
*/


/*
==============
Z_DrawGrid
==============
*/
void Z_DrawGrid (void)
{
	float	zz, zb, ze;
	int		w, h;
	char	text[32];

	w = z.width/2 / z.scale;
	h = z.height/2 / z.scale;

	zb = z.origin[2] - h;
	if (zb < region_mins[2])
		zb = region_mins[2];
	zb = 64 * floor (zb/64);

	ze = z.origin[2] + h;
	if (ze > region_maxs[2])
		ze = region_maxs[2];
	ze = 64 * ceil (ze/64);

	// draw major blocks

	qglColor3fv(g_qeglobals.d_savedinfo.colors[COLOR_GRIDMAJOR]);

	qglBegin (GL_LINES);

	qglVertex2f (0, zb);
	qglVertex2f (0, ze);

	for (zz=zb ; zz<ze ; zz+=64)
	{
		qglVertex2f (-w, zz);
		qglVertex2f (w, zz);
	}

	qglEnd ();

	// draw minor blocks
	if (g_qeglobals.d_showgrid && g_qeglobals.d_gridsize*z.scale >= 4 &&
      g_qeglobals.d_savedinfo.colors[COLOR_GRIDMINOR] != g_qeglobals.d_savedinfo.colors[COLOR_GRIDBACK])
	{
		qglColor3fv(g_qeglobals.d_savedinfo.colors[COLOR_GRIDMINOR]);

		qglBegin (GL_LINES);
		for (zz=zb ; zz<ze ; zz+=g_qeglobals.d_gridsize)
		{
			if ( ! ((int)zz & 63) )
				continue;
			qglVertex2f (-w, zz);
			qglVertex2f (w, zz);
		}
		qglEnd ();
	}

	// draw coordinate text if needed

	qglColor3fv(g_qeglobals.d_savedinfo.colors[COLOR_GRIDTEXT]);

	for (zz=zb ; zz<ze ; zz+=64)
	{
		qglRasterPos2f (-w+1, zz);
		sprintf (text, "%i",(int)zz);
		qglCallLists (strlen(text), GL_UNSIGNED_BYTE, text);
	}
}

#define CAM_HEIGHT		48 // height of main part
#define CAM_GIZMO		8	// height of the gizmo

void ZDrawCameraIcon (void)
{
	float	x, y;
	int	xCam = z.width/4;

	x = 0;
	y = g_pParentWnd->GetCamera()->Camera().origin[2];

	qglColor3f (0.0, 0.0, 1.0);
	qglBegin(GL_LINE_STRIP);
	qglVertex3f (x-xCam,y,0);
	qglVertex3f (x,y+CAM_GIZMO,0);
	qglVertex3f (x+xCam,y,0);
	qglVertex3f (x,y-CAM_GIZMO,0);
	qglVertex3f (x-xCam,y,0);
	qglVertex3f (x+xCam,y,0);
	qglVertex3f (x+xCam,y-CAM_HEIGHT,0);
	qglVertex3f (x-xCam,y-CAM_HEIGHT,0);
	qglVertex3f (x-xCam,y,0);
	qglEnd ();

}

void ZDrawZClip()
{
	float x,y;
	
	x = 0;
	y = g_pParentWnd->GetCamera()->Camera().origin[2];

	if (g_pParentWnd->GetZWnd()->m_pZClip)	// should always be the case I think
		g_pParentWnd->GetZWnd()->m_pZClip->Paint();
}

GLbitfield glbitClear = GL_COLOR_BUFFER_BIT; //HACK

/*
==============
Z_Draw
==============
*/
void Z_Draw (void)
{
    brush_t	*brush;
	float	w, h;
	double	start, end;
	qtexture_t	*q;
	float	top, bottom;
	vec3_t	org_top, org_bottom, dir_up, dir_down;
	int xCam = z.width/3;

	if (!active_brushes.next)
		return;	// not valid yet

	if (z.timing)
		start = Sys_DoubleTime ();

	//
	// clear
	//
	qglViewport(0, 0, z.width, z.height);

	qglClearColor (
		g_qeglobals.d_savedinfo.colors[COLOR_GRIDBACK][0],
		g_qeglobals.d_savedinfo.colors[COLOR_GRIDBACK][1],
		g_qeglobals.d_savedinfo.colors[COLOR_GRIDBACK][2],
		0);

    /* GL Bug */ 
	/* When not using hw acceleration, gl will fault if we clear the depth 
	buffer bit on the first pass. The hack fix is to set the GL_DEPTH_BUFFER_BIT
	only after Z_Draw() has been called once. Yeah, right. */
	qglClear(glbitClear); 
	glbitClear |= GL_DEPTH_BUFFER_BIT;

	qglMatrixMode(GL_PROJECTION);

  qglLoadIdentity ();
	w = z.width/2 / z.scale;
	h = z.height/2 / z.scale;
	qglOrtho (-w, w, z.origin[2]-h, z.origin[2]+h, -8, 8);

	qglDisable(GL_TEXTURE_2D);
	qglDisable(GL_TEXTURE_1D);
	qglDisable(GL_DEPTH_TEST);
	qglDisable(GL_BLEND);


	//
	// now draw the grid
	//
	Z_DrawGrid ();

	//
	// draw stuff
	//

	qglDisable(GL_CULL_FACE);

	qglShadeModel (GL_FLAT);

	qglPolygonMode (GL_FRONT_AND_BACK, GL_FILL);

	qglDisable(GL_TEXTURE_2D);
	qglDisable(GL_BLEND);
	qglDisable(GL_DEPTH_TEST);


	// draw filled interiors and edges
	dir_up[0] = 0 ; dir_up[1] = 0; dir_up[2] = 1;
	dir_down[0] = 0 ; dir_down[1] = 0; dir_down[2] = -1;
	VectorCopy (z.origin, org_top);
	org_top[2] = MAX_WORLD_COORD;//4096;	// MAX_WORLD_COORD  ?  (John said this didn't work, Hmmmmmm)	// !suspect!
	VectorCopy (z.origin, org_bottom);
	org_bottom[2] = MIN_WORLD_COORD;//-4096;	// MIN_WORLD_COORD?    " "  !suspect!

	for (brush = active_brushes.next ; brush != &active_brushes ; brush=brush->next)
	{
		if (brush->mins[0] >= z.origin[0]
			|| brush->maxs[0] <= z.origin[0]
			|| brush->mins[1] >= z.origin[1]
			|| brush->maxs[1] <= z.origin[1])
			continue;

		if (!Brush_Ray (org_top, dir_down, brush, &top))
			continue;
		top = org_top[2] - top;
		if (!Brush_Ray (org_bottom, dir_up, brush, &bottom))
			continue;
		bottom = org_bottom[2] + bottom;

		q = Texture_ForName (brush->brush_faces->texdef.name);
		qglColor3f (q->color[0], q->color[1], q->color[2]);
		qglBegin (GL_QUADS);
		qglVertex2f (-xCam, bottom);
		qglVertex2f (xCam, bottom);
		qglVertex2f (xCam, top);
		qglVertex2f (-xCam, top);
		qglEnd ();

		qglColor3f (1,1,1);
		qglBegin (GL_LINE_LOOP);
		qglVertex2f (-xCam, bottom);
		qglVertex2f (xCam, bottom);
		qglVertex2f (xCam, top);
		qglVertex2f (-xCam, top);
		qglEnd ();
	}

	//
	// now draw selected brushes
	//
	for (brush = selected_brushes.next ; brush != &selected_brushes ; brush=brush->next)
	{
		if ( !(brush->mins[0] >= z.origin[0]
			|| brush->maxs[0] <= z.origin[0]
			|| brush->mins[1] >= z.origin[1]
			|| brush->maxs[1] <= z.origin[1]) )
		{
			if (Brush_Ray (org_top, dir_down, brush, &top))
			{
				top = org_top[2] - top;
				if (Brush_Ray (org_bottom, dir_up, brush, &bottom))
				{
					bottom = org_bottom[2] + bottom;

					q = Texture_ForName (brush->brush_faces->texdef.name);
					qglColor3f (q->color[0], q->color[1], q->color[2]);
					qglBegin (GL_QUADS);
					qglVertex2f (-xCam, bottom);
					qglVertex2f (xCam, bottom);
					qglVertex2f (xCam, top);
					qglVertex2f (-xCam, top);
					qglEnd ();
				}
			}
		}

	  qglColor3fv(g_qeglobals.d_savedinfo.colors[COLOR_SELBRUSHES]);
		qglBegin (GL_LINE_LOOP);
		qglVertex2f (-xCam, brush->mins[2]);
		qglVertex2f (xCam, brush->mins[2]);
		qglVertex2f (xCam, brush->maxs[2]);
		qglVertex2f (-xCam, brush->maxs[2]);
		qglEnd ();
	}


	ZDrawCameraIcon ();
	ZDrawZClip();

  qglFinish();
	QE_CheckOpenGLForErrors();

	if (z.timing)
	{
		end = Sys_DoubleTime ();
		Sys_Printf ("z: %i ms\n", (int)(1000*(end-start)));
	}
}

