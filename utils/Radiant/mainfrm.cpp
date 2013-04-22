// MainFrm.cpp : implementation of the CMainFrame class
//

#include "stdafx.h"
#include "Radiant.h"
#include "qe3.h"
#include "ZWnd.h"
#include "CamWnd.h"
#include "TexWnd.h"
#include "EditWnd.h"
#include "entityw.h"
#include "PrefsDlg.h"
#include "MapInfo.h"
#include "MainFrm.h"
#include "RotateDlg.h"
#include "EntityListDlg.h"
#include "ScriptDlg.h"
#include "NewProjDlg.h"
#include "CommandsDlg.h"
#include "3DFXCamWnd.h"
#include "inc.h"
#include "ScaleDialog.h"
#include "FindTextureDlg.h"
#include "SurfaceDlg.h"
#include "shlobj.h"
#include "DialogTextures.h"
#include "PatchDensityDlg.h"
#include "DialogThick.h"
#include "oddbits.h"
#include "sourcesafe.h"
#include "sourcesafesettings.h"
#include "groupnames.h"
#include "entkeyfindreplace.h"
#include <assert.h>
#include "../libs/pakstuff.h"
#include "autocaulk.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif


// globals
CString g_strAppPath;                   // holds the full path of the executable
CEdit* g_pEdit = NULL;                  // used originally to make qe4 work with mfc.. still used
CMainFrame* g_pParentWnd = NULL;        // used to precast to CMainFrame
CPrefsDlg g_Preferences;                // global prefs instance
CPrefsDlg& g_PrefsDlg = g_Preferences;  // reference used throughout
int g_nUpdateBits = 0;                  // window update flags
bool g_bScreenUpdates = true;           // whether window painting is active, used in a few places
                                        // to disable updates for speed reasons
                                        // both of the above should be made members of CMainFrame

bool g_bSnapToGrid = true;              // early use, no longer in use, clamping pref will be used
CString g_strProject;                   // holds the active project filename
CString g_cstrBehavEdPath = sCHC_BEHAVED_PATH;

#define MIN_ZOOM_SCALE	0.006f
#define MAX_ZOOM_SCALE	16.0f


/////////////////////////////////////////////////////////////////////////////
// CMainFrame

// command mapping stuff
//
// m_strCommand is the command string
// m_nKey is the windows VK_??? equivelant
// m_nModifiers are key states as follows
//  bit
//    0 - shift
//    1 - alt
//    2 - control
//    4 - press only
//
#define	SPEED_MOVE	32
#define	SPEED_TURN	22.5


SCommandInfo g_Commands[] = 
{
// shift control brought to top to insure they are looked at first MLC
  {"ShowClipBrushs",		'B',			RAD_SHIFT + RAD_CONTROL,	ID_VIEW_SHOWCLIP},
  {"ToggleCamera",			'C',			RAD_SHIFT + RAD_CONTROL,	ID_TOGGLECAMERA},
  {"ToggleDetailOnly",		'D',			RAD_SHIFT + RAD_CONTROL,	ID_VIEW_SHOW_DETAIL_ONLY},
  {"RedisperseCols",		'E',			RAD_SHIFT + RAD_CONTROL,	ID_CURVE_REDISPERSE_COLS},
  {"ShowFuncGroups",		'F',			RAD_SHIFT + RAD_CONTROL,	ID_VIEW_SHOWFUNCGROUPS},
  {"ShowLights",			'G',			RAD_SHIFT + RAD_CONTROL,	ID_VIEW_SHOWLIGHTS},
  {"ShowHintBrushs",		'H',			RAD_SHIFT + RAD_CONTROL,	ID_VIEW_SHOWHINT},
  {"InvertCurveTextureX",	'I',			RAD_SHIFT + RAD_CONTROL,	ID_CURVE_NEGATIVETEXTUREY},
  {"NextLeakSpot",			'K',			RAD_SHIFT + RAD_CONTROL,	ID_MISC_NEXTLEAKSPOT},
  {"PrevLeakSpot",			'L',			RAD_SHIFT + RAD_CONTROL,	ID_MISC_PREVIOUSLEAKSPOT},
  {"MakeDetail",			'M',			RAD_SHIFT + RAD_CONTROL,	ID_CURVE_MATRIX_TRANSPOSE},
  {"ShowEntities",			'N',			RAD_SHIFT + RAD_CONTROL,	ID_VIEW_SHOWENT},
  {"ToggleShowPatches",		'P',			RAD_SHIFT + RAD_CONTROL,	ID_CURVE_CYCLECAP},
  {"ToggleShowLightRadii",	'R',			RAD_SHIFT + RAD_CONTROL,	ID_VIEW_SHOWLIGHTRADII_KB},  
  {"CycleCapTextures",		'T',			RAD_SHIFT + RAD_CONTROL,	ID_CURVE_CYCLECAP},
  {"ShowCurves",			'U',			RAD_SHIFT + RAD_CONTROL,	ID_VIEW_SHOWCURVES},
  {"ToggleView",			'V',			RAD_SHIFT + RAD_CONTROL,	ID_TOGGLEVIEW},
  {"ClearPatchOverlays",	'Y',			RAD_SHIFT + RAD_CONTROL,	ID_CURVE_OVERLAY_CLEAR},
  {"ToggleZ",				'Z',			RAD_SHIFT + RAD_CONTROL,	ID_TOGGLEZ},
  {"IncPatchColumn",		VK_ADD,			RAD_SHIFT + RAD_CONTROL,	ID_CURVE_INSERTCOLUMN},
  {"DecPatchColumn",		VK_SUBTRACT,	RAD_SHIFT + RAD_CONTROL,	ID_CURVE_DELETECOLUMN}, 
  {"MapHome",				VK_HOME,		RAD_SHIFT + RAD_CONTROL,	ID_MAPHOME},

  {"ToggleEasy",			'1',			RAD_SHIFT,					ID_SHOW_EASY},
  {"ToggleMedium",			'2',			RAD_SHIFT,					ID_SHOW_MEDIUM},
  {"ToggleHard",			'3',			RAD_SHIFT,					ID_SHOW_HARD},
  {"AutoCaulk",				'A',			RAD_SHIFT,					ID_AUTOCAULK},
  {"DumpSelectedBrush",		'D',			RAD_SHIFT, 					ID_SELECTION_PRINT},
  {"FaceFit",				'F',			RAD_SHIFT,					ID_FACEFIT},
  {"ConnectSelection",		'K',			RAD_SHIFT,	 				ID_SELECTION_CONNECTSMART},
  {"ShowHidden",			'H',			RAD_SHIFT,					ID_VIEW_HIDESHOW_SHOWHIDDEN},
  {"InvertCurveTextureY",	'I',			RAD_SHIFT,					ID_CURVE_NEGATIVETEXTUREX},
  {"ClearNonSolid",			'L',			RAD_SHIFT,					ID_CLEAR_NONSOLID},
  {"TogTexRotLock",			'R',			RAD_SHIFT, 					ID_TOGGLE_ROTATELOCK},
  {"TogTexLock",			'T',			RAD_SHIFT, 					ID_TOGGLE_LOCK},
  {"PasteNoTargetNameChange",'V',			RAD_SHIFT,					ID_EDIT_PASTEBRUSHNOTARGETNAMECHANGE},
  {"TextureFit",			'5',			RAD_SHIFT, 					ID_SELECTION_TEXTURE_FIT},
  {"Patch TAB",				VK_TAB,			RAD_SHIFT,					ID_PATCH_TAB},
  {"SplitSelected",			VK_RETURN,		RAD_SHIFT, 					ID_SPLIT_SELECTED},
  {"TexDecrement",			VK_SUBTRACT,	RAD_SHIFT, 					ID_SELECTION_TEXTURE_DEC},
  {"TexIncrement",			VK_ADD,			RAD_SHIFT, 					ID_SELECTION_TEXTURE_INC},
  {"TexRotateClock",		VK_NEXT,		RAD_SHIFT, 					ID_SELECTION_TEXTURE_ROTATECLOCK},
  {"TexRotateCounter",		VK_PRIOR,		RAD_SHIFT, 					ID_SELECTION_TEXTURE_ROTATECOUNTER},
  {"TexShiftLeft",			VK_LEFT,		RAD_SHIFT, 					ID_SELECTION_TEXTURE_SHIFTLEFT},
  {"TexShiftRight",			VK_RIGHT,		RAD_SHIFT, 					ID_SELECTION_TEXTURE_SHIFTRIGHT},
  {"TexShiftUp",			VK_UP,			RAD_SHIFT, 					ID_SELECTION_TEXTURE_SHIFTUP},
  {"TexShiftDown",			VK_DOWN,		RAD_SHIFT, 					ID_SELECTION_TEXTURE_SHIFTDOWN},
  {"CubicClipZoomOut10",	219,			RAD_SHIFT,					ID_VIEW_CUBEOUT10},
  {"CubicClipZoomIn10",		221,			RAD_SHIFT, 					ID_VIEW_CUBEIN10},

  {"SelectNudgeDown",		VK_DOWN,		RAD_ALT, 					ID_SELECTION_SELECT_NUDGEDOWN},
  {"SelectNudgeLeft",		VK_LEFT,		RAD_ALT, 					ID_SELECTION_SELECT_NUDGELEFT},
  {"SelectNudgeRight",		VK_RIGHT,		RAD_ALT, 					ID_SELECTION_SELECT_NUDGERIGHT},
  {"SelectNudgeUp",			VK_UP,			RAD_ALT, 					ID_SELECTION_SELECT_NUDGEUP},

  {"RegionSetToSelection",	'B',			RAD_CONTROL, 				ID_REGION_SETSELECTION},
  {"Copy",					'C',			RAD_CONTROL, 				ID_EDIT_COPYBRUSH},
  {"ShowDetail",			'D',			RAD_CONTROL, 				ID_VIEW_SHOWDETAIL},
  {"RedisperseRows",		'E',			RAD_CONTROL, 				ID_CURVE_REDISPERSE_ROWS},
  {"RegionOff",				'F',			RAD_CONTROL, 				ID_REGION_OFF},
  {"SnapPatchToGrid",		'G',			RAD_CONTROL, 				ID_SELECT_SNAPTOGRID},
  {"InvertCurve",			'I',			RAD_CONTROL, 				ID_CURVE_NEGATIVE},
  {"ConnectSelection",		'K',			RAD_CONTROL, 				ID_SELECTION_CONNECT},
  {"ShowNonSolid",			'L',			RAD_CONTROL,				ID_VIEW_SHOW_NONSOLID},
  {"MakeDetail",			'M',			RAD_CONTROL, 				ID_SELECTION_MAKE_DETAIL},
  {"NaturalizePatch",		'N',			RAD_CONTROL, 				ID_PATCH_NATURALIZE},
  {"FileOpen",				'O',			RAD_CONTROL, 				ID_FILE_OPEN},
  {"ToggleShowPatches",		'P',			RAD_CONTROL, 				ID_VIEW_SHOWCURVES},
  {"ToggleRealtime",		'R',			RAD_CONTROL, 				ID_VIEW_CAMERAUPDATE},
  {"FileSave",				'S',			RAD_CONTROL, 				ID_FILE_SAVE},
  {"ThickenPatch",			'T',			RAD_CONTROL, 				ID_CURVE_THICKEN},
  {"MakeNonSolid",			'U',			RAD_CONTROL,				ID_MAKE_NONSOLID},  
  {"Paste",					'V',			RAD_CONTROL, 				ID_EDIT_PASTEBRUSH},
  {"WayPointToggle",		'W',			RAD_CONTROL,				ID_VIEW_SHOWWAYPOINTS},
  {"Exit",					'X',			RAD_CONTROL, 				ID_FILE_EXIT},
  {"RegionSetXY",			'Y',			RAD_CONTROL, 				ID_REGION_SETXY},
  {"Undo",					'Z',			RAD_CONTROL, 				ID_EDIT_UNDO},
  {"Brush3Sided",			'3',			RAD_CONTROL, 				ID_BRUSH_3SIDED},
  {"Brush4Sided",			'4',			RAD_CONTROL, 				ID_BRUSH_4SIDED},
  {"Brush5Sided",			'5',			RAD_CONTROL, 				ID_BRUSH_5SIDED},
  {"Brush6Sided",			'6',			RAD_CONTROL, 				ID_BRUSH_6SIDED},
  {"Brush7Sided",			'7',			RAD_CONTROL, 				ID_BRUSH_7SIDED},
  {"Brush8Sided",			'8',			RAD_CONTROL, 				ID_BRUSH_8SIDED},
  {"Brush9Sided",			'9',			RAD_CONTROL, 				ID_BRUSH_9SIDED},
  {"IncPatchRow",			VK_ADD,			RAD_CONTROL, 				ID_CURVE_INSERTROW},
  {"DecPatchRow",			VK_SUBTRACT,	RAD_CONTROL, 				ID_CURVE_DELETEROW},
  {"NextView",				VK_TAB,			RAD_CONTROL, 				ID_VIEW_NEXTVIEW},
  {"FlipClip",				VK_RETURN,		RAD_CONTROL, 				ID_FLIP_CLIP},
  {"ZZoomOut",				VK_INSERT,		RAD_CONTROL, 				ID_VIEW_ZZOOMOUT},
  {"ZZoomIn",				VK_DELETE,		RAD_CONTROL, 				ID_VIEW_ZZOOMIN},
  {"TexScaleUp",			VK_UP,			RAD_CONTROL, 				ID_SELECTION_TEXTURE_SCALEUP},
  {"TexScaleDown",			VK_DOWN,		RAD_CONTROL, 				ID_SELECTION_TEXTURE_SCALEDOWN},
  {"TexScaleLeft",			VK_LEFT,		RAD_CONTROL, 				ID_SELECTION_TEXTURE_SCALELEFT},
  {"TexScaleRight",			VK_RIGHT,		RAD_CONTROL, 				ID_SELECTION_TEXTURE_SCALERIGHT},
  {"CubicClipZoomOut",		219,			RAD_CONTROL, 				ID_VIEW_CUBEOUT},
  {"CubicClipZoomIn",		221,			RAD_CONTROL, 				ID_VIEW_CUBEIN},
  {"ToggleCubicClip",		'\\',			RAD_CONTROL, 				ID_VIEW_CUBICCLIPPING},
  {"CloneSelectionNoTargetNameChange", 
							VK_SPACE,		RAD_CONTROL,				ID_SELECTION_CLONE_NOTARGETNAMECHANGE},  
  {"FindOrReplaceEnt",		VK_F3,			RAD_CONTROL,				ID_MISC_FINDENT},

  {"ToggleSizePaint",		'Q',			RAD_PRESS, 					ID_SELECTION_TOGGLESIZEPAINT},

  {"FindOrReplaceNextEnt",	VK_F3,			0,							ID_MISC_FINDNEXTENT},
  {"CameraAngleUp",			'A',			0, 							ID_CAMERA_ANGLEUP},
  {"BendMode",				'B',			0, 							ID_PATCH_BEND},
  {"CameraDown",			'C',			0, 							ID_CAMERA_DOWN},
  {"CameraUp",				'D',			0, 							ID_CAMERA_UP},
  {"DragEdges",				'E',			0, 							ID_SELECTION_DRAGEDGES},
  {"HideSelected",			'H',			0,							ID_VIEW_HIDESHOW_HIDESELECTED},
  {"EntityColor",			'K',			0, 							ID_MISC_SELECTENTITYCOLOR},
  {"EntityList",			'L',			0, 							ID_ENTITYLIST},
  {"MapInfo",				'M',			0, 							ID_MAPINFO},
  {"ViewEntityInfo",		'N',			0, 							ID_VIEW_ENTITY},
  {"ViewConsole",			'O',			0, 							ID_VIEW_CONSOLE},
  {"ToggleConsole",			'O',			0, 							ID_TOGGLECONSOLE},
  {"Preferences",			'P',			0, 							ID_PREFS},
  {"MouseRotate",			'R',			0, 							ID_SELECT_MOUSEROTATE},
  {"SurfaceInspector",		'S',			0, 							ID_TEXTURES_INSPECTOR},
  {"ViewTextures",			'T',			0, 							ID_VIEW_TEXTURE},
  {"DragVertices",			'V',			0, 							ID_SELECTION_DRAGVERTECIES},
  {"ToggleClipper",			'X',			0, 							ID_VIEW_CLIPPER},
  {"MakeOverlayPatch",		'Y',			0, 							ID_CURVE_OVERLAY_SET},
  {"CameraAngleDown",		'Z',			0, 							ID_CAMERA_ANGLEDOWN},
  {"ToggleGrid",			'0',			0, 							ID_GRID_TOGGLE},
  {"SetGrid1",				'1',			0, 							ID_GRID_1},
  {"SetGrid2",				'2',			0, 							ID_GRID_2},
  {"SetGrid4",				'3',			0, 							ID_GRID_4},
  {"SetGrid8",				'4',			0, 							ID_GRID_8},
  {"SetGrid16",				'5',			0, 							ID_GRID_16},
  {"SetGrid32",				'6',			0, 							ID_GRID_32},
  {"SetGrid64",				'7',			0, 							ID_GRID_64},
  {"Patch TAB",				VK_TAB,			0, 							ID_PATCH_TAB},
  {"CameraForward",			VK_UP,			0, 							ID_CAMERA_FORWARD},
  {"CameraBack",			VK_DOWN,		0, 							ID_CAMERA_BACK},
  {"CameraLeft",			VK_LEFT,		0, 							ID_CAMERA_LEFT},
  {"CameraRight",			VK_RIGHT,		0, 							ID_CAMERA_RIGHT},
  {"CameraStrafeRight",		VK_PERIOD,		0, 							ID_CAMERA_STRAFERIGHT},
  {"CameraStrafeLeft",		VK_COMMA,		0, 							ID_CAMERA_STRAFELEFT},
  {"CloneSelection",		VK_SPACE,		0, 							ID_SELECTION_CLONE},
  {"DeleteSelection",		VK_BACK,		0, 							ID_SELECTION_DELETE},
  {"UnSelectSelection",		VK_ESCAPE,		0, 							ID_SELECTION_DESELECT},
  {"CenterView",			VK_END,			0, 							ID_VIEW_CENTER},
  {"ZoomOut",				VK_INSERT,		0, 							ID_VIEW_ZOOMOUT},
  {"ZoomIn",				VK_DELETE,		0, 							ID_VIEW_ZOOMIN},
  {"UpFloor",				VK_PRIOR,		0, 							ID_VIEW_UPFLOOR},
  {"DownFloor",				VK_NEXT,		0, 							ID_VIEW_DOWNFLOOR},
  {"ClipSelected",			VK_RETURN,		0, 							ID_CLIP_SELECTED},
  {"MoveSelectionDOWN",		VK_SUBTRACT,	0, 							ID_SELECTION_MOVEDOWN},
  {"MoveSelectionUP",		VK_ADD,			0, 							ID_SELECTION_MOVEUP},
  {"GridDown",				219,			0, 							ID_GRID_PREV},
  {"GridUp",				221,			0, 							ID_GRID_NEXT},
  //{"ForceCameraWalk", 'Q', RAD_PRESS, ID_CAMERA_ACTIVE}
};

int g_nCommandCount = sizeof(g_Commands) / sizeof(SCommandInfo);

SKeyInfo g_Keys[] =
{
  {"SPACE",				VK_SPACE},
  {"BACKSPACE",			VK_BACK},
  {"ESCAPE",			VK_ESCAPE},
  {"END",				VK_END},
  {"INSERT",			VK_INSERT},
  {"DELETE",			VK_DELETE},
  {"PAGEUP",			VK_PRIOR},
  {"PAGEDOWN",			VK_NEXT},
  {"UP",				VK_UP},
  {"DOWN",				VK_DOWN},
  {"LEFT",				VK_LEFT},
  {"RIGHT",				VK_RIGHT},
  {"F1",				VK_F1},
  {"F2",				VK_F2},
  {"F3",				VK_F3},
  {"F4",				VK_F4},
  {"F5",				VK_F5},
  {"F6",				VK_F6},
  {"F7",				VK_F7},
  {"F8",				VK_F8},
  {"F9",				VK_F9},
  {"F10",				VK_F10},
  {"F11",				VK_F11},
  {"F12",				VK_F12},
  {"TAB",				VK_TAB},
  {"RETURN",			VK_RETURN},
  {"COMMA",				VK_COMMA},
  {"PERIOD",			VK_PERIOD},
  {"PLUS",				VK_ADD},
  {"MULTIPLY",			VK_MULTIPLY},
  {"SUBTRACT",			VK_SUBTRACT},
  {"NUMPAD0",			VK_NUMPAD0},
  {"NUMPAD1",			VK_NUMPAD1},
  {"NUMPAD2",			VK_NUMPAD2},
  {"NUMPAD3",			VK_NUMPAD3},
  {"NUMPAD4",			VK_NUMPAD4},
  {"NUMPAD5",			VK_NUMPAD5},
  {"NUMPAD6",			VK_NUMPAD6},
  {"NUMPAD7",			VK_NUMPAD7},
  {"NUMPAD8",			VK_NUMPAD8},
  {"NUMPAD9",			VK_NUMPAD9},
  {"[",					219},
  {"]",					221}
};

int g_nKeyCount = sizeof(g_Keys) / sizeof(SKeyInfo);

const int CMD_TEXTUREWAD_END = CMD_TEXTUREWAD + 127;
const int CMD_BSPCOMMAND_END = CMD_BSPCOMMAND + 127;
const int IDMRU_END = IDMRU+NBMRUMENU;

const int g_msgBSPDone = RegisterWindowMessage("_BSPDone");
const int g_msgBSPStatus = RegisterWindowMessage("_BSPStatus");

IMPLEMENT_DYNAMIC(CMainFrame, CFrameWnd)

BEGIN_MESSAGE_MAP(CMainFrame, CFrameWnd)
	//{{AFX_MSG_MAP(CMainFrame)
	ON_WM_PARENTNOTIFY()
	ON_WM_CREATE()
	ON_WM_TIMER()
	ON_WM_DESTROY()
	ON_WM_CLOSE()
	ON_WM_KEYDOWN()
	ON_WM_SIZE()
  ON_COMMAND(ID_VIEW_CAMERATOGGLE, ToggleCamera)
	ON_COMMAND(ID_FILE_CLOSE, OnFileClose)
	ON_COMMAND(ID_FILE_EXIT, OnFileExit)
	ON_COMMAND(ID_FILE_LOADPROJECT, OnFileLoadproject)
	ON_COMMAND(ID_FILE_NEW, OnFileNew)
	ON_COMMAND(ID_FILE_OPEN, OnFileOpen)
	ON_COMMAND(ID_FILE_POINTFILE, OnFilePointfile)
	ON_COMMAND(ID_FILE_PRINT, OnFilePrint)
	ON_COMMAND(ID_FILE_PRINT_PREVIEW, OnFilePrintPreview)
	ON_COMMAND(ID_FILE_SAVE, OnFileSave)
	ON_COMMAND(ID_FILE_SAVEAS, OnFileSaveas)
	ON_COMMAND(ID_VIEW_100, OnView100)
	ON_COMMAND(ID_VIEW_CENTER, OnViewCenter)
	ON_COMMAND(ID_VIEW_CONSOLE, OnViewConsole)
	ON_COMMAND(ID_VIEW_DOWNFLOOR, OnViewDownfloor)
	ON_COMMAND(ID_VIEW_ENTITY, OnViewEntity)
	ON_COMMAND(ID_VIEW_FRONT, OnViewFront)
	ON_COMMAND(ID_VIEW_SHOWBLOCKS, OnViewShowblocks)
	ON_COMMAND(ID_VIEW_SHOWCLIP, OnViewShowclip)
	ON_COMMAND(ID_VIEW_SHOWCOORDINATES, OnViewShowcoordinates)
	ON_COMMAND(ID_VIEW_SHOWDETAIL, OnViewShowdetail)
	ON_COMMAND(ID_VIEW_SHOW_DETAIL_ONLY, OnViewShowDetailOnly)
	ON_COMMAND(ID_SHOW_EASY, OnViewShowEasy)
	ON_COMMAND(ID_SHOW_MEDIUM, OnViewShowMedium)
	ON_COMMAND(ID_SHOW_HARD, OnViewShowHard)
	ON_COMMAND(ID_VIEW_SHOW_NONSOLID, OnViewShowNonSolid)
	ON_COMMAND(ID_MAKE_NONSOLID, OnMakeNonSolid)	
	ON_COMMAND(ID_CLEAR_NONSOLID, OnClearNonSolid)
	ON_COMMAND(ID_VIEW_SHOWENT, OnViewShowent)
	ON_COMMAND(ID_VIEW_SHOWLIGHTS, OnViewShowlights)
	ON_COMMAND(ID_VIEW_SHOWNAMES, OnViewShownames)
	ON_COMMAND(ID_VIEW_SHOWPATH, OnViewShowpath)
	ON_COMMAND(ID_VIEW_SHOWWATER, OnViewShowwater)
	ON_COMMAND(ID_VIEW_SHOWWORLD, OnViewShowworld)
	ON_COMMAND(ID_VIEW_TEXTURE, OnViewTexture)
	ON_COMMAND(ID_VIEW_UPFLOOR, OnViewUpfloor)
	ON_COMMAND(ID_VIEW_XY, OnViewXy)
	ON_COMMAND(ID_VIEW_Z100, OnViewZ100)
	ON_COMMAND(ID_VIEW_ZOOMIN, OnViewZoomin)
	ON_COMMAND(ID_VIEW_ZOOMOUT, OnViewZoomout)
	ON_COMMAND(ID_VIEW_ZZOOMIN, OnViewZzoomin)
	ON_COMMAND(ID_VIEW_ZZOOMOUT, OnViewZzoomout)
	ON_COMMAND(ID_VIEW_SIDE, OnViewSide)
	ON_COMMAND(ID_TEXTURES_SHOWINUSE, OnTexturesShowinuse)
	ON_COMMAND(ID_TEXTURES_INSPECTOR, OnTexturesInspector)
	ON_COMMAND(ID_MISC_BENCHMARK, OnMiscBenchmark)
	ON_COMMAND(ID_MISC_FINDBRUSH, OnMiscFindbrush)
	ON_COMMAND(ID_MISC_GAMMA, OnMiscGamma)
	ON_COMMAND(ID_MISC_NEXTLEAKSPOT, OnMiscNextleakspot)
	ON_COMMAND(ID_MISC_PREVIOUSLEAKSPOT, OnMiscPreviousleakspot)
	ON_COMMAND(ID_MISC_PRINTXY, OnMiscPrintxy)
	ON_COMMAND(ID_MISC_SELECTENTITYCOLOR, OnMiscSelectentitycolor)
	ON_COMMAND(ID_TEXTUREBK, OnTexturebk)
	ON_COMMAND(ID_COLORS_MAJOR, OnColorsMajor)
	ON_COMMAND(ID_COLORS_MINOR, OnColorsMinor)
	ON_COMMAND(ID_COLORS_XYBK, OnColorsXybk)
	ON_COMMAND(ID_BRUSH_3SIDED, OnBrush3sided)
	ON_COMMAND(ID_BRUSH_4SIDED, OnBrush4sided)
	ON_COMMAND(ID_BRUSH_5SIDED, OnBrush5sided)
	ON_COMMAND(ID_BRUSH_6SIDED, OnBrush6sided)
	ON_COMMAND(ID_BRUSH_7SIDED, OnBrush7sided)
	ON_COMMAND(ID_BRUSH_8SIDED, OnBrush8sided)
	ON_COMMAND(ID_BRUSH_9SIDED, OnBrush9sided)
	ON_COMMAND(ID_BRUSH_ARBITRARYSIDED, OnBrushArbitrarysided)
	ON_COMMAND(ID_BRUSH_FLIPX, OnBrushFlipx)
	ON_COMMAND(ID_BRUSH_FLIPY, OnBrushFlipy)
	ON_COMMAND(ID_BRUSH_FLIPZ, OnBrushFlipz)
	ON_COMMAND(ID_BRUSH_ROTATEX, OnBrushRotatex)
	ON_COMMAND(ID_BRUSH_ROTATEY, OnBrushRotatey)
	ON_COMMAND(ID_BRUSH_ROTATEZ, OnBrushRotatez)
	ON_COMMAND(ID_REGION_OFF, OnRegionOff)
	ON_COMMAND(ID_REGION_SETBRUSH, OnRegionSetbrush)
	ON_COMMAND(ID_REGION_SETSELECTION, OnRegionSetselection)
	ON_COMMAND(ID_REGION_SETTALLBRUSH, OnRegionSettallbrush)
	ON_COMMAND(ID_REGION_SETXY, OnRegionSetxy)
	ON_COMMAND(ID_SELECTION_ARBITRARYROTATION, OnSelectionArbitraryrotation)
	ON_COMMAND(ID_SELECTION_CLONE, OnSelectionClone)
	ON_COMMAND(ID_SELECTION_CLONE_NOTARGETNAMECHANGE, OnSelectionClone_NoTargetNameChange)
	ON_COMMAND(ID_SELECTION_CONNECT, OnSelectionConnect)
	ON_COMMAND(ID_SELECTION_CONNECTSMART, OnSelectionConnectSmart)	
	ON_COMMAND(ID_SELECTION_CSGSUBTRACT, OnSelectionCsgsubtract)
	ON_COMMAND(ID_SELECTION_DELETE, OnSelectionDelete)
	ON_COMMAND(ID_SELECTION_DESELECT, OnSelectionDeselect)
	ON_COMMAND(ID_SELECTION_DRAGEDGES, OnSelectionDragedges)
	ON_COMMAND(ID_SELECTION_DRAGVERTECIES, OnSelectionDragvertecies)
	ON_COMMAND(ID_SELECTION_MAKE_DETAIL, OnSelectionMakeDetail)
	ON_COMMAND(ID_SELECTION_MAKE_STRUCTURAL, OnSelectionMakeStructural)
	ON_COMMAND(ID_SELECTION_HIDEWAYPOINTCHILDREN, OnSelectionHideWaypointChildren)
	ON_COMMAND(ID_SELECTION_UNHIDEWAYPOINTCHILDREN, OnSelectionUnHideWaypointChildren)
	ON_COMMAND(ID_SELECTION_UNHIDEALLWAYPOINTS, OnSelectionUnHideAllWaypoints)
	ON_COMMAND(ID_SELECTION_MAKEHOLLOW, OnSelectionMakehollow)
	ON_COMMAND(ID_SELECTION_SELECTCOMPLETETALL, OnSelectionSelectcompletetall)
	ON_COMMAND(ID_SELECTION_SELECTINSIDE, OnSelectionSelectinside)
	ON_COMMAND(ID_SELECTION_SELECTPARTIALTALL, OnSelectionSelectpartialtall)
	ON_COMMAND(ID_SELECTION_SELECTTOUCHING, OnSelectionSelecttouching)
	ON_COMMAND(ID_SELECTION_UNGROUPENTITY, OnSelectionUngroupentity)
	ON_COMMAND(ID_TEXTURES_POPUP, OnTexturesPopup)
	ON_COMMAND(ID_POPUP_SELECTION, OnPopupSelection)
	ON_COMMAND(ID_VIEW_CHANGE, OnViewChange)
	ON_COMMAND(ID_VIEW_CAMERAUPDATE, OnViewCameraupdate)
	ON_UPDATE_COMMAND_UI(ID_VIEW_CAMERAUPDATE, OnUpdateViewCameraupdate)
	ON_WM_SIZING()
	ON_COMMAND(ID_HELP_ABOUT, OnHelpAbout)
	ON_COMMAND(ID_VIEW_CLIPPER, OnViewClipper)
	ON_COMMAND(ID_CAMERA_ANGLEDOWN, OnCameraAngledown)
	ON_COMMAND(ID_CAMERA_ANGLEUP, OnCameraAngleup)
	ON_COMMAND(ID_CAMERA_BACK, OnCameraBack)
	ON_COMMAND(ID_CAMERA_DOWN, OnCameraDown)
	ON_COMMAND(ID_CAMERA_FORWARD, OnCameraForward)
	ON_COMMAND(ID_CAMERA_LEFT, OnCameraLeft)
	ON_COMMAND(ID_CAMERA_RIGHT, OnCameraRight)
	ON_COMMAND(ID_CAMERA_STRAFELEFT, OnCameraStrafeleft)
	ON_COMMAND(ID_CAMERA_STRAFERIGHT, OnCameraStraferight)
	ON_COMMAND(ID_CAMERA_UP, OnCameraUp)
	ON_COMMAND(ID_GRID_TOGGLE, OnGridToggle)
	ON_COMMAND(ID_ENTITYLIST, OnEntitylist)
	ON_COMMAND(ID_MAPINFO, OnMapinfo)
	ON_COMMAND(ID_PREFS, OnPrefs)
	ON_COMMAND(ID_TOGGLECAMERA, OnTogglecamera)
	ON_COMMAND(ID_TOGGLECONSOLE, OnToggleconsole)
	ON_COMMAND(ID_TOGGLEVIEW, OnToggleview)
	ON_COMMAND(ID_TOGGLEZ, OnTogglez)
	ON_COMMAND(ID_TOGGLE_LOCK, OnToggleLock)
	ON_COMMAND(ID_EDIT_MAPINFO, OnEditMapinfo)
	ON_COMMAND(ID_EDIT_ENTITYINFO, OnEditEntityinfo)
	ON_COMMAND(ID_BRUSH_SCRIPTS, OnBrushScripts)
	ON_COMMAND(ID_VIEW_NEXTVIEW, OnViewNextview)
	ON_COMMAND(ID_HELP_COMMANDLIST, OnHelpCommandlist)
	ON_COMMAND(ID_FILE_NEWPROJECT, OnFileNewproject)
	ON_WM_KEYUP()
	ON_COMMAND(ID_FLIP_CLIP, OnFlipClip)
	ON_COMMAND(ID_CLIP_SELECTED, OnClipSelected)
	ON_COMMAND(ID_SPLIT_SELECTED, OnSplitSelected)
	ON_COMMAND(ID_TOGGLEVIEW_XZ, OnToggleviewXz)
	ON_COMMAND(ID_TOGGLEVIEW_YZ, OnToggleviewYz)
	ON_COMMAND(ID_COLORS_BRUSH, OnColorsBrush)
	ON_COMMAND(ID_COLORS_CLIPPER, OnColorsClipper)
	ON_COMMAND(ID_COLORS_GRIDTEXT, OnColorsGridtext)
	ON_COMMAND(ID_COLORS_SELECTEDBRUSH, OnColorsSelectedbrush)
	ON_COMMAND(ID_COLORS_GRIDBLOCK, OnColorsGridblock)
	ON_COMMAND(ID_COLORS_VIEWNAME, OnColorsViewname)
	ON_COMMAND(ID_COLOR_SETORIGINAL, OnColorSetoriginal)
	ON_COMMAND(ID_COLOR_SETQER, OnColorSetqer)
	ON_COMMAND(ID_COLOR_SETBLACK, OnColorSetblack)
	ON_COMMAND(ID_SNAPTOGRID, OnSnaptogrid)
	ON_COMMAND(ID_SELECT_SCALE, OnSelectScale)
	ON_COMMAND(ID_SELECT_MOUSEROTATE, OnSelectMouserotate)
	ON_COMMAND(ID_EDIT_COPYBRUSH, OnEditCopybrush)
	ON_COMMAND(ID_EDIT_PASTEBRUSH, OnEditPastebrush)
	ON_COMMAND(ID_EDIT_PASTEBRUSHNOTARGETNAMECHANGE,OnEditPastebrushNoTargetnameChange)
	ON_COMMAND(ID_EDIT_UNDO, OnEditUndo)
	ON_UPDATE_COMMAND_UI(ID_EDIT_UNDO, OnUpdateEditUndo)
	ON_COMMAND(ID_SELECTION_TEXTURE_DEC, OnSelectionTextureDec)
	ON_COMMAND(ID_SELECTION_TEXTURE_FIT, OnSelectionTextureFit)
	ON_COMMAND(ID_SELECTION_TEXTURE_INC, OnSelectionTextureInc)
	ON_COMMAND(ID_SELECTION_TEXTURE_ROTATECLOCK, OnSelectionTextureRotateclock)
	ON_COMMAND(ID_SELECTION_TEXTURE_ROTATECOUNTER, OnSelectionTextureRotatecounter)
	ON_COMMAND(ID_SELECTION_TEXTURE_SCALEDOWN, OnSelectionTextureScaledown)
	ON_COMMAND(ID_SELECTION_TEXTURE_SCALEUP, OnSelectionTextureScaleup)
	ON_COMMAND(ID_SELECTION_TEXTURE_SHIFTDOWN, OnSelectionTextureShiftdown)
	ON_COMMAND(ID_SELECTION_TEXTURE_SHIFTLEFT, OnSelectionTextureShiftleft)
	ON_COMMAND(ID_SELECTION_TEXTURE_SHIFTRIGHT, OnSelectionTextureShiftright)
	ON_COMMAND(ID_SELECTION_TEXTURE_SHIFTUP, OnSelectionTextureShiftup)
	ON_COMMAND(ID_GRID_NEXT, OnGridNext)
	ON_COMMAND(ID_GRID_PREV, OnGridPrev)
	ON_COMMAND(ID_SELECTION_TEXTURE_SCALELEFT, OnSelectionTextureScaleLeft)
	ON_COMMAND(ID_SELECTION_TEXTURE_SCALERIGHT, OnSelectionTextureScaleRight)
	ON_COMMAND(ID_TEXTURE_REPLACEALL, OnTextureReplaceall)
	ON_COMMAND(ID_SCALELOCKX, OnScalelockx)
	ON_COMMAND(ID_SCALELOCKY, OnScalelocky)
	ON_COMMAND(ID_SCALELOCKZ, OnScalelockz)
	ON_COMMAND(ID_SELECT_MOUSESCALE, OnSelectMousescale)
	ON_COMMAND(ID_VIEW_CUBICCLIPPING, OnViewCubicclipping)
	ON_COMMAND(ID_FILE_IMPORT, OnFileImport)
	ON_COMMAND(ID_FILE_PROJECTSETTINGS, OnFileProjectsettings)
	ON_UPDATE_COMMAND_UI(ID_FILE_IMPORT, OnUpdateFileImport)
	ON_COMMAND(ID_VIEW_CUBEIN, OnViewCubein)
	ON_COMMAND(ID_VIEW_CUBEOUT, OnViewCubeout)
	ON_COMMAND(ID_VIEW_CUBEIN10, OnViewCubein10)
	ON_COMMAND(ID_VIEW_CUBEOUT10, OnViewCubeout10)
	ON_COMMAND(ID_FILE_SAVEREGION, OnFileSaveregion)
	ON_UPDATE_COMMAND_UI(ID_FILE_SAVEREGION, OnUpdateFileSaveregion)
	ON_COMMAND(ID_SELECTION_MOVEDOWN, OnSelectionMovedown)
	ON_COMMAND(ID_SELECTION_MOVEUP, OnSelectionMoveup)
	ON_COMMAND(ID_TOOLBAR_MAIN, OnToolbarMain)
	ON_COMMAND(ID_TOOLBAR_TEXTURE, OnToolbarTexture)
	ON_COMMAND(ID_SELECTION_PRINT, OnSelectionPrint)
	ON_COMMAND(ID_SELECTION_TOGGLESIZEPAINT, OnSelectionTogglesizepaint)
	ON_COMMAND(ID_BRUSH_MAKECONE, OnBrushMakecone)
	ON_COMMAND(ID_TEXTURES_LOAD, OnTexturesLoad)
	ON_COMMAND(ID_TOGGLE_ROTATELOCK, OnToggleRotatelock)
	ON_COMMAND(ID_CURVE_BEVEL, OnCurveBevel)
	ON_COMMAND(ID_CURVE_CYLINDER, OnCurveCylinder)
	ON_COMMAND(ID_CURVE_EIGHTHSPHERE, OnCurveEighthsphere)
	ON_COMMAND(ID_CURVE_ENDCAP, OnCurveEndcap)
	ON_COMMAND(ID_CURVE_HEMISPHERE, OnCurveHemisphere)
	ON_COMMAND(ID_CURVE_INVERTCURVE, OnCurveInvertcurve)
	ON_COMMAND(ID_CURVE_QUARTER, OnCurveQuarter)
	ON_COMMAND(ID_CURVE_SPHERE, OnCurveSphere)
	ON_COMMAND(ID_FILE_IMPORTMAP, OnFileImportmap)
	ON_COMMAND(ID_FILE_EXPORTMAP, OnFileExportmap)
	ON_COMMAND(ID_EDIT_LOADPREFAB, OnEditLoadprefab)
	ON_COMMAND(ID_VIEW_SHOWWAYPOINTS, OnViewShowWayPoints)
	ON_COMMAND(ID_VIEW_SHOWWAYPOINTS_ONLY, OnViewShowWayPointsOnly)
	ON_COMMAND(ID_VIEW_SHOWMISC_MODEL, OnViewShowMiscModels)
	ON_COMMAND(ID_VIEW_SHOWMISC_MODEL_BREAKABLE, OnViewShowMiscModelBreakables)
	ON_COMMAND(ID_VIEW_SHOWMISC_MODEL_XXXX, OnViewShowMiscModelXXXX)	
	ON_COMMAND(ID_VIEW_SHOWTRIGGER_XXXX, OnViewShowTriggerXXXX)
	ON_COMMAND(ID_VIEW_SHOWTARGET_SPEAKER, OnViewShowTargetSpeakers)
	ON_COMMAND(ID_VIEW_SHOWREF_TAGS, OnViewShowRefTags)
	ON_COMMAND(ID_VIEW_SHOWCURVES, OnViewShowcurves)
	ON_COMMAND(ID_SELECTION_SELECT_NUDGEDOWN, OnSelectionSelectNudgedown)
	ON_COMMAND(ID_SELECTION_SELECT_NUDGELEFT, OnSelectionSelectNudgeleft)
	ON_COMMAND(ID_SELECTION_SELECT_NUDGERIGHT, OnSelectionSelectNudgeright)
	ON_COMMAND(ID_SELECTION_SELECT_NUDGEUP, OnSelectionSelectNudgeup)
	ON_WM_SYSKEYDOWN()
	ON_COMMAND(ID_TEXTURES_LOADLIST, OnTexturesLoadlist)
	ON_COMMAND(ID_DONTSELECTCURVE, OnDontselectcurve)
	ON_COMMAND(ID_CONVERTCURVES, OnConvertcurves)
	ON_COMMAND(ID_VIEW_SHOWFACES, OnShowFaces)	
	ON_COMMAND(ID_DYNAMIC_LIGHTING, OnDynamicLighting)
	ON_COMMAND(ID_CURVE_SIMPLEPATCHMESH, OnCurveSimplepatchmesh)
	ON_COMMAND(ID_PATCH_SHOWBOUNDINGBOX, OnPatchToggleBox)
	ON_COMMAND(ID_PATCH_WIREFRAME, OnPatchWireframe)
	ON_COMMAND(ID_CURVE_PATCHCONE, OnCurvePatchcone)
	ON_COMMAND(ID_CURVE_PATCHTUBE, OnCurvePatchtube)
	ON_COMMAND(ID_PATCH_WELD, OnPatchWeld)
	ON_COMMAND(ID_CURVE_PATCHBEVEL, OnCurvePatchbevel)
	ON_COMMAND(ID_CURVE_PATCHENDCAP, OnCurvePatchendcap)
	ON_COMMAND(ID_CURVE_PATCHINVERTEDBEVEL, OnCurvePatchinvertedbevel)
	ON_COMMAND(ID_CURVE_PATCHINVERTEDENDCAP, OnCurvePatchinvertedendcap)
	ON_COMMAND(ID_PATCH_DRILLDOWN, OnPatchDrilldown)
	ON_COMMAND(ID_CURVE_INSERTCOLUMN, OnCurveInsertcolumn)
	ON_COMMAND(ID_CURVE_INSERTROW, OnCurveInsertrow)
	ON_COMMAND(ID_CURVE_DELETECOLUMN, OnCurveDeletecolumn)
	ON_COMMAND(ID_MAPHOME, OnMapHome)
	ON_COMMAND(ID_CURVE_DELETEROW, OnCurveDeleterow)
	ON_COMMAND(ID_CURVE_INSERT_ADDCOLUMN, OnCurveInsertAddcolumn)
	ON_COMMAND(ID_CURVE_INSERT_ADDROW, OnCurveInsertAddrow)
	ON_COMMAND(ID_CURVE_INSERT_INSERTCOLUMN, OnCurveInsertInsertcolumn)
	ON_COMMAND(ID_CURVE_INSERT_INSERTROW, OnCurveInsertInsertrow)
	ON_COMMAND(ID_CURVE_NEGATIVE, OnCurveNegative)
	ON_COMMAND(ID_CURVE_NEGATIVETEXTUREX, OnCurveNegativeTextureX)
	ON_COMMAND(ID_CURVE_NEGATIVETEXTUREY, OnCurveNegativeTextureY)
	ON_COMMAND(ID_CURVE_DELETE_FIRSTCOLUMN, OnCurveDeleteFirstcolumn)
	ON_COMMAND(ID_CURVE_DELETE_FIRSTROW, OnCurveDeleteFirstrow)
	ON_COMMAND(ID_CURVE_DELETE_LASTCOLUMN, OnCurveDeleteLastcolumn)
	ON_COMMAND(ID_CURVE_DELETE_LASTROW, OnCurveDeleteLastrow)
	ON_COMMAND(ID_PATCH_BEND, OnPatchBend)
	ON_COMMAND(ID_PATCH_INSDEL, OnPatchInsdel)
	ON_COMMAND(ID_PATCH_ENTER, OnPatchEnter)
	ON_COMMAND(ID_PATCH_TAB, OnPatchTab)
	ON_COMMAND(ID_CURVE_PATCHDENSETUBE, OnCurvePatchdensetube)
	ON_COMMAND(ID_CURVE_PATCHVERYDENSETUBE, OnCurvePatchverydensetube)
	ON_COMMAND(ID_CURVE_CAP, OnCurveCap)
	ON_COMMAND(ID_CURVE_CAP_INVERTEDBEVEL, OnCurveCapInvertedbevel)
	ON_COMMAND(ID_CURVE_CAP_INVERTEDENDCAP, OnCurveCapInvertedendcap)
	ON_COMMAND(ID_CURVE_REDISPERSE_COLS, OnCurveRedisperseCols)
	ON_COMMAND(ID_CURVE_REDISPERSE_ROWS, OnCurveRedisperseRows)
	ON_COMMAND(ID_PATCH_NATURALIZE, OnPatchNaturalize)
	ON_COMMAND(ID_SELECT_SNAPTOGRID, OnSnapToGrid)
	ON_COMMAND(ID_CURVE_PATCHSQUARE, OnCurvePatchsquare)
	ON_COMMAND(ID_TEXTURES_TEXTUREWINDOWSCALE_10, OnTexturesTexturewindowscale10)
	ON_COMMAND(ID_TEXTURES_TEXTUREWINDOWSCALE_100, OnTexturesTexturewindowscale100)
	ON_COMMAND(ID_TEXTURES_TEXTUREWINDOWSCALE_200, OnTexturesTexturewindowscale200)
	ON_COMMAND(ID_TEXTURES_TEXTUREWINDOWSCALE_25, OnTexturesTexturewindowscale25)
	ON_COMMAND(ID_TEXTURES_TEXTUREWINDOWSCALE_50, OnTexturesTexturewindowscale50)
	ON_COMMAND(ID_TEXTURES_FLUSH, OnTexturesFlush)
	ON_COMMAND(ID_CURVE_OVERLAY_CLEAR, OnCurveOverlayClear)
	ON_COMMAND(ID_CURVE_OVERLAY_SET, OnCurveOverlaySet)
	ON_COMMAND(ID_CURVE_THICKEN, OnCurveThicken)
	ON_COMMAND(ID_CURVE_CYCLECAP, OnCurveCyclecap)
	ON_COMMAND(ID_CURVE_MATRIX_TRANSPOSE, OnCurveMatrixTranspose)
	ON_COMMAND(ID_TEXTURES_RELOADSHADERS, OnTexturesReloadshaders)
	ON_COMMAND(ID_SHOW_ENTITIES, OnShowEntities)
	ON_COMMAND(ID_VIEW_ENTITIESAS_BOUNDINGBOX, OnViewEntitiesasBoundingbox)
	ON_COMMAND(ID_VIEW_ENTITIESAS_SELECTEDSKINNED, OnViewEntitiesasSelectedskinned)
	ON_COMMAND(ID_VIEW_ENTITIESAS_SELECTEDWIREFRAME, OnViewEntitiesasSelectedwireframe)
	ON_COMMAND(ID_VIEW_ENTITIESAS_SKINNED, OnViewEntitiesasSkinned)
	ON_COMMAND(ID_VIEW_ENTITIESAS_SKINNEDANDBOXED, OnViewEntitiesasSkinnedandboxed)
	ON_COMMAND(ID_VIEW_ENTITIESAS_WRITEFRAME, OnViewEntitiesasWriteframe)
	ON_COMMAND(ID_PLUGINS_REFRESH, OnPluginsRefresh)
	ON_COMMAND(ID_VIEW_SHOWHINT, OnViewShowhint)
	ON_UPDATE_COMMAND_UI(ID_TEXTURES_SHOWINUSE, OnUpdateTexturesShowinuse)
	ON_UPDATE_COMMAND_UI(ID_VIEW_SHOWFACES, OnUpdateMiscShowFaces)	
	ON_COMMAND(ID_SS_STATUS, OnSSStatus)
	ON_COMMAND(ID_SS_ADD, OnSSAdd)
	ON_COMMAND(ID_SS_CHECKIN, OnSSCheckIn)
	ON_COMMAND(ID_SS_CHECKOUT, OnSSCheckOut)
	ON_COMMAND(ID_SS_UNDOCHECKOUT, OnSSUndoCheckOut)
	ON_COMMAND(ID_SS_HISTORY, OnSSHistory)
	ON_COMMAND(ID_SS_CONFIGURE, OnSSConfigure)
	ON_COMMAND(ID_MISC_FINDENT, OnFindEnt)
	ON_COMMAND(ID_MISC_FINDNEXTENT, OnFindNextEnt)
	ON_COMMAND(ID_FACEFIT,OnFaceFit)
	ON_COMMAND(ID_VIEW_HIDESHOW_HIDESELECTED, OnViewHideshowHideselected)
	ON_COMMAND(ID_VIEW_HIDESHOW_SHOWHIDDEN, OnViewHideshowShowhidden)
	ON_COMMAND(ID_AUTOCAULK, OnAutocaulk)
	ON_UPDATE_COMMAND_UI(ID_AUTOCAULK, OnUpdateAutocaulk)
	ON_COMMAND(ID_VIEW_SHOWFUNCGROUPS, OnViewShowfuncgroups)
	ON_COMMAND(ID_VIEW_SHOWLIGHTRADII, OnViewShowlightradii)
	ON_COMMAND(ID_VIEW_SHOWLIGHTRADII_KB, OnViewShowlightradii_KB)
	ON_COMMAND(ID_VIEW_SHOWCURVESONLY, OnViewShowcurvesonly)
	//}}AFX_MSG_MAP
  ON_COMMAND_RANGE(CMD_TEXTUREWAD, CMD_TEXTUREWAD_END, OnTextureWad)
  ON_COMMAND_RANGE(CMD_BSPCOMMAND, CMD_BSPCOMMAND_END, OnBspCommand)
  ON_COMMAND_RANGE(IDMRU, IDMRU_END, OnMru)
  ON_COMMAND_RANGE(ID_VIEW_NEAREST, ID_TEXTURES_FLATSHADE, OnViewNearest)
  ON_COMMAND_RANGE(ID_GRID_1, ID_GRID_64, OnGrid1)
  ON_COMMAND_RANGE(ID_PLUGIN_START, ID_PLUGIN_END, OnPlugIn)
  ON_REGISTERED_MESSAGE(g_msgBSPDone, OnBSPDone)
  ON_REGISTERED_MESSAGE(g_msgBSPStatus, OnBSPStatus)
  ON_MESSAGE(WM_DISPLAYCHANGE, OnDisplayChange)

END_MESSAGE_MAP()

static UINT indicators[] =
{
	ID_SEPARATOR,           // status line indicator
	ID_SEPARATOR,           // status line indicator
	ID_SEPARATOR,           // status line indicator
	ID_SEPARATOR,           // status line indicator
	ID_SEPARATOR,           // status line indicator
	ID_SEPARATOR,           // status line indicator
};

void CMainFrame::OnDisplayChange(UINT wParam, long lParam)
{
  int n = wParam;
}


void CMainFrame::OnBSPStatus(UINT wParam, long lParam)
{
}

void CMainFrame::OnBSPDone(UINT wParam, long lParam)
{
  DLLBuildDone();
}


/////////////////////////////////////////////////////////////////////////////
// CMainFrame construction/destruction

CMainFrame::CMainFrame()
{
  m_bDoLoop = false;
  m_bSplittersOK = false;
  g_pParentWnd = this;
  m_pXYWnd = NULL;
  m_pCamWnd = NULL;
  m_pTexWnd = NULL;
  m_pZWnd = NULL;
  m_pEditWnd = NULL;
  m_pYZWnd = NULL;
  m_pXZWnd = NULL;
  m_pActiveXY = NULL;
  m_bCamPreview = true;
}

CMainFrame::~CMainFrame()
{
}

void HandlePopup(CWnd* pWindow, unsigned int uId)
{
  // Get the current position of the mouse
  CPoint ptMouse;
  GetCursorPos(&ptMouse);

  // Load up a menu that has the options we are looking for in it
  CMenu mnuPopup;
  VERIFY(mnuPopup.LoadMenu(uId));
  mnuPopup.GetSubMenu(0)->TrackPopupMenu(TPM_LEFTALIGN | TPM_LEFTBUTTON |
      TPM_RIGHTBUTTON, ptMouse.x, ptMouse.y,pWindow);
  mnuPopup.DestroyMenu();

  // Set focus back to window
  pWindow->SetFocus();
}


void CMainFrame::OnParentNotify(UINT message, LPARAM lParam) 
{
}

void CMainFrame::SetButtonMenuStates()
{
  CMenu* pMenu = GetMenu();
  if (pMenu)
  {
		// by default all of these are checked because that's how they're defined in the menu editor
		if ( !g_qeglobals.d_savedinfo.show_names )
			pMenu->CheckMenuItem(ID_VIEW_SHOWNAMES, MF_BYCOMMAND | MF_UNCHECKED);
		if ( !g_qeglobals.d_savedinfo.show_coordinates )
			pMenu->CheckMenuItem(ID_VIEW_SHOWCOORDINATES, MF_BYCOMMAND | MF_UNCHECKED );
		if ( !g_qeglobals.show_blocks)
			pMenu->CheckMenuItem(ID_VIEW_SHOWBLOCKS, MF_BYCOMMAND | MF_UNCHECKED );
		if ( g_qeglobals.d_savedinfo.exclude & EXCLUDE_LIGHTS )
			pMenu->CheckMenuItem(ID_VIEW_SHOWLIGHTS, MF_BYCOMMAND | MF_UNCHECKED );
		if ( g_qeglobals.d_savedinfo.exclude & EXCLUDE_ENT )
			pMenu->CheckMenuItem(ID_VIEW_SHOWENT, MF_BYCOMMAND | MF_UNCHECKED );
		if ( g_qeglobals.d_savedinfo.exclude & EXCLUDE_FUNC_GROUP )
			pMenu->CheckMenuItem(ID_VIEW_SHOWFUNCGROUPS, MF_BYCOMMAND | MF_UNCHECKED );
		if ( g_qeglobals.d_savedinfo.exclude & EXCLUDE_PATHS )
			pMenu->CheckMenuItem(ID_VIEW_SHOWPATH, MF_BYCOMMAND | MF_UNCHECKED );
		if ( g_qeglobals.d_savedinfo.exclude & EXCLUDE_WATER )
			pMenu->CheckMenuItem(ID_VIEW_SHOWWATER, MF_BYCOMMAND | MF_UNCHECKED );
		if ( g_qeglobals.d_savedinfo.exclude & EXCLUDE_WORLD )
			pMenu->CheckMenuItem(ID_VIEW_SHOWWORLD, MF_BYCOMMAND | MF_UNCHECKED );
		if ( g_qeglobals.d_savedinfo.exclude & EXCLUDE_CLIP )
			pMenu->CheckMenuItem(ID_VIEW_SHOWCLIP, MF_BYCOMMAND | MF_UNCHECKED );
		if ( g_qeglobals.d_savedinfo.exclude & EXCLUDE_HINT )
			pMenu->CheckMenuItem(ID_VIEW_SHOWHINT, MF_BYCOMMAND | MF_UNCHECKED );
		if ( g_qeglobals.d_savedinfo.exclude & EXCLUDE_DETAIL )
			pMenu->CheckMenuItem(ID_VIEW_SHOWDETAIL, MF_BYCOMMAND | MF_UNCHECKED );

#ifdef QUAKE3
		if ( g_qeglobals.d_savedinfo.exclude & EXCLUDE_CURVES )
			pMenu->CheckMenuItem(ID_VIEW_SHOWCURVES, MF_BYCOMMAND | MF_UNCHECKED );
		g_qeglobals.d_savedinfo.exclude &= ~SHOW_CURVES_ONLY;	// lose this option on re-read always!!!!
		if ( !(g_qeglobals.d_savedinfo.exclude & SHOW_CURVES_ONLY) )
			pMenu->CheckMenuItem(ID_VIEW_SHOWCURVESONLY, MF_BYCOMMAND | MF_UNCHECKED );
		if ( g_qeglobals.d_savedinfo.exclude & EXCLUDE_WAYPOINTS )
			pMenu->CheckMenuItem(ID_VIEW_SHOWWAYPOINTS, MF_BYCOMMAND | MF_UNCHECKED );		
		g_qeglobals.d_savedinfo.exclude &= ~SHOW_WAYPOINTS_ONLY;	// lose this option on re-read always!!!!
		if ( !(g_qeglobals.d_savedinfo.exclude & SHOW_WAYPOINTS_ONLY) )
			pMenu->CheckMenuItem(ID_VIEW_SHOWWAYPOINTS_ONLY, MF_BYCOMMAND | MF_UNCHECKED );		
		if ( g_qeglobals.d_savedinfo.exclude & EXCLUDE_MISCMODELS )
			pMenu->CheckMenuItem(ID_VIEW_SHOWMISC_MODEL, MF_BYCOMMAND | MF_UNCHECKED );
		if ( g_qeglobals.d_savedinfo.exclude & EXCLUDE_MISCMODELBREAKABLES )
			pMenu->CheckMenuItem(ID_VIEW_SHOWMISC_MODEL_BREAKABLE, MF_BYCOMMAND | MF_UNCHECKED );
		if ( g_qeglobals.d_savedinfo.exclude & EXCLUDE_MISCMODELXXXX )
			pMenu->CheckMenuItem(ID_VIEW_SHOWMISC_MODEL_XXXX, MF_BYCOMMAND | MF_UNCHECKED );
		if ( g_qeglobals.d_savedinfo.exclude & EXCLUDE_TRIGGERXXXX )
			pMenu->CheckMenuItem(ID_VIEW_SHOWTRIGGER_XXXX, MF_BYCOMMAND | MF_UNCHECKED );
		if ( g_qeglobals.d_savedinfo.exclude & EXCLUDE_TARGETSPEAKERS )
			pMenu->CheckMenuItem(ID_VIEW_SHOWTARGET_SPEAKER, MF_BYCOMMAND | MF_UNCHECKED );
		if ( g_qeglobals.d_savedinfo.exclude & EXCLUDE_REFTAGS )
			pMenu->CheckMenuItem(ID_VIEW_SHOWREF_TAGS, MF_BYCOMMAND | MF_UNCHECKED );
		if ( g_qeglobals.d_savedinfo.exclude & EXCLUDE_NONSOLID )
			pMenu->CheckMenuItem(ID_VIEW_SHOW_NONSOLID, MF_BYCOMMAND | MF_UNCHECKED );

		if ( g_qeglobals.d_savedinfo.exclude & EXCLUDE_EASY )
			pMenu->CheckMenuItem(ID_SHOW_EASY, MF_BYCOMMAND | MF_UNCHECKED );
		if ( g_qeglobals.d_savedinfo.exclude & EXCLUDE_MEDIUM )
			pMenu->CheckMenuItem(ID_SHOW_MEDIUM, MF_BYCOMMAND | MF_UNCHECKED );
		if ( g_qeglobals.d_savedinfo.exclude & EXCLUDE_HARD )
			pMenu->CheckMenuItem(ID_SHOW_HARD, MF_BYCOMMAND | MF_UNCHECKED );
		g_qeglobals.d_savedinfo.exclude &= ~EXCLUDE_ALL_EXCEPT_DETAIL;	// lose this option on re-read always!!!!
		if ( !(g_qeglobals.d_savedinfo.exclude & EXCLUDE_ALL_EXCEPT_DETAIL) )
			pMenu->CheckMenuItem(ID_VIEW_SHOW_DETAIL_ONLY, MF_BYCOMMAND | MF_UNCHECKED );
		if ( g_qeglobals.d_savedinfo.exclude & EXCLUDE_LIGHTS_RADII )
			pMenu->CheckMenuItem(ID_VIEW_SHOWLIGHTRADII, MF_BYCOMMAND | MF_UNCHECKED );
#endif	

    pMenu->CheckMenuItem(ID_TOGGLE_LOCK, MF_BYCOMMAND | (g_PrefsDlg.m_bTextureLock) ? MF_CHECKED : MF_UNCHECKED);
    pMenu->CheckMenuItem(ID_TOGGLE_ROTATELOCK, MF_BYCOMMAND | (g_PrefsDlg.m_bRotateLock) ? MF_CHECKED : MF_UNCHECKED);
    pMenu->CheckMenuItem(ID_VIEW_CUBICCLIPPING, MF_BYCOMMAND | (g_PrefsDlg.m_bCubicClipping) ? MF_CHECKED : MF_UNCHECKED);
    if (m_wndToolBar.GetSafeHwnd())
      m_wndToolBar.GetToolBarCtrl().CheckButton(ID_VIEW_CUBICCLIPPING, (g_PrefsDlg.m_bCubicClipping) ? TRUE : FALSE);

    int n = g_PrefsDlg.m_nTextureScale;
    int id;
    switch (n)
    {
      case 10 : id = ID_TEXTURES_TEXTUREWINDOWSCALE_10; break;
      case 25 : id = ID_TEXTURES_TEXTUREWINDOWSCALE_25; break;
      case 50 : id = ID_TEXTURES_TEXTUREWINDOWSCALE_50; break;
      case 200 : id = ID_TEXTURES_TEXTUREWINDOWSCALE_200; break;
      default : id = ID_TEXTURES_TEXTUREWINDOWSCALE_100; break;
    }
    CheckTextureScale(id);


	}
  if (g_qeglobals.d_project_entity)
  {
    FillTextureMenu();      // redundant but i'll clean it up later.. yeah right.. 
	  FillBSPMenu();
#ifdef QUAKE3
	  LoadMruInReg(g_qeglobals.d_lpMruMenu,"Software\\id\\Q3Trek_QuakeEd4\\MRU");
#else
	  LoadMruInReg(g_qeglobals.d_lpMruMenu,"Software\\id\\QuakeEd4\\MRU");
#endif

    PlaceMenuMRUItem(g_qeglobals.d_lpMruMenu,::GetSubMenu(::GetMenu(GetSafeHwnd()),0), ID_FILE_EXIT);
  }
}

int CMainFrame::OnCreate(LPCREATESTRUCT lpCreateStruct)
{

  //Init3Dfx();

  char* pBuffer = g_strAppPath.GetBufferSetLength(_MAX_PATH + 1);
  int nResult = ::GetModuleFileName(NULL, pBuffer, _MAX_PATH);
  ASSERT(nResult != 0);
  pBuffer[g_strAppPath.ReverseFind('\\') + 1] = '\0';
  g_strAppPath.ReleaseBuffer();

  InitCommonControls ();
	g_qeglobals.d_hInstance = AfxGetInstanceHandle();
  MFCCreate(AfxGetInstanceHandle());

  //g_PrefsDlg.LoadPrefs();
  
  if (CFrameWnd::OnCreate(lpCreateStruct) == -1)
		return -1;

  UINT nStyle;
  UINT nID = (g_PrefsDlg.m_bWideToolbar) ? IDR_TOOLBAR_ADVANCED : IDR_TOOLBAR1;

	if (!m_wndToolBar.Create(this) ||
		!m_wndToolBar.LoadToolBar(nID))
	{
		TRACE0("Failed to create toolbar\n");
		return -1;      // fail to create
	}

	if (!m_wndStatusBar.Create(this) ||
		!m_wndStatusBar.SetIndicators(indicators,
		  sizeof(indicators)/sizeof(UINT)))
	{
		TRACE0("Failed to create status bar\n");
		return -1;      // fail to create
	}

#if 0
	if (!m_wndScaleBar.Create(this) ||
		!m_wndScaleBar.LoadToolBar(IDR_TOOLBAR_SCALELOCK))
	{
		TRACE0("Failed to create scaling toolbar\n");
		return -1;      // fail to create
	}
#endif

	// TODO: Remove this if you don't want tool tips or a resizeable toolbar
	m_wndToolBar.SetBarStyle(m_wndToolBar.GetBarStyle() |
		CBRS_TOOLTIPS | CBRS_FLYBY | CBRS_SIZE_DYNAMIC);

	// TODO: Delete these three lines if you don't want the toolbar to
	//  be dockable
	m_wndToolBar.EnableDocking(CBRS_ALIGN_ANY);
	EnableDocking(CBRS_ALIGN_ANY);
	DockControlBar(&m_wndToolBar);

	//m_wndScaleBar.SetBarStyle(m_wndScaleBar.GetBarStyle() |	CBRS_TOOLTIPS | CBRS_FLYBY | CBRS_SIZE_DYNAMIC);
	//m_wndScaleBar.EnableDocking(CBRS_ALIGN_ANY);
  //m_wndScaleBar.ShowWindow(SW_HIDE);

  int nImage;
  int nIndex = m_wndToolBar.CommandToIndex(ID_VIEW_CAMERATOGGLE);
  if (nIndex >= 0)
  {
    m_wndToolBar.GetButtonInfo(nIndex, nID, nStyle, nImage);
    m_wndToolBar.SetButtonInfo(nIndex, nID, TBBS_CHECKBOX, nImage);
    m_wndToolBar.GetToolBarCtrl().CheckButton(nID);
  }
  m_bCamPreview = true;

  nIndex = m_wndToolBar.CommandToIndex(ID_VIEW_CUBICCLIPPING);
  if (nIndex >= 0)
  {
    m_wndToolBar.GetButtonInfo(nIndex, nID, nStyle, nImage);
    m_wndToolBar.SetButtonInfo(nIndex, nID, TBBS_CHECKBOX, nImage);
  }
  nIndex = m_wndToolBar.CommandToIndex(ID_VIEW_ENTITY);
  if (nIndex >= 0)
  {
    m_wndToolBar.GetButtonInfo(nIndex, nID, nStyle, nImage);
    m_wndToolBar.SetButtonInfo(nIndex, nID, TBBS_CHECKBOX, nImage);
  }
  nIndex = m_wndToolBar.CommandToIndex(ID_VIEW_CLIPPER);
  if (nIndex >= 0)
  {
    m_wndToolBar.GetButtonInfo(nIndex, nID, nStyle, nImage);
    m_wndToolBar.SetButtonInfo(nIndex, nID, TBBS_CHECKBOX, nImage);
  }
  nIndex = m_wndToolBar.CommandToIndex(ID_SELECT_MOUSEROTATE);
  if (nIndex >= 0)
  {
    m_wndToolBar.GetButtonInfo(nIndex, nID, nStyle, nImage);
    m_wndToolBar.SetButtonInfo(nIndex, nID, TBBS_CHECKBOX, nImage);
  }

  nIndex = m_wndToolBar.CommandToIndex(ID_SELECT_MOUSESCALE);
  if (nIndex >= 0)
  {
    m_wndToolBar.GetButtonInfo(nIndex, nID, nStyle, nImage);
    m_wndToolBar.SetButtonInfo(nIndex, nID, TBBS_CHECKBOX, nImage);
  }

  nIndex = m_wndToolBar.CommandToIndex(ID_SCALELOCKX);
  if (nIndex >= 0)
  {
    m_wndToolBar.GetButtonInfo(nIndex, nID, nStyle, nImage);
    m_wndToolBar.SetButtonInfo(nIndex, nID, TBBS_CHECKBOX, nImage);
    m_wndToolBar.GetToolBarCtrl().CheckButton(ID_SCALELOCKX, TRUE);
  }

  nIndex = m_wndToolBar.CommandToIndex(ID_SCALELOCKY);
  if (nIndex >= 0)
  {
    m_wndToolBar.GetButtonInfo(nIndex, nID, nStyle, nImage);
    m_wndToolBar.SetButtonInfo(nIndex, nID, TBBS_CHECKBOX, nImage);
    m_wndToolBar.GetToolBarCtrl().CheckButton(ID_SCALELOCKY, TRUE);
  }

  nIndex = m_wndToolBar.CommandToIndex(ID_SCALELOCKZ);
  if (nIndex >= 0)
  {
    m_wndToolBar.GetButtonInfo(nIndex, nID, nStyle, nImage);
    m_wndToolBar.SetButtonInfo(nIndex, nID, TBBS_CHECKBOX, nImage);
    m_wndToolBar.GetToolBarCtrl().CheckButton(ID_SCALELOCKZ, TRUE);
  }

#ifdef QUAKE3
  nIndex = m_wndToolBar.CommandToIndex(ID_DONTSELECTCURVE);
  if (nIndex >= 0)
  {
    m_wndToolBar.GetButtonInfo(nIndex, nID, nStyle, nImage);
    m_wndToolBar.SetButtonInfo(nIndex, nID, TBBS_CHECKBOX, nImage);
	m_wndToolBar.GetToolBarCtrl().CheckButton(ID_DONTSELECTCURVE, (g_PrefsDlg.m_bSelectCurves) ? FALSE : TRUE);
  }
  nIndex = m_wndToolBar.CommandToIndex(ID_PATCH_SHOWBOUNDINGBOX);
  if (nIndex >= 0)
  {
    m_wndToolBar.GetButtonInfo(nIndex, nID, nStyle, nImage);
    m_wndToolBar.SetButtonInfo(nIndex, nID, TBBS_CHECKBOX, nImage);
    m_wndToolBar.GetToolBarCtrl().CheckButton(ID_PATCH_SHOWBOUNDINGBOX, g_bPatchShowBounds);
  }
  nIndex = m_wndToolBar.CommandToIndex(ID_PATCH_WELD);
  if (nIndex >= 0)
  {
    m_wndToolBar.GetButtonInfo(nIndex, nID, nStyle, nImage);
    m_wndToolBar.SetButtonInfo(nIndex, nID, TBBS_CHECKBOX, nImage);
    m_wndToolBar.GetToolBarCtrl().CheckButton(ID_PATCH_WELD, TRUE);
  }
  nIndex = m_wndToolBar.CommandToIndex(ID_PATCH_DRILLDOWN);
  if (nIndex >= 0)
  {
    m_wndToolBar.GetButtonInfo(nIndex, nID, nStyle, nImage);
    m_wndToolBar.SetButtonInfo(nIndex, nID, TBBS_CHECKBOX, nImage);
    m_wndToolBar.GetToolBarCtrl().CheckButton(ID_PATCH_DRILLDOWN, TRUE);
  }
  nIndex = m_wndToolBar.CommandToIndex(ID_PATCH_BEND);
  if (nIndex >= 0)
  {
    m_wndToolBar.GetButtonInfo(nIndex, nID, nStyle, nImage);
    m_wndToolBar.SetButtonInfo(nIndex, nID, TBBS_CHECKBOX, nImage);
  }
  nIndex = m_wndToolBar.CommandToIndex(ID_PATCH_INSDEL);
  if (nIndex >= 0)
  {
    m_wndToolBar.GetButtonInfo(nIndex, nID, nStyle, nImage);
    m_wndToolBar.SetButtonInfo(nIndex, nID, TBBS_CHECKBOX, nImage);
  }
#else
  m_wndToolBar.GetToolBarCtrl().HideButton(ID_DONTSELECTCURVE);
  m_wndToolBar.GetToolBarCtrl().HideButton(ID_PATCH_SHOWBOUNDINGBOX);
  m_wndToolBar.GetToolBarCtrl().HideButton(ID_PATCH_WELD);
  m_wndToolBar.GetToolBarCtrl().HideButton(ID_PATCH_WIREFRAME);
#endif
  g_nScaleHow = (SCALE_X | SCALE_Y | SCALE_Z);


#ifdef QUAKE3
  g_pParentWnd->GetMenu()->DestroyMenu();
  CMenu* pMenu = new CMenu();
  pMenu->LoadMenu(IDR_MENU_QUAKE3);
  g_pParentWnd->SetMenu(pMenu);
#else
  CMenu* pMenu = GetMenu();
#endif

  m_wndTextureBar.Create(this, IDD_TEXTUREBAR, CBRS_BOTTOM, 7433);
  m_wndTextureBar.EnableDocking(CBRS_ALIGN_ANY);
  DockControlBar(&m_wndTextureBar);
 
  g_qeglobals.d_lpMruMenu = CreateMruMenuDefault();

  m_bAutoMenuEnable = FALSE;

  LoadCommandMap();

  CFont* pFont = new CFont();
  pFont->CreatePointFont(g_PrefsDlg.m_nStatusSize * 10, "Arial");
  m_wndStatusBar.SetFont(pFont);

  if (g_PrefsDlg.m_bRunBefore == FALSE)
  {
    g_PrefsDlg.m_bRunBefore = TRUE;
    g_PrefsDlg.SavePrefs();
    if (MessageBox("Would you like QERadiant to build and load a default project? If this is the first time you have run QERadiant or you are not familiar with editing QE4 project files directly, this is HIGHLY recommended", "Create a default project?", MB_YESNO) == IDYES)
    {
      OnFileNewproject();
    }
  }
  else
  {
    // hack that keeps SGI OpenGL from crashing on texture load with no map
#if 0
    if (g_PrefsDlg.m_bSGIOpenGL)
    {
      vec3_t vMin, vMax;
      vMin[0] = vMin[1] = vMin[2] = 0;
      vMax[0] = vMax[1] = vMax[2] = 8;
      brush_t* pBrush = Brush_Create(vMin, vMax, &g_qeglobals.d_texturewin.texdef);
	    Entity_LinkBrush (world_entity, pBrush);
      Brush_Build(pBrush);
	    Brush_AddToList (pBrush, &active_brushes);
      Select_Brush(pBrush);
      Sys_UpdateWindows(W_ALL);
      PostMessage(WM_COMMAND, ID_SELECTION_DELETE, 0); 
    }
#endif
    if (g_PrefsDlg.m_bLoadLastMap && g_PrefsDlg.m_strLastMap.GetLength() > 0)
      Map_LoadFile(g_PrefsDlg.m_strLastMap.GetBuffer(0));
  }

  SetGridStatus();
  SetTexValStatus();
  SetButtonMenuStates();
  LoadBarState("RadiantToolBars");
  if (!g_PrefsDlg.m_bTextureBar)
    ShowControlBar(&m_wndTextureBar, FALSE, TRUE);
  else
    ShowControlBar(&m_wndTextureBar, TRUE, TRUE);

  ShowControlBar(&m_wndToolBar, (m_wndToolBar.GetStyle() & WS_VISIBLE), TRUE);

  SetActiveXY(m_pXYWnd);
  m_pXYWnd->SetFocus();

  SS_LoadFromRegistry();


////
  char sBuffer[256];
  LONG lSize = sizeof(sBuffer);
  if (LoadRegistryInfo("Radiant::BEHAVED_PATH",	&sBuffer, &lSize))	
	g_cstrBehavEdPath = sBuffer;	
////

  PostMessage(WM_KEYDOWN, 'O', NULL);
  OnPluginsRefresh();
  return 0;
}

void CMainFrame::LoadCommandMap()
{
  CString strINI;
  char* pBuff = new char[1024];
  if (g_PrefsDlg.m_strUserPath.GetLength() > 0)
    strINI = g_PrefsDlg.m_strUserPath;
  else
  {
    strINI = g_strAppPath;
    strINI += "\\radiant.ini";
  }

  for (int i = 0; i < g_nCommandCount; i++)
  {
    int nLen = GetPrivateProfileString("Commands", g_Commands[i].m_strCommand, "", pBuff, 1024, strINI);
    if (nLen > 0)
    {
      CString strBuff = pBuff;
      strBuff.TrimLeft();
      strBuff.TrimRight();
      int nSpecial = strBuff.Find("+alt");
      if (nSpecial >= 0)
      {
        g_Commands[i].m_nModifiers |= RAD_ALT;
        FindReplace(strBuff, "+alt", "");
      }
      nSpecial = strBuff.Find("+ctrl");
      if (nSpecial >= 0)
      {
        g_Commands[i].m_nModifiers |= RAD_CONTROL;
        FindReplace(strBuff, "+ctrl", "");
      }
      nSpecial = strBuff.Find("+shift");
      if (nSpecial >= 0)
      {
        g_Commands[i].m_nModifiers |= RAD_SHIFT;
        FindReplace(strBuff, "+shift", "");
      }
      strBuff.TrimLeft();
      strBuff.TrimRight();
      strBuff.MakeUpper();
      if (nLen == 1) // most often case.. deal with first
      {
        g_Commands[i].m_nKey = __toascii(strBuff.GetAt(0));
      }
      else // special key
      {
        for (int j = 0; j < g_nKeyCount; j++)
        {
          if (strBuff.CompareNoCase(g_Keys[j].m_strName) == 0)
          {
            g_Commands[i].m_nKey = g_Keys[j].m_nVKKey;
            break;
          }
        }
      }
    }
  }
  delete []pBuff;
}

BOOL CMainFrame::PreCreateWindow(CREATESTRUCT& cs)
{
	// TODO: Modify the Window class or styles here by modifying
	//  the CREATESTRUCT cs
	return CFrameWnd::PreCreateWindow(cs);
}

/////////////////////////////////////////////////////////////////////////////
// CMainFrame diagnostics

#ifdef _DEBUG
void CMainFrame::AssertValid() const
{
	CFrameWnd::AssertValid();
}

void CMainFrame::Dump(CDumpContext& dc) const
{
	CFrameWnd::Dump(dc);
}

#endif //_DEBUG

/////////////////////////////////////////////////////////////////////////////
// CMainFrame message handlers
void CMainFrame::CreateQEChildren()
{
	// the project file can be specified on the command line,
	// or implicitly found in the scripts directory
  bool bProjectLoaded = false;
	if (AfxGetApp()->m_lpCmdLine && strlen(AfxGetApp()->m_lpCmdLine))
	{
		ParseCommandLine (AfxGetApp()->m_lpCmdLine);
		bProjectLoaded = QE_LoadProject(argv[1]);
	}
	else 
  {
    if (g_PrefsDlg.m_bLoadLast && g_PrefsDlg.m_strLastProject.GetLength() > 0)
	{
	    bProjectLoaded = QE_LoadProject(g_PrefsDlg.m_strLastProject.GetBuffer(0));
	}
    if (!bProjectLoaded)
    {
      CString str = g_strAppPath;
      AddSlash(str);
      str += "../base/scripts/StarWars.qe4";
      char cWork[1024];
      char *pFile = NULL;
      GetFullPathName(str, 1024, cWork, &pFile);
      bProjectLoaded = QE_LoadProject(cWork);
    }
    if (!bProjectLoaded)
	{
      bProjectLoaded = QE_LoadProject("scripts/StarWars.qe4");
	}
  }

  if (!bProjectLoaded)
  {
#if 0
    // let's try the default project directory..
    char* pBuff = new char[1024];
    ::GetCurrentDirectory(1024, pBuff);
    CString strDefProj = g_strAppPath;
    AddSlash(strDefProj);
    strDefProj += "defproj";
    if (::SetCurrentDirectory(strDefProj))
    {
	    bProjectLoaded = QE_LoadProject("scripts/StarWars.qe4");
      if (bProjectLoaded)
      {
        // setup auto load stuff for the default map
        g_PrefsDlg.m_bLoadLast = TRUE;
        AddSlash(strDefProj);
        strDefProj += "maps\\defproj.map";
        g_PrefsDlg.m_strLastMap = strDefProj;
        g_PrefsDlg.SavePrefs();
      }
    }
    else
    {
      ::SetCurrentDirectory(pBuff);
    }
    delete []pBuff;
#endif

    if (!bProjectLoaded)
    {
      Sys_Printf ("Using default.qe4. You may experience problems. See the readme.txt\n");
      CString strProj = g_strAppPath;
      strProj += "\\default.qe4";
      bProjectLoaded = QE_LoadProject(strProj.GetBuffer(0));

      if (!bProjectLoaded)
      {
        CFileDialog dlgFile(true, NULL, NULL, OFN_HIDEREADONLY | OFN_OVERWRITEPROMPT, "QERadiant Project files (*.qe4, *.prj)|*.qe4|*.prj||", this);
        if (dlgFile.DoModal() == IDOK)
          bProjectLoaded = QE_LoadProject(dlgFile.GetPathName().GetBuffer(0));
      }
    }
  }

  if (!bProjectLoaded)
    Error("Unable to load project file. It was unavailable in the scripts path and the default could not be found");

//  if (g_PrefsDlg.m_bPAK == TRUE)
//    InitPakFile();

  if (g_PrefsDlg.m_bPAK == TRUE)
  {
    // FIXME: pay attention to Q3 pref
    //InitPakFile(ValueForKey(g_qeglobals.d_project_entity, "basepath"), g_PrefsDlg.m_strPAKFile);
    InitPakFile(ValueForKey(g_qeglobals.d_project_entity, "basepath"), NULL);
  }


	QE_Init ();

	Sys_Printf ("Entering message loop\n");

  m_bDoLoop = true;
	SetTimer(QE_TIMER0, 1000, NULL);

}

BOOL CMainFrame::OnCommand(WPARAM wParam, LPARAM lParam) 
{
	return CFrameWnd::OnCommand(wParam, lParam);
}

LRESULT CMainFrame::DefWindowProc(UINT message, WPARAM wParam, LPARAM lParam) 
{
  RoutineProcessing();
	return CFrameWnd::DefWindowProc(message, wParam, lParam);
}


void CMainFrame::RoutineProcessing()
{
  if (m_bDoLoop)
  {
	  double time = 0.0;
    double oldtime = 0.0;
    double delta= 0.0;

    CheckBspProcess ();
	  time = Sys_DoubleTime ();
	  delta = time - oldtime;
	  oldtime = time;
	  if (delta > 0.2)
		  delta = 0.2;
	  
    // run time dependant behavior
    if (m_pCamWnd)
	    m_pCamWnd->Cam_MouseControl(delta);

    if (g_PrefsDlg.m_bQE4Painting && g_nUpdateBits)
    {
      int nBits = g_nUpdateBits;      // this is done to keep this routine from being
      g_nUpdateBits = 0;              // re-entered due to the paint process.. only
      UpdateWindows(nBits);           // happens in rare cases but causes a stack overflow
    }

  }
}

LRESULT CMainFrame::WindowProc(UINT message, WPARAM wParam, LPARAM lParam) 
{
	return CFrameWnd::WindowProc(message, wParam, lParam);
}

bool MouseDown()
{
  if (::GetAsyncKeyState(VK_LBUTTON))
    return true;
  if (::GetAsyncKeyState(VK_RBUTTON))
    return true;
  if (::GetAsyncKeyState(VK_MBUTTON))
    return true;
  return false;
}


void CMainFrame::OnTimer(UINT nIDEvent) 
{
  if (!MouseDown())
  {
    QE_CountBrushesAndUpdateStatusBar();
    QE_CheckAutoSave();
  }
}
struct SplitInfo
{
  int m_nMin;
  int m_nCur;
};

bool LoadWindowPlacement(HWND hwnd, const char* pName)
{
  WINDOWPLACEMENT wp;
  wp.length = sizeof(WINDOWPLACEMENT);
	LONG lSize = sizeof(wp);
	if (LoadRegistryInfo(pName, &wp, &lSize))
  {
    ::SetWindowPlacement(hwnd, &wp);
    return true;
  }
  return false;
}

void SaveWindowPlacement(HWND hwnd, const char* pName)
{
  WINDOWPLACEMENT wp;
  wp.length = sizeof(WINDOWPLACEMENT);
  if (::GetWindowPlacement(hwnd, &wp))
  {
	  SaveRegistryInfo(pName, &wp, sizeof(wp));
  }
}


void CMainFrame::OnDestroy() 
{
  KillTimer(QE_TIMER0);

  SS_SaveToRegistry();
  SS_Shutdown_OnceOnly();

////
  char sBuffer[256];
  strcpy(sBuffer,g_cstrBehavEdPath);
  SaveRegistryInfo("Radiant::BEHAVED_PATH", &sBuffer, sizeof(sBuffer));
////

  SaveBarState("RadiantToolBars");

  // FIXME
  // original mru stuff needs replaced with mfc stuff
#ifdef QUAKE3
	SaveMruInReg(g_qeglobals.d_lpMruMenu,"Software\\id\\Q3Trek_QuakeEd4\\MRU");
#else
	SaveMruInReg(g_qeglobals.d_lpMruMenu,"Software\\id\\QuakeEd4\\MRU");
#endif

	DeleteMruMenu(g_qeglobals.d_lpMruMenu);

  SaveWindowPlacement(GetSafeHwnd(), "Radiant::MainWindowPlace");
  //SaveWindowState(GetSafeHwnd(), "Radiant::MainWindow");
  //if (m_nCurrentStyle == QR_QE4)
    //SaveWindowPlacement(g_qeglobals.d_hwndEntity, "EntityWindowPlace");

  if (m_nCurrentStyle == 0 || m_nCurrentStyle == 3)
  {
    SaveWindowState(m_wndSplit.GetSafeHwnd(), "Radiant::Split");
    SaveWindowState(m_wndSplit2.GetSafeHwnd(), "Radiant::Split2");
    SaveWindowState(m_wndSplit3.GetSafeHwnd(), "Radiant::Split3");

    SplitInfo spinfo;
    m_wndSplit.GetRowInfo(0, spinfo.m_nCur, spinfo.m_nMin);
	  SaveRegistryInfo("Radiant::Split::Row_0", &spinfo, sizeof(spinfo));
    m_wndSplit.GetRowInfo(1, spinfo.m_nCur, spinfo.m_nMin);
	  SaveRegistryInfo("Radiant::Split::Row_1", &spinfo, sizeof(spinfo));

    m_wndSplit2.GetColumnInfo(0, spinfo.m_nCur, spinfo.m_nMin);
	  SaveRegistryInfo("Radiant::Split2::Col_0", &spinfo, sizeof(spinfo));
    m_wndSplit2.GetColumnInfo(1, spinfo.m_nCur, spinfo.m_nMin);
	  SaveRegistryInfo("Radiant::Split2::Col_1", &spinfo, sizeof(spinfo));
    m_wndSplit2.GetColumnInfo(2, spinfo.m_nCur, spinfo.m_nMin);
	  SaveRegistryInfo("Radiant::Split2::Col_2", &spinfo, sizeof(spinfo));

    m_wndSplit3.GetRowInfo(0, spinfo.m_nCur, spinfo.m_nMin);
	  SaveRegistryInfo("Radiant::Split3::Row_0", &spinfo, sizeof(spinfo));
    m_wndSplit3.GetRowInfo(1, spinfo.m_nCur, spinfo.m_nMin);
	  SaveRegistryInfo("Radiant::Split3::Row_1", &spinfo, sizeof(spinfo));
  }
  else 
  {
    SaveWindowPlacement(m_pXYWnd->GetSafeHwnd(), "xywindow");
    SaveWindowPlacement(m_pXZWnd->GetSafeHwnd(), "xzwindow");
    SaveWindowPlacement(m_pYZWnd->GetSafeHwnd(), "yzwindow");
	  SaveWindowPlacement(m_pCamWnd->GetSafeHwnd(), "camerawindow");
	  SaveWindowPlacement(m_pZWnd->GetSafeHwnd(), "zwindow");
	  SaveWindowState(m_pTexWnd->GetSafeHwnd(), "texwindow");
	  SaveWindowState(m_pEditWnd->GetSafeHwnd(), "editwindow");
  }

  if (m_pXYWnd->GetSafeHwnd())
    m_pXYWnd->SendMessage(WM_DESTROY, 0, 0);
  delete m_pXYWnd; m_pXYWnd = NULL;
  
  if (m_pYZWnd->GetSafeHwnd())
    m_pYZWnd->SendMessage(WM_DESTROY, 0, 0);
  delete m_pYZWnd; m_pYZWnd = NULL;
  
  if (m_pXZWnd->GetSafeHwnd())
    m_pXZWnd->SendMessage(WM_DESTROY, 0, 0);
  delete m_pXZWnd; m_pXZWnd = NULL;
  
  if (m_pZWnd->GetSafeHwnd())
    m_pZWnd->SendMessage(WM_DESTROY, 0, 0);
  delete m_pZWnd; m_pZWnd = NULL;
  
  if (m_pTexWnd->GetSafeHwnd())
    m_pTexWnd->SendMessage(WM_DESTROY, 0, 0);
  delete m_pTexWnd; m_pTexWnd = NULL;
  
  if (m_pEditWnd->GetSafeHwnd())
    m_pEditWnd->SendMessage(WM_DESTROY, 0, 0);
  delete m_pEditWnd; m_pEditWnd = NULL;

  if (m_pCamWnd->GetSafeHwnd())
    m_pCamWnd->SendMessage(WM_DESTROY, 0, 0);
  delete m_pCamWnd;m_pCamWnd = NULL;

	SaveRegistryInfo("SavedInfo", &g_qeglobals.d_savedinfo, sizeof(g_qeglobals.d_savedinfo));

	// this is tacky, but they only want a few extra things saved....
	//
	SaveRegistryInfo("_g_bPatchShowBounds", &g_bPatchShowBounds, sizeof(g_bPatchShowBounds));
	SaveRegistryInfo("_g_PrefsDlg.m_bSelectCurves", &g_PrefsDlg.m_bSelectCurves, sizeof(g_PrefsDlg.m_bSelectCurves));

  if (strcmpi(currentmap, "unnamed.map") != 0)
  {
    g_PrefsDlg.m_strLastMap = currentmap;
    g_PrefsDlg.SavePrefs();
  }
  CleanUpEntities();

  while (active_brushes.next != &active_brushes)
	  Brush_Free (active_brushes.next);
	while (selected_brushes.next != &selected_brushes)
		Brush_Free (selected_brushes.next);
	while (filtered_brushes.next != &filtered_brushes)
		Brush_Free (filtered_brushes.next);

	while (entities.next != &entities)
		Entity_Free (entities.next);

	epair_t* pEPair = g_qeglobals.d_project_entity->epairs;
  while (pEPair)
  {
    epair_t* pNextEPair = pEPair->next;
    free (pEPair->key);
    free (pEPair->value);
    free (pEPair);
    pEPair = pNextEPair;
  }

  Grouping_Shutdown();

	entity_t* pEntity = g_qeglobals.d_project_entity->next;
  while (pEntity != NULL && pEntity != g_qeglobals.d_project_entity)
  {
    entity_t* pNextEntity = pEntity->next;
    Entity_Free(pEntity);
    pEntity = pNextEntity;
  }

  if (g_qeglobals.d_qtextures)
  {
	  qtexture_t* pTex = g_qeglobals.d_qtextures->next;
    while (pTex != NULL && pTex != g_qeglobals.d_qtextures)
    {
      qtexture_t* pNextTex = pTex->next;
      free(pTex);
      pTex = pNextTex;
    }
  }

  if (world_entity)
    Entity_Free(world_entity);

  if (notexture)
    free(notexture);

  //if (current_texture)
  //  free(current_texture);
  ClosePakFile();

  FreeShaders();

	CFrameWnd::OnDestroy();
}

void CMainFrame::OnClose() 
{
  if (ConfirmModified())
  {
	extern void	Map_Free (void);	// unfortunately if I include map.h for this at the top I get other errors...
	Select_Deselect();				// prevent query during Map_Free about copying selected brushes
    Map_Free();	// this is now being called to give the user a chance to checkin/undocheckout the map, and it
				//	also cuts down on the memory leak mess toy get exiting in debugger mode
	CFrameWnd::OnClose();	
  }
}
                        
void CMainFrame::OnKeyUp(UINT nChar, UINT nRepCnt, UINT nFlags) 
{
  // run through our list to see if we have a handler for nChar
  //

  for (int i = 0; i < g_nCommandCount; i++)
  {
    if (g_Commands[i].m_nKey == nChar)    // find a match?
    {
      bool bGo = true;
      if (g_Commands[i].m_nModifiers & RAD_PRESS)
      {
        int nModifiers = g_Commands[i].m_nModifiers & ~RAD_PRESS;
        if (nModifiers)     // are there modifiers present?
        {
          if (nModifiers & RAD_ALT)
            if (!(GetKeyState(VK_MENU) & 0x8000))
              bGo = false;
          if (nModifiers & RAD_CONTROL)
            if (!(GetKeyState(VK_CONTROL) & 0x8000))
              bGo = false;
          if (nModifiers & RAD_SHIFT)
            if (!(GetKeyState(VK_SHIFT) & 0x8000))
              bGo = false;
        }
        else  // no modifiers make sure none of those keys are pressed
        {
          if (GetKeyState(VK_MENU) & 0x8000)
            bGo = false;
          if (GetKeyState(VK_CONTROL) & 0x8000)
            bGo = false;
          if (GetKeyState(VK_SHIFT) & 0x8000)
            bGo = false;
        }
        if (bGo)
        {
          SendMessage(WM_COMMAND, g_Commands[i].m_nCommand, 0);
          break;
        }
      }
    }
  }
}

bool CamOK(unsigned int nKey)
{
  if (nKey == VK_UP || nKey == VK_LEFT || nKey == VK_RIGHT || nKey == VK_DOWN)
  {
    if (::GetAsyncKeyState(nKey))
      return true;
    else
      return false;
  }
  return true;
}


void CMainFrame::OnSysKeyDown(UINT nChar, UINT nRepCnt, UINT nFlags) 
{
	//OnKeyDown(nChar, nRepCnt, nFlags);
  if (nChar == VK_DOWN)
  {
    OnKeyDown(nChar, nRepCnt, nFlags);
  }
	CFrameWnd::OnSysKeyDown(nChar, nRepCnt, nFlags);
}

void CMainFrame::OnKeyDown(UINT nChar, UINT nRepCnt, UINT nFlags) 
{
    for (int i = 0; i < g_nCommandCount; i++)
    {
      if (g_Commands[i].m_nKey == nChar)    // find a match?
      {		 
        bool bGo = true;
        if (g_Commands[i].m_nModifiers)     // are there modifiers present?
        {
          if (g_Commands[i].m_nModifiers & RAD_ALT)
            if (!(GetKeyState(VK_MENU) & 0x8000))
              bGo = false;
          if (g_Commands[i].m_nModifiers & RAD_CONTROL)
            if (!(GetKeyState(VK_CONTROL) & 0x8000))
              bGo = false;
          if (g_Commands[i].m_nModifiers & RAD_SHIFT)
            if (!(GetKeyState(VK_SHIFT) & 0x8000))
              bGo = false;
        }
        else  // no modifiers make sure none of those keys are pressed
        {
          if (GetKeyState(VK_MENU) & 0x8000)
            bGo = false;
          if (GetKeyState(VK_CONTROL) & 0x8000)
            bGo = false;
          if (GetKeyState(VK_SHIFT) & 0x8000)
            bGo = false;
        }
        if (bGo)
        {
          SendMessage(WM_COMMAND, g_Commands[i].m_nCommand, 0);
          break;
        }
      }
    }

  CFrameWnd::OnKeyDown(nChar, nRepCnt, nFlags);
}

BOOL CMainFrame::OnCreateClient(LPCREATESTRUCT lpcs, CCreateContext* pContext) 
{
	g_qeglobals.d_hwndMain = GetSafeHwnd();
	g_qeglobals.d_hwndStatus = GetMessageBar()->GetSafeHwnd();

  if (g_PrefsDlg.m_bRunBefore == FALSE)
  {
    MessageBox("In the following dialog, please make sure the Quake2 .exe information is correct.\nQERadiant will NOT run correctly without this information");
    g_PrefsDlg.DoModal();
  }

  m_nCurrentStyle = g_PrefsDlg.m_nView;
  CreateEntityWindow(AfxGetInstanceHandle());
  if (!LoadWindowPlacement(GetSafeHwnd(), "Radiant::MainWindowPlace"))
    LoadWindowState(GetSafeHwnd(), "Radiant::MainWindow");
  
  //if (m_nCurrentStyle == QR_QE4)
  //  LoadWindowPlacement(g_qeglobals.d_hwndEntity, "EntityWindowPlace");

  CRect rect(5,25, 100, 100);

  if (g_PrefsDlg.m_nView == 0 || g_PrefsDlg.m_nView == 3)
  {
    m_wndSplit.CreateStatic(this, 2, 1);
    m_wndSplit2.CreateStatic(&m_wndSplit, 1, 3);
    m_wndSplit3.CreateStatic(&m_wndSplit2, 2,1);

    m_wndSplit.CreateView(1,0,RUNTIME_CLASS(CEditWnd), CSize(25, 100), pContext);
    g_pEdit = dynamic_cast<CEdit*>(m_wndSplit.GetPane(1,0));
    if (g_pEdit)
	    g_qeglobals.d_hwndEdit = g_pEdit->GetSafeHwnd();

    m_wndSplit3.CreateView(0,0,RUNTIME_CLASS(CCamWnd), CSize(25, 100), pContext);
    m_pCamWnd = dynamic_cast<CCamWnd*>(m_wndSplit3.GetPane(0,0));
  
    m_wndSplit2.CreateView(0,1,RUNTIME_CLASS(CXYWnd), CSize(25, 100), pContext);
    m_pXYWnd = dynamic_cast<CXYWnd*>(m_wndSplit2.GetPane(0,1));
    m_pXYWnd->SetViewType(XY);

    m_pCamWnd->SetXYFriend(m_pXYWnd);

    m_wndSplit2.CreateView(0,2,RUNTIME_CLASS(CZWnd), CSize(25, 100), pContext);
    m_pZWnd = dynamic_cast<CZWnd*>(m_wndSplit2.GetPane(0,2));

	  m_wndSplit3.CreateView(1,0,RUNTIME_CLASS(CTexWnd), CSize(25, 100), pContext);
    m_pTexWnd = dynamic_cast<CTexWnd*>(m_wndSplit3.GetPane(1,0));

    CreateQEChildren();

    if (g_PrefsDlg.m_nView == 0)
    {
      // the following bit switches the left and right views
      CWnd* pRight = m_wndSplit2.GetPane(0,2);
      long lRightID = ::GetWindowLong(pRight->GetSafeHwnd(), GWL_ID);
      long lLeftID = ::GetWindowLong(m_wndSplit3.GetSafeHwnd(), GWL_ID);
      ::SetWindowLong(pRight->GetSafeHwnd(), GWL_ID, lLeftID);
      ::SetWindowLong(m_wndSplit3.GetSafeHwnd(), GWL_ID, lRightID);
    }

    CRect rctParent;
    GetClientRect(rctParent);

    m_wndSplit.SetRowInfo(0, rctParent.Height() * .85, 50);
    m_wndSplit.SetRowInfo(1, rctParent.Height() * .15, 5);

    float fLeft = (g_PrefsDlg.m_nView == 0) ? .05 : .25;
    float fRight = (g_PrefsDlg.m_nView == 0) ? .25 : .05;
    int nMin1 = (g_PrefsDlg.m_nView == 0) ? 10 : 25;
    int nMin2 = (nMin1 == 10) ? 25 : 10;

    m_wndSplit2.SetColumnInfo(0, rctParent.Width() * fLeft, nMin1);
    m_wndSplit2.SetColumnInfo(1, rctParent.Width() * .70, 100);
    m_wndSplit2.SetColumnInfo(2, rctParent.Width() * fRight, nMin2);

    m_wndSplit3.SetRowInfo(1, (rctParent.Height() * .85) * .40, 15);
    m_wndSplit3.SetRowInfo(0, (rctParent.Height() * .85) * .60, 15);

    LoadWindowState(m_wndSplit.GetSafeHwnd(), "Radiant::Split");
    LoadWindowState(m_wndSplit2.GetSafeHwnd(), "Radiant::Split2");
    LoadWindowState(m_wndSplit3.GetSafeHwnd(), "Radiant::Split3");
    ::ShowWindow(g_qeglobals.d_hwndEntity, SW_HIDE);

    SplitInfo spinfo;
    long lSize;
	  if (LoadRegistryInfo("Radiant::Split::Row_0", &spinfo, &lSize))
      m_wndSplit.SetRowInfo(0, spinfo.m_nCur, spinfo.m_nMin);
	  if (LoadRegistryInfo("Radiant::Split::Row_1", &spinfo, &lSize))
      m_wndSplit.SetRowInfo(1, spinfo.m_nCur, spinfo.m_nMin);

	  if (LoadRegistryInfo("Radiant::Split2::Col_0", &spinfo, &lSize))
      m_wndSplit2.SetColumnInfo(0, spinfo.m_nCur, spinfo.m_nMin);
	  if (LoadRegistryInfo("Radiant::Split2::Col_1", &spinfo, &lSize))
      m_wndSplit2.SetColumnInfo(1, spinfo.m_nCur, spinfo.m_nMin);
	  if (LoadRegistryInfo("Radiant::Split2::Col_2", &spinfo, &lSize))
      m_wndSplit2.SetColumnInfo(2, spinfo.m_nCur, spinfo.m_nMin);

	  if (LoadRegistryInfo("Radiant::Split3::Row_0", &spinfo, &lSize))
      m_wndSplit3.SetRowInfo(0, spinfo.m_nCur, spinfo.m_nMin);
	  if (LoadRegistryInfo("Radiant::Split3::Row_1", &spinfo, &lSize))
      m_wndSplit3.SetRowInfo(1, spinfo.m_nCur, spinfo.m_nMin);

    m_wndSplit.RecalcLayout();
    m_wndSplit2.RecalcLayout();
    m_wndSplit3.RecalcLayout();
  }
  else if (g_PrefsDlg.m_nView == 1)
  {
    m_pCamWnd = new CCamWnd();
    m_pCamWnd->Create(CAMERA_WINDOW_CLASS, "", QE3_CHILDSTYLE, rect, this, 1234);
  
    m_pZWnd = new CZWnd();
    m_pZWnd->Create(Z_WINDOW_CLASS, "", QE3_CHILDSTYLE, rect, this, 1238);
    
    m_pXYWnd = new CXYWnd();
    m_pXYWnd->Create(XY_WINDOW_CLASS, "", QE3_CHILDSTYLE, rect, this, 1235);
    m_pXYWnd->SetViewType(XY);

    m_pXZWnd = new CXYWnd();
    m_pXZWnd->Create(XY_WINDOW_CLASS, "", QE3_CHILDSTYLE, rect, this, 1236);
    m_pXZWnd->SetViewType(XZ);

    m_pYZWnd = new CXYWnd();
    m_pYZWnd->Create(XY_WINDOW_CLASS, "", QE3_CHILDSTYLE, rect, this, 1237);
    m_pYZWnd->SetViewType(YZ);

    m_pCamWnd->SetXYFriend(m_pXYWnd);

    m_pTexWnd = new CTexWnd();
    //m_pTexWnd->Create(TEXTURE_WINDOW_CLASS, "", QE3_CHILDSTYLE, rect, this, 1237);
    m_pTexWnd->Create(TEXTURE_WINDOW_CLASS, "", QE3_SPLITTER_STYLE, rect, this, 1239);
    ::SetParent(m_pTexWnd->GetSafeHwnd(), g_qeglobals.d_hwndEntity);

    //m_pEditWnd = new CRADEditWnd();
    //m_pEditWnd->Create(NULL, "Console", QE3_CHILDSTYLE, rect, this, 1238);
    //g_pEdit = m_pEditWnd->GetEditWnd();
    //if (g_pEdit)
	  //  g_qeglobals.d_hwndEdit = g_pEdit->GetSafeHwnd();

    LoadWindowPlacement(m_pXYWnd->GetSafeHwnd(), "xywindow");
    LoadWindowPlacement(m_pXZWnd->GetSafeHwnd(), "xzwindow");
    LoadWindowPlacement(m_pYZWnd->GetSafeHwnd(), "yzwindow");
	  LoadWindowPlacement(m_pCamWnd->GetSafeHwnd(), "camerawindow");
	  LoadWindowPlacement(m_pZWnd->GetSafeHwnd(), "zwindow");
    if (!g_PrefsDlg.m_bXZVis)
      m_pXZWnd->ShowWindow(SW_HIDE);
    if (!g_PrefsDlg.m_bYZVis)
      m_pYZWnd->ShowWindow(SW_HIDE);
    if (!g_PrefsDlg.m_bZVis)
      m_pZWnd->ShowWindow(SW_HIDE);
	  //LoadWindowPlacement(m_pTexWnd->GetSafeHwnd(), "texwindow");
	  //LoadWindowPlacement(m_pEditWnd->GetSafeHwnd(), "editwindow");
    CreateQEChildren();
  }
  else // 4 way
  {
    m_wndSplit.CreateStatic(this, 2, 2);

    m_wndSplit.CreateView(0,0,RUNTIME_CLASS(CCamWnd), CSize(25, 100), pContext);
    m_pCamWnd = dynamic_cast<CCamWnd*>(m_wndSplit.GetPane(0,0));

    m_wndSplit.CreateView(0,1,RUNTIME_CLASS(CXYWnd), CSize(25, 100), pContext);
    m_pXYWnd = dynamic_cast<CXYWnd*>(m_wndSplit.GetPane(0,1));
    m_pXYWnd->SetViewType(XY);

    m_wndSplit.CreateView(1,0,RUNTIME_CLASS(CXYWnd), CSize(25, 100), pContext);
    m_pYZWnd = dynamic_cast<CXYWnd*>(m_wndSplit.GetPane(1,0));
    m_pYZWnd->SetViewType(YZ);

    m_wndSplit.CreateView(1,1,RUNTIME_CLASS(CXYWnd), CSize(25, 100), pContext);
    m_pXZWnd = dynamic_cast<CXYWnd*>(m_wndSplit.GetPane(1,1));
    m_pXZWnd->SetViewType(XZ);

    m_pCamWnd->SetXYFriend(m_pXYWnd);

    m_pTexWnd = new CTexWnd();
    m_pTexWnd->Create(TEXTURE_WINDOW_CLASS, "", QE3_SPLITTER_STYLE, rect, this, 1237);
    ::SetParent(m_pTexWnd->GetSafeHwnd(), g_qeglobals.d_hwndEntity);

    m_pZWnd = new CZWnd();
    m_pZWnd->Create(Z_WINDOW_CLASS, "", QE3_CHILDSTYLE, rect, this, 1236);
    m_pZWnd->ShowWindow(SW_HIDE);


    //m_pEditWnd = new CRADEditWnd();
    //m_pEditWnd->Create(NULL, "Console", QE3_STYLE, rect, this, 1238);
    //g_pEdit = m_pEditWnd->GetEditWnd();
    //if (g_pEdit)
	  //  g_qeglobals.d_hwndEdit = g_pEdit->GetSafeHwnd();

	  LoadWindowState(m_pTexWnd->GetSafeHwnd(), "texwindow");
	  LoadWindowState(m_pEditWnd->GetSafeHwnd(), "editwindow");
    ::ShowWindow(g_qeglobals.d_hwndEntity, SW_HIDE);

    CreateQEChildren();

    CRect rctParent;
    GetClientRect(rctParent);

    m_wndSplit.SetRowInfo(0, rctParent.Height() * .5, 50);
    m_wndSplit.SetRowInfo(1, rctParent.Height() * .5, 50);

    m_wndSplit.SetColumnInfo(0, rctParent.Width() * .5, 50);
    m_wndSplit.SetColumnInfo(1, rctParent.Width() * .5, 50);

    LoadWindowState(m_wndSplit.GetSafeHwnd(), "Radiant::SplitSPLIT");

    m_wndSplit.RecalcLayout();
  }

  if (g_pEdit)
    g_pEdit->SendMessage(WM_SETFONT, (WPARAM)::GetStockObject(DEFAULT_GUI_FONT), (LPARAM)TRUE);


  if (m_pXYWnd)
    m_pXYWnd->SetActive(true);
  m_bSplittersOK = true;
	Texture_SetMode(g_qeglobals.d_savedinfo.iTexMenu);

  return TRUE;
}

CRect g_rctOld(0,0,0,0);
void CMainFrame::OnSize(UINT nType, int cx, int cy) 
{
	CFrameWnd::OnSize(nType, cx, cy);

  CRect rctParent;
  GetClientRect(rctParent);

  UINT nID;
  UINT nStyle;
  int nWidth;
  if (m_wndStatusBar.GetSafeHwnd())
  {
    m_wndStatusBar.GetPaneInfo(0, nID, nStyle, nWidth);
    m_wndStatusBar.SetPaneInfo(0, nID, nStyle, rctParent.Width() * .19);
    m_wndStatusBar.GetPaneInfo(1, nID, nStyle, nWidth);
    m_wndStatusBar.SetPaneInfo(1, nID, nStyle, rctParent.Width() * .19);
    m_wndStatusBar.GetPaneInfo(2, nID, nStyle, nWidth);
    m_wndStatusBar.SetPaneInfo(2, nID, nStyle, rctParent.Width() * .19);
    m_wndStatusBar.GetPaneInfo(3, nID, nStyle, nWidth);
    m_wndStatusBar.SetPaneInfo(3, nID, nStyle, rctParent.Width() * .19);
    m_wndStatusBar.GetPaneInfo(4, nID, nStyle, nWidth);
    m_wndStatusBar.SetPaneInfo(4, nID, nStyle, rctParent.Width() * .13);
    m_wndStatusBar.GetPaneInfo(5, nID, nStyle, nWidth);
    m_wndStatusBar.SetPaneInfo(5, nID, nStyle, rctParent.Width() * .01);
  }

  if (nType == SIZE_RESTORED && m_bSplittersOK && g_rctOld.Width() > 0)
  {
    if (m_nCurrentStyle == 0 || m_nCurrentStyle == 3)
    {
      SplitInfo spinfo;
      m_wndSplit.GetRowInfo(0, spinfo.m_nCur, spinfo.m_nMin);
      float fpc1 = (float)spinfo.m_nCur / g_rctOld.Height();
      m_wndSplit.GetRowInfo(1, spinfo.m_nCur, spinfo.m_nMin);
      float fpc2 = (float)spinfo.m_nCur / g_rctOld.Height();
      m_wndSplit2.GetColumnInfo(0, spinfo.m_nCur, spinfo.m_nMin);
      float fpc3 = (float)spinfo.m_nCur / g_rctOld.Width();
      m_wndSplit2.GetColumnInfo(1, spinfo.m_nCur, spinfo.m_nMin);
      float fpc4 = (float)spinfo.m_nCur / g_rctOld.Width();
      m_wndSplit2.GetColumnInfo(2, spinfo.m_nCur, spinfo.m_nMin);
      float fpc5 = (float)spinfo.m_nCur / g_rctOld.Width();
      m_wndSplit3.GetRowInfo(0, spinfo.m_nCur, spinfo.m_nMin);
      float fpc6 = (float)spinfo.m_nCur / g_rctOld.Height();
      m_wndSplit3.GetRowInfo(1, spinfo.m_nCur, spinfo.m_nMin);
      float fpc7 = (float)spinfo.m_nCur / g_rctOld.Height();

      m_wndSplit.SetRowInfo(0, rctParent.Height() * fpc1, 100);
      m_wndSplit.SetRowInfo(1, rctParent.Height() * fpc2, 25);

      int nMin1 = (m_nCurrentStyle == 0) ? 10 : 25;
      int nMin2 = (nMin1 == 10) ? 25 : 10;

      m_wndSplit2.SetColumnInfo(0, rctParent.Width() * fpc3, nMin1);
      m_wndSplit2.SetColumnInfo(1, rctParent.Width() * fpc4, 100);
      m_wndSplit2.SetColumnInfo(2, rctParent.Width() * fpc5, nMin2);

      m_wndSplit3.SetRowInfo(0, rctParent.Height() * fpc6, 50);
      m_wndSplit3.SetRowInfo(1, rctParent.Height() * fpc7, 50);

      m_wndSplit.RecalcLayout();
      m_wndSplit2.RecalcLayout();
      m_wndSplit3.RecalcLayout();
    }
  }

	
}


void OpenDialog (void);
void SaveAsDialog (bool bRegion);
void  Select_Ungroup (void);

void CMainFrame::ToggleCamera()
{
  if (m_bCamPreview)
    m_bCamPreview = false;
  else
    m_bCamPreview = true;
}

void CMainFrame::OnFileClose() 
{
	
}

void CMainFrame::OnFileExit() 
{
	// confirm-query removed because I put one in the OnClose member instead, this way I also catch it when people
	//	exit the program via clicking on the X button in window-frame top right, not just File/Exit
	//
    PostMessage (WM_CLOSE, 0, 0L);
}

void CMainFrame::OnFileLoadproject() 
{
  if (ConfirmModified())
	  ProjectDialog ();
}

void CMainFrame::OnFileNew() 
{
  if (ConfirmModified())
	  Map_New ();
}

void CMainFrame::OnFileOpen() 
{
  if (ConfirmModified())
	  OpenDialog ();
}

void CMainFrame::OnFilePointfile() 
{
  if (g_qeglobals.d_pointfile_display_list)
	  Pointfile_Clear ();
	else
	  Pointfile_Check ();
}

void CMainFrame::OnFilePrint() 
{
	
}

void CMainFrame::OnFilePrintPreview() 
{
	
}

void CMainFrame::OnFileSave() 
{
  if (!strcmp(currentmap, "unnamed.map"))
	  SaveAsDialog (false);
	else
		Map_SaveFile (currentmap, false);
}

void CMainFrame::OnFileSaveas() 
{
  SaveAsDialog(false);
}

void CMainFrame::OnView100() 
{
  if (m_pXYWnd)
    m_pXYWnd->SetScale(1);
  if (m_pXZWnd)
    m_pXZWnd->SetScale(1);
  if (m_pYZWnd)
    m_pYZWnd->SetScale(1);
	Sys_UpdateWindows (W_XY|W_XY_OVERLAY);
}

void CMainFrame::OnViewCenter() 
{
  m_pCamWnd->Camera().angles[ROLL] = m_pCamWnd->Camera().angles[PITCH] = 0;
	m_pCamWnd->Camera().angles[YAW] = 22.5 * 
	floor( (m_pCamWnd->Camera().angles[YAW]+11)/22.5 );
	Sys_UpdateWindows (W_CAMERA | W_XY_OVERLAY);
}

void CMainFrame::OnViewConsole() 
{
  if (m_nCurrentStyle > 0 && m_nCurrentStyle < 3) // QE4 style
  {
    if (inspector_mode == W_CONSOLE && m_nCurrentStyle != QR_QE4) // are we in console mode already?
    {
      if (::IsWindowVisible(g_qeglobals.d_hwndEntity))
        ::ShowWindow(g_qeglobals.d_hwndEntity, SW_HIDE);
      else
        ::ShowWindow(g_qeglobals.d_hwndEntity, SW_NORMAL);
    }
    else
    {
      ::ShowWindow(g_qeglobals.d_hwndEntity, SW_NORMAL);
      SetInspectorMode(W_CONSOLE);
    }
  }
}

void CMainFrame::OnViewDownfloor() 
{
  m_pCamWnd->Cam_ChangeFloor (false);
}

void CMainFrame::OnViewEntity() 
{
  if (m_nCurrentStyle == 0 || m_nCurrentStyle == 3)
  {
    if (::IsWindowVisible(g_qeglobals.d_hwndEntity))
      ::ShowWindow(g_qeglobals.d_hwndEntity, SW_HIDE);
    else
    {
      ::ShowWindow(g_qeglobals.d_hwndEntity, SW_NORMAL);
      SetInspectorMode(W_ENTITY);
    }
  }
  else
  {
    if (inspector_mode == W_ENTITY && m_nCurrentStyle != QR_QE4)
    {
      if (::IsWindowVisible(g_qeglobals.d_hwndEntity))
        ::ShowWindow(g_qeglobals.d_hwndEntity, SW_HIDE);
      else
        ::ShowWindow(g_qeglobals.d_hwndEntity, SW_NORMAL);
    }
    else
    {
      ::ShowWindow(g_qeglobals.d_hwndEntity, SW_NORMAL);
      SetInspectorMode(W_ENTITY);
    }
  }
}

void CMainFrame::OnViewFront() 
{
  if (m_nCurrentStyle != 2)
  {
    m_pXYWnd->SetViewType(YZ);
    m_pXYWnd->PositionView();
  }
	Sys_UpdateWindows (W_XY);
}

void CMainFrame::OnMru(unsigned int nID) 
{
  DoMru(GetSafeHwnd(),nID);
}

void CMainFrame::OnViewNearest(unsigned int nID) 
{
  Texture_SetMode(nID);
}

void CMainFrame::OnTextureWad(unsigned int nID) 
{
  Sys_BeginWait ();
	Texture_ShowDirectory (nID);
	Sys_UpdateWindows (W_ALL);
}

void CMainFrame::OnBspCommand(unsigned int nID) 
{
  if (g_PrefsDlg.m_bSnapShots && stricmp(currentmap, "unnamed.map") != 0)
    Map_Snapshot();

	RunBsp (bsp_commands[LOWORD(nID-CMD_BSPCOMMAND)]);
}



void CMainFrame::OnViewShowblocks() 
{
  g_qeglobals.show_blocks ^= 1;
  CheckMenuItem ( ::GetMenu(g_qeglobals.d_hwndMain), ID_VIEW_SHOWBLOCKS, MF_BYCOMMAND | (g_qeglobals.show_blocks ? MF_CHECKED : MF_UNCHECKED)  );
	Sys_UpdateWindows (W_XY);
}

void CMainFrame::OnViewShowclip() 
{
	if ( ( g_qeglobals.d_savedinfo.exclude ^= EXCLUDE_CLIP ) & EXCLUDE_CLIP )
    CheckMenuItem ( ::GetMenu(g_qeglobals.d_hwndMain), ID_VIEW_SHOWCLIP, MF_BYCOMMAND | MF_UNCHECKED );
	else
    CheckMenuItem ( ::GetMenu(g_qeglobals.d_hwndMain), ID_VIEW_SHOWCLIP, MF_BYCOMMAND | MF_CHECKED );
	Sys_UpdateWindows (W_XY|W_CAMERA);
}

void CMainFrame::OnViewShowcoordinates() 
{
  g_qeglobals.d_savedinfo.show_coordinates ^= 1;
  CheckMenuItem ( ::GetMenu(g_qeglobals.d_hwndMain), ID_VIEW_SHOWCOORDINATES, MF_BYCOMMAND | (g_qeglobals.d_savedinfo.show_coordinates ? MF_CHECKED : MF_UNCHECKED)  );
	Sys_UpdateWindows (W_XY);
}

void CMainFrame::OnViewShowdetail() 
{
	if ( ( g_qeglobals.d_savedinfo.exclude ^= EXCLUDE_DETAIL ) & EXCLUDE_DETAIL )
	{
    CheckMenuItem ( ::GetMenu(g_qeglobals.d_hwndMain), ID_VIEW_SHOWDETAIL, MF_BYCOMMAND | MF_UNCHECKED );
    ::SetWindowText (g_qeglobals.d_hwndCamera, "Camera View (DETAIL EXCLUDED)");
	}
	else
	{
    CheckMenuItem ( ::GetMenu(g_qeglobals.d_hwndMain), ID_VIEW_SHOWDETAIL, MF_BYCOMMAND | MF_CHECKED );
    ::SetWindowText (g_qeglobals.d_hwndCamera, "Camera View");
	}
	Sys_UpdateWindows (W_XY|W_CAMERA);
}

void CMainFrame::OnViewShowDetailOnly()
{
	bool Excluded = !!(g_qeglobals.d_savedinfo.exclude & EXCLUDE_ALL_EXCEPT_DETAIL);

	if ( !(( g_qeglobals.d_savedinfo.exclude ^= EXCLUDE_ALL_EXCEPT_DETAIL ) & EXCLUDE_ALL_EXCEPT_DETAIL) )
		CheckMenuItem( ::GetMenu(g_qeglobals.d_hwndMain), ID_VIEW_SHOW_DETAIL_ONLY, MF_BYCOMMAND | MF_UNCHECKED);
	else
		CheckMenuItem( ::GetMenu(g_qeglobals.d_hwndMain), ID_VIEW_SHOW_DETAIL_ONLY, MF_BYCOMMAND | MF_CHECKED);
	Sys_UpdateWindows (W_XY|W_CAMERA);
}


void CMainFrame::OnViewShowent() 
{
  if ( ( g_qeglobals.d_savedinfo.exclude ^= EXCLUDE_ENT ) & EXCLUDE_ENT )
    CheckMenuItem( ::GetMenu(g_qeglobals.d_hwndMain), ID_VIEW_SHOWENT, MF_BYCOMMAND | MF_UNCHECKED);
	else
    CheckMenuItem( ::GetMenu(g_qeglobals.d_hwndMain), ID_VIEW_SHOWENT, MF_BYCOMMAND | MF_CHECKED);
	Sys_UpdateWindows (W_XY|W_CAMERA);
}

void CMainFrame::OnViewShowlights() 
{
  if ( ( g_qeglobals.d_savedinfo.exclude ^= EXCLUDE_LIGHTS ) & EXCLUDE_LIGHTS )
    CheckMenuItem ( ::GetMenu(g_qeglobals.d_hwndMain), ID_VIEW_SHOWLIGHTS, MF_BYCOMMAND | MF_UNCHECKED );
	else
    CheckMenuItem ( ::GetMenu(g_qeglobals.d_hwndMain), ID_VIEW_SHOWLIGHTS, MF_BYCOMMAND | MF_CHECKED );				
	Sys_UpdateWindows (W_XY|W_CAMERA);
}

extern bool gbSelectedLightBrushesAlwaysDrawRadii;
void CMainFrame::OnViewShowlightradii() 
{
  if ( ( g_qeglobals.d_savedinfo.exclude ^= EXCLUDE_LIGHTS_RADII ) & EXCLUDE_LIGHTS_RADII )
    CheckMenuItem ( ::GetMenu(g_qeglobals.d_hwndMain), ID_VIEW_SHOWLIGHTRADII, MF_BYCOMMAND | MF_UNCHECKED );
	else
    CheckMenuItem ( ::GetMenu(g_qeglobals.d_hwndMain), ID_VIEW_SHOWLIGHTRADII, MF_BYCOMMAND | MF_CHECKED );				

	bool bRadiiOn = !(g_qeglobals.d_savedinfo.exclude & EXCLUDE_LIGHTS_RADII);
	Sys_Printf("( Show light radii = %s %s)\n",(!bRadiiOn?"OFF":"ON"),(!bRadiiOn && gbSelectedLightBrushesAlwaysDrawRadii)?"( unless selected )":"");
	Sys_UpdateWindows (W_XY|W_CAMERA);
}

void CMainFrame::OnViewShowlightradii_KB() 
{
	if ( !( g_qeglobals.d_savedinfo.exclude & EXCLUDE_LIGHTS_RADII ))
	{
		gbSelectedLightBrushesAlwaysDrawRadii = !gbSelectedLightBrushesAlwaysDrawRadii;
	}

	OnViewShowlightradii();
}


void CMainFrame::OnViewShownames() 
{
  g_qeglobals.d_savedinfo.show_names = !g_qeglobals.d_savedinfo.show_names;
  CheckMenuItem ( ::GetMenu(g_qeglobals.d_hwndMain), ID_VIEW_SHOWNAMES, MF_BYCOMMAND | (g_qeglobals.d_savedinfo.show_names ? MF_CHECKED : MF_UNCHECKED)  );
	Map_BuildBrushData();
	Sys_UpdateWindows (W_XY);
}

void CMainFrame::OnViewShowpath() 
{
  if ( ( g_qeglobals.d_savedinfo.exclude ^= EXCLUDE_PATHS ) & EXCLUDE_PATHS )
    CheckMenuItem ( ::GetMenu(g_qeglobals.d_hwndMain), ID_VIEW_SHOWPATH, MF_BYCOMMAND | MF_UNCHECKED );
	else
    CheckMenuItem ( ::GetMenu(g_qeglobals.d_hwndMain), ID_VIEW_SHOWPATH, MF_BYCOMMAND | MF_CHECKED );
	Sys_UpdateWindows (W_XY|W_CAMERA);
}

void CMainFrame::OnViewShowwater() 
{
	if ( ( g_qeglobals.d_savedinfo.exclude ^= EXCLUDE_WATER ) & EXCLUDE_WATER )
    CheckMenuItem ( ::GetMenu(g_qeglobals.d_hwndMain), ID_VIEW_SHOWWATER, MF_BYCOMMAND | MF_UNCHECKED );
	else
    CheckMenuItem ( ::GetMenu(g_qeglobals.d_hwndMain), ID_VIEW_SHOWWATER, MF_BYCOMMAND | MF_CHECKED );
	Sys_UpdateWindows (W_XY|W_CAMERA);
}

void CMainFrame::OnViewShowworld() 
{
	if ( ( g_qeglobals.d_savedinfo.exclude ^= EXCLUDE_WORLD ) & EXCLUDE_WORLD )
    CheckMenuItem ( ::GetMenu(g_qeglobals.d_hwndMain), ID_VIEW_SHOWWORLD, MF_BYCOMMAND | MF_UNCHECKED );
	else
    CheckMenuItem ( ::GetMenu(g_qeglobals.d_hwndMain), ID_VIEW_SHOWWORLD, MF_BYCOMMAND | MF_CHECKED );
	Sys_UpdateWindows (W_XY|W_CAMERA);
}

void CMainFrame::OnViewTexture() 
{
  if (m_nCurrentStyle > 0 && m_nCurrentStyle < 3) // QE4 style
  {
    if (inspector_mode == W_TEXTURE && m_nCurrentStyle != QR_QE4) // are we in console mode already?
    {
      if (::IsWindowVisible(g_qeglobals.d_hwndEntity))
        ::ShowWindow(g_qeglobals.d_hwndEntity, SW_HIDE);
      else
        ::ShowWindow(g_qeglobals.d_hwndEntity, SW_SHOW);
    }
    else
    {
      ::ShowWindow(g_qeglobals.d_hwndEntity, SW_SHOW);
      SetInspectorMode(W_TEXTURE);
    }
  }
}

void CMainFrame::OnViewUpfloor() 
{
  m_pCamWnd->Cam_ChangeFloor (true);
}

void CMainFrame::OnViewXy() 
{
  if (m_nCurrentStyle != 2)
  {
    m_pXYWnd->SetViewType(XY);
    m_pXYWnd->PositionView();
  }
	Sys_UpdateWindows (W_XY);
}

void CMainFrame::OnViewZ100() 
{
  z.scale = 1;
	Sys_UpdateWindows (W_Z|W_Z_OVERLAY);
}

void CMainFrame::OnViewZoomin() 
{
  if (m_pXYWnd && m_pXYWnd->Active())
  {
    m_pXYWnd->SetScale(m_pXYWnd->Scale() * 5.0 / 4);
    if (m_pXYWnd->Scale() > MAX_ZOOM_SCALE)
      m_pXYWnd->SetScale(MAX_ZOOM_SCALE);
  }

  if (m_pXZWnd && m_pXZWnd->Active())
  {
    m_pXZWnd->SetScale(m_pXZWnd->Scale() * 5.0 / 4);
    if (m_pXZWnd->Scale() > MAX_ZOOM_SCALE)
      m_pXZWnd->SetScale(MAX_ZOOM_SCALE);
  }

  if (m_pYZWnd && m_pYZWnd->Active())
  {
    m_pYZWnd->SetScale(m_pYZWnd->Scale() * 5.0 / 4);
    if (m_pYZWnd->Scale() > MAX_ZOOM_SCALE)
      m_pYZWnd->SetScale(MAX_ZOOM_SCALE);
  }


	Sys_UpdateWindows (W_XY|W_XY_OVERLAY);
}

void CMainFrame::OnViewZoomout() 
{

  if (m_pXYWnd && m_pXYWnd->Active())
  {
    m_pXYWnd->SetScale(m_pXYWnd->Scale() * 4.0 / 5);
    if (m_pXYWnd->Scale() < MIN_ZOOM_SCALE)
      m_pXYWnd->SetScale(MIN_ZOOM_SCALE);
  }

  if (m_pXZWnd && m_pXZWnd->Active())
  {
    m_pXZWnd->SetScale(m_pXZWnd->Scale() * 4.0 / 5);
    if (m_pXZWnd->Scale() < MIN_ZOOM_SCALE)
      m_pXZWnd->SetScale(MIN_ZOOM_SCALE);
  }

  if (m_pYZWnd && m_pYZWnd->Active())
  {
    m_pYZWnd->SetScale(m_pYZWnd->Scale() * 4.0 / 5);
    if (m_pYZWnd->Scale() < MIN_ZOOM_SCALE)
      m_pYZWnd->SetScale(MIN_ZOOM_SCALE);
  }
	Sys_UpdateWindows (W_XY|W_XY_OVERLAY);
}

void CMainFrame::OnViewZzoomin() 
{
  z.scale *= 5.0/4;
	if (z.scale > MAX_ZOOM_SCALE)
	  z.scale = MAX_ZOOM_SCALE;
	Sys_UpdateWindows (W_Z|W_Z_OVERLAY);
}

void CMainFrame::OnViewZzoomout() 
{
  z.scale *= 4.0/5;
	if (z.scale < MIN_ZOOM_SCALE)
	  z.scale = MIN_ZOOM_SCALE;
	Sys_UpdateWindows (W_Z|W_Z_OVERLAY);
}

void CMainFrame::OnViewSide() 
{
  if (m_nCurrentStyle != 2)
  {
    m_pXYWnd->SetViewType(XZ);
    m_pXYWnd->PositionView();
  }
	Sys_UpdateWindows (W_XY);
}

void CMainFrame::OnGrid1(unsigned int nID) 
{
  HMENU hMenu = ::GetMenu(GetSafeHwnd());
	
	CheckMenuItem(hMenu, ID_GRID_1, MF_BYCOMMAND | MF_UNCHECKED);
	CheckMenuItem(hMenu, ID_GRID_2, MF_BYCOMMAND | MF_UNCHECKED);
	CheckMenuItem(hMenu, ID_GRID_4, MF_BYCOMMAND | MF_UNCHECKED);
	CheckMenuItem(hMenu, ID_GRID_8, MF_BYCOMMAND | MF_UNCHECKED);
	CheckMenuItem(hMenu, ID_GRID_16, MF_BYCOMMAND | MF_UNCHECKED);
	CheckMenuItem(hMenu, ID_GRID_32, MF_BYCOMMAND | MF_UNCHECKED);
	CheckMenuItem(hMenu, ID_GRID_64, MF_BYCOMMAND | MF_UNCHECKED);

	switch (nID)
	{
		case ID_GRID_1: g_qeglobals.d_gridsize = 0; break;
		case ID_GRID_2: g_qeglobals.d_gridsize = 1; break;
		case ID_GRID_4: g_qeglobals.d_gridsize = 2; break;
		case ID_GRID_8: g_qeglobals.d_gridsize = 3; break;
		case ID_GRID_16: g_qeglobals.d_gridsize = 4; break;
		case ID_GRID_32: g_qeglobals.d_gridsize = 5; break;
		case ID_GRID_64: g_qeglobals.d_gridsize = 6; break;
	}
	g_qeglobals.d_gridsize = 1 << g_qeglobals.d_gridsize;

  if (g_PrefsDlg.m_bSnapTToGrid)
    g_qeglobals.d_savedinfo.m_nTextureTweak = g_qeglobals.d_gridsize;

  SetGridStatus();
	CheckMenuItem(hMenu, nID, MF_BYCOMMAND | MF_CHECKED);
	Sys_UpdateWindows (W_XY|W_Z);
	
}

void CMainFrame::OnTexturesShowinuse() 
{
  Sys_BeginWait ();
	Texture_ShowInuse ();
  if (m_pTexWnd)
    m_pTexWnd->RedrawWindow();
}

//from TexWnd.cpp
extern qboolean	texture_showinuse;
void CMainFrame::OnUpdateTexturesShowinuse(CCmdUI* pCmdUI) 
{
//  pCmdUI->SetCheck(texture_showinuse);	// this is bollocks, this menu is a command, not a bool-toggler
}

void CMainFrame::OnUpdateMiscShowFaces(CCmdUI* pCmdUI)
{
	pCmdUI->SetCheck(qbShowFaces);
}

void CMainFrame::OnTexturesInspector() 
{
  DoSurface ();
}

void CMainFrame::OnMiscBenchmark() 
{
  m_pCamWnd->BenchMark();
}

void CMainFrame::OnMiscFindbrush() 
{
  DoFind();
}

void CMainFrame::OnMiscGamma() 
{
  DoGamma();
  MessageBox("You must restart QERadiant for Gamma settings to take place");
}

void CMainFrame::OnMiscNextleakspot() 
{
  Pointfile_Next();
}

void CMainFrame::OnMiscPreviousleakspot() 
{
  Pointfile_Prev();
}

void CMainFrame::OnMiscPrintxy() 
{
  WXY_Print();
}

void CMainFrame::OnMiscSelectentitycolor() 
{
  if (edit_entity)
  {
    CString strColor = ValueForKey(edit_entity, "_color");
    if (strColor.GetLength() > 0)
    {
      float fR, fG, fB;
	    int n = sscanf(strColor,"%f %f %f", &fR, &fG, &fB);
      if (n == 3)
      {
        g_qeglobals.d_savedinfo.colors[COLOR_ENTITY][0] = fR;
        g_qeglobals.d_savedinfo.colors[COLOR_ENTITY][1] = fG;
        g_qeglobals.d_savedinfo.colors[COLOR_ENTITY][2] = fB;
      }
    }

    if (inspector_mode == W_ENTITY && (DoColor(COLOR_ENTITY)))
    {
	    char buffer[100];
	    sprintf(buffer, "%f %f %f", g_qeglobals.d_savedinfo.colors[COLOR_ENTITY][0],
		          g_qeglobals.d_savedinfo.colors[COLOR_ENTITY][1],
		          g_qeglobals.d_savedinfo.colors[COLOR_ENTITY][2]);
	  
      ::SetWindowText( hwndEnt[EntValueField], buffer );
      ::SetWindowText( hwndEnt[EntKeyField], "_color" );
	    AddProp();
//DK - SOF change to get color to entity quickly
		//--::SetWindowText( hwndEnt[EntValueField], buffer );
//--		::SetWindowText( hwndEnt[EntKeyField], "color" );
//--		AddProp();
    }
    Sys_UpdateWindows( W_ALL );
  }
}

void CMainFrame::OnTexturebk() 
{
  DoColor(COLOR_TEXTUREBACK);
	Sys_UpdateWindows (W_ALL);
}

void CMainFrame::OnColorsMajor() 
{
  DoColor(COLOR_GRIDMAJOR);
	Sys_UpdateWindows (W_ALL);
}

void CMainFrame::OnColorsMinor() 
{
  DoColor(COLOR_GRIDMINOR);
	Sys_UpdateWindows (W_ALL);
}

void CMainFrame::OnColorsXybk() 
{
  DoColor(COLOR_GRIDBACK);
	Sys_UpdateWindows (W_ALL);
}

void CMainFrame::OnBrush3sided() 
{
  Brush_MakeSided(3);
}

void CMainFrame::OnBrush4sided() 
{
  Brush_MakeSided(4);
	
}

void CMainFrame::OnBrush5sided() 
{
  Brush_MakeSided(5);
}

void CMainFrame::OnBrush6sided() 
{
  Brush_MakeSided(6);
}

void CMainFrame::OnBrush7sided() 
{
  Brush_MakeSided(7);
}

void CMainFrame::OnBrush8sided() 
{
  Brush_MakeSided(8);
}

void CMainFrame::OnBrush9sided() 
{
  Brush_MakeSided(9);
}

void CMainFrame::OnBrushArbitrarysided() 
{
  DoSides();
}

void CMainFrame::OnBrushFlipx() 
{
	Select_FlipAxis (0);		// turns curves inside-out
	OnCurveNegative();			//	outside-in them again :-)
}

void CMainFrame::OnBrushFlipy() 
{
	Select_FlipAxis (1);		// turns curves inside-out
	OnCurveNegative();			//	outside-in them again :-)
}

void CMainFrame::OnBrushFlipz() 
{
	Select_FlipAxis (2);		// turns curves inside-out
	OnCurveNegative();			//	outside-in them again :-)
}

void CMainFrame::OnBrushRotatex() 
{
	Select_RotateAxis (0, 90);	
}

void CMainFrame::OnBrushRotatey() 
{
	Select_RotateAxis (1, 90);	
}

void CMainFrame::OnBrushRotatez() 
{
	Select_RotateAxis (2, 90);	
}

void CMainFrame::OnRegionOff() 
{
	Map_RegionOff ();
}

void CMainFrame::OnRegionSetbrush() 
{
	Map_RegionBrush ();
}

void CMainFrame::OnRegionSetselection() 
{
	Map_RegionSelectedBrushes ();
}

void CMainFrame::OnRegionSettallbrush() 
{
	Map_RegionTallBrush ();
}

void CMainFrame::OnRegionSetxy() 
{
	Map_RegionXY ();
}

void CMainFrame::OnSelectionArbitraryrotation() 
{
  if (ActiveXY())
    ActiveXY()->UndoCopy();
  CRotateDlg dlg;
  dlg.DoModal();
  //DoRotate ();
}

bool gbInhibitDupTargetCorrection = false;	// important default!!!!!!!!!
void CMainFrame::OnSelectionClone_NoTargetNameChange() 
{
#ifdef QUAKE3		// probably best not to alter the SoF version so near to project end
	if (ActiveXY())
		ActiveXY()->UndoCopy();

	gbInhibitDupTargetCorrection = true;
	Select_Clone ();
	gbInhibitDupTargetCorrection = false;
#endif
}

void CMainFrame::OnSelectionClone() 
{
  if (ActiveXY())
    ActiveXY()->UndoCopy();
	Select_Clone ();
}

void CMainFrame::OnSelectionConnect() 
{
	ConnectEntities();
}

void CMainFrame::OnSelectionConnectSmart()
{
	ConnectEntities(true);		
}

void CMainFrame::OnSelectionCsgsubtract() 
{
  if (ActiveXY())
    ActiveXY()->UndoCopy();
	CSG_Subtract ();
}

void CMainFrame::OnSelectionDelete() 
{
  if (ActiveXY())
    ActiveXY()->UndoCopy();
	Select_Delete ();
}

void CMainFrame::OnSelectionDeselect() 
{
  if (!ByeByeSurfaceDialog())
  {
    if (g_bClipMode)
      OnViewClipper();
    else 
    if (g_bRotateMode)
      OnSelectMouserotate();
    else
    if (g_bScaleMode)
      OnSelectMousescale();
    else
    if (g_bPathMode)
    {
      if (ActiveXY())
        ActiveXY()->KillPathMode();
    }
    else
    {
      if (g_qeglobals.d_select_mode == sel_curvepoint && g_qeglobals.d_num_move_points > 0)
      {
        g_qeglobals.d_num_move_points = 0;
        Sys_UpdateWindows(W_ALL);
      }
      else
      {
	      Select_Deselect ();
        SetStatusText(2, " ");
      }
    }
  }
}

void CMainFrame::OnSelectionDragedges() 
{
	if (g_qeglobals.d_select_mode == sel_edge)
	{
		g_qeglobals.d_select_mode = sel_brush;
		Sys_UpdateWindows (W_ALL);
	}
	else
	{
		SetupVertexSelection ();
		if (g_qeglobals.d_numpoints)
			g_qeglobals.d_select_mode = sel_edge;
		Sys_UpdateWindows (W_ALL);
	}
}

void CMainFrame::OnSelectionDragvertecies() 
{
	if (g_qeglobals.d_select_mode == sel_vertex || g_qeglobals.d_select_mode == sel_curvepoint)
	{
		g_qeglobals.d_select_mode = sel_brush;
		Sys_UpdateWindows (W_ALL);
	}
	else
	{
	  //--if (QE_SingleBrush() && selected_brushes.next->patchBrush)
    if (OnlyPatchesSelected())
    {
      Patch_EditPatch();
    }
    else if (!AnyPatchesSelected())
    {
		  SetupVertexSelection ();
		  if (g_qeglobals.d_numpoints)
		    g_qeglobals.d_select_mode = sel_vertex;
    }
		Sys_UpdateWindows (W_ALL);
	}
}

void CMainFrame::OnSelectionMakeDetail() 
{	
//	Select_MakeDetail ();
	Select_AutoCaulk(true);	// bMakeDetail
}

void CMainFrame::OnSelectionMakeStructural() 
{
	Select_MakeStructural ();
}

void CMainFrame::OnMakeNonSolid()
{
	Select_MakeNonSolid();
}
void CMainFrame::OnClearNonSolid()
{
	Select_ClearNonSolid();
}

void CMainFrame::OnSelectionHideWaypointChildren()
{
	Select_HideWaypointChildren();
}
void CMainFrame::OnSelectionUnHideWaypointChildren()
{
	Select_UnHideWaypointChildren();
}
void CMainFrame::OnSelectionUnHideAllWaypoints()
{
	Select_UnHideAllWaypoints();
}

void CMainFrame::OnSelectionMakehollow() 
{
  if (ActiveXY())
    ActiveXY()->UndoCopy();
	CSG_MakeHollow ();
}

void CMainFrame::OnSelectionSelectcompletetall() 
{
  if (ActiveXY())
    ActiveXY()->UndoCopy();
  Select_CompleteTall ();
}

void CMainFrame::OnSelectionSelectinside() 
{
	Select_Inside ();
}

void CMainFrame::OnSelectionSelectpartialtall() 
{
  Select_PartialTall ();
}

void CMainFrame::OnSelectionSelecttouching() 
{
	Select_Touching ();
}

void CMainFrame::OnSelectionUngroupentity() 
{
  Select_Ungroup ();
}

void CMainFrame::OnSelectionGroupNamesDisplay()
{
	g_qeglobals.d_savedinfo.exclude ^= SHOW_GROUPNAMES;
	Sys_UpdateWindows (W_XY|W_CAMERA);
}

void CMainFrame::OnSelectionGroupNamesActive()
{
	bGrouping_IsActive = !bGrouping_IsActive;
	if (bGrouping_IsActive)
	{
		Select_Deselect();			// needed to prevent problems with things that shouldn't be selectable?
	}
	Sys_UpdateWindows (W_XY|W_CAMERA);
}

void CMainFrame::OnSelectionGroupNamesGhosted()
{
	bGrouping_IsGhosted = !bGrouping_IsGhosted;
	Sys_UpdateWindows (W_XY|W_CAMERA);
}

void CMainFrame::OnTexturesPopup() 
{
  HandlePopup(this, IDR_POPUP_TEXTURE); 
}

void CMainFrame::OnPopupSelection() 
{
  HandlePopup(this, IDR_POPUP_SELECTION); 
}

void CMainFrame::OnViewChange() 
{
  OnViewNextview();
  //HandlePopup(this, IDR_POPUP_VIEW); 
}

void CMainFrame::OnViewCameraupdate() 
{
  Sys_UpdateWindows(W_CAMERA);
}

void CMainFrame::OnUpdateViewCameraupdate(CCmdUI* pCmdUI) 
{
	pCmdUI->Enable(m_bCamPreview == false);
}

void CMainFrame::OnSizing(UINT fwSide, LPRECT pRect) 
{
  CFrameWnd::OnSizing(fwSide, pRect);
	GetClientRect(g_rctOld);
}

void CMainFrame::OnHelpAbout() 
{
  DoAbout();
}

void CMainFrame::OnViewClipper() 
{
  if (ActiveXY())
  {
    if (ActiveXY()->ClipMode())
    {
      ActiveXY()->SetClipMode(false);
      m_wndToolBar.GetToolBarCtrl().CheckButton(ID_VIEW_CLIPPER, FALSE);
    }
    else
    {
      if (ActiveXY()->RotateMode())
        OnSelectMouserotate();
      ActiveXY()->SetClipMode(true);
      m_wndToolBar.GetToolBarCtrl().CheckButton(ID_VIEW_CLIPPER);
    }
  }
}

void CMainFrame::OnCameraAngledown() 
{
  m_pCamWnd->Camera().angles[0] -= SPEED_TURN;
	if (m_pCamWnd->Camera().angles[0] < -85)
	  m_pCamWnd->Camera().angles[0] = -85;
	Sys_UpdateWindows (W_CAMERA|W_XY_OVERLAY);
}

void CMainFrame::OnCameraAngleup() 
{
  m_pCamWnd->Camera().angles[0] += SPEED_TURN;
  if (m_pCamWnd->Camera().angles[0] > 85)
	  m_pCamWnd->Camera().angles[0] = 85;
  Sys_UpdateWindows (W_CAMERA|W_XY_OVERLAY);
}

void CMainFrame::OnCameraBack() 
{
  VectorMA (m_pCamWnd->Camera().origin, -SPEED_MOVE, m_pCamWnd->Camera().forward, m_pCamWnd->Camera().origin);
  int nUpdate = (g_PrefsDlg.m_bCamXYUpdate) ? (W_CAMERA | W_XY) : (W_CAMERA);
	Sys_UpdateWindows (nUpdate);
}

void CMainFrame::OnCameraDown() 
{
  m_pCamWnd->Camera().origin[2] -= SPEED_MOVE;
  int nUpdate = (g_PrefsDlg.m_bCamXYUpdate) ? (W_CAMERA | W_XY) : (W_CAMERA);
	Sys_UpdateWindows (nUpdate);
}

void CMainFrame::OnCameraForward() 
{
  VectorMA (m_pCamWnd->Camera().origin, SPEED_MOVE, m_pCamWnd->Camera().forward, m_pCamWnd->Camera().origin);
  int nUpdate = (g_PrefsDlg.m_bCamXYUpdate) ? (W_CAMERA | W_XY) : (W_CAMERA);
	Sys_UpdateWindows (nUpdate);
}

void CMainFrame::OnCameraLeft() 
{
  m_pCamWnd->Camera().angles[1] += SPEED_TURN;
  int nUpdate = (g_PrefsDlg.m_bCamXYUpdate) ? (W_CAMERA | W_XY) : (W_CAMERA);
	Sys_UpdateWindows (nUpdate);
}

void CMainFrame::OnCameraRight() 
{
  m_pCamWnd->Camera().angles[1] -= SPEED_TURN;
  int nUpdate = (g_PrefsDlg.m_bCamXYUpdate) ? (W_CAMERA | W_XY) : (W_CAMERA);
	Sys_UpdateWindows (nUpdate);
}

void CMainFrame::OnCameraStrafeleft() 
{
  VectorMA (m_pCamWnd->Camera().origin, -SPEED_MOVE, m_pCamWnd->Camera().right, m_pCamWnd->Camera().origin);
  int nUpdate = (g_PrefsDlg.m_bCamXYUpdate) ? (W_CAMERA | W_XY) : (W_CAMERA);
	Sys_UpdateWindows (nUpdate);
}

void CMainFrame::OnCameraStraferight() 
{
  VectorMA (m_pCamWnd->Camera().origin, SPEED_MOVE, m_pCamWnd->Camera().right, m_pCamWnd->Camera().origin);
  int nUpdate = (g_PrefsDlg.m_bCamXYUpdate) ? (W_CAMERA | W_XY) : (W_CAMERA);
	Sys_UpdateWindows (nUpdate);
}

void CMainFrame::OnCameraUp() 
{
  m_pCamWnd->Camera().origin[2] += SPEED_MOVE;
  int nUpdate = (g_PrefsDlg.m_bCamXYUpdate) ? (W_CAMERA | W_XY) : (W_CAMERA);
	Sys_UpdateWindows (nUpdate);
}

void CMainFrame::OnGridToggle() 
{
  g_qeglobals.d_showgrid = !g_qeglobals.d_showgrid;
	Sys_UpdateWindows (W_XY|W_Z);
}

void CMainFrame::OnEntitylist() 
{
}

void CMainFrame::OnMapinfo() 
{
}

void CMainFrame::OnPrefs() 
{
  int nView = g_PrefsDlg.m_nView;
  BOOL bToolbar = g_PrefsDlg.m_bWideToolbar;
  BOOL bTextureBar = g_PrefsDlg.m_bTextureBar;
  BOOL bSGIOpenGL = g_PrefsDlg.m_bSGIOpenGL;
  BOOL bBuggyICD = g_PrefsDlg.m_bBuggyICD;
  BOOL bShaderTest = g_PrefsDlg.m_bShaderTest;
  g_PrefsDlg.LoadPrefs();
  if (g_PrefsDlg.DoModal() == IDOK)
  {
    if (g_PrefsDlg.m_nView != nView || g_PrefsDlg.m_bWideToolbar != bToolbar 
        || g_PrefsDlg.m_bSGIOpenGL != bSGIOpenGL || g_PrefsDlg.m_bBuggyICD != bBuggyICD || bShaderTest != g_PrefsDlg.m_bShaderTest)
      MessageBox("You need to restart QERadiant for the view changes to take place.");
    if (m_pTexWnd)
      m_pTexWnd->UpdatePrefs();
    if (bTextureBar != g_PrefsDlg.m_bTextureBar)
    {
      if (bTextureBar) // was turned on
        ShowControlBar(&m_wndTextureBar, TRUE, TRUE);
      else // was turned off
        ShowControlBar(&m_wndTextureBar, FALSE, TRUE);
      m_wndTextureBar.Invalidate();
    }
  }
}

// 0 = radiant styel
// 1 = qe4 style
void CMainFrame::SetWindowStyle(int nStyle)
{
}

void CMainFrame::OnTogglecamera() 
{
  if (m_nCurrentStyle > 0 && m_nCurrentStyle < 3) // QE4 style
  {
    if (m_pCamWnd && m_pCamWnd->GetSafeHwnd())
    {
      if (m_pCamWnd->IsWindowVisible())
        m_pCamWnd->ShowWindow(SW_HIDE);
      else
        m_pCamWnd->ShowWindow(SW_SHOW);
    }
  }
}

void CMainFrame::OnToggleconsole() 
{
  if (m_nCurrentStyle > 0 && m_nCurrentStyle < 3) // QE4 style
  {
    if (m_pEditWnd && m_pEditWnd->GetSafeHwnd())
    {
      if (m_pEditWnd->IsWindowVisible())
        m_pEditWnd->ShowWindow(SW_HIDE);
      else
        m_pEditWnd->ShowWindow(SW_SHOW);
    }
  }
}

void CMainFrame::OnToggleview() 
{
  if (m_nCurrentStyle == 1) // QE4 style
  {
    if (m_pXYWnd && m_pXYWnd->GetSafeHwnd())
    {
      if (m_pXYWnd->IsWindowVisible())
        m_pXYWnd->ShowWindow(SW_HIDE);
      else
        m_pXYWnd->ShowWindow(SW_SHOW);
    }
  }
}

void CMainFrame::OnTogglez() 
{
  if (m_nCurrentStyle == 1 || m_nCurrentStyle == 2) // QE4 style
  {
    if (m_pZWnd && m_pZWnd->GetSafeHwnd())
    {
      if (m_pZWnd->IsWindowVisible())
        m_pZWnd->ShowWindow(SW_HIDE);
      else
        m_pZWnd->ShowWindow(SW_SHOW);
    }
  }
}

void CMainFrame::OnToggleLock() 
{
  g_PrefsDlg.m_bTextureLock = !g_PrefsDlg.m_bTextureLock;
  CMenu* pMenu = GetMenu();
  if (pMenu)
    pMenu->CheckMenuItem(ID_TOGGLE_LOCK, MF_BYCOMMAND | (g_PrefsDlg.m_bTextureLock) ? MF_CHECKED : MF_UNCHECKED);
  g_PrefsDlg.SavePrefs();

//  Sys_Printf(va("Texture Lock: %s\n",g_PrefsDlg.m_bTextureLock?"ON":"OFF"));
  SetGridStatus();
}

void CMainFrame::OnEditMapinfo() 
{
  CMapInfo dlg;
  dlg.DoModal();
}

void CMainFrame::OnEditEntityinfo() 
{
  CEntityListDlg dlg;
  dlg.DoModal();
}



void CMainFrame::OnBrushScripts() 
{
  CScriptDlg dlg;
  dlg.DoModal();
}

void CMainFrame::OnViewNextview() 
{
  if (m_nCurrentStyle != 2)
  {
    if (m_pXYWnd->GetViewType() == XY)
      m_pXYWnd->SetViewType(XZ);
    else 
    if (m_pXYWnd->GetViewType() ==  XZ)
      m_pXYWnd->SetViewType(YZ);
    else
      m_pXYWnd->SetViewType(XY);
    m_pXYWnd->PositionView();
	  Sys_UpdateWindows (W_XY);
  }
}

void CMainFrame::OnHelpCommandlist() 
{
  CCommandsDlg dlg;
  dlg.DoModal();
#if 0
  if (g_b3Dfx)
  {
    C3DFXCamWnd* pWnd = new C3DFXCamWnd();
    CRect rect(50,50,400, 400);
    pWnd->Create(_3DFXCAMERA_WINDOW_CLASS, "", QE3_CHILDSTYLE, rect, this, 1234);
    pWnd->ShowWindow(SW_SHOW);
  }
#endif
}

void CMainFrame::OnFileNewproject() 
{
  CNewProjDlg dlg;
  if (dlg.DoModal() == IDOK && dlg.m_strName.GetLength() > 0)
  {
    CString strQ2;
    CString strQ2File;
    ExtractPath_and_Filename(g_PrefsDlg.m_strQuake2, strQ2, strQ2File);


    AddSlash(strQ2);
    strQ2 += dlg.m_strName;
    CString strProjToLoad;
    CString strMapToLoad;
    bool bGood = true;
    if (::CreateDirectory(strQ2, NULL))
    {
      CString strDir = strQ2;
      strDir += "\\maps";
      if (::CreateDirectory(strDir, NULL))
      {
        CString strSource = g_strAppPath;
        AddSlash(strSource);
        strSource += "projmap.dat";
        CString strDest = strDir;
        AddSlash(strDest);
        CString strName;
        strName.Format("%s.map", dlg.m_strName);
        strDest += strName;
        strMapToLoad = strDest;
        if (!::CopyFile(strSource, strDest, FALSE))
          bGood = false;
      }
      else bGood = false;

      strDir = strQ2;
      strDir += "\\pics";
      if (::CreateDirectory(strDir, NULL))
      {
        CString strSource = g_strAppPath;
        AddSlash(strSource);
        strSource += "colormap.pcx";
        CString strDest = strDir;
        AddSlash(strDest);
        strDest += "colormap.pcx";
        if (!::CopyFile(strSource, strDest, FALSE))
          bGood = false;
      }
      else bGood = false;

      strDir = strQ2;
      strDir += "\\scripts";
      if (::CreateDirectory(strDir, NULL))
      {
        CString strSource = g_strAppPath;
        AddSlash(strSource);
        strSource += "projqe4.dat";
        CString strDest = strDir;
        AddSlash(strDest);
        strDest += "quake.qe4";
        if (!::CopyFile(strSource, strDest, FALSE))
          bGood = false;
        else
          strProjToLoad = strDest;
      }
      else bGood = false;
      if (bGood && strProjToLoad.GetLength() > 0)
      {
	      if (QE_LoadProject(strProjToLoad.GetBuffer(0)))
        {
          if (strMapToLoad.GetLength() > 0)
            Map_LoadFile(strMapToLoad.GetBuffer(0));
        }
      }
    }
    else 
    {
      CString strMsg;
      strMsg.Format("Unable to create directory %s", strQ2);
      MessageBox(strMsg);
    }

  }
}

void CMainFrame::UpdateStatusText()
{
  for (int n = 0; n < 6; n++)
  {
    if (m_strStatus[n].GetLength() >= 0 && m_wndStatusBar.GetSafeHwnd())
		  m_wndStatusBar.SetPaneText(n, m_strStatus[n]);
  }
}

void CMainFrame::SetStatusText(int nPane, const char * pText)
{
  if (pText && nPane <= 5 && nPane > 0)
  {
    m_strStatus[nPane] = pText;
    UpdateStatusText();
  }
}

void CMainFrame::UpdateWindows(int nBits)
{

  if (!g_bScreenUpdates)
    return;

  if (nBits & (W_XY | W_XY_OVERLAY))
  {
	  if (m_pXYWnd)
      m_pXYWnd->RedrawWindow(NULL, NULL, RDW_INVALIDATE | RDW_UPDATENOW);
	  if (m_pXZWnd)
      m_pXZWnd->RedrawWindow(NULL, NULL, RDW_INVALIDATE | RDW_UPDATENOW);
	  if (m_pYZWnd)
      m_pYZWnd->RedrawWindow(NULL, NULL, RDW_INVALIDATE | RDW_UPDATENOW);
  }

	if (nBits & W_CAMERA || ((nBits & W_CAMERA_IFON) && m_bCamPreview))
  {
    if (m_pCamWnd)
    {
      m_pCamWnd->RedrawWindow(NULL, NULL, RDW_INVALIDATE | RDW_UPDATENOW);
    }
  }

  if (nBits & (W_Z | W_Z_OVERLAY))
  {
    if (m_pZWnd)
      m_pZWnd->RedrawWindow(NULL, NULL, RDW_INVALIDATE | RDW_UPDATENOW);
  }
		
	if (nBits & W_TEXTURE)
  {
    if (m_pTexWnd)
      m_pTexWnd->RedrawWindow(NULL, NULL, RDW_INVALIDATE | RDW_UPDATENOW);
  }
}

void Sys_UpdateWindows (int nBits)
{
  if (g_PrefsDlg.m_bQE4Painting)
    g_nUpdateBits |= nBits;
  else
    g_pParentWnd->UpdateWindows(nBits);
}



void CMainFrame::OnFlipClip() 
{
	if (m_pActiveXY)
    m_pActiveXY->FlipClip();
}

void CMainFrame::OnClipSelected() 
{
	if (m_pActiveXY && m_pActiveXY->ClipMode())
  {
    m_pActiveXY->Clip();
  }
  else
  {
    if (g_bPatchBendMode)
      Patch_BendHandleENTER();
    else if (g_bPatchBendMode)
      Patch_InsDelHandleENTER();
  }
}

void CMainFrame::OnSplitSelected() 
{
	if (m_pActiveXY)
    m_pActiveXY->SplitClip();
}

CXYWnd* CMainFrame::ActiveXY()
{
  return m_pActiveXY;
}


void CMainFrame::OnToggleviewXz() 
{
  if (m_nCurrentStyle == 1) // QE4 style
  {
    if (m_pXZWnd && m_pXZWnd->GetSafeHwnd())
    {
       // get windowplacement doesn't actually save this so we will here
      g_PrefsDlg.m_bXZVis = m_pXZWnd->IsWindowVisible();
      if (g_PrefsDlg.m_bXZVis)
        m_pXZWnd->ShowWindow(SW_HIDE);
      else
        m_pXZWnd->ShowWindow(SW_SHOW);
      g_PrefsDlg.m_bXZVis ^= 1;
      g_PrefsDlg.SavePrefs();
    }
  }
}

void CMainFrame::OnToggleviewYz() 
{
  if (m_nCurrentStyle == 1) // QE4 style
  {
    if (m_pYZWnd && m_pYZWnd->GetSafeHwnd())
    {
      g_PrefsDlg.m_bYZVis = m_pYZWnd->IsWindowVisible();
      if (g_PrefsDlg.m_bYZVis)
        m_pYZWnd->ShowWindow(SW_HIDE);
      else
        m_pYZWnd->ShowWindow(SW_SHOW);
      g_PrefsDlg.m_bYZVis ^= 1;
      g_PrefsDlg.SavePrefs();
    }
  }
}

void CMainFrame::OnColorsBrush() 
{
  DoColor(COLOR_BRUSHES);
	Sys_UpdateWindows (W_ALL);
}

void CMainFrame::OnColorsClipper() 
{
  DoColor(COLOR_CLIPPER);
	Sys_UpdateWindows (W_ALL);
}

void CMainFrame::OnColorsGridtext() 
{
  DoColor(COLOR_GRIDTEXT);
	Sys_UpdateWindows (W_ALL);
}

void CMainFrame::OnColorsSelectedbrush() 
{
  DoColor(COLOR_SELBRUSHES);
	Sys_UpdateWindows (W_ALL);
}

void CMainFrame::OnColorsGridblock() 
{
  DoColor(COLOR_GRIDBLOCK);
	Sys_UpdateWindows (W_ALL);
}



void CMainFrame::OnColorsViewname() 
{
  DoColor(COLOR_VIEWNAME);
	Sys_UpdateWindows (W_ALL);
}

void CMainFrame::OnColorSetoriginal() 
{
	for (int i=0 ; i<3 ; i++)
	{
		g_qeglobals.d_savedinfo.colors[COLOR_TEXTUREBACK][i] = 0.25;
		g_qeglobals.d_savedinfo.colors[COLOR_GRIDBACK][i] = 1.0;
		g_qeglobals.d_savedinfo.colors[COLOR_GRIDMINOR][i] = 0.75;
		g_qeglobals.d_savedinfo.colors[COLOR_GRIDMAJOR][i] = 0.5;
		g_qeglobals.d_savedinfo.colors[COLOR_CAMERABACK][i] = 0.25;
	}

	g_qeglobals.d_savedinfo.colors[COLOR_GRIDBLOCK][0] = 0.0;
	g_qeglobals.d_savedinfo.colors[COLOR_GRIDBLOCK][1] = 0.0;
	g_qeglobals.d_savedinfo.colors[COLOR_GRIDBLOCK][2] = 1.0;

	g_qeglobals.d_savedinfo.colors[COLOR_GRIDTEXT][0] = 0.0;
	g_qeglobals.d_savedinfo.colors[COLOR_GRIDTEXT][1] = 0.0;
	g_qeglobals.d_savedinfo.colors[COLOR_GRIDTEXT][2] = 0.0;

	g_qeglobals.d_savedinfo.colors[COLOR_SELBRUSHES][0] = 1.0;
	g_qeglobals.d_savedinfo.colors[COLOR_SELBRUSHES][1] = 0.0;
	g_qeglobals.d_savedinfo.colors[COLOR_SELBRUSHES][2] = 0.0;

	g_qeglobals.d_savedinfo.colors[COLOR_CLIPPER][0] = 0.0;
	g_qeglobals.d_savedinfo.colors[COLOR_CLIPPER][1] = 0.0;
	g_qeglobals.d_savedinfo.colors[COLOR_CLIPPER][2] = 1.0;

	g_qeglobals.d_savedinfo.colors[COLOR_BRUSHES][0] = 0.0;
	g_qeglobals.d_savedinfo.colors[COLOR_BRUSHES][1] = 0.0;
	g_qeglobals.d_savedinfo.colors[COLOR_BRUSHES][2] = 0.0;

	g_qeglobals.d_savedinfo.colors[COLOR_VIEWNAME][0] = 0.5;
	g_qeglobals.d_savedinfo.colors[COLOR_VIEWNAME][1] = 0.0;
	g_qeglobals.d_savedinfo.colors[COLOR_VIEWNAME][2] = 0.75;
	Sys_UpdateWindows (W_ALL);
}

void CMainFrame::OnColorSetqer() 
{
	for (int i=0 ; i<3 ; i++)
	{
		g_qeglobals.d_savedinfo.colors[COLOR_TEXTUREBACK][i] = 0.25;
		g_qeglobals.d_savedinfo.colors[COLOR_GRIDBACK][i] = 1.0;
		g_qeglobals.d_savedinfo.colors[COLOR_GRIDMINOR][i] = 1.0;
		g_qeglobals.d_savedinfo.colors[COLOR_GRIDMAJOR][i] = 0.5;
		g_qeglobals.d_savedinfo.colors[COLOR_CAMERABACK][i] = 0.25;
	}

	g_qeglobals.d_savedinfo.colors[COLOR_GRIDBLOCK][0] = 0.0;
	g_qeglobals.d_savedinfo.colors[COLOR_GRIDBLOCK][1] = 0.0;
	g_qeglobals.d_savedinfo.colors[COLOR_GRIDBLOCK][2] = 1.0;

	g_qeglobals.d_savedinfo.colors[COLOR_GRIDTEXT][0] = 0.0;
	g_qeglobals.d_savedinfo.colors[COLOR_GRIDTEXT][1] = 0.0;
	g_qeglobals.d_savedinfo.colors[COLOR_GRIDTEXT][2] = 0.0;

	g_qeglobals.d_savedinfo.colors[COLOR_SELBRUSHES][0] = 1.0;
	g_qeglobals.d_savedinfo.colors[COLOR_SELBRUSHES][1] = 0.0;
	g_qeglobals.d_savedinfo.colors[COLOR_SELBRUSHES][2] = 0.0;

	g_qeglobals.d_savedinfo.colors[COLOR_CLIPPER][0] = 0.0;
	g_qeglobals.d_savedinfo.colors[COLOR_CLIPPER][1] = 0.0;
	g_qeglobals.d_savedinfo.colors[COLOR_CLIPPER][2] = 1.0;

	g_qeglobals.d_savedinfo.colors[COLOR_BRUSHES][0] = 0.0;
	g_qeglobals.d_savedinfo.colors[COLOR_BRUSHES][1] = 0.0;
	g_qeglobals.d_savedinfo.colors[COLOR_BRUSHES][2] = 0.0;

	g_qeglobals.d_savedinfo.colors[COLOR_VIEWNAME][0] = 0.5;
	g_qeglobals.d_savedinfo.colors[COLOR_VIEWNAME][1] = 0.0;
	g_qeglobals.d_savedinfo.colors[COLOR_VIEWNAME][2] = 0.75;
	Sys_UpdateWindows (W_ALL);
}

void CMainFrame::OnColorSetblack() 
{
	for (int i=0 ; i<3 ; i++)
	{
		g_qeglobals.d_savedinfo.colors[COLOR_TEXTUREBACK][i] = 0.25;
		g_qeglobals.d_savedinfo.colors[COLOR_GRIDBACK][i] = 0.0;
		g_qeglobals.d_savedinfo.colors[COLOR_GRIDMINOR][i] = 0.0;
		g_qeglobals.d_savedinfo.colors[COLOR_CAMERABACK][i] = 0.25;
	}

	g_qeglobals.d_savedinfo.colors[COLOR_GRIDMAJOR][0] = 0.3;
	g_qeglobals.d_savedinfo.colors[COLOR_GRIDMAJOR][1] = 0.5;
	g_qeglobals.d_savedinfo.colors[COLOR_GRIDMAJOR][2] = 0.5;

	g_qeglobals.d_savedinfo.colors[COLOR_GRIDBLOCK][0] = 0.0;
	g_qeglobals.d_savedinfo.colors[COLOR_GRIDBLOCK][1] = 0.0;
	g_qeglobals.d_savedinfo.colors[COLOR_GRIDBLOCK][2] = 1.0;

	g_qeglobals.d_savedinfo.colors[COLOR_GRIDTEXT][0] = 1.0;
	g_qeglobals.d_savedinfo.colors[COLOR_GRIDTEXT][1] = 1.0;
	g_qeglobals.d_savedinfo.colors[COLOR_GRIDTEXT][2] = 1.0;

	g_qeglobals.d_savedinfo.colors[COLOR_SELBRUSHES][0] = 1.0;
	g_qeglobals.d_savedinfo.colors[COLOR_SELBRUSHES][1] = 0.0;
	g_qeglobals.d_savedinfo.colors[COLOR_SELBRUSHES][2] = 0.0;

	g_qeglobals.d_savedinfo.colors[COLOR_CLIPPER][0] = 0.0;
	g_qeglobals.d_savedinfo.colors[COLOR_CLIPPER][1] = 0.0;
	g_qeglobals.d_savedinfo.colors[COLOR_CLIPPER][2] = 1.0;

	g_qeglobals.d_savedinfo.colors[COLOR_BRUSHES][0] = 1.0;
	g_qeglobals.d_savedinfo.colors[COLOR_BRUSHES][1] = 1.0;
	g_qeglobals.d_savedinfo.colors[COLOR_BRUSHES][2] = 1.0;

	g_qeglobals.d_savedinfo.colors[COLOR_VIEWNAME][0] = 0.7;
	g_qeglobals.d_savedinfo.colors[COLOR_VIEWNAME][1] = 0.7;
	g_qeglobals.d_savedinfo.colors[COLOR_VIEWNAME][2] = 0.0;
	Sys_UpdateWindows (W_ALL);
}

void CMainFrame::OnSnaptogrid() 
{
  g_bSnapToGrid ^= 1;
  CMenu* pMenu = GetMenu();
  if (pMenu)
    pMenu->CheckMenuItem(ID_SNAPTOGRID, MF_BYCOMMAND | (g_bSnapToGrid) ? MF_CHECKED : MF_UNCHECKED);
}


void CMainFrame::OnSelectScale() 
{
	if (ActiveXY())
		ActiveXY()->UndoCopy();
	CScaleDialog dlg;
	if (dlg.DoModal() == IDOK)
	{
		if (dlg.m_fX > 0 && dlg.m_fY > 0 && dlg.m_fZ > 0)
		{
			Select_Scale(dlg.m_fX, dlg.m_fY, dlg.m_fZ);
			Sys_UpdateWindows (W_ALL);
		}
		else
			Sys_Printf("Warning.. Tried to scale by a zero value.");
	}
}

void CMainFrame::OnSelectMouserotate() 
{
	if (ActiveXY())
	{
		if (ActiveXY()->ClipMode())
			OnViewClipper();
		if (ActiveXY()->RotateMode())
		{
			ActiveXY()->SetRotateMode(false);
			m_wndToolBar.GetToolBarCtrl().CheckButton(ID_SELECT_MOUSEROTATE, FALSE);
			Map_BuildBrushData();
		}
		else
		{
			ActiveXY()->SetRotateMode(true);
			m_wndToolBar.GetToolBarCtrl().CheckButton(ID_SELECT_MOUSEROTATE);
		}
	}
}

void CMainFrame::OnEditCopybrush() 
{
	if (ActiveXY())
		ActiveXY()->Copy();
}

void CMainFrame::OnEditPastebrush() 
{
	if (ActiveXY())
		ActiveXY()->Paste();
}

void CMainFrame::OnEditPastebrushNoTargetnameChange()	// a Mike G request
{
	gbInhibitDupTargetCorrection = true;

	if (ActiveXY())
		ActiveXY()->Paste();

	gbInhibitDupTargetCorrection = false;
}

void CMainFrame::OnEditUndo()
{
	if (ActiveXY())
		ActiveXY()->Undo();
}

void CMainFrame::OnUpdateEditUndo(CCmdUI* pCmdUI)
{
	BOOL bEnable = false;
	if (ActiveXY())
		bEnable = ActiveXY()->UndoAvailable();
	pCmdUI->Enable(bEnable);
}

void CMainFrame::OnSelectionTextureDec() 
{
	g_qeglobals.d_savedinfo.m_nTextureTweak--;
	if (g_qeglobals.d_savedinfo.m_nTextureTweak == 0)
		g_qeglobals.d_savedinfo.m_nTextureTweak--;
	SetTexValStatus();
	Sys_Printf("Texture scaling set to %d\n",g_qeglobals.d_savedinfo.m_nTextureTweak);
}

void CMainFrame::OnSelectionTextureFit() 
{
	// TODO: Add your command handler code here
	
}

void CMainFrame::OnSelectionTextureInc() 
{
  g_qeglobals.d_savedinfo.m_nTextureTweak++;
  if (g_qeglobals.d_savedinfo.m_nTextureTweak == 0)
    g_qeglobals.d_savedinfo.m_nTextureTweak++;
  SetTexValStatus();
  Sys_Printf("Texture scaling set to %d\n",g_qeglobals.d_savedinfo.m_nTextureTweak);
}

void CMainFrame::OnSelectionTextureRotateclock() 
{
  Select_RotateTexture(abs(g_PrefsDlg.m_nRotation));
}

void CMainFrame::OnSelectionTextureRotatecounter() 
{
  Select_RotateTexture(-abs(g_PrefsDlg.m_nRotation));
}

void CMainFrame::OnSelectionTextureScaledown() 
{
	Select_ScaleTexture(0, -abs(g_qeglobals.d_savedinfo.m_nTextureTweak));
}

void CMainFrame::OnSelectionTextureScaleup() 
{
	Select_ScaleTexture(0, abs(g_qeglobals.d_savedinfo.m_nTextureTweak));
}

void CMainFrame::OnSelectionTextureScaleLeft() 
{
	Select_ScaleTexture(-abs(g_qeglobals.d_savedinfo.m_nTextureTweak),0);
}

void CMainFrame::OnSelectionTextureScaleRight() 
{
	Select_ScaleTexture(abs(g_qeglobals.d_savedinfo.m_nTextureTweak),0);
}



void CMainFrame::OnSelectionTextureShiftdown() 
{
  Select_ShiftTexture(0, -abs(g_qeglobals.d_savedinfo.m_nTextureTweak));
}

void CMainFrame::OnSelectionTextureShiftleft() 
{
  Select_ShiftTexture(-abs(g_qeglobals.d_savedinfo.m_nTextureTweak), 0);
}

void CMainFrame::OnSelectionTextureShiftright() 
{
  Select_ShiftTexture(abs(g_qeglobals.d_savedinfo.m_nTextureTweak), 0);
}

void CMainFrame::OnSelectionTextureShiftup() 
{
  Select_ShiftTexture(0, abs(g_qeglobals.d_savedinfo.m_nTextureTweak));
}

void CMainFrame::OnGridNext() 
{
  if (g_qeglobals.d_gridsize < 64)
  {
    g_qeglobals.d_gridsize = g_qeglobals.d_gridsize << 1;
    Sys_UpdateWindows(W_XY | W_Z);
    SetGridStatus();

    HMENU hMenu = ::GetMenu(GetSafeHwnd());
	  CheckMenuItem(hMenu, ID_GRID_1, MF_BYCOMMAND | MF_UNCHECKED);
	  CheckMenuItem(hMenu, ID_GRID_2, MF_BYCOMMAND | MF_UNCHECKED);
	  CheckMenuItem(hMenu, ID_GRID_4, MF_BYCOMMAND | MF_UNCHECKED);
	  CheckMenuItem(hMenu, ID_GRID_8, MF_BYCOMMAND | MF_UNCHECKED);
	  CheckMenuItem(hMenu, ID_GRID_16, MF_BYCOMMAND | MF_UNCHECKED);
	  CheckMenuItem(hMenu, ID_GRID_32, MF_BYCOMMAND | MF_UNCHECKED);
	  CheckMenuItem(hMenu, ID_GRID_64, MF_BYCOMMAND | MF_UNCHECKED);

    int nID;
	  switch (g_qeglobals.d_gridsize)
	  {
		  case  1: nID = ID_GRID_1; break;
		  case  2: nID = ID_GRID_2; break;
		  case  4: nID = ID_GRID_4; break;
		  case  8: nID = ID_GRID_8; break;
		  case  16: nID = ID_GRID_16; break;
		  case  32: nID = ID_GRID_32; break;
		  case  64: nID = ID_GRID_64; break;
	  }
	  CheckMenuItem(hMenu, nID, MF_BYCOMMAND | MF_CHECKED);
  }
}

void CMainFrame::OnGridPrev() 
{
  if (g_qeglobals.d_gridsize > 1)
  {
    g_qeglobals.d_gridsize = g_qeglobals.d_gridsize >> 1;
    Sys_UpdateWindows(W_XY | W_Z);
    SetGridStatus();
    HMENU hMenu = ::GetMenu(GetSafeHwnd());
	  CheckMenuItem(hMenu, ID_GRID_1, MF_BYCOMMAND | MF_UNCHECKED);
	  CheckMenuItem(hMenu, ID_GRID_2, MF_BYCOMMAND | MF_UNCHECKED);
	  CheckMenuItem(hMenu, ID_GRID_4, MF_BYCOMMAND | MF_UNCHECKED);
	  CheckMenuItem(hMenu, ID_GRID_8, MF_BYCOMMAND | MF_UNCHECKED);
	  CheckMenuItem(hMenu, ID_GRID_16, MF_BYCOMMAND | MF_UNCHECKED);
	  CheckMenuItem(hMenu, ID_GRID_32, MF_BYCOMMAND | MF_UNCHECKED);
	  CheckMenuItem(hMenu, ID_GRID_64, MF_BYCOMMAND | MF_UNCHECKED);

    int nID;
	  switch (g_qeglobals.d_gridsize)
	  {
		  case  1: nID = ID_GRID_1; break;
		  case  2: nID = ID_GRID_2; break;
		  case  4: nID = ID_GRID_4; break;
		  case  8: nID = ID_GRID_8; break;
		  case  16: nID = ID_GRID_16; break;
		  case  32: nID = ID_GRID_32; break;
		  case  64: nID = ID_GRID_64; break;
	  }
	  CheckMenuItem(hMenu, nID, MF_BYCOMMAND | MF_CHECKED);
  }
}


void CMainFrame::SetGridStatus()
{
  CString strStatus;
  char c1;
  char c2;
  c1 = (g_PrefsDlg.m_bTextureLock) ? 'M' : ' ';
  c2 = (g_PrefsDlg.m_bRotateLock) ? 'R' : ' ';
  strStatus.Format("G:%i T:%i R:%i C:%i L:%c%c", g_qeglobals.d_gridsize, g_qeglobals.d_savedinfo.m_nTextureTweak, g_PrefsDlg.m_nRotation, g_PrefsDlg.m_nCubicScale, c1, c2);
  SetStatusText(4, strStatus);
}


void CMainFrame::SetTexValStatus()
{
  //CString strStatus;
  //strStatus.Format("T: %i C: %i", g_nTextureTweak, g_nCubicScale);
  //SetStatusText(5, strStatus.GetBuffer(0));
  SetGridStatus();
}

void CMainFrame::OnTextureReplaceall() 
{
  CFindTextureDlg::show();
}


void CMainFrame::OnScalelockx() 
{
  if (g_nScaleHow & SCALE_X)
  {
    g_nScaleHow ^= SCALE_X;
    m_wndToolBar.GetToolBarCtrl().CheckButton(ID_SCALELOCKX, FALSE);
  }
  else
  {
    g_nScaleHow |= SCALE_X;
    m_wndToolBar.GetToolBarCtrl().CheckButton(ID_SCALELOCKX);
  }
}

void CMainFrame::OnScalelocky() 
{
  if (g_nScaleHow & SCALE_Y)
  {
    g_nScaleHow ^= SCALE_Y;
    m_wndToolBar.GetToolBarCtrl().CheckButton(ID_SCALELOCKY, FALSE);
  }
  else
  {
    g_nScaleHow |= SCALE_Y;
    m_wndToolBar.GetToolBarCtrl().CheckButton(ID_SCALELOCKY);
  }
}

void CMainFrame::OnScalelockz() 
{
  if (g_nScaleHow & SCALE_Z)
  {
    g_nScaleHow ^= SCALE_Z;
    m_wndToolBar.GetToolBarCtrl().CheckButton(ID_SCALELOCKZ, FALSE);
  }
  else
  {
    g_nScaleHow |= SCALE_Z;
    m_wndToolBar.GetToolBarCtrl().CheckButton(ID_SCALELOCKZ);
  }
}

void CMainFrame::OnSelectMousescale() 
{
  if (ActiveXY())
  {
    if (ActiveXY()->ClipMode())
      OnViewClipper();
    if (ActiveXY()->RotateMode())
    {
      ActiveXY()->SetRotateMode(false);
      m_wndToolBar.GetToolBarCtrl().CheckButton(ID_SELECT_MOUSESCALE, FALSE);
    }
    if (ActiveXY()->ScaleMode())
    {
      ActiveXY()->SetScaleMode(false);
      m_wndToolBar.GetToolBarCtrl().CheckButton(ID_SELECT_MOUSESCALE, FALSE);
    }
    else
    {
      ActiveXY()->SetScaleMode(true);
      m_wndToolBar.GetToolBarCtrl().CheckButton(ID_SELECT_MOUSESCALE);
    }
  }
}

void CMainFrame::OnFileImport() 
{
}

void CMainFrame::OnFileProjectsettings() 
{
  DoProjectSettings();
}

void CMainFrame::OnUpdateFileImport(CCmdUI* pCmdUI) 
{
  pCmdUI->Enable(FALSE);
}

#define CUBIC_MAX_LIMIT (WORLD_SIZE/64)

void CMainFrame::OnViewCubein() 
{
  g_PrefsDlg.m_nCubicScale--;
  if (g_PrefsDlg.m_nCubicScale < 1)
    g_PrefsDlg.m_nCubicScale = 1;
  Sys_Printf("CubicScale: %d/%d\n",g_PrefsDlg.m_nCubicScale,CUBIC_MAX_LIMIT);
  g_PrefsDlg.SavePrefs();
	Sys_UpdateWindows(W_CAMERA);
  SetTexValStatus();
}

void CMainFrame::OnViewCubeout() 
{
  g_PrefsDlg.m_nCubicScale++;
	if (g_PrefsDlg.m_nCubicScale > CUBIC_MAX_LIMIT)
	  g_PrefsDlg.m_nCubicScale = CUBIC_MAX_LIMIT;

  Sys_Printf("CubicScale: %d/%d\n",g_PrefsDlg.m_nCubicScale,CUBIC_MAX_LIMIT);
  g_PrefsDlg.SavePrefs();
	Sys_UpdateWindows(W_CAMERA);
  SetTexValStatus();
}

void CMainFrame::OnViewCubein10() 
{
  g_PrefsDlg.m_nCubicScale-=10;
  if (g_PrefsDlg.m_nCubicScale < 1)
    g_PrefsDlg.m_nCubicScale = 1;
  Sys_Printf("CubicScale: %d/%d\n",g_PrefsDlg.m_nCubicScale,CUBIC_MAX_LIMIT);
  g_PrefsDlg.SavePrefs();
	Sys_UpdateWindows(W_CAMERA);
  SetTexValStatus();
}

void CMainFrame::OnViewCubeout10() 
{
  g_PrefsDlg.m_nCubicScale+=10;
	if (g_PrefsDlg.m_nCubicScale > CUBIC_MAX_LIMIT)
	  g_PrefsDlg.m_nCubicScale = CUBIC_MAX_LIMIT;

  Sys_Printf("CubicScale: %d/%d\n",g_PrefsDlg.m_nCubicScale,CUBIC_MAX_LIMIT);
  g_PrefsDlg.SavePrefs();
	Sys_UpdateWindows(W_CAMERA);
  SetTexValStatus();
}


void CMainFrame::OnViewCubicclipping() 
{
  g_PrefsDlg.m_bCubicClipping ^= 1;
  CMenu* pMenu = GetMenu();
  if (pMenu)
    pMenu->CheckMenuItem(ID_VIEW_CUBICCLIPPING, MF_BYCOMMAND | (g_PrefsDlg.m_bCubicClipping) ? MF_CHECKED : MF_UNCHECKED);
  m_wndToolBar.GetToolBarCtrl().CheckButton(ID_VIEW_CUBICCLIPPING, (g_PrefsDlg.m_bCubicClipping) ? TRUE : FALSE);
  g_PrefsDlg.SavePrefs();
  Map_BuildBrushData ();
  Sys_UpdateWindows(W_CAMERA);
}


void CMainFrame::OnFileSaveregion() 
{
  SaveAsDialog (true);
}

void CMainFrame::OnUpdateFileSaveregion(CCmdUI* pCmdUI) 
{
  pCmdUI->Enable(static_cast<BOOL>(region_active));
}

void CMainFrame::OnSelectionMovedown() 
{
  vec3_t vAmt;
  vAmt[0] = vAmt[1] = 0.0;
  vAmt[2] = -g_qeglobals.d_gridsize;
	Select_Move (vAmt);
  Sys_UpdateWindows(W_CAMERA | W_XY | W_Z);
}

void CMainFrame::OnSelectionMoveup() 
{
  vec3_t vAmt;
  vAmt[0] = vAmt[1] = 0.0;
  vAmt[2] = g_qeglobals.d_gridsize;
	Select_Move (vAmt);
  Sys_UpdateWindows(W_CAMERA | W_XY | W_Z);
}

void CMainFrame::OnToolbarMain() 
{

}

void CMainFrame::OnToolbarTexture() 
{
	
}

void CMainFrame::OnSelectionPrint() 
{
  for (brush_t* b=selected_brushes.next ; b != &selected_brushes ; b=b->next)
    Brush_Print(b);
}

void CMainFrame::UpdateTextureBar()
{
  if (m_wndTextureBar.GetSafeHwnd())
    m_wndTextureBar.GetSurfaceAttributes();
}

bool g_bTABDown = false;
bool g_bOriginalFlag;
void CMainFrame::OnSelectionTogglesizepaint() 
{
  if (::GetAsyncKeyState('Q'))
  {
    if (!g_bTABDown)
    {
      g_bTABDown = true;
      g_bOriginalFlag = g_PrefsDlg.m_bSizePaint;
      g_PrefsDlg.m_bSizePaint = !g_bOriginalFlag;
      Sys_UpdateWindows(W_XY);
      return;
    }
  }
  else
  {
    g_bTABDown = false;
    g_PrefsDlg.m_bSizePaint = g_bOriginalFlag;
    Sys_UpdateWindows(W_XY);
    return;
  }
}

void CMainFrame::OnBrushMakecone() 
{
  DoSides(true);
}


void CMainFrame::OnTexturesLoad() 
{
  BROWSEINFO bi;
  CString strPath;
  char* p = strPath.GetBuffer(MAX_PATH+1);
  bi.hwndOwner = GetSafeHwnd();
  bi.pidlRoot = NULL;
  bi.pszDisplayName = p;
  bi.lpszTitle = "Load textures from path";
  bi.ulFlags = 0;
  bi.lpfn = NULL;
  bi.lParam = NULL;
  bi.iImage = 0;
  LPITEMIDLIST pidlBrowse;
  pidlBrowse = SHBrowseForFolder(&bi);
  if (pidlBrowse)
  {
    SHGetPathFromIDList(pidlBrowse, p);
    strPath.ReleaseBuffer();
    AddSlash(strPath);
    Texture_ShowDirectory(strPath.GetBuffer(0));
  }
}

void CMainFrame::OnToggleRotatelock() 
{
  g_PrefsDlg.m_bRotateLock ^= 1;
  CMenu* pMenu = GetMenu();
  if (pMenu)
    pMenu->CheckMenuItem(ID_TOGGLE_ROTATELOCK, MF_BYCOMMAND | (g_PrefsDlg.m_bRotateLock) ? MF_CHECKED : MF_UNCHECKED);
  g_PrefsDlg.SavePrefs();

//  Sys_Printf(va("Texture Rotation Lock: %s\n",g_PrefsDlg.m_bRotateLock?"ON":"OFF"));
  SetGridStatus();
}


void CMainFrame::OnCurveBevel() 
{
  Curve_MakeCurvedBrush (false,	false, false,	false, false, true, true);
}

void CMainFrame::OnCurveCylinder() 
{
  Curve_MakeCurvedBrush (false,	false, false,	true, true, true, true);
}

void CMainFrame::OnCurveEighthsphere() 
{
  Curve_MakeCurvedBrush (false,	true, false, true, true, false, false);
}

void CMainFrame::OnCurveEndcap() 
{
  Curve_MakeCurvedBrush (false,	false, false,	false, true, true, true);
}

void CMainFrame::OnCurveHemisphere() 
{
  Curve_MakeCurvedBrush (false,	true, false, true, true, true, true);
}

void CMainFrame::OnCurveInvertcurve() 
{
  Curve_Invert ();
}

void CMainFrame::OnCurveQuarter() 
{
  Curve_MakeCurvedBrush (false,	true, false, true, true, true, false);
}

void CMainFrame::OnCurveSphere() 
{
	Curve_MakeCurvedBrush (false,	true, true,	true, true, true, true);
}

void CMainFrame::OnFileImportmap() 
{
  CFileDialog dlgFile(TRUE, NULL, NULL, OFN_HIDEREADONLY | OFN_OVERWRITEPROMPT, "Map files (*.map)|*.map||", this);
  if (dlgFile.DoModal() == IDOK)
  {
    Map_ImportFile(dlgFile.GetPathName().GetBuffer(0));
  }
}

void CMainFrame::OnFileExportmap() 
{
  CFileDialog dlgFile(FALSE, "map", NULL, OFN_HIDEREADONLY | OFN_OVERWRITEPROMPT, "Map files (*.map)|*.map||", this);
  if (dlgFile.DoModal() == IDOK)
  {
    Map_SaveSelected(dlgFile.GetPathName().GetBuffer(0));
  }
}

void CMainFrame::OnEditLoadprefab() 
{
  char CurPath[1024];
  ::GetCurrentDirectory(1024, CurPath);
  if (g_PrefsDlg.m_strPrefabPath.GetLength() > 0)
    ::SetCurrentDirectory(g_PrefsDlg.m_strPrefabPath);
  OnFileImportmap();
  ::SetCurrentDirectory(CurPath);
}

void CMainFrame::OnViewShowcurves() 
{
	if ( ( g_qeglobals.d_savedinfo.exclude ^= EXCLUDE_CURVES ) & EXCLUDE_CURVES )
    CheckMenuItem ( ::GetMenu(g_qeglobals.d_hwndMain), ID_VIEW_SHOWCURVES, MF_BYCOMMAND | MF_UNCHECKED );
	else
    CheckMenuItem ( ::GetMenu(g_qeglobals.d_hwndMain), ID_VIEW_SHOWCURVES, MF_BYCOMMAND | MF_CHECKED );
	Sys_UpdateWindows (W_XY|W_CAMERA);
}

void CMainFrame::OnViewShowWayPoints() 
{
	if ( ( g_qeglobals.d_savedinfo.exclude ^= EXCLUDE_WAYPOINTS ) & EXCLUDE_WAYPOINTS )
		CheckMenuItem ( ::GetMenu(g_qeglobals.d_hwndMain), ID_VIEW_SHOWWAYPOINTS, MF_BYCOMMAND | MF_UNCHECKED );
	else
		CheckMenuItem ( ::GetMenu(g_qeglobals.d_hwndMain), ID_VIEW_SHOWWAYPOINTS, MF_BYCOMMAND | MF_CHECKED );
	Sys_UpdateWindows (W_XY|W_CAMERA);	
}


void CMainFrame::OnViewShowWayPointsOnly()
{
	if ( ( g_qeglobals.d_savedinfo.exclude ^= SHOW_WAYPOINTS_ONLY ) & SHOW_WAYPOINTS_ONLY )
		CheckMenuItem ( ::GetMenu(g_qeglobals.d_hwndMain), ID_VIEW_SHOWWAYPOINTS_ONLY, MF_BYCOMMAND | MF_CHECKED );
	else
		CheckMenuItem ( ::GetMenu(g_qeglobals.d_hwndMain), ID_VIEW_SHOWWAYPOINTS_ONLY, MF_BYCOMMAND | MF_UNCHECKED );
	Sys_UpdateWindows (W_XY|W_CAMERA);	
}

void CMainFrame::OnViewShowMiscModels()
{
	if ( ( g_qeglobals.d_savedinfo.exclude ^= EXCLUDE_MISCMODELS ) & EXCLUDE_MISCMODELS )
		CheckMenuItem ( ::GetMenu(g_qeglobals.d_hwndMain), ID_VIEW_SHOWMISC_MODEL, MF_BYCOMMAND | MF_UNCHECKED );
	else
		CheckMenuItem ( ::GetMenu(g_qeglobals.d_hwndMain), ID_VIEW_SHOWMISC_MODEL, MF_BYCOMMAND | MF_CHECKED );
	Sys_UpdateWindows (W_XY|W_CAMERA);	
}

void CMainFrame::OnViewShowMiscModelBreakables()
{
	if ( ( g_qeglobals.d_savedinfo.exclude ^= EXCLUDE_MISCMODELBREAKABLES ) & EXCLUDE_MISCMODELBREAKABLES )
		CheckMenuItem ( ::GetMenu(g_qeglobals.d_hwndMain), ID_VIEW_SHOWMISC_MODEL_BREAKABLE, MF_BYCOMMAND | MF_UNCHECKED );
	else
		CheckMenuItem ( ::GetMenu(g_qeglobals.d_hwndMain), ID_VIEW_SHOWMISC_MODEL_BREAKABLE, MF_BYCOMMAND | MF_CHECKED );
	Sys_UpdateWindows (W_XY|W_CAMERA);	
}

void CMainFrame::OnViewShowMiscModelXXXX()
{
	if ( ( g_qeglobals.d_savedinfo.exclude ^= EXCLUDE_MISCMODELXXXX ) & EXCLUDE_MISCMODELXXXX )
		CheckMenuItem ( ::GetMenu(g_qeglobals.d_hwndMain), ID_VIEW_SHOWMISC_MODEL_XXXX, MF_BYCOMMAND | MF_UNCHECKED );
	else
		CheckMenuItem ( ::GetMenu(g_qeglobals.d_hwndMain), ID_VIEW_SHOWMISC_MODEL_XXXX, MF_BYCOMMAND | MF_CHECKED );
	Sys_UpdateWindows (W_XY|W_CAMERA);	
}

void CMainFrame::OnViewShowTriggerXXXX()
{
	if ( ( g_qeglobals.d_savedinfo.exclude ^= EXCLUDE_TRIGGERXXXX ) & EXCLUDE_TRIGGERXXXX )
		CheckMenuItem ( ::GetMenu(g_qeglobals.d_hwndMain), ID_VIEW_SHOWTRIGGER_XXXX, MF_BYCOMMAND | MF_UNCHECKED );
	else
		CheckMenuItem ( ::GetMenu(g_qeglobals.d_hwndMain), ID_VIEW_SHOWTRIGGER_XXXX, MF_BYCOMMAND | MF_CHECKED );
	Sys_UpdateWindows (W_XY|W_CAMERA);	
}

void CMainFrame::OnViewShowTargetSpeakers()
{
	if ( ( g_qeglobals.d_savedinfo.exclude ^= EXCLUDE_TARGETSPEAKERS ) & EXCLUDE_TARGETSPEAKERS )
		CheckMenuItem ( ::GetMenu(g_qeglobals.d_hwndMain), ID_VIEW_SHOWTARGET_SPEAKER, MF_BYCOMMAND | MF_UNCHECKED );
	else
		CheckMenuItem ( ::GetMenu(g_qeglobals.d_hwndMain), ID_VIEW_SHOWTARGET_SPEAKER, MF_BYCOMMAND | MF_CHECKED );
	Sys_UpdateWindows (W_XY|W_CAMERA);	
}

void CMainFrame::OnViewShowRefTags()
{
	if ( ( g_qeglobals.d_savedinfo.exclude ^= EXCLUDE_REFTAGS ) & EXCLUDE_REFTAGS )
		CheckMenuItem ( ::GetMenu(g_qeglobals.d_hwndMain), ID_VIEW_SHOWREF_TAGS, MF_BYCOMMAND | MF_UNCHECKED );
	else
		CheckMenuItem ( ::GetMenu(g_qeglobals.d_hwndMain), ID_VIEW_SHOWREF_TAGS, MF_BYCOMMAND | MF_CHECKED );
	Sys_UpdateWindows (W_XY|W_CAMERA);	
}

void CMainFrame::OnViewShowNonSolid()
{	
	if ( ( g_qeglobals.d_savedinfo.exclude ^= EXCLUDE_NONSOLID ) & EXCLUDE_NONSOLID )
		CheckMenuItem ( ::GetMenu(g_qeglobals.d_hwndMain), ID_VIEW_SHOW_NONSOLID, MF_BYCOMMAND | MF_UNCHECKED );
	else
		CheckMenuItem ( ::GetMenu(g_qeglobals.d_hwndMain), ID_VIEW_SHOW_NONSOLID, MF_BYCOMMAND | MF_CHECKED );

	Sys_UpdateWindows (W_XY|W_CAMERA);
}

void CMainFrame::OnViewShowEasy()
{
	if ( ( g_qeglobals.d_savedinfo.exclude ^= EXCLUDE_EASY ) & EXCLUDE_EASY )
		CheckMenuItem ( ::GetMenu(g_qeglobals.d_hwndMain), ID_SHOW_EASY, MF_BYCOMMAND | MF_UNCHECKED );
	else
		CheckMenuItem ( ::GetMenu(g_qeglobals.d_hwndMain), ID_SHOW_EASY, MF_BYCOMMAND | MF_CHECKED );

	Sys_UpdateWindows (W_XY|W_CAMERA);
}

void CMainFrame::OnViewShowMedium()
{
	if ( ( g_qeglobals.d_savedinfo.exclude ^= EXCLUDE_MEDIUM ) & EXCLUDE_MEDIUM )
		CheckMenuItem ( ::GetMenu(g_qeglobals.d_hwndMain), ID_SHOW_MEDIUM, MF_BYCOMMAND | MF_UNCHECKED );
	else
		CheckMenuItem ( ::GetMenu(g_qeglobals.d_hwndMain), ID_SHOW_MEDIUM, MF_BYCOMMAND | MF_CHECKED );

	Sys_UpdateWindows (W_XY|W_CAMERA);
}

void CMainFrame::OnViewShowHard()
{
	if ( ( g_qeglobals.d_savedinfo.exclude ^= EXCLUDE_HARD ) & EXCLUDE_HARD )
		CheckMenuItem ( ::GetMenu(g_qeglobals.d_hwndMain), ID_SHOW_HARD, MF_BYCOMMAND | MF_UNCHECKED );
	else
		CheckMenuItem ( ::GetMenu(g_qeglobals.d_hwndMain), ID_SHOW_HARD, MF_BYCOMMAND | MF_CHECKED );

	Sys_UpdateWindows (W_XY|W_CAMERA);
}

void CMainFrame::OnSelectionSelectNudgedown() 
{
  NudgeSelection(3, g_qeglobals.d_savedinfo.m_nTextureTweak);
}

void CMainFrame::OnSelectionSelectNudgeleft() 
{
  NudgeSelection(0, g_qeglobals.d_savedinfo.m_nTextureTweak);
}

void CMainFrame::OnSelectionSelectNudgeright() 
{
  NudgeSelection(2, g_qeglobals.d_savedinfo.m_nTextureTweak);
}

void CMainFrame::OnSelectionSelectNudgeup() 
{
  NudgeSelection(1, g_qeglobals.d_savedinfo.m_nTextureTweak);
}

void CMainFrame::NudgeSelection(int nDirection, int nAmount)
{
  if (ActiveXY()->RotateMode())
  {
    int nAxis = 0;
    if (ActiveXY()->GetViewType() == XY)
    {
      nAxis = 2;
    }
    else 
    if (g_pParentWnd->ActiveXY()->GetViewType() == XZ)
    {
      nAxis = 1;
      nAmount = -nAmount;
    }

    if (nDirection == 2 || nDirection == 3)
    {
      nAmount = -nAmount;
    }

    float fDeg = -nAmount;
    float fAdj = nAmount;

    g_pParentWnd->ActiveXY()->Rotation()[nAxis] += fAdj;
    CString strStatus;
    strStatus.Format("Rotation x:: %.1f  y:: %.1f  z:: %.1f", g_pParentWnd->ActiveXY()->Rotation()[0], g_pParentWnd->ActiveXY()->Rotation()[1], g_pParentWnd->ActiveXY()->Rotation()[2]);
    g_pParentWnd->SetStatusText(2, strStatus);
    Select_RotateAxis(nAxis, fDeg, false, true);
    Sys_UpdateWindows (W_ALL);
  }
  else
  if (ActiveXY()->ScaleMode())
  {
    if (nDirection == 0 || nDirection == 3)
    {
      nAmount = -nAmount;
    }
    vec3_t v;
    v[0] = v[1] = v[2] = 1.0;
    if (nAmount > 0)
    {
      v[0] = 1.1;
      v[1] = 1.1;
      v[2] = 1.1;
    }
    else 
    {
      v[0] = 0.9;
      v[1] = 0.9;
      v[2] = 0.9;
    }

    Select_Scale((g_nScaleHow & SCALE_X) ? v[0] : 1.0,
                 (g_nScaleHow & SCALE_Y) ? v[1] : 1.0,
                 (g_nScaleHow & SCALE_Z) ? v[2] : 1.0);
	  Sys_UpdateWindows (W_ALL);
  }
  else
  {
    // 0 - left, 1 - up, 2 - right, 3 - down
    int nDim;
    if (nDirection == 0)
    {
      nDim = ActiveXY()->GetViewType() == YZ ? 1 : 0;
      nAmount = -nAmount;
    }
    else if (nDirection == 1)
    {
      nDim = ActiveXY()->GetViewType() == XY ? 1 : 2;
    }
    else if (nDirection == 2)
    {
      nDim = ActiveXY()->GetViewType() == YZ ? 1 : 0;
    }
    else
    {
      nDim = ActiveXY()->GetViewType() == XY ? 1 : 2;
      nAmount = -nAmount;
    }
    Nudge(nDim, nAmount);
  }
}


BOOL CMainFrame::PreTranslateMessage(MSG* pMsg) 
{
	return CFrameWnd::PreTranslateMessage(pMsg);
}

void CMainFrame::Nudge(int nDim, float fNudge)
{
  vec3_t vMove;
  vMove[0] = vMove[1] = vMove[2] = 0;
  vMove[nDim] = fNudge;
  Select_Move(vMove, true);
  Sys_UpdateWindows (W_ALL);
}

void CMainFrame::OnTexturesLoadlist() 
{
  CDialogTextures dlg;
  if (dlg.DoModal() == IDOK && dlg.m_nSelection >= 0)
  {
    Texture_ShowDirectory(dlg.m_nSelection + CMD_TEXTUREWAD);
  }
}

void CMainFrame::OnDontselectcurve() 
{
  g_PrefsDlg.m_bSelectCurves ^= 1;
  m_wndToolBar.GetToolBarCtrl().CheckButton(ID_DONTSELECTCURVE, (g_PrefsDlg.m_bSelectCurves) ? FALSE : TRUE);
}

void CMainFrame::OnShowFaces() 
{	
	qbShowFaces = !qbShowFaces;
	Sys_UpdateWindows(W_CAMERA);
}



CString strFindKey;
CString strFindValue;
CString strReplaceKey;
CString strReplaceValue;
bool    gbWholeStringMatchOnly = true;
bool	gbSelectAllMatchingEnts= false;
brush_t* gpPrevEntBrushFound = NULL;

// all this because there's no ansi stristr(), sigh...
//
LPCSTR String_ToLower(LPCSTR psString)
{
	const int iBufferSize = 4096;
	static char sString[8][iBufferSize];
	static int iIndex=0;

	if (strlen(psString)>=iBufferSize)
	{
		assert(0);
		Sys_Printf("String_ToLower(): Warning, input string was %d bytes too large, performing strlwr() inline!\n",strlen(psString)-(iBufferSize-1));
		return strlwr(const_cast<char*>(psString));
	}

	iIndex=++iIndex&7;

	strcpy(sString[iIndex],psString);
	strlwr(sString[iIndex]);

	return sString[iIndex];
}

bool FindNextBrush(brush_t* pPrevFoundBrush)	// can be NULL for fresh search
{	
	bool bFoundSomething = false;
	entity_t *pLastFoundEnt;
	brush_t  *pLastFoundBrush;

	CWaitCursor waitcursor;

	Select_Deselect(true);	// bool bDeSelectToListBack

	// see whether to start search from prev_brush->next by checking if prev_brush is still in the active list...
	//
	brush_t *pStartBrush = active_brushes.next;
/*
	if (pPrevFoundBrush && !gbSelectAllMatchingEnts)
	{
		brush_t *pPrev = NULL;
		for (brush_t* b = active_brushes.next ; b != &active_brushes ; b = b->next)
		{
			if (pPrev == pPrevFoundBrush && pPrevFoundBrush)
			{
				pStartBrush = b;
				break;
			}
			pPrev = b;
		}
	}
*/
	// now do the search proper...
	//	
	int iBrushesScanned = 0;
	int iBrushesSelected=0;
	int iEntsScanned = 0;

	brush_t* pNextBrush;		
	for (brush_t* b = pStartBrush; b != &active_brushes ; b = pNextBrush)
	{	
		// setup the <nextbrush> ptr before going any further (because selecting a brush down below moves it to a 
		//	different link list), but we need to ensure that the next brush has a different ent-owner than the current
		//	one, or multi-brush ents will confuse the list process if they get selected (infinite loop badness)...
		//
		// pNextBrush = &active_brushes;	// default to loop-stop condition
		pNextBrush = b->next;
		while (pNextBrush->owner == b->owner && pNextBrush!=&active_brushes)
		{
			pNextBrush = pNextBrush->next;
		}

		iBrushesScanned++;

		// a simple progress bar so they don't think it's locked up on long searches...
		//
		static int iDotBodge=0;
		if (!(++iDotBodge&15))
			Sys_Printf(".");	// cut down on printing		

		bool bMatch = false;
		entity_t* ent = b->owner;

		if (ent && ent!= world_entity)	// needed!
		{
			iEntsScanned++;
	 		if (FilterBrush (b))
	 			continue;			

			// only check the find-key if there was one specified...
			//
			if (!strFindKey.IsEmpty())
			{
				char *psEntFoundValue = ValueForKey(ent, strFindKey);

				if (strlen(psEntFoundValue)
						&&
						(
//							(stricmp(strFindValue, psEntFoundValue)==0)	// found this exact key/value
							(
							(gbWholeStringMatchOnly && stricmp(psEntFoundValue, strFindValue)==0)
							||
							(!gbWholeStringMatchOnly && strstr(String_ToLower(psEntFoundValue), String_ToLower(strFindValue)))
							)
							||											//  or
							(strFindValue.IsEmpty())					// any value for this key if blank value search specified
						)
					)
				{
					bMatch = true;
				}
			}
			else
			{
				// no FIND key specified, so just scan all of them...
				//
				int iNumEntKeys = GetNumKeys(ent);
				for (int i=0; i<iNumEntKeys; i++)
				{
					char *psEntFoundValue = ValueForKey(ent, GetKeyString(ent, i));
					if (psEntFoundValue)
					{
						if (	(strlen(psEntFoundValue) &&	strFindValue.IsEmpty())	// if blank <value> search specified then any found-value is ok
								||
								(gbWholeStringMatchOnly && stricmp(psEntFoundValue, strFindValue)==0)
								||
								(!gbWholeStringMatchOnly && strstr(String_ToLower(psEntFoundValue), String_ToLower(strFindValue)))
							)
						{
							if (!gbWholeStringMatchOnly && strstr(String_ToLower(psEntFoundValue), String_ToLower(strFindValue)))
							{
//								OutputDebugString(va("Matching because: psEntFoundValue '%s' & strFindValue '%s'\n",psEntFoundValue, strFindValue));
//								Sys_Printf("Matching because: psEntFoundValue '%s' & strFindValue '%s'\n",psEntFoundValue, strFindValue);

//								if (strstr(psEntFoundValue,"killsplat"))
//								{
//									DebugBreak();
//								}
							}
							bMatch = true;
							break;
						}
					}
				}
			}

			if (bMatch)
			{
				bFoundSomething = true;
				pLastFoundEnt	= ent;
				pLastFoundBrush	= b;
				iBrushesSelected++;

				g_bScreenUpdates = false;	// !!!!!!!!!!!!!!!!!!!!!!!!!!!!
				
					Select_Brush(b);

				g_bScreenUpdates = true;	// !!!!!!!!!!!!!!!!!!!!!!!!!!!!

				if (!gbSelectAllMatchingEnts)
					break;
			}
		}
	}
	if (gbSelectAllMatchingEnts)
	{
		Sys_Printf("\nBrushes Selected: %d           (Brushes Scanned %d, Ents Scanned %d)\n", iBrushesSelected, iBrushesScanned, iEntsScanned);
	}

	if (bFoundSomething)
	{
		vec3_t v3Origin;

		if (pLastFoundEnt->origin[0] != 0.0f || pLastFoundEnt->origin[1] != 0.0f || pLastFoundEnt->origin[2] != 0.0f)
		{
			VectorCopy(pLastFoundEnt->origin,v3Origin);
		}
		else
		{
			// pLastFoundEnt's origin is zero, so use average point of brush mins maxs instead...
			//
			v3Origin[0] = (pLastFoundBrush->mins[0] + pLastFoundBrush->maxs[0])/2;
			v3Origin[1] = (pLastFoundBrush->mins[1] + pLastFoundBrush->maxs[1])/2;
			v3Origin[2] = (pLastFoundBrush->mins[2] + pLastFoundBrush->maxs[2])/2;
		}

		// got one, jump the camera to it...
		//
		VectorCopy(v3Origin, g_pParentWnd->GetCamera()->Camera().origin);
							 g_pParentWnd->GetCamera()->Camera().origin[1] -= 32;	// back off a touch to look at it
		VectorSet (g_pParentWnd->GetCamera()->Camera().angles,0,90,0);

		// force main screen into XY camera mode (just in case)...
		//
		g_pParentWnd->SetActiveXY(g_pParentWnd->GetXYWnd());
		g_pParentWnd->GetXYWnd()->PositionView();

		Sys_UpdateWindows (W_ALL);
		//
		// and record for next find request (F3)...
		//
		gpPrevEntBrushFound = pLastFoundBrush;
	}

	return bFoundSomething;
}

void CMainFrame::OnFindEnt()
{	
	CEntKeyFindReplace FindReplace(&strFindKey, &strFindValue, &strReplaceKey, &strReplaceValue, &gbWholeStringMatchOnly, &gbSelectAllMatchingEnts);

	switch (FindReplace.DoModal())
	{
		case ID_RET_REPLACE:
		{	
			brush_t* next = NULL;
			int iOccurences = 0;
			for (brush_t* b = active_brushes.next ; b != &active_brushes ; b = next)
			{
				next = b->next;	// important to do this here, in case brush gets linked to a different list
				entity_t* ent = b->owner;

				if (ent)	// needed!
				{
	 				if (FilterBrush (b))
	 					continue;

					char *psEntFoundValue = ValueForKey(ent, strFindKey);

					if (stricmp(strFindValue, psEntFoundValue)==0 ||		// found this exact key/value
						(strlen(psEntFoundValue) &&	strFindValue.IsEmpty()) // or any value for this key if blank value search specified
						)
					{
						// found this search key/value, so delete it...
						//
						DeleteKey(ent,strFindKey);
						//
						// and replace with the new key/value (if specified)...
						//
						if (!strReplaceKey.IsEmpty() && !strReplaceValue.IsEmpty())
						{
							SetKeyValue (ent, strReplaceKey, strReplaceValue);
						}
						iOccurences++;
					}
				}
			}
			if (iOccurences)
			{
				Sys_Printf("%d occurence(s) replaced\n",iOccurences);
			}
			else
			{
				Sys_Printf("Nothing found to replace\n");
			}
		}
		break;
		case ID_RET_FIND:
		{
			gpPrevEntBrushFound = NULL;
			FindNextBrush(NULL);
		}
		break;
	}
}
void CMainFrame::OnFindNextEnt()
{
	// try it once, if it fails, try it again from top, and give up if still failed after that...
	//
	if (!FindNextBrush(gpPrevEntBrushFound))
	{
		gpPrevEntBrushFound = NULL;
		FindNextBrush(NULL);
	}
}

void CMainFrame::OnFaceFit()
{
	FaceFit();
}

void CMainFrame::OnViewHideshowHideselected() 
{
  Select_Hide();  
  Select_Deselect();
}

void CMainFrame::OnViewHideshowShowhidden() 
{
  Select_ShowAllHidden();
}


void CMainFrame::OnConvertcurves() 
{
#if 0
  Select_Deselect();
	for (brush_t* pb = active_brushes.next ; pb != &active_brushes ; pb = pb->next)
	{
    if (pb->curveBrush)
    {
	    for (face_t* f = pb->brush_faces ; f ; f=f->next) 
      {
		    if (f->texdef.contents & CONTENTS_LADDER)
        {
          f->texdef.contents &= ~CONTENTS_LADDER;
          f->texdef.contents |= CONTENTS_NEGATIVE_CURVE;
        }
      }
		}
  }
  Map_BuildBrushData();
#endif

}

void CMainFrame::OnDynamicLighting() 
{
  CCamWnd* pCam = new CCamWnd();
  CRect rect(100, 100, 300, 300);
  pCam->Create(CAMERA_WINDOW_CLASS, "", WS_OVERLAPPEDWINDOW, rect, GetDesktopWindow(), 12345);
  pCam->ShowWindow(SW_SHOW);
}


void CMainFrame::OnCurveSimplepatchmesh() 
{
  CPatchDensityDlg dlg;
  dlg.DoModal();
}


void CMainFrame::OnPatchToggleBox()
{
  g_bPatchShowBounds ^= 1;
  m_wndToolBar.GetToolBarCtrl().CheckButton(ID_PATCH_SHOWBOUNDINGBOX, (g_bPatchShowBounds) ? TRUE : FALSE);
  Sys_UpdateWindows(W_ALL);
}

void CMainFrame::OnPatchWireframe() 
{
	g_bPatchWireFrame ^= 1;
  m_wndToolBar.GetToolBarCtrl().CheckButton(ID_PATCH_WIREFRAME, (g_bPatchWireFrame) ? TRUE : FALSE);
  Sys_UpdateWindows(W_ALL);
}

void CMainFrame::OnCurvePatchcone() 
{
  Patch_BrushToMesh(true);
  Sys_UpdateWindows (W_ALL);
}

void CMainFrame::OnCurvePatchtube() 
{
  Patch_BrushToMesh(false);
  Sys_UpdateWindows (W_ALL);
}

void CMainFrame::OnPatchWeld() 
{
	g_bPatchWeld ^= 1;
  m_wndToolBar.GetToolBarCtrl().CheckButton(ID_PATCH_WELD, (g_bPatchWeld) ? TRUE : FALSE);
  Sys_UpdateWindows(W_ALL);
}

void CMainFrame::OnCurvePatchbevel() 
{
  Patch_BrushToMesh(false, true, false);
  Sys_UpdateWindows (W_ALL);
}

void CMainFrame::OnCurvePatchendcap() 
{
  Patch_BrushToMesh(false, false, true);
  Sys_UpdateWindows (W_ALL);
}

void CMainFrame::OnCurvePatchinvertedbevel() 
{
  //Patch_BrushToMesh(false, true, false, true);
  //Sys_UpdateWindows (W_ALL);
}

void CMainFrame::OnCurvePatchinvertedendcap() 
{
  //Patch_BrushToMesh(false, false, true, true);
  //Sys_UpdateWindows (W_ALL);
}

void CMainFrame::OnPatchDrilldown() 
{
	g_bPatchDrillDown ^= 1;
  m_wndToolBar.GetToolBarCtrl().CheckButton(ID_PATCH_DRILLDOWN, (g_bPatchDrillDown) ? TRUE : FALSE);
  Sys_UpdateWindows(W_ALL);
}


void CMainFrame::OnCurveInsertcolumn() 
{
	//Patch_AdjustSelectedRowCols(0, 2);
  Patch_AdjustSelected(true, true, true);
  Sys_UpdateWindows(W_ALL);
}

void CMainFrame::OnCurveInsertrow() 
{
	//Patch_AdjustSelectedRowCols(2, 0);
  Patch_AdjustSelected(true, false, true);
  Sys_UpdateWindows(W_ALL);
}

void CMainFrame::OnMapHome()
{
  if (m_pXYWnd)
  {
	  m_pXYWnd->XY_Init();
	  Sys_UpdateWindows(W_ALL);
  }  
}

void CMainFrame::OnCurveDeletecolumn() 
{
  Patch_AdjustSelected(false, true, true);
  Sys_UpdateWindows(W_ALL);
}

void CMainFrame::OnCurveDeleterow() 
{
  Patch_AdjustSelected(false, false, true);
  Sys_UpdateWindows(W_ALL);
}

void CMainFrame::OnCurveInsertAddcolumn() 
{
  Patch_AdjustSelected(true, true, true);
  Sys_UpdateWindows(W_ALL);
}

void CMainFrame::OnCurveInsertAddrow() 
{
  Patch_AdjustSelected(true, false, true);
  Sys_UpdateWindows(W_ALL);
}

void CMainFrame::OnCurveInsertInsertcolumn() 
{
  Patch_AdjustSelected(true, true, false);
  Sys_UpdateWindows(W_ALL);
}

void CMainFrame::OnCurveInsertInsertrow() 
{
  Patch_AdjustSelected(true, false, false);
  Sys_UpdateWindows(W_ALL);
}

void CMainFrame::OnCurveNegative() 
{
  Patch_ToggleInverted();
  //Sys_UpdateWindows(W_ALL);
}

void CMainFrame::OnCurveNegativeTextureX() 
{
  Patch_InvertTexture(false);
  //Sys_UpdateWindows(W_ALL);
}

void CMainFrame::OnCurveNegativeTextureY() 
{
  Patch_InvertTexture(true);
  //Sys_UpdateWindows(W_ALL);
}


void CMainFrame::OnCurveDeleteFirstcolumn() 
{
  Patch_AdjustSelected(false, true, true);
  Sys_UpdateWindows(W_ALL);
}

void CMainFrame::OnCurveDeleteFirstrow() 
{
  Patch_AdjustSelected(false, false, true);
  Sys_UpdateWindows(W_ALL);
}

void CMainFrame::OnCurveDeleteLastcolumn() 
{
  Patch_AdjustSelected(false, true, false);
  Sys_UpdateWindows(W_ALL);
}

void CMainFrame::OnCurveDeleteLastrow() 
{
  Patch_AdjustSelected(false, false, false);
  Sys_UpdateWindows(W_ALL);
}

void CMainFrame::OnPatchBend() 
{
  Patch_BendToggle();
  m_wndToolBar.GetToolBarCtrl().CheckButton(ID_PATCH_BEND, (g_bPatchBendMode) ? TRUE : FALSE);
  Sys_UpdateWindows(W_ALL);
}

void CMainFrame::OnPatchInsdel() 
{
  Patch_InsDelToggle();
  m_wndToolBar.GetToolBarCtrl().CheckButton(ID_PATCH_INSDEL, (g_bPatchInsertMode) ? TRUE : FALSE);
  Sys_UpdateWindows(W_ALL);
}

void CMainFrame::OnPatchEnter() 
{
	
}

void CMainFrame::OnPatchTab() 
{
  if (g_bPatchBendMode)
    Patch_BendHandleTAB();
  else if (g_bPatchInsertMode)
    Patch_InsDelHandleTAB();
  else
  {
    // check to see if the selected brush is part of a func group
    // if it is, deselect everything and reselect the next brush 
    // in the group
	  brush_t *b = selected_brushes.next;
    entity_t * e;
    if (b != &selected_brushes)
    {
	    if (strcmpi(b->owner->eclass->name, "worldspawn") != 0)
      {
        e = b->owner;
        Select_Deselect();
		    for (brush_t * b2 = e->brushes.onext ; b2 != &e->brushes ; b2 = b2->onext)
		    {
          if (b == b2)
          {
            b2 = b2->onext;
            break;
          }
        }
        if (b2 == &e->brushes)
          b2 = b2->onext;

        Select_Brush(b2, false);
        Sys_UpdateWindows(W_ALL);
      }
    }
  }
}

void CMainFrame::UpdatePatchToolbarButtons() 
{
  m_wndToolBar.GetToolBarCtrl().CheckButton(ID_PATCH_BEND, (g_bPatchBendMode) ? TRUE : FALSE);
  m_wndToolBar.GetToolBarCtrl().CheckButton(ID_PATCH_INSDEL, (g_bPatchInsertMode) ? TRUE : FALSE);
}

void CMainFrame::OnCurvePatchdensetube() 
{
  Patch_BrushToMesh(false);
  OnCurveInsertAddrow();
  OnCurveInsertInsertrow();
  Sys_UpdateWindows (W_ALL);
}

void CMainFrame::OnCurvePatchverydensetube() 
{
  Patch_BrushToMesh(false);
  OnCurveInsertAddrow();
  OnCurveInsertInsertrow();
  OnCurveInsertAddrow();
  OnCurveInsertInsertrow();
  Sys_UpdateWindows (W_ALL);
}

void CMainFrame::OnCurveCap() 
{
  Patch_CapCurrent();
  Sys_UpdateWindows (W_ALL);
}


void CMainFrame::OnCurveCapInvertedbevel() 
{
  Patch_CapCurrent(true);
  Sys_UpdateWindows (W_ALL);
}

void CMainFrame::OnCurveCapInvertedendcap() 
{
  Patch_CapCurrent(false, true);
  Sys_UpdateWindows (W_ALL);
}

void CMainFrame::OnCurveRedisperseCols() 
{
  Patch_DisperseColumns();
  Sys_UpdateWindows (W_ALL);
}

void CMainFrame::OnCurveRedisperseRows() 
{
  Patch_DisperseRows();
  Sys_UpdateWindows (W_ALL);
}

void CMainFrame::OnPatchNaturalize()
{
  Patch_NaturalizeSelected();
  Sys_UpdateWindows (W_ALL);
}

void CMainFrame::OnSnapToGrid()
{
  Select_SnapToGrid();
  Sys_UpdateWindows (W_ALL);
}

void CMainFrame::OnCurvePatchsquare() 
{
  Patch_BrushToMesh(false, false, false, true);
  Sys_UpdateWindows (W_ALL);
}

void CMainFrame::CheckTextureScale(int id)
{
  CMenu* pMenu = GetMenu();
  if (pMenu)
  {
    pMenu->CheckMenuItem(ID_TEXTURES_TEXTUREWINDOWSCALE_10, MF_BYCOMMAND | MF_UNCHECKED);
    pMenu->CheckMenuItem(ID_TEXTURES_TEXTUREWINDOWSCALE_25, MF_BYCOMMAND | MF_UNCHECKED);
    pMenu->CheckMenuItem(ID_TEXTURES_TEXTUREWINDOWSCALE_50, MF_BYCOMMAND | MF_UNCHECKED);
    pMenu->CheckMenuItem(ID_TEXTURES_TEXTUREWINDOWSCALE_100, MF_BYCOMMAND | MF_UNCHECKED);
    pMenu->CheckMenuItem(ID_TEXTURES_TEXTUREWINDOWSCALE_200, MF_BYCOMMAND | MF_UNCHECKED);
    pMenu->CheckMenuItem(id, MF_BYCOMMAND | MF_CHECKED);
  }
  g_PrefsDlg.SavePrefs();
	Texture_ResetPosition();
  Sys_UpdateWindows(W_TEXTURE);
}

void CMainFrame::OnTexturesTexturewindowscale10() 
{
  g_PrefsDlg.m_nTextureScale = 10;
  CheckTextureScale(ID_TEXTURES_TEXTUREWINDOWSCALE_10);
}

void CMainFrame::OnTexturesTexturewindowscale100() 
{
  g_PrefsDlg.m_nTextureScale = 100;
  CheckTextureScale(ID_TEXTURES_TEXTUREWINDOWSCALE_100);
}

void CMainFrame::OnTexturesTexturewindowscale200() 
{
  g_PrefsDlg.m_nTextureScale = 200;
  CheckTextureScale(ID_TEXTURES_TEXTUREWINDOWSCALE_200);
}

void CMainFrame::OnTexturesTexturewindowscale25() 
{
  g_PrefsDlg.m_nTextureScale = 25;
  CheckTextureScale(ID_TEXTURES_TEXTUREWINDOWSCALE_25);
}

void CMainFrame::OnTexturesTexturewindowscale50() 
{
  g_PrefsDlg.m_nTextureScale = 50;
  CheckTextureScale(ID_TEXTURES_TEXTUREWINDOWSCALE_50);
}



void CMainFrame::OnTexturesFlush() 
{
	Texture_Flush();
  Sys_UpdateWindows(W_ALL);
}

void CMainFrame::OnCurveOverlayClear() 
{
  Patch_ClearOverlays();
  Sys_UpdateWindows(W_ALL);
}

void CMainFrame::OnCurveOverlaySet() 
{
  Patch_SetOverlays();
  Sys_UpdateWindows(W_ALL);
}

void CMainFrame::OnCurveThicken() 
{
  CDialogThick dlg;
  if (dlg.DoModal() == IDOK)
  {
    Patch_Thicken(dlg.m_nAmount, dlg.m_bSeams);
    Sys_UpdateWindows(W_ALL);
  }
}

void CMainFrame::OnCurveCyclecap() 
{
  Patch_NaturalizeSelected(true, true);
  Sys_UpdateWindows (W_ALL);
}

void CMainFrame::OnCurveMatrixTranspose() 
{
  Patch_Transpose();
  Sys_UpdateWindows (W_ALL);
}

void CMainFrame::OnTexturesReloadshaders() 
{
  CWaitCursor wait;
  ReloadShaders();
  Sys_UpdateWindows (W_ALL);
}

void CMainFrame::SetEntityCheck()
{
  CMenu* pMenu = GetMenu();
  if (pMenu)
  {
    pMenu->CheckMenuItem(ID_VIEW_ENTITIESAS_BOUNDINGBOX, MF_BYCOMMAND | (g_PrefsDlg.m_nEntityShowState == ENTITY_BOX) ? MF_CHECKED : MF_UNCHECKED);
    pMenu->CheckMenuItem(ID_VIEW_ENTITIESAS_WIREFRAME, MF_BYCOMMAND | (g_PrefsDlg.m_nEntityShowState == ENTITY_WIRE) ? MF_CHECKED : MF_UNCHECKED);
    pMenu->CheckMenuItem(ID_VIEW_ENTITIESAS_SELECTEDWIREFRAME, MF_BYCOMMAND | (g_PrefsDlg.m_nEntityShowState == ENTITY_SELECTED) ? MF_CHECKED : MF_UNCHECKED);
    pMenu->CheckMenuItem(ID_VIEW_ENTITIESAS_SELECTEDSKINNED, MF_BYCOMMAND | (g_PrefsDlg.m_nEntityShowState == ENTITY_SELECTED_SKIN) ? MF_CHECKED : MF_UNCHECKED);
    pMenu->CheckMenuItem(ID_VIEW_ENTITIESAS_SKINNED, MF_BYCOMMAND | (g_PrefsDlg.m_nEntityShowState == ENTITY_SKINNED) ? MF_CHECKED : MF_UNCHECKED);
    pMenu->CheckMenuItem(ID_VIEW_ENTITIESAS_SKINNEDANDBOXED, MF_BYCOMMAND | (g_PrefsDlg.m_nEntityShowState == ENTITY_SKINNED_BOXED) ? MF_CHECKED : MF_UNCHECKED); 
  }
}


void CMainFrame::OnShowEntities() 
{
  HandlePopup(this, IDR_POPUP_ENTITY); 
}

void CMainFrame::OnViewEntitiesasBoundingbox() 
{
  g_PrefsDlg.m_nEntityShowState = ENTITY_BOX;
  SetEntityCheck();
  g_PrefsDlg.SavePrefs();
  Sys_UpdateWindows (W_ALL);
}

void CMainFrame::OnViewEntitiesasSelectedskinned() 
{
  g_PrefsDlg.m_nEntityShowState = ENTITY_SELECTED_SKIN;
  SetEntityCheck();
  g_PrefsDlg.SavePrefs();
  Sys_UpdateWindows (W_ALL);
}

void CMainFrame::OnViewEntitiesasSelectedwireframe() 
{
  g_PrefsDlg.m_nEntityShowState = ENTITY_SELECTED;
  SetEntityCheck();
  g_PrefsDlg.SavePrefs();
  Sys_UpdateWindows (W_ALL);
}

void CMainFrame::OnViewEntitiesasSkinned() 
{
  g_PrefsDlg.m_nEntityShowState = ENTITY_SKINNED;
  SetEntityCheck();
  g_PrefsDlg.SavePrefs();
  Sys_UpdateWindows (W_ALL);
}

void CMainFrame::OnViewEntitiesasSkinnedandboxed() 
{
  g_PrefsDlg.m_nEntityShowState = ENTITY_SKINNED_BOXED;
  SetEntityCheck();
  g_PrefsDlg.SavePrefs();
  Sys_UpdateWindows (W_ALL);
}

void CMainFrame::OnViewEntitiesasWriteframe() 
{
  g_PrefsDlg.m_nEntityShowState = ENTITY_WIRE;
  SetEntityCheck();
  g_PrefsDlg.SavePrefs();
  Sys_UpdateWindows (W_ALL);
}


void CMainFrame::OnSSStatus()
{	
	CString		string;	

	if ( SS_SetupOk() )
	{
		bool bEnabled = SS_FunctionsAvailable();
		string += va("SourceSafe appears to be setup ok %s",bEnabled?"and is enabled":"but is disabled");

		if (bEnabled)
		{
			LPSTR filename = currentmap;

			bool bUnderSourceControl = SS_IsUnderSourceControl( filename );

			string += va("\n\n(Current file is \"%s\" %s)",filename,bUnderSourceControl?"and is under SourceSafe control":"but is not under SourceSafe control");

			if ( bUnderSourceControl )
			{
				if ( SS_IsCheckedOut( filename ))
				{
					CString strCheckOuts;
					int iCount;

					if ( SS_ListCheckOuts( filename, strCheckOuts, iCount ))
					{
						if ( (iCount != 1) || !SS_IsCheckedOutByMe( filename ))	
						{
							string += va("\n\nIt is checked out to:\n\n%s",(LPCSTR) strCheckOuts);
						}
						else
						{
							string += "\n\nYou have it checked out";
						}
					}
				}
				else
				{
					string += "\n\nIt is not checked out to anyone";
				}
			}
		}
	}
	else
	{
		string += "This machine does not appear to have SourceSafe setup properly (if at all)";
	}

	InfoBox((LPCSTR) string);
}

void CMainFrame::OnSSAdd()
{
	if ( SS_FunctionsAvailable() )
	{
		//LPCSTR filename = GetCurrentFileName();
		LPSTR filename = currentmap;

		if ( !SS_IsUnderSourceControl( filename ))
		{
			if ( SS_Add( filename ))
			{
				Sys_Printf("(File was added to SourceSafe Ok)\n");

				if (GetYesNo("Check this file out as well?"))
				{
					if ( SS_CheckOut( filename ))
					{
						Sys_Printf("(Checked out ok)\n");
					}
					else
					{
						ErrorBox("Failed during CheckOut\n");
					}
				}
			}
			else
			{
				ErrorBox( va("Error adding file \"%s\" to SourceSafe",filename));				
			}
		}
		else
		{
			ErrorBox( va("File \"%s\" is already under SourceSafe control",filename));
		}

	}
	else
	{
		ErrorBox("Function not available");
	}
}


void CMainFrame::OnSSCheckIn()
{
	if ( SS_FunctionsAvailable() )
	{
		LPSTR filename = currentmap;

		if ( SS_IsUnderSourceControl( filename ))
		{
			if ( SS_IsCheckedOutByMe( filename ))
			{
				if ( SS_CheckIn( filename ))
				{
					Sys_Printf("(Checked in ok)\n");
				}
				else
				{
					Sys_Printf("Error during CheckIn\n");
				}
			}
			else
			{
				ErrorBox( va("You do not have file \"%s\" checked out",filename));
			}
		}
		else
		{
			ErrorBox( va("File \"%s\" is not under SourceSafe control",filename));
		}
	}
	else
	{
		ErrorBox("Function not available");
	}
}

void CMainFrame::OnSSCheckOut()
{
	if ( SS_FunctionsAvailable() )
	{
		LPSTR filename = currentmap;

		if ( SS_IsUnderSourceControl( filename ))
		{
			if ( SS_IsCheckedOutByMe( filename ))
			{
				InfoBox( va("You already have file \"%s\" checked out",filename));
			}
			else
			{
				if (SS_IsCheckedOut( filename ))
				{
					CString strCheckOuts;
					int iCount;

					if ( SS_ListCheckOuts( filename, strCheckOuts, iCount ))
					{
						ErrorBox( va("File \"%s\" is already checked out by:\n\n%s",filename,(LPCSTR) strCheckOuts));
					}
				}
				else
				{				
					if ( SS_CheckOut( filename ))
					{
						Sys_Printf("(Checked out ok)\n");
					}
					else
					{
						Sys_Printf("Error during CheckOut\n");
					}
				}
			}
		}
		else
		{
			ErrorBox( va("File \"%s\" is not under SourceSafe control",filename));
		}
	}
	else
	{
		ErrorBox("Function not available");
	}
}

void CMainFrame::OnSSUndoCheckOut()
{
	if ( SS_FunctionsAvailable() )
	{
		LPSTR filename = currentmap;

		if ( SS_IsUnderSourceControl( filename ))
		{
			if ( SS_IsCheckedOutByMe( filename ))
			{
				if ( SS_UndoCheckOut( filename ))
				{
					Sys_Printf("(Undo CheckOut ok)\n");
				}
				else
				{
					Sys_Printf("Error during Undo CheckOut\n");
				}
			}
			else
			{
				ErrorBox( va("You do not have file \"%s\" checked out",filename));
			}
		}
		else
		{
			ErrorBox( va("File \"%s\" is not under SourceSafe control",filename));
		}
	}
	else
	{
		ErrorBox("Function not available");
	}
}

void CMainFrame::OnSSHistory()
{
	CString		string;

	if ( SS_FunctionsAvailable() )
	{
		LPSTR filename = currentmap;

		if ( SS_IsUnderSourceControl( filename ) )
		{
			CString strHistory;

			if ( SS_ListVersions( filename, strHistory ))
			{
				string += va("History of \"%s\"...\n\n%s",filename,(LPCSTR)strHistory);
			}
		}
		else
		{
			string += va("File \"%s\" doesn't appear to be under SourceSafe control",filename);
		}	
	}
	else
	{
		string += "This machine does not appear to have SourceSafe setup properly (if at all)";
	}

	InfoBox((LPCSTR) string);
}

void CMainFrame::OnSSConfigure()
{
	CSourceSafeSettings dialog(&g_cstrSourceSafeINI, &g_cstrSourceSafeProject, &g_bUseSourceSafe, &g_cstrBehavEdPath);
	dialog.DoModal();
}


void CMainFrame::OnPluginsRefresh() 
{
  CleanPlugInMenu();
  CString str(g_strAppPath);  
  AddSlash(str);
  str += "plugins\\";
  m_PlugInMgr.Init(str);
}

void CMainFrame::CleanPlugInMenu()
{
	m_nNextPlugInID = ID_PLUGIN_START;
	CMenu* pMenu = GetMenu();
	//--pMenu->RemoveMenu(MENU_PLUGIN, MF_BYPOSITION);
	//--pMenu->InsertMenu(MENU_PLUGIN, MF_BYPOSITION, 0, "Plugins");
	//--DrawMenuBar();
	CMenu* pSub = pMenu->GetSubMenu(MENU_PLUGIN);
	if (pSub)
	{
		int n = pSub->GetMenuItemCount();
		for (int i = n; i > 1 ; i--)
		{
			pSub->RemoveMenu(i, MF_BYPOSITION);
		}
	}
}

void CMainFrame::AddPlugInMenuItem(CPlugIn* pPlugIn)
{
	const char	*menuText;		//PGM
	CMenu* pMenu = GetMenu();
	CMenu* pSub = pMenu->GetSubMenu(MENU_PLUGIN);
	if (pSub)
	{
		CMenu* pChild = new CMenu();
		pChild->CreateMenu();
		int nCount = pPlugIn->getCommandCount();
    if (nCount > 0)
    {
		  while (nCount > 0)
		  {
			  menuText = pPlugIn->getCommand(--nCount);
        if (menuText != NULL && strlen(menuText) > 0)
        {
			    if(!strcmp(menuText, "-"))
				    pChild->AppendMenu(MF_SEPARATOR, NULL);
			    else
				    pChild->AppendMenu(MF_STRING, m_nNextPlugInID, menuText);
			    pPlugIn->addMenuID(m_nNextPlugInID++);
        }
		  }
		  pSub->AppendMenu(MF_POPUP, reinterpret_cast<unsigned int>(pChild->GetSafeHmenu()), pPlugIn->getMenuName());
    }
	}
}

void CMainFrame::OnPlugIn(unsigned int nID) 
{
  CMenu* pMenu = GetMenu();
  CString str;
  pMenu->GetMenuString(nID, str, MF_BYCOMMAND);
  m_PlugInMgr.Dispatch(nID, str);
}

void CMainFrame::OnViewShowhint() 
{
	if ( ( g_qeglobals.d_savedinfo.exclude ^= EXCLUDE_HINT ) & EXCLUDE_HINT )
    CheckMenuItem ( ::GetMenu(g_qeglobals.d_hwndMain), ID_VIEW_SHOWHINT, MF_BYCOMMAND | MF_UNCHECKED );
	else
    CheckMenuItem ( ::GetMenu(g_qeglobals.d_hwndMain), ID_VIEW_SHOWHINT, MF_BYCOMMAND | MF_CHECKED );
	Sys_UpdateWindows (W_XY|W_CAMERA);
}

void CMainFrame::OnAutocaulk() 
{
	Select_AutoCaulk(false);	// bMakeDetail
}

void CMainFrame::OnUpdateAutocaulk(CCmdUI* pCmdUI) 
{
	pCmdUI->Enable( selected_brushes.next != &selected_brushes);	
}

void CMainFrame::OnViewShowfuncgroups() 
{
	if ( ( g_qeglobals.d_savedinfo.exclude ^= EXCLUDE_FUNC_GROUP ) & EXCLUDE_FUNC_GROUP )
		CheckMenuItem( ::GetMenu(g_qeglobals.d_hwndMain), ID_VIEW_SHOWFUNCGROUPS, MF_BYCOMMAND | MF_UNCHECKED);
	else
		CheckMenuItem( ::GetMenu(g_qeglobals.d_hwndMain), ID_VIEW_SHOWFUNCGROUPS, MF_BYCOMMAND | MF_CHECKED);
	Sys_UpdateWindows (W_XY|W_CAMERA);	
}


void CMainFrame::OnViewShowcurvesonly() 
{
	if ( ( g_qeglobals.d_savedinfo.exclude ^= SHOW_CURVES_ONLY ) & SHOW_CURVES_ONLY )
		CheckMenuItem ( ::GetMenu(g_qeglobals.d_hwndMain), ID_VIEW_SHOWCURVESONLY, MF_BYCOMMAND | MF_CHECKED );
	else
		CheckMenuItem ( ::GetMenu(g_qeglobals.d_hwndMain), ID_VIEW_SHOWCURVESONLY, MF_BYCOMMAND | MF_UNCHECKED );
	Sys_UpdateWindows (W_XY|W_CAMERA);	
}

