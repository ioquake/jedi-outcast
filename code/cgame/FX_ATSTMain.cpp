// Bowcaster Weapon

// this line must stay at top so the whole PCH thing works...
#include "cg_headers.h"

//#include "cg_local.h"
#include "cg_media.h"
#include "FxScheduler.h"


/*
---------------------------
FX_ATSTMainProjectileThink
---------------------------
*/
void FX_ATSTMainProjectileThink( centity_t *cent, const struct weaponInfo_s *weapon )
{
	vec3_t forward;

	if ( VectorNormalize2( cent->currentState.pos.trDelta, forward ) == 0.0f )
	{
		forward[2] = 1.0f;
	}

	theFxScheduler.PlayEffect( "atst/shot", cent->lerpOrigin, forward );
}

/*
---------------------------
FX_ATSTMainHitWall
---------------------------
*/
void FX_ATSTMainHitWall( vec3_t origin, vec3_t normal )
{
	theFxScheduler.PlayEffect( "atst/wall_impact", origin, normal );
}

/*
---------------------------
FX_ATSTMainHitPlayer
---------------------------
*/
void FX_ATSTMainHitPlayer( vec3_t origin, vec3_t normal, qboolean humanoid )
{
	if ( humanoid )
	{
		theFxScheduler.PlayEffect( "atst/flesh_impact", origin, normal );
	}
	else
	{
		theFxScheduler.PlayEffect( "atst/droid_impact", origin, normal );
	}
}

/*
---------------------------
FX_ATSTSideAltProjectileThink
---------------------------
*/
void FX_ATSTSideAltProjectileThink( centity_t *cent, const struct weaponInfo_s *weapon )
{
	vec3_t forward;

	if ( VectorNormalize2( cent->currentState.pos.trDelta, forward ) == 0.0f )
	{
		forward[2] = 1.0f;
	}

	theFxScheduler.PlayEffect( "atst/side_alt_shot", cent->lerpOrigin, forward );
}

/*
---------------------------
FX_ATSTSideMainProjectileThink
---------------------------
*/
void FX_ATSTSideMainProjectileThink( centity_t *cent, const struct weaponInfo_s *weapon )
{
	vec3_t forward;

	if ( VectorNormalize2( cent->currentState.pos.trDelta, forward ) == 0.0f )
	{
		forward[2] = 1.0f;
	}

	theFxScheduler.PlayEffect( "atst/side_main_shot", cent->lerpOrigin, forward );
}
