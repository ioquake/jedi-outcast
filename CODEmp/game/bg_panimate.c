// BG_PAnimate.c

#include "q_shared.h"
#include "bg_public.h"
#include "bg_local.h"
#include "anims.h"
#include "../cgame/animtable.h"

qboolean BG_InSpecialJump( int anim )
{
	switch ( (anim&~ANIM_TOGGLEBIT) )
	{
	case BOTH_WALL_RUN_RIGHT:
	case BOTH_WALL_RUN_RIGHT_FLIP:
	case BOTH_WALL_RUN_LEFT:
	case BOTH_WALL_RUN_LEFT_FLIP:
	case BOTH_WALL_FLIP_RIGHT:
	case BOTH_WALL_FLIP_LEFT:
	case BOTH_FLIP_BACK1:
	case BOTH_FLIP_BACK2:
	case BOTH_FLIP_BACK3:
	case BOTH_WALL_FLIP_BACK1:
	case BOTH_BUTTERFLY_LEFT:
	case BOTH_BUTTERFLY_RIGHT:
		return qtrue;
	}
	return qfalse;
}

qboolean BG_FlippingAnim( int anim )
{
	switch ( (anim&~ANIM_TOGGLEBIT) )
	{
	case BOTH_FLIP_F:			//# Flip forward
	case BOTH_FLIP_B:			//# Flip backwards
	case BOTH_FLIP_L:			//# Flip left
	case BOTH_FLIP_R:			//# Flip right
		return qtrue;
		break;
	}

	return qfalse;
}

qboolean BG_SpinningSaberAnim( int anim )
{
	switch ( (anim&~ANIM_TOGGLEBIT) )
	{
	case BOTH_T1_BR_BL:
	case BOTH_T1__R__L:
	case BOTH_T1__R_BL:
	case BOTH_T1_TR_BL:
	case BOTH_T1_BR_TL:
	case BOTH_T1_BR__L:
	case BOTH_T1_TL_BR:
	case BOTH_T1__L_BR:
	case BOTH_T1__L__R:
	case BOTH_T1_BL_BR:
	case BOTH_T1_BL__R:
	case BOTH_T1_BL_TR:
		return qtrue;
		break;
	}
	return qfalse;
}

void PM_DebugLegsAnim(int anim)
{
	int oldAnim = (pm->ps->legsAnim & ~ANIM_TOGGLEBIT);
	int newAnim = (anim & ~ANIM_TOGGLEBIT);

	if (oldAnim < MAX_TOTALANIMATIONS && oldAnim >= BOTH_DEATH1 &&
		newAnim < MAX_TOTALANIMATIONS && newAnim >= BOTH_DEATH1)
	{
		Com_Printf("OLD: %s\n", animTable[oldAnim]);
		Com_Printf("NEW: %s\n", animTable[newAnim]);
	}
}

qboolean PM_InRoll( playerState_t *ps, int anim );

/*
======================
BG_ParseAnimationFile

Read a configuration file containing animation coutns and rates
models/players/visor/animation.cfg, etc

======================
*/
char		BGPAFtext[40000];
qboolean BG_ParseAnimationFile( const char *filename, animation_t *animations) 
{
	char		*text_p;
	int			len;
	int			i;
	char		*token;
	float		fps;
	int			skip;

	fileHandle_t	f;
	int			animNum;


	// load the file
	len = trap_FS_FOpenFile( filename, &f, FS_READ );
	if ( len <= 0 ) 
	{
		return qfalse;
	}
	if ( len >= sizeof( BGPAFtext ) - 1 ) 
	{
//		gi.Printf( "File %s too long\n", filename );
		return qfalse;
	}

	trap_FS_Read( BGPAFtext, len, f );
	BGPAFtext[len] = 0;
	trap_FS_FCloseFile( f );

	// parse the text
	text_p = BGPAFtext;
	skip = 0;	// quiet the compiler warning

	//FIXME: have some way of playing anims backwards... negative numFrames?

	//initialize anim array so that from 0 to MAX_ANIMATIONS, set default values of 0 1 0 100
	for(i = 0; i < MAX_ANIMATIONS; i++)
	{
		animations[i].firstFrame = 0;
		animations[i].numFrames = 0;
		animations[i].loopFrames = -1;
		animations[i].frameLerp = 100;
		animations[i].initialLerp = 100;
	}

	// read information for each frame
	while(1) 
	{
		token = COM_Parse( (const char **)(&text_p) );

		if ( !token || !token[0]) 
		{
			break;
		}

		animNum = GetIDForString(animTable, token);
		if(animNum == -1)
		{
//#ifndef FINAL_BUILD
#ifdef _DEBUG
			Com_Printf(S_COLOR_RED"WARNING: Unknown token %s in %s\n", token, filename);
#endif
			continue;
		}

		token = COM_Parse( (const char **)(&text_p) );
		if ( !token ) 
		{
			break;
		}
		animations[animNum].firstFrame = atoi( token );

		token = COM_Parse( (const char **)(&text_p) );
		if ( !token ) 
		{
			break;
		}
		animations[animNum].numFrames = atoi( token );

		token = COM_Parse( (const char **)(&text_p) );
		if ( !token ) 
		{
			break;
		}
		animations[animNum].loopFrames = atoi( token );

		token = COM_Parse( (const char **)(&text_p) );
		if ( !token ) 
		{
			break;
		}
		fps = atof( token );
		if ( fps == 0 ) 
		{
			fps = 1;//Don't allow divide by zero error
		}
		if ( fps < 0 )
		{//backwards
			animations[animNum].frameLerp = floor(1000.0f / fps);
		}
		else
		{
			animations[animNum].frameLerp = ceil(1000.0f / fps);
		}

		animations[animNum].initialLerp = ceil(1000.0f / fabs(fps));
	}

	return qtrue;
}



/*
===================
LEGS Animations
Base animation for overall body
===================
*/
static void PM_StartLegsAnim( int anim ) {
	if ( pm->ps->pm_type >= PM_DEAD ) {
		return;
	}
	if ( pm->ps->legsTimer > 0 ) {
		return;		// a high priority animation is running
	}

	if (pm->ps->usingATST)
	{ //animation is handled mostly client-side with only a few exceptions
		return;
	}

	if (anim == BOTH_STAND2 && pm->ps->weapon == WP_SABER && pm->ps->dualBlade)
	{ //a bit of a hack, but dualblade is cheat-only anyway
		anim = BOTH_STAND1;
	}

	pm->ps->legsAnim = ( ( pm->ps->legsAnim & ANIM_TOGGLEBIT ) ^ ANIM_TOGGLEBIT )
		| anim;

	if ( pm->debugLevel ) {
		Com_Printf("%d:  StartLegsAnim %d, on client#%d\n", pm->cmd.serverTime, anim, pm->ps->clientNum);
	}
}

void PM_ContinueLegsAnim( int anim ) {
	if ( ( pm->ps->legsAnim & ~ANIM_TOGGLEBIT ) == anim ) {
		return;
	}
	if ( pm->ps->legsTimer > 0 ) {
		return;		// a high priority animation is running
	}

	PM_StartLegsAnim( anim );
}

void PM_ForceLegsAnim( int anim) {
	if (BG_InSpecialJump(pm->ps->legsAnim) &&
		pm->ps->legsTimer > 0 &&
		!BG_InSpecialJump(anim))
	{
		return;
	}

	if (PM_InRoll(pm->ps, pm->ps->legsAnim) &&
		pm->ps->legsTimer > 0 &&
		!PM_InRoll(pm->ps, anim))
	{
		return;
	}

	pm->ps->legsTimer = 0;
	PM_StartLegsAnim( anim );
}



/*
===================
TORSO Animations
Override animations for upper body
===================
*/
void PM_StartTorsoAnim( int anim ) {
	if ( pm->ps->pm_type >= PM_DEAD ) {
		return;
	}

	if (pm->ps->usingATST)
	{ //animation is handled mostly client-side with only a few exceptions
		return;
	}

	if (anim == BOTH_STAND2 && pm->ps->weapon == WP_SABER && pm->ps->dualBlade)
	{ //a bit of a hack, but dualblade is cheat-only anyway
		anim = BOTH_STAND1;
	}

	pm->ps->torsoAnim = ( ( pm->ps->torsoAnim & ANIM_TOGGLEBIT ) ^ ANIM_TOGGLEBIT )
		| anim;
}

static void PM_ContinueTorsoAnim( int anim ) {
	if ( ( pm->ps->torsoAnim & ~ANIM_TOGGLEBIT ) == anim ) {
		return;
	}
	if ( pm->ps->torsoTimer > 0 ) {
		return;		// a high priority animation is running
	}
	PM_StartTorsoAnim( anim);
}


/*
==============
PM_TorsoAnimation

==============
*/
void PM_TorsoAnimation( void ) {
	if ( pm->ps->weaponstate == WEAPON_READY ) {
		if ( pm->ps->weapon == WP_SABER ) {
			PM_ContinueTorsoAnim( BOTH_STAND2 );
		} else {
			PM_ContinueTorsoAnim( WeaponReadyAnim[pm->ps->weapon] );
		}
		return;
	}
}


/*
-------------------------
PM_SetLegsAnimTimer
-------------------------
*/

void PM_SetLegsAnimTimer(int time )
{
	pm->ps->legsTimer = time;

	if (pm->ps->legsTimer < 0 && time != -1 )
	{//Cap timer to 0 if was counting down, but let it be -1 if that was intentional.  NOTENOTE Yeah this seems dumb, but it mirrors SP.
		pm->ps->legsTimer = 0;
	}
}

/*
-------------------------
PM_SetTorsoAnimTimer
-------------------------
*/

void PM_SetTorsoAnimTimer(int time )
{
	pm->ps->torsoTimer = time;

	if (pm->ps->torsoTimer < 0 && time != -1 )
	{//Cap timer to 0 if was counting down, but let it be -1 if that was intentional.  NOTENOTE Yeah this seems dumb, but it mirrors SP.
		pm->ps->torsoTimer = 0;
	}
}

void PM_SaberStartTransAnim( int saberAnimLevel, int anim, float *animSpeed )
{
	if ( ( (anim&~ANIM_TOGGLEBIT) >= BOTH_T1_BR__R && 
		(anim&~ANIM_TOGGLEBIT) <= BOTH_T1_BL_TL ) ||
		( (anim&~ANIM_TOGGLEBIT) >= BOTH_T2_BR__R && 
		(anim&~ANIM_TOGGLEBIT) <= BOTH_T2_BL_TL ) ||
		( (anim&~ANIM_TOGGLEBIT) >= BOTH_T3_BR__R && 
		(anim&~ANIM_TOGGLEBIT) <= BOTH_T3_BL_TL ) )
	{
		if ( saberAnimLevel == FORCE_LEVEL_1 )
		{
			*animSpeed *= 1.5;
		}
		else if ( saberAnimLevel == FORCE_LEVEL_3 )
		{
			*animSpeed *= 0.75;
		}
	}
}

/*
-------------------------
PM_SetAnimFinal
-------------------------
*/
void PM_SetAnimFinal(int setAnimParts,int anim,int setAnimFlags,
					 int blendTime)		// default blendTime=350
{
	animation_t *animations = pm->animations;

	float editAnimSpeed = 0;

	if (!animations)
	{
		return;
	}

	//NOTE: Setting blendTime here breaks actual blending..
	blendTime = 0;

	PM_SaberStartTransAnim(pm->ps->fd.saberAnimLevel, anim, &editAnimSpeed);

	// Set torso anim
	if (setAnimParts & SETANIM_TORSO)
	{
		// Don't reset if it's already running the anim
		if( !(setAnimFlags & SETANIM_FLAG_RESTART) && (pm->ps->torsoAnim & ~ANIM_TOGGLEBIT ) == anim )
		{
			goto setAnimLegs;
		}
		// or if a more important anim is running
		if( !(setAnimFlags & SETANIM_FLAG_OVERRIDE) && ((pm->ps->torsoTimer > 0)||(pm->ps->torsoTimer == -1)) )
		{	
			goto setAnimLegs;
		}

		PM_StartTorsoAnim( anim );

		if (setAnimFlags & SETANIM_FLAG_HOLD)
		{//FIXME: allow to set a specific time?
			if (setAnimFlags & SETANIM_FLAG_HOLDLESS)
			{	// Make sure to only wait in full 1/20 sec server frame intervals.
				int dur;
				
				dur = (animations[anim].numFrames ) * fabs(animations[anim].frameLerp);
				//dur = ((int)(dur/50.0)) * 50 / timeScaleMod;
				dur -= blendTime+fabs(animations[anim].frameLerp)*2;
				if (dur > 1)
				{
					pm->ps->torsoTimer = dur-1;
				}
				else
				{
					pm->ps->torsoTimer = fabs(animations[anim].frameLerp);
				}
			}
			else
			{
				pm->ps->torsoTimer = ((animations[anim].numFrames ) * fabs(animations[anim].frameLerp));
			}

			if (pm->ps->fd.forcePowersActive & (1 << FP_RAGE))
			{
				pm->ps->torsoTimer /= 1.7;
			}

			if (editAnimSpeed)
			{
				pm->ps->torsoTimer /= editAnimSpeed;
			}
		}
	}

setAnimLegs:
	// Set legs anim
	if (setAnimParts & SETANIM_LEGS)
	{
		// Don't reset if it's already running the anim
		if( !(setAnimFlags & SETANIM_FLAG_RESTART) && (pm->ps->legsAnim & ~ANIM_TOGGLEBIT ) == anim )
		{
			goto setAnimDone;
		}
		// or if a more important anim is running
		if( !(setAnimFlags & SETANIM_FLAG_OVERRIDE) && ((pm->ps->legsTimer > 0)||(pm->ps->legsTimer == -1)) )
		{	
			goto setAnimDone;
		}

		PM_StartLegsAnim(anim);

		if (setAnimFlags & SETANIM_FLAG_HOLD)
		{//FIXME: allow to set a specific time?
			if (setAnimFlags & SETANIM_FLAG_HOLDLESS)
			{	// Make sure to only wait in full 1/20 sec server frame intervals.
				int dur;
				
				dur = (animations[anim].numFrames -1) * fabs(animations[anim].frameLerp);
				//dur = ((int)(dur/50.0)) * 50 / timeScaleMod;
				dur -= blendTime+fabs(animations[anim].frameLerp)*2;
				if (dur > 1)
				{
					pm->ps->legsTimer = dur-1;
				}
				else
				{
					pm->ps->legsTimer = fabs(animations[anim].frameLerp);
				}
			}
			else
			{
				pm->ps->legsTimer = ((animations[anim].numFrames ) * fabs(animations[anim].frameLerp));
			}

			/*
			PM_DebugLegsAnim(anim);
			Com_Printf("%i\n", pm->ps->legsTimer);
			*/

			if (pm->ps->fd.forcePowersActive & (1 << FP_RAGE))
			{
				pm->ps->legsTimer /= 1.3;
			}
			else if (pm->ps->fd.forcePowersActive & (1 << FP_SPEED))
			{
				pm->ps->legsTimer /= 1.7;
			}
		}
	}

setAnimDone:
	return;
}



// Imported from single-player, this function is mainly intended to make porting from SP easier.
void PM_SetAnim(int setAnimParts,int anim,int setAnimFlags, int blendTime)
{	
	if (BG_InSpecialJump(anim))
	{
		setAnimFlags |= SETANIM_FLAG_RESTART;
	}

	if (PM_InRoll(pm->ps, pm->ps->legsAnim))
	{
		//setAnimFlags |= SETANIM_FLAG_RESTART;
		return;
	}

	if (setAnimFlags&SETANIM_FLAG_OVERRIDE)
	{
		if (setAnimParts & SETANIM_TORSO)
		{
			if( (setAnimFlags & SETANIM_FLAG_RESTART) || (pm->ps->torsoAnim & ~ANIM_TOGGLEBIT ) != anim )
			{
				PM_SetTorsoAnimTimer(0);
			}
		}
		if (setAnimParts & SETANIM_LEGS)
		{
			if( (setAnimFlags & SETANIM_FLAG_RESTART) || (pm->ps->legsAnim & ~ANIM_TOGGLEBIT ) != anim )
			{
				PM_SetLegsAnimTimer(0);
			}
		}
	}

	PM_SetAnimFinal(setAnimParts, anim, setAnimFlags, blendTime);
}


