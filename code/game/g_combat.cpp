// g_combat.c

// leave this line at the top for all g_xxxx.cpp files...
#include "g_headers.h"




#include "g_local.h"
#include "b_local.h"
#include "g_functions.h"
#include "anims.h"
#include "objectives.h"
#include "..\cgame\cg_local.h"
#include "g_icarus.h"
#include "wp_saber.h"
#include "Q3_Interface.h"

extern	cvar_t	*g_debugDamage;
extern qboolean	stop_icarus;
extern cvar_t	*g_dismemberment;
extern cvar_t	*g_realisticSaberDamage;
extern cvar_t		*g_timescale;
extern cvar_t		*d_slowmodeath;
extern gentity_t *player;

gentity_t *g_lastClientDamaged;

int killPlayerTimer = 0;

extern void NPC_TempLookTarget ( gentity_t *self, int lookEntNum, int minLookTime, int maxLookTime );
extern void G_AddVoiceEvent( gentity_t *self, int event, int speakDebounceTime );
extern qboolean PM_HasAnimation( gentity_t *ent, int animation );
extern qboolean G_TeamEnemy( gentity_t *self );
extern void CG_ChangeWeapon( int num );
extern void ChangeWeapon( gentity_t *ent, int newWeapon );

extern void G_SetEnemy( gentity_t *self, gentity_t *enemy );
extern void PM_SetLegsAnimTimer( gentity_t *ent, int *legsAnimTimer, int time );
extern void PM_SetTorsoAnimTimer( gentity_t *ent, int *torsoAnimTimer, int time );
extern int PM_PickAnim( gentity_t *self, int minAnim, int maxAnim );
extern qboolean PM_InOnGroundAnim ( playerState_t *ps );
extern void G_SoundOnEnt( gentity_t *ent, soundChannel_t channel, const char *soundPath );
extern void G_ATSTCheckPain( gentity_t *self, gentity_t *other, vec3_t point, int damage, int mod,int hitLoc );
extern qboolean Jedi_WaitingAmbush( gentity_t *self );
extern qboolean G_ClearViewEntity( gentity_t *ent );
extern qboolean PM_CrouchAnim( int anim );
extern qboolean PM_InKnockDown( playerState_t *ps );
extern qboolean PM_InRoll( playerState_t *ps );
extern qboolean PM_SpinningAnim( int anim );
extern qboolean PM_RunningAnim( int anim );
extern int PM_PowerLevelForSaberAnim( playerState_t *ps );
extern qboolean PM_SaberInSpecialAttack( int anim );
extern qboolean PM_SpinningSaberAnim( int anim );
extern qboolean PM_FlippingAnim( int anim );
extern qboolean PM_InSpecialJump( int anim );
extern qboolean PM_RollingAnim( int anim );
extern qboolean PM_InAnimForSaberMove( int anim, int saberMove );
extern qboolean PM_SaberInStart( int move );
extern qboolean PM_SaberInReturn( int move );
extern int PM_AnimLength( int index, animNumber_t anim );

qboolean G_CheckForLedge( gentity_t *self, vec3_t fallCheckDir );
static int G_CheckSpecialDeathAnim( gentity_t *self, vec3_t point, int damage, int mod, int hitLoc );
static int G_PickDeathAnim( gentity_t *self, vec3_t point, int damage, int mod, int hitLoc );
static void G_DoDismemberment( gentity_t *self, vec3_t point, int mod, int damage, int hitLoc );
extern gitem_t	*FindItemForAmmo( ammo_t ammo );
/*
============
AddScore

Adds score to both the client and his team
============
*/
void AddScore( gentity_t *ent, int score ) {
	if ( !ent->client ) {
		return;
	}
	// no scoring during pre-match warmup
	ent->client->ps.persistant[PERS_SCORE] += score;
}

/*
=================
TossClientItems

Toss the weapon and powerups for the killed player
=================
*/
extern gentity_t *WP_DropThermal( gentity_t *ent );
extern void WP_SaberLose( gentity_t *self, vec3_t throwDir );
gentity_t *TossClientItems( gentity_t *self ) 
{
	gentity_t	*dropped = NULL;
	gitem_t		*item;
	int			weapon;

	if ( self->client->NPC_class == CLASS_SEEKER || self->client->NPC_class == CLASS_REMOTE )
	{
		// these things are so small that they shouldn't bother throwing anything
		return NULL;
	}

	// drop the weapon if not a saber or enemy-only weapon
	weapon = self->s.weapon;
	if ( weapon == WP_SABER )
	{
		WP_SaberLose( self, NULL );
		if ( self->client->NPC_class == CLASS_SHADOWTROOPER )
		{//drop a force crystal
			item = FindItemForAmmo( AMMO_FORCE );
			Drop_Item( self, item, 0, qtrue );
		}
	}
	else if ( weapon > WP_SABER && weapon <= MAX_PLAYER_WEAPONS )//&& self->client->ps.ammo[ weaponData[weapon].ammoIndex ]
	{
		// find the item type for this weapon
		item = FindItemForWeapon( (weapon_t) weapon );

		if ( weapon == WP_THERMAL && (self->client->ps.torsoAnim == BOTH_ATTACK10))
		{//drop it! 
			self->client->ps.weaponChargeTime = level.time - FRAMETIME;//so it just kind of drops it
			dropped = WP_DropThermal( self );
		}
		else
		{
			// spawn the item
			dropped = Drop_Item( self, item, 0, qtrue );
			//TEST: dropped items never go away
			dropped->e_ThinkFunc = thinkF_NULL;
			dropped->nextthink = -1;

			if ( !self->s.number )
			{//player's dropped items never go away
				//dropped->e_ThinkFunc = thinkF_NULL;
				//dropped->nextthink = -1;
				dropped->count = 0;//no ammo
			}
			else
			{//FIXME: base this on the NPC's actual amount of ammo he's used up... 
				switch ( weapon )
				{
				case WP_BRYAR_PISTOL:
					dropped->count = 20;
					break;
				case WP_BLASTER:
					dropped->count = 25;
					break;
				case WP_DISRUPTOR:
					dropped->count = 20;
					break;
				case WP_BOWCASTER:
					dropped->count = 5;
					break;
				case WP_REPEATER:
					dropped->count = 20;
					break;
				case WP_DEMP2:
					dropped->count = 10;
					break;
				case WP_FLECHETTE:
					dropped->count = 30;
					break;
				case WP_ROCKET_LAUNCHER:
					dropped->count = 3;
					break;
				case WP_THERMAL:
					dropped->count = 3;
					break;
				case WP_TRIP_MINE:
					dropped->count = 3;
					break;
				case WP_DET_PACK:
					dropped->count = 1;
					break;
				case WP_STUN_BATON:
					dropped->count = 20;
					break;
				default:
					dropped->count = 0;
					break;
				}
			}
		}


		// well, dropped weapons are G2 models, so they have to be initialised if they want to draw..give us a radius so we don't get prematurely culled
		gi.G2API_InitGhoul2Model( dropped->ghoul2, item->world_model, G_ModelIndex( item->world_model ));
		dropped->s.radius = 10;

	}
	else if (( self->client->NPC_class == CLASS_SENTRY ) || ( self->client->NPC_class == CLASS_PROBE ))
	{
		item = FindItemForAmmo( AMMO_BLASTER );
		Drop_Item( self, item, 0, qtrue );
	}
	else if ( self->client->NPC_class == CLASS_MARK1 ) 
	{

		if (Q_irand( 1, 2 )>1)
		{
			item = FindItemForAmmo( AMMO_METAL_BOLTS );
		}
		else
		{
			item = FindItemForAmmo( AMMO_BLASTER );
		}
		Drop_Item( self, item, 0, qtrue );
	}
	else if ( self->client->NPC_class == CLASS_MARK2 ) 
	{

		if (Q_irand( 1, 2 )>1)
		{
			item = FindItemForAmmo( AMMO_METAL_BOLTS );
		}
		else
		{
			item = FindItemForAmmo( AMMO_POWERCELL );
		}
		Drop_Item( self, item, 0, qtrue );
	}

	return dropped;//NOTE: presumes only drop one thing
}

void G_DropKey( gentity_t *self )
{//drop whatever security key I was holding
	gitem_t		*item = FindItemForInventory( INV_SECURITY_KEY1 );
	gentity_t	*dropped = Drop_Item( self, item, 0, qtrue );
	//Don't throw the key
	VectorClear( dropped->s.pos.trDelta );
	dropped->message = G_NewString( self->message );
	self->message = NULL;
}

/*
==================
LookAtKiller
==================
*/
void LookAtKiller( gentity_t *self, gentity_t *inflictor, gentity_t *attacker ) {
	vec3_t		dir;
	//vec3_t		angles;

	if ( attacker && attacker != self ) 
	{
		VectorSubtract( attacker->s.pos.trBase, self->s.pos.trBase, dir );
	} 
	else if ( inflictor && inflictor != self ) 
	{
		VectorSubtract( inflictor->s.pos.trBase, self->s.pos.trBase, dir );
	}
	else 
	{
		self->client->ps.stats[STAT_DEAD_YAW] = self->currentAngles[YAW];
		return;
	}

	self->client->ps.stats[STAT_DEAD_YAW] = vectoyaw ( dir );

	/*
	angles[YAW] = vectoyaw ( dir );
	angles[PITCH] = 0; 
	angles[ROLL] = 0;
	*/
}

void ObjectDie (gentity_t *self, gentity_t *inflictor, gentity_t *attacker, int damage, int meansOfDeath )
{
	if(self->target)
		G_UseTargets(self, attacker);

	//remove my script_targetname
	G_FreeEntity( self );
}
/*
==================
ExplodeDeath
==================
*/

//FIXME: all hacked up...

//void CG_SurfaceExplosion( vec3_t origin, vec3_t normal, float radius, float shake_speed, qboolean smoke );
void ExplodeDeath( gentity_t *self ) 
{
//	gentity_t	*tent;
	vec3_t		forward={0,0,1};

	self->takedamage = qfalse;//stop chain reaction runaway loops

	self->s.loopSound = 0;

	VectorCopy( self->currentOrigin, self->s.pos.trBase );

//	tent = G_TempEntity( self->s.origin, EV_FX_EXPLOSION );
//	AngleVectors(self->s.angles, forward, NULL, NULL);  // FIXME: letting effect always shoot up?  Might be ok.

	if ( self->fxID > 0 )
	{
		G_PlayEffect( self->fxID, self->currentOrigin, forward );
	}
//	else
//	{
//		CG_SurfaceExplosion( self->currentOrigin, forward, 20.0f, 12.0f, ((self->spawnflags&4)==qfalse) );	//FIXME: This needs to be consistent to all exploders!
//		G_Sound(self, self->sounds );
//	}
	
	if(self->splashDamage > 0 && self->splashRadius > 0)
	{
		gentity_t *attacker = self;
		if ( self->owner )
		{
			attacker = self->owner;
		}
		G_RadiusDamage( self->currentOrigin, attacker, self->splashDamage, self->splashRadius, 
				attacker, MOD_UNKNOWN );
	}

	ObjectDie( self, self, self, 20, 0 );
}

void ExplodeDeath_Wait( gentity_t *self, gentity_t *inflictor, gentity_t *attacker, int damage, int meansOfDeath,int hitLoc ) 
{
	self->e_DieFunc = dieF_NULL;
	self->nextthink = level.time + Q_irand(100, 500);
	self->e_ThinkFunc = thinkF_ExplodeDeath;
}

void ExplodeDeath( gentity_t *self, gentity_t *inflictor, gentity_t *attacker, int damage, int meansOfDeath,int hitLoc ) 
{
	self->currentOrigin[2] += 16; // me bad for hacking this.  should either do it in the effect file or make a custom explode death??
	ExplodeDeath( self );
}

void GoExplodeDeath( gentity_t *self, gentity_t *other, gentity_t *activator) 
{
	G_ActivateBehavior(self,BSET_USE);

	self->targetname = "";	//Make sure this entity cannot be told to explode again (recursive death fix)

	ExplodeDeath( self );
}

qboolean G_ActivateBehavior (gentity_t *self, int bset );
void G_CheckVictoryScript(gentity_t *self)
{
	if ( !G_ActivateBehavior( self, BSET_VICTORY ) )
	{
		if ( self->NPC && self->s.weapon == WP_SABER )
		{//Jedi taunt from within their AI
			self->NPC->blockedSpeechDebounceTime = 0;//get them ready to taunt
			return;
		}
		//FIXME: any way to not say this *right away*?  Wait for victim's death anim/scream to finish?
		if ( self->NPC && self->NPC->group && self->NPC->group->commander && self->NPC->group->commander->NPC->rank > self->NPC->rank && !Q_irand( 0, 2 ) )
		{//sometimes have the group commander speak instead
			G_AddVoiceEvent( self->NPC->group->commander, Q_irand(EV_VICTORY1, EV_VICTORY3), 2000 );
		}
		else
		{
			G_AddVoiceEvent( self, Q_irand(EV_VICTORY1, EV_VICTORY3), 2000 );
		}
	}
}

qboolean OnSameTeam( gentity_t *ent1, gentity_t *ent2 )
{
	if ( !ent1->client || !ent2->client )
	{
		if ( ent1->noDamageTeam )
		{
			if ( ent2->client && ent2->client->playerTeam == ent1->noDamageTeam )
			{
				return qtrue;
			}
			else if ( ent2->noDamageTeam == ent1->noDamageTeam )
			{
				if ( ent1->splashDamage && ent2->splashDamage && Q_stricmp("ambient_etherian_fliers", ent1->classname) != 0 )
				{//Barrels, exploding breakables and mines will blow each other up
					return qfalse;
				}
				else
				{
					return qtrue;
				}
			}
		}
		return qfalse;
	}

	// shouldn't need this anymore, there were problems with certain droids, but now they have been labeled TEAM_ENEMY so this isn't needed
//	if ((( ent1->client->playerTeam == TEAM_IMPERIAL ) && ( ent1->client->playerTeam == TEAM_BOTS )) ||
//		(( ent1->client->playerTeam == TEAM_BOTS ) && ( ent1->client->playerTeam == TEAM_IMPERIAL )))
//	{
//		return qtrue;
//	}

	return ( ent1->client->playerTeam == ent2->client->playerTeam );
}

/*
-------------------------
G_AlertTeam
-------------------------
*/

void G_AlertTeam( gentity_t *victim, gentity_t *attacker, float radius, float soundDist )
{
	gentity_t	*radiusEnts[ 128 ];
	vec3_t		mins, maxs;
	int			numEnts;

	if ( attacker == NULL || attacker->client == NULL )
		return;

	//Setup the bbox to search in
	for ( int i = 0; i < 3; i++ )
	{
		mins[i] = victim->currentOrigin[i] - radius;
		maxs[i] = victim->currentOrigin[i] + radius;
	}

	//Get the number of entities in a given space
	numEnts = gi.EntitiesInBox( mins, maxs, radiusEnts, 128 );

	//Cull this list
	for ( i = 0; i < numEnts; i++ )
	{
		//Validate clients
		if ( radiusEnts[i]->client == NULL )
			continue;

		//only want NPCs
		if ( radiusEnts[i]->NPC == NULL )
			continue;

		//Don't bother if they're ignoring enemies
		if ( radiusEnts[i]->svFlags & SVF_IGNORE_ENEMIES )
			continue;

		//This NPC specifically flagged to ignore alerts
		if ( radiusEnts[i]->NPC->scriptFlags & SCF_IGNORE_ALERTS )
			continue;

		//This NPC specifically flagged to ignore alerts
		if ( !(radiusEnts[i]->NPC->scriptFlags&SCF_LOOK_FOR_ENEMIES) )
			continue;

		//this ent does not participate in group AI
		if ( (radiusEnts[i]->NPC->scriptFlags&SCF_NO_GROUPS) )
			continue;

		//Skip the requested avoid radiusEnts[i] if present
		if ( radiusEnts[i] == victim )
			continue;

		//Must be on the same team
		if ( radiusEnts[i]->client->playerTeam != victim->client->playerTeam )
			continue;

		//Must be alive
		if ( radiusEnts[i]->health <= 0 )
			continue;

		if ( radiusEnts[i]->enemy == NULL )
		{
			if ( soundDist <= 0 ||DistanceSquared( radiusEnts[i]->currentOrigin, victim->currentOrigin ) > (soundDist*soundDist) )
			{
				if ( InFOV( victim, radiusEnts[i], radiusEnts[i]->NPC->stats.hfov, radiusEnts[i]->NPC->stats.vfov ) &&  NPC_ClearLOS( radiusEnts[i], victim->currentOrigin ) )
				{
					G_SetEnemy( radiusEnts[i], attacker );
				}

				continue;
			}

			//FIXME: This can have a nasty cascading effect if setup wrong...
			G_SetEnemy( radiusEnts[i], attacker );
			//FIXME: base this delay on distance and facing? Or NPC type? Rank?
			TIMER_Set( radiusEnts[i], "attackDelay", Q_irand( 7500, 4500 ) );
			TIMER_Set( radiusEnts[i], "roamTime", Q_irand( 1000, 3500 ) );
		}
	}
}

/*
-------------------------
G_DeathAlert
-------------------------
*/

#define	DEATH_ALERT_RADIUS			512
#define	DEATH_ALERT_SOUND_RADIUS	256

void G_DeathAlert( gentity_t *victim, gentity_t *attacker )
{//FIXME: with all the other alert stuff, do we really need this?
	G_AlertTeam( victim, attacker, DEATH_ALERT_RADIUS, DEATH_ALERT_SOUND_RADIUS );
}

/*
----------------------------------------
DeathFX

Applies appropriate special effects that occur while the entity is dying
Not to be confused with NPC_RemoveBodyEffects (NPC.cpp), which only applies effect when removing the body
----------------------------------------
*/

void DeathFX( gentity_t *ent )
{
	if ( !ent || !ent->client )
		return;
/*
	switch( ent->client->playerTeam )
	{
	case TEAM_BOTS:
		if (!Q_stricmp( ent->NPC_type, "mouse" ))
		{
			vec3_t		effectPos;
			VectorCopy( ent->currentOrigin, effectPos );
			effectPos[2] -= 20;

			G_PlayEffect( "mouseexplosion1", effectPos );
			G_PlayEffect( "smaller_chunks", effectPos );

		}
		else if (!Q_stricmp( ent->NPC_type, "probe" ))
		{
			vec3_t		effectPos;
			VectorCopy( ent->currentOrigin, effectPos );
			effectPos[2] += 50;

			G_PlayEffect( "probeexplosion1", effectPos );
			G_PlayEffect( "small_chunks", effectPos );
		}
		else
		{
			vec3_t		effectPos;
			VectorCopy( ent->currentOrigin, effectPos );
			effectPos[2] -= 15;
			G_PlayEffect( "droidexplosion1", effectPos );
			G_PlayEffect( "small_chunks", effectPos );
		}

		break;

	default:
		break;
	}
*/
	// team no longer indicates species/race.  NPC_class should be used to identify certain npc types
	vec3_t		effectPos;
	switch(ent->client->NPC_class)
	{
	case CLASS_MOUSE:
		VectorCopy( ent->currentOrigin, effectPos );
		effectPos[2] -= 20;
		G_PlayEffect( "env/small_explode", effectPos );
		break;

	case CLASS_PROBE:
		VectorCopy( ent->currentOrigin, effectPos );
		effectPos[2] += 50;
		G_PlayEffect( "probeexplosion1", effectPos );
		G_PlayEffect( "small_chunks", effectPos );
		break;
		
	case CLASS_ATST: 
		VectorCopy( ent->currentOrigin, effectPos );
		effectPos[2] -= 15;
		G_PlayEffect( "droidexplosion1", effectPos );
		G_PlayEffect( "small_chunks", effectPos );
		break;

	case CLASS_SEEKER:
	case CLASS_REMOTE:
		G_PlayEffect( "env/small_explode", ent->currentOrigin );
		break;

	case CLASS_GONK:
//		statusTextIndex = Q_irand( IGT_RESISTANCEISFUTILE, IGT_NAMEIS8OF12 );
		G_SoundOnEnt( ent, CHAN_AUTO, va("sound/chars/gonk/misc/death%d.wav",Q_irand( 1, 3 )) );
		// fall through, Gonk should still explode like the rest

	// should list all remaining droids here, hope I didn't miss any
	case CLASS_R2D2:
	case CLASS_R5D2:
	case CLASS_PROTOCOL:
	case CLASS_MARK1:
	case CLASS_MARK2:
	case CLASS_INTERROGATOR:

		VectorCopy( ent->currentOrigin, effectPos );
		effectPos[2] -= 15;
		G_PlayEffect( "droidexplosion1", effectPos );
		G_PlayEffect( "small_chunks", effectPos );
		break;

	case CLASS_SENTRY:
		G_SoundOnEnt( ent, CHAN_AUTO, "sound/chars/sentry/misc/death.wav");
		VectorCopy( ent->currentOrigin, effectPos );
		G_PlayEffect( "env/med_explode", effectPos );
		break;

	default:
		break;

	}

}

void G_SetMissionStatusText( gentity_t *attacker, int mod )
{
	if ( statusTextIndex >= 0 )
	{
		return;
	}

	if ( mod == MOD_FALLING )
	{//fell to your death
		statusTextIndex = STAT_WATCHYOURSTEP;
	}
	else if ( mod == MOD_CRUSH )
	{//crushed
		statusTextIndex = STAT_JUDGEMENTMUCHDESIRED;
	}
	// borg no longer exist
//	else if ( attacker && attacker->client && attacker->client->playerTeam == TEAM_BORG )
//	{//assimilated
//		statusTextIndex = Q_irand( IGT_RESISTANCEISFUTILE, IGT_NAMEIS8OF12 );
//	}
	else if ( attacker && Q_stricmp( "trigger_hurt", attacker->classname ) == 0 )
	{//Killed by something that should have been clearly dangerous
//		statusTextIndex = Q_irand( IGT_JUDGEMENTDESIRED, IGT_JUDGEMENTMUCHDESIRED );
		statusTextIndex = STAT_JUDGEMENTMUCHDESIRED;
	}
	else if ( attacker && attacker->s.number != 0 && attacker->client && attacker->client->playerTeam == TEAM_PLAYER )
	{//killed by a teammate
		statusTextIndex = STAT_INSUBORDINATION;
	}
	/*
	else if ()
	{//killed a teammate- note: handled above
		if ( Q_irand( 0, 1 ) )
		{
			statusTextIndex = IGT_YOUCAUSEDDEATHOFTEAMMATE;
		}
		else
		{
			statusTextIndex = IGT_KILLEDANINNOCENTCREWMAN;
		}
	}
	else
	{
		//This next block is not contiguous
		IGT_INADEQUATE,
		IGT_RESPONSETIME,
		IGT_SHOOTINRANGE,
		IGT_TRYAGAIN,
		IGT_TRAINONHOLODECK,
		IGT_WHATCOLORSHIRT,
		IGT_NOTIMPRESS7OF9,
		IGT_NEELIXFAREDBETTER,
		IGT_THATMUSTHURT,
		IGT_TUVOKDISAPPOINTED,
		IGT_STARFLEETNOTIFYFAMILY,
		IGT_TEAMMATESWILLMISSYOU,
		IGT_LESSTHANEXEMPLARY,
		IGT_SACRIFICEDFORTHEWHOLE,
		IGT_NOTLIVELONGANDPROSPER,
		IGT_BETTERUSEOFSIMULATIONS,
	}
	*/

	/*
	//These can be set by designers
	IGT_INSUBORDINATION,
	IGT_YOUCAUSEDDEATHOFTEAMMATE,
	IGT_DIDNTPROTECTTECH,
	IGT_DIDNTPROTECT7OF9,
	IGT_NOTSTEALTHYENOUGH,
	IGT_STEALTHTACTICSNECESSARY,
	*/
}

void G_MakeTeamVulnerable( void )
{
	int i, newhealth;
	gentity_t *ent = &g_entities[0];
	gentity_t *self = &g_entities[0];
	if ( !self->client )
	{
		return;
	}

	for ( i = 0; i < globals.num_entities ; i++, ent++) 
	{
		if ( !ent )
		{
			continue;
		}
		if ( !ent->inuse  )
		{
			continue;
		}
		if ( !ent->client  )
		{
			continue;
		}
		if ( ent->client->playerTeam != TEAM_PLAYER )
		{
			continue;
		}
		if ( !(ent->flags&FL_UNDYING) )
		{
			continue;
		}
		if ( !ent->client->squadname )
		{
			continue;
		}
		if ( !ent->client->squadname[0] )
		{
			continue;
		}
		if ( Q_stricmp(self->client->squadname, ent->client->squadname) )
		{
			continue;
		}
		ent->flags &= ~FL_UNDYING;
		newhealth = Q_irand( 5, 40 );
		if ( ent->health > newhealth )
		{
			ent->health = newhealth;
		}
	}
}

void G_StartMatrixEffect( gentity_t *ent, qboolean falling = qfalse, int length = 1000 )
{//FIXME: only do this if no other enemies around?
	if ( g_timescale->value != 1.0 )
	{//already in some slow-mo mode
		return;
	}

	gentity_t	*matrix = G_Spawn();
	if ( matrix )
	{
		G_SetOrigin( matrix, ent->currentOrigin );
		gi.linkentity( matrix );
		matrix->s.otherEntityNum = ent->s.number;
		matrix->e_clThinkFunc = clThinkF_CG_MatrixEffect;
		matrix->s.eType = ET_THINKER;
		matrix->svFlags |= SVF_BROADCAST;// Broadcast to all clients
		matrix->s.time = level.time;
		matrix->s.eventParm = length;
		matrix->e_ThinkFunc = thinkF_G_FreeEntity;
		matrix->nextthink = level.time + length + 500;
		if ( falling )
		{//no timescale or vert bob
			matrix->s.weapon = 1;
		}
	}
}

qboolean G_JediInRoom( vec3_t from )
{
	gentity_t *ent = NULL;
	int i;
	for ( i = 1, ent = &g_entities[1]; i < globals.num_entities; i++, ent++ )
	{
		if ( !ent )
		{
			continue;
		}
		if ( !ent->inuse )
		{
			continue;
		}
		if ( !ent->NPC )
		{
			continue;
		}
		if ( ent->health <= 0 )
		{
			continue;
		}
		if ( ent->s.eFlags&EF_NODRAW )
		{
			continue;
		}
		if ( ent->s.weapon != WP_SABER )
		{
			continue;
		}
		if ( !gi.inPVS( ent->currentOrigin, from ) )
		{
			continue;
		}
		return qtrue;
	}
	return qfalse;
}

/*
==================
player_die
==================
*/
void NPC_SetAnim(gentity_t	*ent,int type,int anim,int priority);
extern void AI_DeleteSelfFromGroup( gentity_t *self );
extern void AI_GroupMemberKilled( gentity_t *self );
extern qboolean FlyingCreature( gentity_t *ent );
extern void G_DrivableATSTDie( gentity_t *self );
void player_die( gentity_t *self, gentity_t *inflictor, gentity_t *attacker, int damage, int meansOfDeath,int hitLoc ) 
{
	int			anim;
	int			contents;
	qboolean	deathScript = qfalse;
	qboolean	lastInGroup = qfalse;

	//FIXME: somehow people are sometimes not completely dying???
	if ( self->client->ps.pm_type == PM_DEAD && (meansOfDeath != MOD_SNIPER || (self->flags & FL_DISINTEGRATED)) )
	{//do dismemberment/twitching
		anim = G_PickDeathAnim( self, self->pos1, damage, meansOfDeath, hitLoc );
		G_DoDismemberment( self, self->pos1, meansOfDeath, damage, hitLoc );
		if ( anim >= 0 )
		{
			NPC_SetAnim(self, SETANIM_BOTH, anim, SETANIM_FLAG_OVERRIDE|SETANIM_FLAG_RESTART|SETANIM_FLAG_HOLD);
		}
		return;
	}

	if ( d_saberCombat->integer && attacker && attacker->client )
	{
		gi.Printf( S_COLOR_RED"combatant %s died, killer anim = %s\n", self->targetname, animTable[attacker->client->ps.torsoAnim].name );
	}

	if ( self->NPC )
	{
		if ( self->client && Jedi_WaitingAmbush( self ) )
		{//ambushing trooper
			self->client->noclip = qfalse;
		}
		NPC_FreeCombatPoint( self->NPC->combatPoint );
		if ( self->NPC->group )
		{
			lastInGroup = (self->NPC->group->numGroup < 2);
			AI_GroupMemberKilled( self );
			AI_DeleteSelfFromGroup( self );
		}

		if ( self->NPC->tempGoal )
		{
			G_FreeEntity( self->NPC->tempGoal );
		}
		if ( self->s.eFlags & EF_LOCKED_TO_WEAPON )
		{
			// dumb, just get the NPC out of the chair
extern void RunEmplacedWeapon( gentity_t *ent, usercmd_t **ucmd );

			usercmd_t cmd, *ad_cmd;

			memset( &cmd, 0, sizeof( usercmd_t ));

			//gentity_t *old = self->owner;

			if ( self->owner )
			{
				self->owner->s.frame = self->owner->startFrame = self->owner->endFrame = 0;
				self->owner->svFlags &= ~SVF_ANIMATING;
			}

			cmd.buttons |= BUTTON_USE;
			ad_cmd = &cmd;
			RunEmplacedWeapon( self, &ad_cmd );
			//self->owner = old;
		}
	}
	if ( attacker && attacker->NPC && attacker->NPC->group && attacker->NPC->group->enemy == self )
	{
		attacker->NPC->group->enemy = NULL;
	}
	if ( self->client->ps.saberEntityNum != ENTITYNUM_NONE && self->client->ps.saberEntityNum > 0 )
	{
		if ( self->client->ps.saberInFlight )
		{//just drop it
			self->client->ps.saberActive = qfalse;
		}
		else
		{
			if ( (hitLoc != HL_HAND_RT 
				|| !self->client->dismemberable
				|| meansOfDeath != MOD_SABER )//if might get hand cut off, leave saber in hand
				&& ( Q_irand( 0, 1 ) 
					|| meansOfDeath == MOD_EXPLOSIVE 
					|| meansOfDeath == MOD_EXPLOSIVE_SPLASH ) )
			{//drop it
				TossClientItems( self );
			}
			else
			{//just free it
				if ( g_entities[self->client->ps.saberEntityNum].inuse )
				{
					G_FreeEntity( &g_entities[self->client->ps.saberEntityNum] );
				}
				self->client->ps.saberEntityNum = ENTITYNUM_NONE;
			}
		}
	}
	//Use any target we had
	if ( meansOfDeath != MOD_KNOCKOUT )
	{
		G_UseTargets( self, self );
	}

	if ( attacker )
	{
		if ( attacker->client && !attacker->s.number )
		{
			attacker->client->sess.missionStats.enemiesKilled++;
		}
		G_CheckVictoryScript(attacker);
		//player killing a jedi with a lightsaber spawns a matrix-effect entity
		if ( d_slowmodeath->integer )
		{
			if ( !self->s.number )
			{//what the hell, always do slow-mo when player dies
				//FIXME: don't do this when crushed to death?
				if ( meansOfDeath == MOD_FALLING && self->client->ps.groundEntityNum == ENTITYNUM_NONE )
				{//falling to death, have not hit yet
					G_StartMatrixEffect( self, qtrue, 10000 );
				}
				else
				{
					G_StartMatrixEffect( self );
				}
			}
			else if ( d_slowmodeath->integer < 4 )
			{//any jedi killed by player-saber
				if ( d_slowmodeath->integer < 3 )
				{//must be the last jedi in the room
					if ( !G_JediInRoom( attacker->currentOrigin ) )
					{
						lastInGroup = qtrue;
					}
					else
					{
						lastInGroup = qfalse;
					}
				}
				if ( !attacker->s.number 
					&& self->s.weapon == WP_SABER 
					&& meansOfDeath == MOD_SABER 
					&& attacker->client 
					&& attacker->client->ps.weapon == WP_SABER 
					&& !attacker->client->ps.saberInFlight 
					&& (d_slowmodeath->integer > 2||lastInGroup) )//either slow mo death level 3 (any jedi) or 2 and I was the last jedi in the room
				{//Matrix!
					G_StartMatrixEffect( self );
				}
			}
			else
			{//all player-saber kills
				if ( !attacker->s.number 
					&& meansOfDeath == MOD_SABER 
					&& attacker->client 
					&& attacker->client->ps.weapon == WP_SABER 
					&& !attacker->client->ps.saberInFlight 
					&& (d_slowmodeath->integer > 4||lastInGroup))//either slow mo death level 5 (any enemy) or 4 and I was the last in my group
				{//Matrix!
					G_StartMatrixEffect( self );
				}
			}
		}
	}

	self->enemy = attacker;
	self->client->renderInfo.lookTarget = ENTITYNUM_NONE;

	self->client->ps.persistant[PERS_KILLED]++;
	if ( self->client->playerTeam == TEAM_PLAYER )
	{//FIXME: just HazTeam members in formation on away missions?
		//or more controlled- via deathscripts?
		// Don't count player
		if (( &g_entities[0] != NULL && g_entities[0].client ) && (self->s.number != 0))
		{//add to the number of teammates lost
			g_entities[0].client->ps.persistant[PERS_TEAMMATES_KILLED]++;
		}
		else	// Player died, fire off scoreboard soon
		{
			cg.missionStatusDeadTime = level.time + 1000;	// Too long?? Too short??
			cg.zoomMode = 0; // turn off zooming when we die
		}
	}

	if ( self->s.number == 0 && attacker )
	{
//		G_SetMissionStatusText( attacker, meansOfDeath );
		//TEST: If player killed, unmark all teammates from being undying so they can buy it too
		//NOTE: we want this to happen ONLY on our squad ONLY on missions... in the tutorial or on voyager levels this could be really weird.
		G_MakeTeamVulnerable();
	}

	if ( attacker && attacker->client) 
	{
		if ( attacker == self || OnSameTeam (self, attacker ) ) 
		{
			AddScore( attacker, -1 );
		} 
		else 
		{
			AddScore( attacker, 1 );
		}
	} 
	else 
	{
		AddScore( self, -1 );
	}

	// if client is in a nodrop area, don't drop anything
	contents = gi.pointcontents( self->currentOrigin, -1 );
	if ( self->s.weapon != WP_SABER 
		&& self->s.number != 0 
		&& !( contents & CONTENTS_NODROP ) 
		&& meansOfDeath != MOD_SNIPER 
		&& (!self->client||self->client->NPC_class!=CLASS_GALAKMECH))
	{
		TossClientItems( self );
	}

	if ( meansOfDeath == MOD_SNIPER )
	{//I was disintegrated
		if ( self->message )
		{//I was holding a key
			//drop the key
			G_DropKey( self );
		}
	}

	if ( self->s.weapon == WP_SABER )
	{//never drop a lightsaber!
		if ( self->client->ps.saberActive )
		{
			self->client->ps.saberActive = qfalse;
			if ( self->client->playerTeam == TEAM_PLAYER )
			{
				G_SoundOnEnt( self, CHAN_AUTO, "sound/weapons/saber/saberoff.wav" );
			}
			else
			{
				G_SoundOnEnt( self, CHAN_AUTO, "sound/weapons/saber/enemy_saber_off.wav" );
			}
		}
	}
	else
	{// Sigh...borg shouldn't drop their weapon attachments when they die..
		self->s.weapon = WP_NONE;
		if ( self->weaponModel != -1 && self->ghoul2.size())
		{
			gi.G2API_RemoveGhoul2Model( self->ghoul2, self->weaponModel );
			self->weaponModel = -1;
		}
	}

	self->s.powerups = 0;
	//FIXME: do this on a callback?  So people can't walk through long death anims?
	//Maybe set on last frame?  Would be cool for big blocking corpses if the never got set?
	//self->contents = CONTENTS_CORPSE;//now done a second after death
	/*
	self->takedamage = qfalse;	// no gibbing
	if ( self->client->playerTeam == TEAM_PARASITE )
	{
		self->contents = CONTENTS_NONE; // FIXME: temp fix
	}
	else
	{
		self->contents = CONTENTS_CORPSE;
		self->maxs[2] = -8;
	}
	*/
	if ( !self->s.number )
	{//player
		self->contents = CONTENTS_CORPSE;
		self->maxs[2] = -8;
	}
	self->clipmask&=~(CONTENTS_MONSTERCLIP|CONTENTS_BOTCLIP);//so dead NPC can fly off ledges
	
	//FACING==========================================================
	if ( attacker && self->s.number == 0 )
	{
		LookAtKiller( self, inflictor, attacker );
	}
	self->currentAngles[PITCH] = 0; 
	self->currentAngles[ROLL] = 0;
	if ( self->NPC )
	{
		self->NPC->desiredYaw = 0; 
		self->NPC->desiredPitch = 0;
		self->NPC->confusionTime = 0;
		self->NPC->charmedTime = 0;
	}
	VectorCopy( self->currentAngles, self->client->ps.viewangles );
	//FACING==========================================================
	if ( player && player->client && player->client->ps.viewEntity == self->s.number )
	{//I was the player's viewentity and I died, kick him back to his normal view
		G_ClearViewEntity( player );
	}

	self->s.loopSound = 0;

	// remove powerups
	memset( self->client->ps.powerups, 0, sizeof(self->client->ps.powerups) );

	if ( self->client->NPC_class == CLASS_GALAKMECH )
	{//FIXME: need keyframed explosions?
		NPC_SetAnim( self, SETANIM_BOTH, BOTH_DEATH1, SETANIM_FLAG_OVERRIDE|SETANIM_FLAG_HOLD );
		G_AddEvent( self, Q_irand(EV_DEATH1, EV_DEATH3), self->health );
	}
	else if ( self->client->NPC_class == CLASS_ATST )
	{//FIXME: need keyframed explosions
		if ( !self->s.number )
		{
			G_DrivableATSTDie( self );
		}
		anim = PM_PickAnim( self, BOTH_DEATH1, BOTH_DEATH19 );	//initialize to good data
		if ( anim != -1 )
		{
			NPC_SetAnim( self, SETANIM_BOTH, anim, SETANIM_FLAG_OVERRIDE|SETANIM_FLAG_HOLD );
		}
	}
	else if ( self->s.number && self->message && meansOfDeath != MOD_SNIPER )
	{//imp with a key on his arm
		//pick a death anim that leaves key visible
		switch ( Q_irand( 0, 3 ) )
		{
		case 0:
			anim = BOTH_DEATH4;
			break;
		case 1:
			anim = BOTH_DEATH15;
			break;
		case 2:
			anim = BOTH_DEATH17;
			break;
		case 3:
		default:
			anim = BOTH_DEATH18;
			break;
		}
		//FIXME: verify we have this anim?
		NPC_SetAnim( self, SETANIM_BOTH, anim, SETANIM_FLAG_OVERRIDE|SETANIM_FLAG_HOLD );
		if ( meansOfDeath == MOD_KNOCKOUT || meansOfDeath == MOD_MELEE )
		{
			G_AddEvent( self, EV_JUMP, 0 );
		}
		else
		{
			G_AddEvent( self, Q_irand(EV_DEATH1, EV_DEATH3), self->health );
		}
	}
	else if ( meansOfDeath == MOD_FALLING || (self->client->ps.legsAnim == BOTH_FALLDEATH1INAIR && self->client->ps.torsoAnim == BOTH_FALLDEATH1INAIR) || (self->client->ps.legsAnim == BOTH_FALLDEATH1 && self->client->ps.torsoAnim == BOTH_FALLDEATH1) )
	{
		//FIXME: no good way to predict you're going to fall to your death... need falling bushes/triggers?
		if ( self->client->ps.groundEntityNum == ENTITYNUM_NONE //in the air
			&& self->client->ps.velocity[2] < 0 //falling
			&& self->client->ps.legsAnim != BOTH_FALLDEATH1INAIR //not already in falling loop
			&& self->client->ps.torsoAnim != BOTH_FALLDEATH1INAIR )//not already in falling loop
		{
			NPC_SetAnim(self, SETANIM_BOTH, BOTH_FALLDEATH1INAIR, SETANIM_FLAG_OVERRIDE|SETANIM_FLAG_HOLD);
			if ( !self->NPC )
			{
				G_SoundOnEnt( self, CHAN_VOICE, "*falling1.wav" );//CHAN_VOICE_ATTEN
			}
			else if (!(self->NPC->aiFlags&NPCAI_DIE_ON_IMPACT) )
			{
				G_SoundOnEnt( self, CHAN_VOICE, "*falling1.wav" );//CHAN_VOICE_ATTEN
				//so we don't do this again
				self->NPC->aiFlags |= NPCAI_DIE_ON_IMPACT;
				//self->client->ps.gravity *= 0.5;//Fall a bit slower
				self->client->ps.friction = 1;
			}
		}
		else
		{
			int	deathAnim = BOTH_FALLDEATH1LAND;
			if ( PM_InOnGroundAnim( &self->client->ps ) )
			{
				if ( AngleNormalize180(self->client->renderInfo.torsoAngles[PITCH]) < 0 )
				{
					deathAnim = BOTH_DEATH_LYING_UP;	//# Death anim when lying on back
				}
				else
				{
					deathAnim = BOTH_DEATH_LYING_DN;	//# Death anim when lying on front
				}
			}
			else if ( PM_InKnockDown( &self->client->ps ) )
			{
				if ( AngleNormalize180(self->client->renderInfo.torsoAngles[PITCH]) < 0 )
				{
					deathAnim = BOTH_DEATH_FALLING_UP;	//# Death anim when falling on back
				}
				else
				{
					deathAnim = BOTH_DEATH_FALLING_DN;	//# Death anim when falling on face
				}
			}
			NPC_SetAnim(self, SETANIM_BOTH, deathAnim, SETANIM_FLAG_OVERRIDE|SETANIM_FLAG_HOLD);
			G_SoundOnEnt( self, CHAN_BODY, "sound/player/fallsplat.wav" );
			if ( gi.VoiceVolume[self->s.number]
				&& self->NPC && (self->NPC->aiFlags&NPCAI_DIE_ON_IMPACT) )
			{//I was talking, so cut it off... with a jump sound?
				G_SoundOnEnt( self, CHAN_VOICE_ATTEN, "*pain100.wav" );
			}
		}
	}
	else
	{// normal death
		qboolean	cliff_fall = qfalse;
		anim = G_CheckSpecialDeathAnim( self, self->pos1, damage, meansOfDeath, hitLoc );
		if ( anim == -1 )
		{
			if ( self->s.legsAnim == BOTH_RESTRAINED1 || self->s.legsAnim == BOTH_RESTRAINED1POINT )
			{//super special case, floating in air lying down
				anim = BOTH_RESTRAINED1;
			}
			else if ( PM_InOnGroundAnim( &self->client->ps ) && PM_HasAnimation( self, BOTH_LYINGDEATH1 ) )
			{//on ground, need different death anim
				anim = BOTH_LYINGDEATH1;
			}
			else if ( meansOfDeath != MOD_SNIPER )
			{
				//		Intelligent Ledge-Diving Deaths:
				//		If I'm an NPC, check for nearby ledges and fall off it if possible
				//		How should/would/could this interact with knockback if we already have some?
				//		Ideally - apply knockback if there are no ledges or a ledge in that dir
				//		But if there is a ledge and it's not in the dir of my knockback, fall off the ledge instead
				vec3_t	fallForwardDir, fallRightDir;
				vec3_t	angles = {0};

				if ( !VectorCompare( self->client->ps.velocity, vec3_origin ) )
				{//already moving in a dir
					angles[1] = vectoyaw( self->client->ps.velocity );
					AngleVectors( angles, fallForwardDir, fallRightDir, NULL );
				}
				else
				{//try forward first
					angles[1] = self->client->ps.viewangles[1];
					AngleVectors( angles, fallForwardDir, fallRightDir, NULL );
				}
				VectorNormalize( fallForwardDir );
				float fallDist = G_CheckForLedge( self, fallForwardDir );
				if ( fallDist >= 128 )
				{
					VectorClear( self->client->ps.velocity );
					G_Throw( self, fallForwardDir, 85 );
					self->client->ps.velocity[2] = 100;
					self->client->ps.groundEntityNum = ENTITYNUM_NONE;
					if ( fallDist >= 256 )
					{
						cliff_fall = qtrue;
					}
				}
				else 
				{
					VectorScale( fallForwardDir, -1, fallForwardDir );
					fallDist = G_CheckForLedge( self, fallForwardDir );
					if ( fallDist >= 128 )
					{
						VectorClear( self->client->ps.velocity );
						G_Throw( self, fallForwardDir, 85 );
						self->client->ps.velocity[2] = 100;
						self->client->ps.groundEntityNum = ENTITYNUM_NONE;
						if ( fallDist >= 256 )
						{
							cliff_fall = qtrue;
						}
					}
					else
					{//try sides
						/*
						VectorNormalize( fallRightDir );
						if ( G_CheckForLedge( self, fallRightDir ) )
						{
							VectorClear( self->client->ps.velocity );
							G_Throw( self, fallRightDir, 85 );
							self->client->ps.velocity[2] = 100;
							cliff_fall = qtrue;
						}
						else 
						{
							VectorScale( fallRightDir, -1, fallRightDir );
							if ( G_CheckForLedge( self, fallRightDir ) )
							{
								VectorClear( self->client->ps.velocity );
								G_Throw( self, fallRightDir, 85 );
								self->client->ps.velocity[2] = 100;
								cliff_fall = qtrue;
							}
						}
						*/
					}
				}
				if ( cliff_fall )
				{
					if ( !FlyingCreature( self ) && g_gravity->value > 0 )
					{
						if ( !self->NPC )
						{
							G_SoundOnEnt( self, CHAN_VOICE, "*falling1.wav" );//CHAN_VOICE_ATTEN
						}
						else if (!(self->NPC->aiFlags&NPCAI_DIE_ON_IMPACT) )
						{
							G_SoundOnEnt( self, CHAN_VOICE, "*falling1.wav" );//CHAN_VOICE_ATTEN
							self->NPC->aiFlags |= NPCAI_DIE_ON_IMPACT;
							self->client->ps.friction = 0;
						}
					}
				}
				if ( self->client->ps.pm_time > 0 && self->client->ps.pm_flags & PMF_TIME_KNOCKBACK && self->client->ps.velocity[2] > 0 )
				{
					float	thrown, dot;
					vec3_t	throwdir, forward;
					
					AngleVectors(self->currentAngles, forward, NULL, NULL);
					thrown = VectorNormalize2(self->client->ps.velocity, throwdir);
					dot = DotProduct(forward, throwdir);
					if(thrown > 100) 
					{
						if ( dot > 0.3 )
						{//falling forward
							if ( cliff_fall && PM_HasAnimation( self, BOTH_FALLDEATH1 ) )
							{
								anim = BOTH_FALLDEATH1;
							}
							else if ( PM_HasAnimation( self, BOTH_DEATHFORWARD1 ))
							{
								self->client->ps.gravity *= 0.8;
								self->client->ps.friction = 0;
								if ( PM_HasAnimation( self, BOTH_DEATHFORWARD2) ) 
								{
									anim = Q_irand(BOTH_DEATHFORWARD1, BOTH_DEATHFORWARD2);//okay, i assume he has 2 since he has 1
								} 
								else 
								{
									anim = BOTH_DEATHFORWARD1;
								}
							}
						}
						else if ( dot < -0.3 && PM_HasAnimation( self, BOTH_DEATHBACKWARD1 )) 
						{
							self->client->ps.gravity *= 0.8;
							self->client->ps.friction = 0;
							if ( PM_HasAnimation( self, BOTH_DEATHBACKWARD2) && !Q_irand( 0, 3 ) ) 
							{
								anim = Q_irand(BOTH_DEATHBACKWARD1, BOTH_DEATHBACKWARD2);
							} 
							else 
							{
								anim = BOTH_DEATHBACKWARD1;
							}
						}
						else
						{//falling to one of the sides
							if ( cliff_fall && PM_HasAnimation( self, BOTH_FALLDEATH1 ) )
							{
								anim = BOTH_FALLDEATH1;
							}
						}
					}
				}
			}
		}

		if ( anim == -1 )
		{
			if ( meansOfDeath == MOD_ELECTROCUTE )
			{
				anim = BOTH_DEATH17;
			}
			else
			{
				anim = G_PickDeathAnim( self, self->pos1, damage, meansOfDeath, hitLoc );
			}
		}
		if ( anim == -1 )
		{
			anim = PM_PickAnim( self, BOTH_DEATH1, BOTH_DEATH19 );	//initialize to good data
			//TEMP HACK: these spinny deaths should happen less often
			if ( ( anim == BOTH_DEATH8 || anim == BOTH_DEATH14 ) && Q_irand( 0, 1 ) )
			{
				anim = PM_PickAnim( self, BOTH_DEATH1, BOTH_DEATH19 );	//initialize to good data
			}
		}


		if ( meansOfDeath == MOD_KNOCKOUT )
		{
			//FIXME: knock-out sound, and don't remove me
			G_AddEvent( self, EV_JUMP, 0 );
			G_UseTargets2( self, self, self->target2 );
			G_AlertTeam( self, attacker, 512, 32 );
			if ( self->NPC )
			{//stick around for a while
				self->NPC->timeOfDeath = level.time + 10000;
			}
		}
		
		else if ( meansOfDeath == MOD_SNIPER )
		{
			gentity_t	*tent;
			vec3_t		spot;

			VectorCopy( self->currentOrigin, spot );
			
			self->flags |= FL_DISINTEGRATED;
			tent = G_TempEntity( spot, EV_DISINTEGRATION );
			tent->s.eventParm = PW_DISRUPTION;
			tent->owner = self;
	
			G_AlertTeam( self, attacker, 512, 88 );

			if ( self->playerModel >= 0 )
			{
				// don't let 'em animate
				gi.G2API_PauseBoneAnimIndex( &self->ghoul2[self->playerModel], self->rootBone, cg.time );
				gi.G2API_PauseBoneAnimIndex( &self->ghoul2[self->playerModel], self->motionBone, cg.time );
				gi.G2API_PauseBoneAnimIndex( &self->ghoul2[self->playerModel], self->lowerLumbarBone, cg.time );
				anim = -1;
			}

			//not solid anymore
			self->contents = 0;
			self->maxs[2] = -8;

			if ( self->NPC )
			{
				//need to pad deathtime some to stick around long enough for death effect to play
				self->NPC->timeOfDeath = level.time + 2000;
			}
		}
		else 
		{
			if ( hitLoc == HL_HEAD )
			{//no sound when killed by headshot
				G_AlertTeam( self, attacker, 512, 0 );
				if ( gi.VoiceVolume[self->s.number] )
				{//I was talking, so cut it off... with a jump sound?
					G_SoundOnEnt( self, CHAN_VOICE, "*jump1.wav" );
				}
			}
			else 
			{
				if ( !cliff_fall )
				{
					if ( meansOfDeath == MOD_KNOCKOUT || meansOfDeath == MOD_MELEE )
					{
						G_AddEvent( self, EV_JUMP, 0 );
					}
					else
					{
						G_AddEvent( self, Q_irand(EV_DEATH1, EV_DEATH3), self->health );
					}
				}
				G_DeathAlert( self, attacker );
			}
		}

		if ( attacker && attacker->s.number == 0 )
		{//killed by player
			//FIXME: this should really be wherever my body comes to rest...
			AddSightEvent( attacker, self->currentOrigin, 384, AEL_DISCOVERED, 10 );
			//FIXME: danger event so that others will run away from this area since it's obviously dangerous
		}

		if ( anim != -1 )
		{
			NPC_SetAnim(self, SETANIM_BOTH, anim, SETANIM_FLAG_OVERRIDE|SETANIM_FLAG_HOLD);
		}
	}

	//do any dismemberment if there's any to do...
	G_DoDismemberment( self, self->pos1, meansOfDeath, damage, hitLoc );

	// don't allow player to respawn for a few seconds
	self->client->respawnTime = level.time + 2000;//self->client->ps.legsAnimTimer;

	//lock it to this anim forever
	if ( self->client )
	{
		PM_SetLegsAnimTimer( self, &self->client->ps.legsAnimTimer, -1 );
		PM_SetTorsoAnimTimer( self, &self->client->ps.torsoAnimTimer, -1 );
	}

	//Flying creatures should drop when killed
	//FIXME: This may screw up certain things that expect to float even while dead <?>
	self->svFlags &= ~SVF_CUSTOM_GRAVITY;

	self->client->ps.pm_type = PM_DEAD;
	//need to update STAT_HEALTH here because ClientThink_real for self may happen before STAT_HEALTH is updated from self->health and pmove will stomp death anim with a move anim
	self->client->ps.stats[STAT_HEALTH] = self->health;

	if ( self->NPC )
	{//If an NPC, make sure we start running our scripts again- this gets set to infinite while we fall to our deaths
		self->NPC->nextBStateThink = level.time;
	}

	if ( G_ActivateBehavior( self, BSET_DEATH ) )
	{
		deathScript = qtrue;
	}
	
	if ( self->NPC && (self->NPC->scriptFlags&SCF_FFDEATH) && G_ActivateBehavior( self, BSET_FFDEATH ) )
	{//FIXME: should running this preclude running the normal deathscript?
		deathScript = qtrue;
	}
	
	if ( !deathScript && !(self->svFlags&SVF_KILLED_SELF) )
	{
		//Should no longer run scripts
		//WARNING!!! DO NOT DO THIS WHILE RUNNING A SCRIPT, ICARUS WILL CRASH!!!
		//FIXME: shouldn't ICARUS handle this internally?
		ICARUS_FreeEnt(self);
	}

	// Free up any timers we may have on us.
	TIMER_Clear( self );

	// Set pending objectives to failed
	OBJ_SetPendingObjectives(self);
	
	gi.linkentity (self);

	self->bounceCount = -1; // This is a cheap hack for optimizing the pointcontents check in deadthink
	if ( self->NPC )
	{
		self->NPC->timeOfDeath = level.time;//this will change - used for debouncing post-death events
		self->s.time = level.time;//this will not chage- this is actual time of death
	}

	// Start any necessary death fx for this entity
	DeathFX( self );
}

qboolean G_CheckForStrongAttackMomentum( gentity_t *self )
{//see if our saber attack has too much momentum to be interrupted
	if ( PM_PowerLevelForSaberAnim( &self->client->ps ) > FORCE_LEVEL_2 )
	{//strong attacks can't be interrupted
		if ( PM_InAnimForSaberMove( self->client->ps.torsoAnim, self->client->ps.saberMove ) )
		{//our saberMove was not already interupted by some other anim (like pain)
			if ( PM_SaberInStart( self->client->ps.saberMove ) )
			{
				float animLength = PM_AnimLength( self->client->clientInfo.animFileIndex, (animNumber_t)self->client->ps.torsoAnim );
				if ( animLength - self->client->ps.torsoAnimTimer > 750 )
				{//start anim is already 3/4 of a second into it, can't interrupt it now
					return qtrue;
				}
			}
			else if ( PM_SaberInReturn( self->client->ps.saberMove ) )
			{
				if ( self->client->ps.torsoAnimTimer > 750 )
				{//still have a good amount of time left in the return anim, can't interrupt it
					return qtrue;
				}
			}
			else
			{//cannot interrupt actual transitions and attacks
				return qtrue;
			}
		}
	}
	return qfalse;
}

void PlayerPain( gentity_t *self, gentity_t *inflictor, gentity_t *other, vec3_t point, int damage, int mod, int hitLoc )
{
	if ( self->client->NPC_class == CLASS_ATST )
	{//different kind of pain checking altogether
		G_ATSTCheckPain( self, other, point, damage, mod, hitLoc );
		int blasterTest = gi.G2API_GetSurfaceRenderStatus( &self->ghoul2[self->playerModel], "head_light_blaster_cann" );
		int chargerTest = gi.G2API_GetSurfaceRenderStatus( &self->ghoul2[self->playerModel], "head_concussion_charger" );
		if ( blasterTest && chargerTest )
		{//lost both side guns
			//take away that weapon
			self->client->ps.stats[STAT_WEAPONS] &= ~( 1 << WP_ATST_SIDE );
			//switch to primary guns
			if ( self->client->ps.weapon == WP_ATST_SIDE )
			{
				CG_ChangeWeapon( WP_ATST_MAIN );
			}
		}
	}
	else
	{
		// play an apropriate pain sound
		if ( level.time > self->painDebounceTime && !(self->flags & FL_GODMODE) ) 
		{//first time hit this frame and not in godmode
			self->client->ps.damageEvent++;
			if ( !Q3_TaskIDPending( self, TID_CHAN_VOICE ) )
			{
				if ( self->client->damage_blood )
				{//took damage myself, not just armor
					G_AddEvent( self, EV_PAIN, self->health );
				}
			}
		}
		if ( damage != -1 && (mod==MOD_MELEE || damage==0/*fake damage*/ || (Q_irand( 0, 10 ) <= damage && self->client->damage_blood)) )
		{//-1 == don't play pain anim
			if ( ( ((mod==MOD_SABER||mod==MOD_MELEE)&&self->client->damage_blood) || mod == MOD_CRUSH ) && (self->s.weapon == WP_SABER||self->s.weapon==WP_MELEE) )//FIXME: not only if using saber, but if in third person at all?  But then 1st/third person functionality is different...
			{//FIXME: only strong-level saber attacks should make me play pain anim?
				if ( !G_CheckForStrongAttackMomentum( self ) && !PM_SpinningSaberAnim( self->client->ps.legsAnim )
					&& !PM_SaberInSpecialAttack( self->client->ps.torsoAnim )
					&& !PM_InKnockDown( &self->client->ps ) )
				{//strong attacks and spins cannot be interrupted by pain, no pain when in knockdown
					int	parts = SETANIM_BOTH;
					if ( self->client->ps.groundEntityNum != ENTITYNUM_NONE &&
						!PM_SpinningSaberAnim( self->client->ps.legsAnim ) &&
						!PM_FlippingAnim( self->client->ps.legsAnim ) &&
						!PM_InSpecialJump( self->client->ps.legsAnim ) &&
						!PM_RollingAnim( self->client->ps.legsAnim )&&
						!PM_CrouchAnim( self->client->ps.legsAnim )&&
						!PM_RunningAnim( self->client->ps.legsAnim ))
					{//if on a surface and not in a spin or flip, play full body pain
						parts = SETANIM_BOTH;
					}
					else
					{//play pain just in torso
						parts = SETANIM_TORSO;
					}
					if ( self->painDebounceTime < level.time )
					{
						//temp HACK: these are the only 2 pain anims that look good when holding a saber
						NPC_SetAnim( self, parts, PM_PickAnim( self, BOTH_PAIN2, BOTH_PAIN3 ), SETANIM_FLAG_OVERRIDE|SETANIM_FLAG_HOLD );
						self->client->ps.saberMove = LS_READY;//don't finish whatever saber move you may have been in
						//WTF - insn't working
						if ( self->health < 10 && d_slowmodeath->integer > 5 )
						{
							G_StartMatrixEffect( self );
						}
					}
					if ( parts == SETANIM_BOTH && damage > 30 || (self->painDebounceTime>level.time&&damage>10))
					{//took a lot of damage in 1 hit //or took 2 hits in quick succession
						self->aimDebounceTime = level.time + self->client->ps.torsoAnimTimer;
						self->client->ps.pm_time = self->client->ps.torsoAnimTimer; 
						self->client->ps.pm_flags |= PMF_TIME_KNOCKBACK;
					}
					self->client->ps.weaponTime = self->client->ps.torsoAnimTimer;
					self->attackDebounceTime = level.time + self->client->ps.torsoAnimTimer;
				}
				self->painDebounceTime = level.time + self->client->ps.torsoAnimTimer;
			}
		}
	}
	if ( self->painDebounceTime <= level.time )
	{
		self->painDebounceTime = level.time + 700;
	}
}
/*
================
CheckArmor
================
*/
int CheckArmor (gentity_t *ent, int damage, int dflags)
{
	gclient_t	*client;
	int			save;
	int			count;

	if (!damage)
		return 0;

	client = ent->client;

	if (!client)
		return 0;

	if ( (dflags&DAMAGE_NO_ARMOR) )
		return 0;

	if ( client->NPC_class == CLASS_GALAKMECH )
	{//special case
		if ( client->ps.stats[STAT_ARMOR] <= 0 )
		{//no shields
			client->ps.powerups[PW_GALAK_SHIELD] = 0;
			return 0;
		}
		else
		{//shields take all the damage
			client->ps.stats[STAT_ARMOR] -= damage;
			if ( client->ps.stats[STAT_ARMOR] <= 0 )
			{
				client->ps.powerups[PW_GALAK_SHIELD] = 0;
				client->ps.stats[STAT_ARMOR] = 0;
			}
			return damage;
		}
	}
	else
	{
		// armor
		count = client->ps.stats[STAT_ARMOR];

		// No damage to entity until armor is at less than 50% strength
		if (count > (client->ps.stats[STAT_MAX_HEALTH]/2)) // MAX_HEALTH is considered max armor. Or so I'm told.
		{
			save = damage;
		}
		else
		{
			if ( !ent->s.number && client->NPC_class == CLASS_ATST )
			{//player in ATST... armor takes *all* the damage
				save = damage;
			}
			else
			{
				save = ceil( (float) damage * ARMOR_PROTECTION );
			}
		}

		//Always round up
		if (damage == 1)
		{
			if ( client->ps.stats[STAT_ARMOR] > 0 ) 
				client->ps.stats[STAT_ARMOR] -= save;

			return 0;
		}

		if (save >= count)
			save = count;

		if (!save)
			return 0;

		client->ps.stats[STAT_ARMOR] -= save;

		return save;
	}
}

void G_ApplyKnockback( gentity_t *targ, vec3_t newDir, float knockback )
{
	vec3_t	kvel;
	float	mass;

	//--- TEMP TEST
	if ( newDir[2] <= 0.0f )
	{

		newDir[2] += (( 0.0f - newDir[2] ) * 1.2f );
	}

	knockback *= 2.0f;

	if ( knockback > 120 )
	{
		knockback = 120;
	}
	//--- TEMP TEST

	if ( targ->physicsBounce > 0 )	//overide the mass
		mass = targ->physicsBounce;
	else
		mass = 200;

	if ( g_gravity->value > 0 )
	{
		VectorScale( newDir, g_knockback->value * (float)knockback / mass * 0.8, kvel );
		kvel[2] = newDir[2] * ( g_knockback->value * (float)knockback ) / ( mass * 1.5 ) + 20;
	}
	else
	{
		VectorScale( newDir, g_knockback->value * (float)knockback / mass, kvel );
	}

	if ( targ->client )
	{
		VectorAdd( targ->client->ps.velocity, kvel, targ->client->ps.velocity );
	}
	else if ( targ->s.pos.trType != TR_STATIONARY && targ->s.pos.trType != TR_LINEAR_STOP && targ->s.pos.trType != TR_NONLINEAR_STOP )
	{
		VectorAdd( targ->s.pos.trDelta, kvel, targ->s.pos.trDelta );
		VectorCopy( targ->currentOrigin, targ->s.pos.trBase );
		targ->s.pos.trTime = level.time;
	}

	// set the timer so that the other client can't cancel
	// out the movement immediately
	if ( targ->client && !targ->client->ps.pm_time ) 
	{
		int		t;

		t = knockback * 2;
		if ( t < 50 ) {
			t = 50;
		}
		if ( t > 200 ) {
			t = 200;
		}
		targ->client->ps.pm_time = t;
		targ->client->ps.pm_flags |= PMF_TIME_KNOCKBACK;
	}
}

int G_CheckForLedge( gentity_t *self, vec3_t fallCheckDir )
{
	vec3_t	start, end;
	trace_t	tr;

	VectorMA( self->currentOrigin, 128, fallCheckDir, end );
	//Should have clip burshes masked out by now and have bbox resized to death size
	gi.trace( &tr, self->currentOrigin, self->mins, self->maxs, end, self->s.number, self->clipmask );
	if ( tr.allsolid || tr.startsolid )
	{
		return 0;
	}
	VectorCopy( tr.endpos, start );
	VectorCopy( start, end );
	end[2] -= 256;

	gi.trace( &tr, start, self->mins, self->maxs, end, self->s.number, self->clipmask );
	if ( tr.allsolid || tr.startsolid )
	{
		return 0;
	}
	if ( tr.fraction >= 1.0 )
	{
		return (start[2]-tr.endpos[2]);
	}
	return 0;
}

qboolean G_GetHitLocFromSurfName( gentity_t *ent, const char *surfName, int *hitLoc, vec3_t point, vec3_t dir, vec3_t bladeDir )
{
	qboolean dismember = qfalse;

	*hitLoc = HL_NONE;

	if ( !surfName || !surfName[0] )
	{
		return qfalse;
	}


	if ( ent->client && (ent->client->NPC_class == CLASS_ATST) )
	{
		//FIXME: almost impossible to hit these... perhaps we should
		//		check for splashDamage and do radius damage to these parts?
		//		Or, if we ever get bbox G2 traces, that may fix it, too
		if (!Q_stricmp("head_light_blaster_cann",surfName))
		{
			*hitLoc = HL_ARM_LT;
		}
		else if (!Q_stricmp("head_concussion_charger",surfName))
		{
			*hitLoc = HL_ARM_RT;
		}
		return(qfalse);
	}
	else if ( ent->client && (ent->client->NPC_class == CLASS_MARK1) )
	{
		if (!Q_stricmp("l_arm",surfName))
		{
			*hitLoc = HL_ARM_LT;
		}
		else if (!Q_stricmp("r_arm",surfName))
		{
			*hitLoc = HL_ARM_RT;
		}
		else if (!Q_stricmp("torso_front",surfName))
		{
			*hitLoc = HL_CHEST;
		}
		else if (!Q_stricmp("torso_tube1",surfName))
		{
			*hitLoc = HL_GENERIC1;
		}
		else if (!Q_stricmp("torso_tube2",surfName))
		{
			*hitLoc = HL_GENERIC2;
		}
		else if (!Q_stricmp("torso_tube3",surfName))
		{
			*hitLoc = HL_GENERIC3;
		}
		else if (!Q_stricmp("torso_tube4",surfName))
		{
			*hitLoc = HL_GENERIC4;
		}
		else if (!Q_stricmp("torso_tube5",surfName))
		{
			*hitLoc = HL_GENERIC5;
		}
		else if (!Q_stricmp("torso_tube6",surfName))
		{
			*hitLoc = HL_GENERIC6;
		}
		return(qfalse);
	}
	else if ( ent->client && (ent->client->NPC_class == CLASS_MARK2) )
	{
		if (!Q_stricmp("torso_canister1",surfName))
		{
			*hitLoc = HL_GENERIC1;
		}
		else if (!Q_stricmp("torso_canister2",surfName))
		{
			*hitLoc = HL_GENERIC2;
		}
		else if (!Q_stricmp("torso_canister3",surfName))
		{
			*hitLoc = HL_GENERIC3;
		}
		return(qfalse);
	}
	else if ( ent->client && (ent->client->NPC_class == CLASS_GALAKMECH) )
	{
		if (!Q_stricmp("torso_antenna",surfName)||!Q_stricmp("torso_antenna_base",surfName))
		{
			*hitLoc = HL_GENERIC1;
		}
		else if (!Q_stricmp("torso_shield",surfName))
		{
			*hitLoc = HL_GENERIC2;
		}
		else
		{
			*hitLoc = HL_CHEST;
		}
		return(qfalse);
	}

	//FIXME: check the hitLoc and hitDir against the cap tag for the place 
	//where the split will be- if the hit dir is roughly perpendicular to 
	//the direction of the cap, then the split is allowed, otherwise we
	//hit it at the wrong angle and should not dismember...
	int	actualTime = (cg.time?cg.time:level.time);
	if ( !Q_strncmp( "hips", surfName, 4 ) )
	{//FIXME: test properly for legs
		*hitLoc = HL_WAIST;
		if ( ent->client != NULL && ent->ghoul2.size() )
		{
			mdxaBone_t	boltMatrix;
			vec3_t	tagOrg, angles;

			VectorSet( angles, 0, ent->currentAngles[YAW], 0 );
			gi.G2API_GetBoltMatrix( ent->ghoul2, ent->playerModel, ent->kneeLBolt, 
							&boltMatrix, angles, ent->currentOrigin,
							actualTime, NULL, ent->s.modelScale );
			gi.G2API_GiveMeVectorFromMatrix( boltMatrix, ORIGIN, tagOrg );
			if ( DistanceSquared( point, tagOrg ) < 100 )
			{//actually hit the knee
				*hitLoc = HL_LEG_LT;
			}
			else
			{
				gi.G2API_GetBoltMatrix( ent->ghoul2, ent->playerModel, ent->kneeRBolt, 
								&boltMatrix, angles, ent->currentOrigin,
								actualTime, NULL, ent->s.modelScale );
				gi.G2API_GiveMeVectorFromMatrix( boltMatrix, ORIGIN, tagOrg );
				if ( DistanceSquared( point, tagOrg ) < 100 )
				{//actually hit the knee
					*hitLoc = HL_LEG_RT;
				}
			}
		}
	}
	else if ( !Q_strncmp( "torso", surfName, 5 ) )
	{
		if ( !ent->client )
		{
			*hitLoc = HL_CHEST;
		}
		else
		{
			vec3_t	t_fwd, t_rt, t_up, dirToImpact;
			float frontSide, rightSide, upSide;
			AngleVectors( ent->client->renderInfo.torsoAngles, t_fwd, t_rt, t_up );
			VectorSubtract( point, ent->client->renderInfo.torsoPoint, dirToImpact );
			frontSide = DotProduct( t_fwd, dirToImpact );
			rightSide = DotProduct( t_rt, dirToImpact );
			upSide = DotProduct( t_up, dirToImpact );
			if ( upSide < 0 )
			{//hit at waist
				*hitLoc = HL_WAIST;
			}
			else
			{//hit on upper torso
				if ( rightSide > 10 )
				{
					*hitLoc = HL_ARM_RT;
				}
				else if ( rightSide < -10 )
				{
					*hitLoc = HL_ARM_LT;
				}
				else if ( rightSide > 4 )
				{
					if ( frontSide > 0 )
					{
						*hitLoc = HL_CHEST_RT;
					}
					else
					{
						*hitLoc = HL_BACK_RT;
					}
				}
				else if ( rightSide < -4 )
				{
					if ( frontSide > 0 )
					{
						*hitLoc = HL_CHEST_LT;
					}
					else
					{
						*hitLoc = HL_BACK_LT;
					}
				}
				else if ( upSide > 6 )
				{
					*hitLoc = HL_HEAD;
				}
				else if ( frontSide > 0 )
				{
					*hitLoc = HL_CHEST;
				}
				else
				{
					*hitLoc = HL_BACK;
				}
			}
		}
	}
	else if ( !Q_strncmp( "head", surfName, 4 ) )
	{
		*hitLoc = HL_HEAD;
	}
	else if ( !Q_strncmp( "r_arm", surfName, 5 ) )
	{
		*hitLoc = HL_ARM_RT;
		if ( ent->client != NULL && ent->ghoul2.size() )
		{
			mdxaBone_t	boltMatrix;
			vec3_t	tagOrg, angles;

			VectorSet( angles, 0, ent->currentAngles[YAW], 0 );
			gi.G2API_GetBoltMatrix( ent->ghoul2, ent->playerModel, ent->handRBolt, 
							&boltMatrix, angles, ent->currentOrigin,
							actualTime, NULL, ent->s.modelScale );
			gi.G2API_GiveMeVectorFromMatrix( boltMatrix, ORIGIN, tagOrg );
			if ( DistanceSquared( point, tagOrg ) < 256 )
			{//actually hit the hand
				*hitLoc = HL_HAND_RT;
			}
		}
	}
	else if ( !Q_strncmp( "l_arm", surfName, 5 ) )
	{
		*hitLoc = HL_ARM_LT;
		if ( ent->client != NULL && ent->ghoul2.size() )
		{
			mdxaBone_t	boltMatrix;
			vec3_t	tagOrg, angles;

			VectorSet( angles, 0, ent->currentAngles[YAW], 0 );
			gi.G2API_GetBoltMatrix( ent->ghoul2, ent->playerModel, ent->handLBolt, 
							&boltMatrix, angles, ent->currentOrigin,
							actualTime, NULL, ent->s.modelScale );
			gi.G2API_GiveMeVectorFromMatrix( boltMatrix, ORIGIN, tagOrg );
			if ( DistanceSquared( point, tagOrg ) < 256 )
			{//actually hit the hand
				*hitLoc = HL_HAND_LT;
			}
		}
	}
	else if ( !Q_strncmp( "r_leg", surfName, 5 ) )
	{
		*hitLoc = HL_LEG_RT;
		if ( ent->client != NULL && ent->ghoul2.size() )
		{
			mdxaBone_t	boltMatrix;
			vec3_t	tagOrg, angles;

			VectorSet( angles, 0, ent->currentAngles[YAW], 0 );
			gi.G2API_GetBoltMatrix( ent->ghoul2, ent->playerModel, ent->footRBolt, 
							&boltMatrix, angles, ent->currentOrigin,
							actualTime, NULL, ent->s.modelScale );
			gi.G2API_GiveMeVectorFromMatrix( boltMatrix, ORIGIN, tagOrg );
			if ( DistanceSquared( point, tagOrg ) < 100 )
			{//actually hit the foot
				*hitLoc = HL_FOOT_RT;
			}
		}
	}
	else if ( !Q_strncmp( "l_leg", surfName, 5 ) )
	{
		*hitLoc = HL_LEG_LT;
		if ( ent->client != NULL && ent->ghoul2.size() )
		{
			mdxaBone_t	boltMatrix;
			vec3_t	tagOrg, angles;

			VectorSet( angles, 0, ent->currentAngles[YAW], 0 );
			gi.G2API_GetBoltMatrix( ent->ghoul2, ent->playerModel, ent->footLBolt, 
							&boltMatrix, angles, ent->currentOrigin,
							actualTime, NULL, ent->s.modelScale );
			gi.G2API_GiveMeVectorFromMatrix( boltMatrix, ORIGIN, tagOrg );
			if ( DistanceSquared( point, tagOrg ) < 100 )
			{//actually hit the foot
				*hitLoc = HL_FOOT_LT;
			}
		}
	}
	else if ( !Q_strncmp( "r_hand", surfName, 6 ) )
	{
		*hitLoc = HL_HAND_RT;
	}
	else if ( !Q_strncmp( "l_hand", surfName, 6 ) )
	{
		*hitLoc = HL_HAND_LT;
	}
	if ( g_realisticSaberDamage->integer )
	{
		dismember = qtrue;
	}
	else
	{
		if ( dir && (dir[0] || dir[1] || dir[2]) &&
			bladeDir && (bladeDir[0] || bladeDir[1] || bladeDir[2]) )
		{//we care about direction (presumably for dismemberment)
			char *tagName = NULL;
			//dir must be roughly perpendicular to the hitLoc's cap bolt
			switch ( *hitLoc )
			{
				case HL_LEG_RT:
					tagName = "*hips_cap_r_leg";
					break;
				case HL_LEG_LT:
					tagName = "*hips_cap_l_leg";
					break;
				case HL_WAIST:
					tagName = "*hips_cap_torso";
					break;
				case HL_CHEST_RT:
				case HL_ARM_RT:
				case HL_BACK_RT:
					tagName = "*torso_cap_r_arm";
					break;
				case HL_CHEST_LT:
				case HL_ARM_LT:
				case HL_BACK_LT:
					tagName = "*torso_cap_l_arm";
					break;
				case HL_HAND_RT:
					tagName = "*r_arm_cap_r_hand";
					break;
				case HL_HAND_LT:
					tagName = "*l_arm_cap_l_hand";
					break;
				case HL_HEAD:
					tagName = "*torso_cap_head";
					break;
				case HL_CHEST:
				case HL_BACK:
				case HL_FOOT_RT:
				case HL_FOOT_LT:
				default:
					//no dismemberment possible with these, so no checks needed
					break;
			}
			if ( tagName )
			{
				int tagBolt = gi.G2API_AddBolt( &ent->ghoul2[ent->playerModel], tagName );
				if ( tagBolt != -1 )
				{
					mdxaBone_t	boltMatrix;
					vec3_t	tagOrg, tagDir, angles;
					VectorSet( angles, 0, ent->currentAngles[YAW], 0 );
					gi.G2API_GetBoltMatrix( ent->ghoul2, ent->playerModel, tagBolt, 
									&boltMatrix, angles, ent->currentOrigin,
									actualTime, NULL, ent->s.modelScale );
					gi.G2API_GiveMeVectorFromMatrix( boltMatrix, ORIGIN, tagOrg );
					gi.G2API_GiveMeVectorFromMatrix( boltMatrix, NEGATIVE_Y, tagDir );
					if ( DistanceSquared( point, tagOrg ) < 256 )
					{//hit close
						float dot = DotProduct( dir, tagDir );
						if ( dot < 0.25 && dot > -0.25 )
						{//hit roughly perpendicular
							dot = DotProduct( bladeDir, tagDir );
							if ( dot < 0.25 && dot > -0.25 )
							{//blade was roughly perpendicular
								dismember = qtrue;
							}
						}
					}
				}
			}
		}
	}

	return dismember;
}

int G_GetHitLocation ( gentity_t *target, vec3_t ppoint )
{
	vec3_t			point, point_dir;
	vec3_t			forward, right, up;
	vec3_t			tangles, tcenter;
	float			tradius;
	float			udot, fdot, rdot;
	int				Vertical, Forward, Lateral;
	int				HitLoc;

//get target forward, right and up
	if(target->client)
	{//ignore player's pitch and roll
		VectorSet(tangles, 0, target->currentAngles[YAW], 0);
	}

	AngleVectors(tangles, forward, right, up);

//get center of target
	VectorAdd(target->absmin, target->absmax, tcenter);
	VectorScale(tcenter, 0.5, tcenter);

//get radius width of target
	tradius = (fabs(target->maxs[0]) + fabs(target->maxs[1]) + fabs(target->mins[0]) + fabs(target->mins[1]))/4;

//get impact point
	if(ppoint && !VectorCompare(ppoint, vec3_origin))
	{
		VectorCopy(ppoint, point);
	}
	else
	{
		return HL_NONE;
	}

/*
//get impact dir
	if(pdir && !VectorCompare(pdir, vec3_origin))
	{
		VectorCopy(pdir, dir);
	}
	else
	{
		return;
	}

//put point at controlled distance from center
	VectorSubtract(point, tcenter, tempvec);
	tempvec[2] = 0;
	hdist = VectorLength(tempvec);

	VectorMA(point, hdist - tradius, dir, point);
	//now a point on the surface of a cylinder with a radius of tradius
*/	
	VectorSubtract(point, tcenter, point_dir);
	VectorNormalize(point_dir);

	//Get bottom to top (Vertical) position index
	udot = DotProduct(up, point_dir);
	if(udot>.800)
		Vertical = 4;
	else if(udot>.400)
		Vertical = 3;
	else if(udot>-.333)
		Vertical = 2;
	else if(udot>-.666)
		Vertical = 1;
	else
		Vertical = 0;

	//Get back to front (Forward) position index
	fdot = DotProduct(forward, point_dir);
	if(fdot>.666)
		Forward = 4;
	else if(fdot>.333)
		Forward = 3;
	else if(fdot>-.333)
		Forward = 2;
	else if(fdot>-.666)
		Forward = 1;
	else
		Forward = 0;

	//Get left to right (Lateral) position index
	rdot = DotProduct(right, point_dir);
	if(rdot>.666)
		Lateral = 4;
	else if(rdot>.333)
		Lateral = 3;
	else if(rdot>-.333)
		Lateral = 2;
	else if(rdot>-.666)
		Lateral = 1;
	else
		Lateral = 0;

	HitLoc = Vertical * 25 + Forward * 5 + Lateral;

	if(HitLoc <= 10)
	{//feet
		if ( rdot > 0 )
		{
			return HL_FOOT_RT;
		}
		else
		{
			return HL_FOOT_LT;
		}
	}
	else if(HitLoc <= 50)
	{//legs
		if ( rdot > 0 )
		{
			return HL_LEG_RT;
		}
		else
		{
			return HL_LEG_LT;
		}
	}
	else if ( HitLoc == 56||HitLoc == 60||HitLoc == 61||HitLoc == 65||HitLoc == 66||HitLoc == 70 )
	{//hands
		if ( rdot > 0 )
		{
			return HL_HAND_RT;
		}
		else
		{
			return HL_HAND_LT;
		}
	}
	else if ( HitLoc == 83||HitLoc == 87||HitLoc == 88||HitLoc == 92||HitLoc == 93||HitLoc == 97 )
	{//arms
		if ( rdot > 0 )
		{
			return HL_ARM_RT;
		}
		else
		{
			return HL_ARM_LT;
		}
	}
	else if((HitLoc >= 107 && HitLoc <= 109)||
		(HitLoc >= 112 && HitLoc <= 114)||
		(HitLoc >= 117 && HitLoc <= 119))
	{//head
		return HL_HEAD;
	}
	else
	{
		if ( udot < 0.3 )
		{
			return HL_WAIST;
		}
		else if ( fdot < 0 )
		{
			if ( rdot > 0.4 )
			{
				return HL_BACK_RT;
			}
			else if ( rdot < -0.4 )
			{
				return HL_BACK_LT;
			}
			else if ( fdot < 0 )
			{
				return HL_BACK;
			}
		}
		else
		{
			if ( rdot > 0.3 )
			{
				return HL_CHEST_RT;
			}
			else if ( rdot < -0.3 )
			{
				return HL_CHEST_LT;
			}
			else if ( fdot < 0 )
			{
				return HL_CHEST;
			}
		}
	}
	return HL_NONE;
}

int G_PickPainAnim( gentity_t *self, vec3_t point, int damage, int hitLoc = HL_NONE )
{
	if ( hitLoc == HL_NONE )
	{
		hitLoc = G_GetHitLocation( self, point );
	}
	switch( hitLoc )
	{
	case HL_FOOT_RT:
		return BOTH_PAIN12;
		//PAIN12 = right foot
		break;
	case HL_FOOT_LT:
		return -1;
		break;
	case HL_LEG_RT:
		if ( !Q_irand( 0, 1 ) )
		{
			return BOTH_PAIN11;
		}
		else
		{
			return BOTH_PAIN13;
		}
		//PAIN11 = twitch right leg
		//PAIN13 = right knee
		break;
	case HL_LEG_LT:
		return BOTH_PAIN14;
		//PAIN14 = twitch left leg
		break;
	case HL_BACK_RT:
		return BOTH_PAIN7;
		//PAIN7 = med left shoulder
		break;
	case HL_BACK_LT:
		return Q_irand( BOTH_PAIN15, BOTH_PAIN16 );
		//PAIN15 = med right shoulder
		//PAIN16 = twitch right shoulder
		break;
	case HL_BACK:
		if ( !Q_irand( 0, 1 ) )
		{
			return BOTH_PAIN1;
		}
		else
		{
			return BOTH_PAIN5;
		}
		//PAIN1 = back
		//PAIN5 = same as 1
		break;
	case HL_CHEST_RT:
		return BOTH_PAIN3;
		//PAIN3 = long, right shoulder
		break;
	case HL_CHEST_LT:
		return BOTH_PAIN2;
		//PAIN2 = long, left shoulder
		break;
	case HL_WAIST:
	case HL_CHEST:
		if ( !Q_irand( 0, 3 ) )
		{
			return BOTH_PAIN6;
		}
		else if ( !Q_irand( 0, 2 ) )
		{
			return BOTH_PAIN8;
		}
		else if ( !Q_irand( 0, 1 ) )
		{
			return BOTH_PAIN17;
		}
		else
		{
			return BOTH_PAIN19;
		}
		//PAIN6 = gut
		//PAIN8 = chest
		//PAIN17 = twitch crotch
		//PAIN19 = med crotch
		break;
	case HL_ARM_RT:
	case HL_HAND_RT:
		return BOTH_PAIN9;
		//PAIN9 = twitch right arm
		break;
	case HL_ARM_LT:
	case HL_HAND_LT:
		return BOTH_PAIN10;
		//PAIN10 = twitch left arm
		break;
	case HL_HEAD:
		return BOTH_PAIN4;
		//PAIN4 = head
		break;
	default:
		return -1;
		break;
	}
}

extern void G_BounceMissile( gentity_t *ent, trace_t *trace );
void LimbThink( gentity_t *ent )
{//FIXME: just use object thinking?
	vec3_t		origin;
	trace_t		tr;

	ent->nextthink = level.time + FRAMETIME;
	if ( ent->enemy )
	{//alert people that I am a piece of one of their friends
		AddSightEvent( ent->enemy, ent->currentOrigin, 384, AEL_DISCOVERED );
	}

	// get current position
	EvaluateTrajectory( &ent->s.pos, level.time, origin );
	// get current angles
	EvaluateTrajectory( &ent->s.apos, level.time, ent->currentAngles );

	// trace a line from the previous position to the current position,
	// ignoring interactions with the missile owner
	gi.trace( &tr, ent->currentOrigin, ent->mins, ent->maxs, origin, 
		ent->owner ? ent->owner->s.number : ENTITYNUM_NONE, ent->clipmask );

	VectorCopy( tr.endpos, ent->currentOrigin );

	if ( tr.startsolid ) 
	{
		tr.fraction = 0;
	}

	gi.linkentity( ent );

	if ( tr.fraction != 1 ) 
	{
		G_BounceMissile( ent, &tr );
		if ( ent->s.pos.trType == TR_STATIONARY )
		{//stopped, stop spinning
			//lay flat
			//pitch
			if ( ent->s.apos.trBase[0] == -1 )
			{//any pitch is okay
				ent->s.apos.trBase[0] = ent->currentAngles[0];
			}
			else
			{//lay flat
				if ( ent->currentAngles[0] > ent->s.apos.trBase[0]+90 || ent->currentAngles[0] < ent->s.apos.trBase[0]-90 )
				{
					ent->s.apos.trBase[0] += 180;
				}
			}
			//yaw
			ent->s.apos.trBase[1] = ent->currentAngles[1];
			//roll
			if ( ent->s.apos.trBase[2] == -1 )
			{//any roll is okay
				ent->s.apos.trBase[2] = ent->currentAngles[2];
			}
			else
			{
				if ( ent->currentAngles[2] > ent->s.apos.trBase[2]+90 || ent->currentAngles[2] < ent->s.apos.trBase[2]-90 )
				{
					ent->s.apos.trBase[2] += 180;
				}
			}
			VectorClear( ent->s.apos.trDelta );
			ent->nextthink = level.time + Q_irand( 5000, 15000 );
			ent->e_ThinkFunc = thinkF_G_FreeEntity;
			//FIXME: these keep drawing for a frame or so after being freed?!  See them lerp to origin of world...
		}
	}
}

float hitLocHealthPercentage[HL_MAX] = 
{
	0.0f,	//HL_NONE = 0,
	0.05f,	//HL_FOOT_RT,
	0.05f,	//HL_FOOT_LT,
	0.20f,	//HL_LEG_RT,
	0.20f,	//HL_LEG_LT,
	0.30f,	//HL_WAIST,
	0.15f,	//HL_BACK_RT,
	0.15f,	//HL_BACK_LT,
	0.30f,	//HL_BACK,
	0.15f,	//HL_CHEST_RT,
	0.15f,	//HL_CHEST_LT,
	0.30f,	//HL_CHEST,
	0.05f,	//HL_ARM_RT,
	0.05f,	//HL_ARM_LT,
	0.01f,	//HL_HAND_RT,
	0.01f,	//HL_HAND_LT,
	0.10f	//HL_HEAD
};

void G_Dismember( gentity_t *ent, vec3_t point, 
				 const char *limbBone, const char *rotateBone, char *limbName, 
				 char *limbCapName, char *stubCapName, char *limbTagName, char *stubTagName, 
				 int limbAnim, float limbRollBase, float limbPitchBase,
				 int damage, int hitLoc )
{
	int newBolt;
	vec3_t	dir, newPoint;
	gentity_t *limb;

	//make sure this limb hasn't been lopped off already!
	if ( gi.G2API_GetSurfaceRenderStatus( &ent->ghoul2[ent->playerModel], limbName ) )
	{//already lost this limb
		return;
	}

	//add the passed-in damage to the locationDamage array, check to see if it's taken enough damage to actually dismember
	if ( g_dismemberment->integer < 4 
		&& !g_realisticSaberDamage->integer && 
		ent->locationDamage[hitLoc] < ent->client->ps.stats[STAT_MAX_HEALTH]*hitLocHealthPercentage[hitLoc] )
	{//this location has not taken enough damage to dismember
		return;
	}

	/*
	float dismemberProb = 0;
	// check which part of the body it is. Then check the npc's probability
	// of that body part coming off, if it doesn't pass, return out.
	switch(limbAnim)
	{
	case BOTH_DISMEMBER_RLEG:
	case BOTH_DISMEMBER_LLEG:
		dismemberProb = ent->client->dismemberProbLegs;
		break;
	case BOTH_DISMEMBER_TORSO1:
		dismemberProb = ent->client->dismemberProbWaist;
		break;
	case BOTH_DISMEMBER_RARM:
	case BOTH_DISMEMBER_LARM:
		dismemberProb = ent->client->dismemberProbArms;
		break;
	case BOTH_DISMEMBER_HEAD1:
		dismemberProb = ent->client->dismemberProbHead;
		break;
	}

	//check probability of this hapening on this npc
	if ( (random()*100) > dismemberProb )
	{	
		return;
	}
	*/
	

	ent->client->dismemberable = qfalse;

//0) create a limb ent
	VectorCopy( point, newPoint );
	newPoint[2] += 6;
	limb = G_Spawn();
	G_SetOrigin( limb, newPoint );
	VectorCopy( newPoint, limb->s.pos.trBase );
//1) copy the g2 instance of the victim into the limb
	gi.G2API_CopyGhoul2Instance( ent->ghoul2, limb->ghoul2 );//, int modelIndex = -1);
	limb->playerModel = 0;//assumption!
	limb->craniumBone = ent->craniumBone;
	limb->cervicalBone = ent->cervicalBone;
	limb->thoracicBone = ent->thoracicBone;
	limb->upperLumbarBone = ent->upperLumbarBone;
	limb->lowerLumbarBone = ent->lowerLumbarBone;
	limb->rootBone = ent->rootBone;
//2) set the root surf on the limb
	if ( limbTagName )
	{//add smoke to cap tag
		newBolt = gi.G2API_AddBolt( &limb->ghoul2[limb->playerModel], limbTagName );
		if ( newBolt != -1 )
		{
			G_PlayEffect( "blaster/smoke_bolton", limb->playerModel, newBolt, limb->s.number);
		}
	}
	/*
	if ( limbBone )
	{
		gi.G2API_StopBoneAnim( &limb->ghoul2[limb->playerModel], "model_root" );
		gi.G2API_StopBoneAnim( &limb->ghoul2[limb->playerModel], "upper_lumbar" );
	}
	*/
	gi.G2API_SetRootSurface( limb->ghoul2, limb->playerModel, limbName );
	/*
	if ( limbBone )
	{
		animation_t *animations = knownAnimFileSets[ent->client->clientInfo.animFileIndex].animations;
		//play the proper dismember anim on the limb
		gi.G2API_SetBoneAnim(&limb->ghoul2[limb->playerModel], limbBone, animations[limbAnim].firstFrame - 1, 
							animations[limbAnim].numFrames + animations[limbAnim].firstFrame - 1,
							BONE_ANIM_OVERRIDE_FREEZE, 1, cg.time);
	}
	*/
	if ( rotateBone )
	{
 		gi.G2API_SetNewOrigin( &limb->ghoul2[0], gi.G2API_AddBolt( &limb->ghoul2[0], rotateBone ) );
	}
	if ( limbCapName )
	{//turn on caps
		gi.G2API_SetSurfaceOnOff( &limb->ghoul2[limb->playerModel], limbCapName, 0 );
	}
//3) turn off w/descendants that surf in original model
	if ( stubTagName )
	{//add smoke to cap surf, spawn effect
		newBolt = gi.G2API_AddBolt( &ent->ghoul2[ent->playerModel], stubTagName );
		if ( newBolt != -1 )
		{
			G_PlayEffect( "blaster/smoke_bolton", ent->playerModel, newBolt, ent->s.number);
		}
	}
	gi.G2API_SetSurfaceOnOff( &ent->ghoul2[ent->playerModel], limbName, 0x00000100 );//G2SURFACEFLAG_NODESCENDANTS
	if ( stubCapName )
	{//turn on caps
		gi.G2API_SetSurfaceOnOff( &ent->ghoul2[ent->playerModel], stubCapName, 0 );
	}
	limb->s.radius = 60;
//4) toss the limb away
	limb->classname = "limb";
	limb->owner = ent;
	limb->enemy = ent->enemy;
	if ( limbAnim == BOTH_DISMEMBER_RARM || limbAnim == BOTH_DISMEMBER_TORSO1 )//&& ent->s.weapon == WP_SABER && ent->weaponModel != -1 )
	{//FIXME: is this first check needed with this lower one?
		if ( !gi.G2API_GetSurfaceRenderStatus( &limb->ghoul2[0], "r_hand" ) )
		{//only copy the weapon over if the right hand is actually on this limb...
			limb->s.weapon = ent->s.weapon;
			limb->weaponModel = ent->weaponModel;
		}
	}
	limb->e_ThinkFunc = thinkF_LimbThink;
	limb->nextthink = level.time + FRAMETIME;
	gi.linkentity( limb );
	//need size, contents, clipmask
	limb->svFlags = SVF_USE_CURRENT_ORIGIN;
	limb->clipmask = MASK_SOLID;
	limb->contents = CONTENTS_CORPSE;
	VectorSet( limb->mins, -3.0f, -3.0f, -6.0f );
	VectorSet( limb->maxs, 3.0f, 3.0f, 6.0f );
	//move it
	limb->s.eType = ET_GENERAL;
	limb->physicsBounce = 0.2f;
	limb->s.pos.trType = TR_GRAVITY;
	limb->s.pos.trTime = level.time;								// move a bit on the very first frame
	VectorSubtract( point, ent->currentOrigin, dir );
	VectorNormalize( dir );
	VectorMA( ent->client->ps.velocity, 200, dir, limb->s.pos.trDelta );
	//make it bounce some
	limb->s.eFlags |= EF_BOUNCE_HALF;
	//no trDuration?
	//spin it
	VectorClear( limb->s.apos.trBase );
	limb->s.apos.trBase[0] = limbPitchBase;
	limb->s.apos.trBase[1] = ent->client->ps.viewangles[1];
	limb->s.apos.trBase[2] = limbRollBase;
	limb->s.apos.trTime = level.time;
	limb->s.apos.trType = TR_LINEAR;
	VectorClear( limb->s.apos.trDelta );

	limb->s.apos.trDelta[0] = Q_irand( -300, 300 );
	limb->s.apos.trDelta[1] = Q_irand( -800, 800 );
	//limb->s.apos.trDelta[2] = Q_irand( -300, 300 );//FIXME: this scales it down @ 80% and does weird stuff in timescale != 1.0
	//limb->s.apos.trDelta[2] = limbRoll;

	//preserve scale so giants don't have tiny limbs
	VectorCopy( ent->s.modelScale, limb->s.modelScale );

	//FIXME: make limb dismemberable?
}

extern qboolean G_StandardHumanoid( const char *modelName );
static void G_DoDismemberment( gentity_t *self, vec3_t point, int mod, int damage, int hitLoc )
{
	// dismemberment -- FIXME: should have a check for how long npc has been dead so people can't
	// continue to dismember a dead body long after it's been dead
	//NOTE that you can only cut one thing off unless the super dismemberment is > 3
	if ( g_dismemberment->integer && self->client->dismemberable && mod == MOD_SABER )//only lightsaber
	{//FIXME: don't do strcmps here
		if ( G_StandardHumanoid( self->NPC_type ) )
		{//temp hack because only these models are set up the right way so far
			//FIXME: check the hitLoc and hitDir against the cap tag for the place 
			//where the split will be- if the hit dir is roughly perpendicular to 
			//the direction of the cap, then the split is allowed, otherwise we
			//hit it at the wrong angle and should not dismember...
			switch( hitLoc )//self->hitLoc
			{
			case HL_LEG_RT:
				if ( g_dismemberment->integer > 1 )
				{
					G_Dismember( self, point, "rtibia", "rtalus", "r_leg", "r_leg_cap_hips_off", "hips_cap_r_leg_off", "*r_leg_cap_hips", "*hips_cap_r_leg", BOTH_DISMEMBER_RLEG, 90, 0, damage, hitLoc );
				}
				break;
			case HL_LEG_LT:
				if ( g_dismemberment->integer > 1 )
				{
					G_Dismember( self, point, "ltibia", "ltalus", "l_leg", "l_leg_cap_hips_off", "hips_cap_l_leg_off", "*l_leg_cap_hips", "*hips_cap_l_leg", BOTH_DISMEMBER_LLEG, 90, 0, damage, hitLoc );
				}
				break;
			case HL_WAIST:
				if ( g_dismemberment->integer > 2 &&
					(!self->s.number||!self->message))
				{
					G_Dismember( self, point, "pelvis", "thoracic", "torso", "torso_cap_hips_off", "hips_cap_torso_off", "*torso_cap_hips", "*hips_cap_torso", BOTH_DISMEMBER_TORSO1, 0, 0, damage, hitLoc );//deathAnim
				}
				break;
			case HL_CHEST_RT:
			case HL_ARM_RT:
			case HL_BACK_RT:
				if ( g_dismemberment->integer )
				{
					G_Dismember( self, point, "rhumerus", "rradius", "r_arm", "r_arm_cap_torso_off", "torso_cap_r_arm_off", "*r_arm_cap_torso", "*torso_cap_r_arm", BOTH_DISMEMBER_RARM, 90, 0, damage, hitLoc );
				}
				break;
			case HL_CHEST_LT:
			case HL_ARM_LT:
			case HL_BACK_LT:
				if ( g_dismemberment->integer &&
					(!self->s.number||!self->message))
				{//either the player or not carrying a key on my arm
					G_Dismember( self, point, "lhumerus", "lradius", "l_arm", "l_arm_cap_torso_off", "torso_cap_l_arm_off", "*l_arm_cap_torso", "*torso_cap_l_arm", BOTH_DISMEMBER_LARM, 90, 0, damage, hitLoc );
				}
				break;
			case HL_HAND_RT:
				if ( g_dismemberment->integer )
				{
					G_Dismember( self, point, "rradiusX", "rhand", "r_hand", "r_hand_cap_r_arm_off", "r_arm_cap_r_hand_off", "*r_hand_cap_r_arm", "*r_arm_cap_r_hand", BOTH_DISMEMBER_RARM, 90, 0, damage, hitLoc );
				}
				break;
			case HL_HAND_LT:
				if ( g_dismemberment->integer )
				{
					G_Dismember( self, point, "lradiusX", "lhand", "l_hand", "l_hand_cap_l_arm_off", "l_arm_cap_l_hand_off", "*l_hand_cap_l_arm", "*l_arm_cap_l_hand", BOTH_DISMEMBER_RARM, 90, 0, damage, hitLoc );
				}
				break;
			case HL_HEAD:
				if ( g_dismemberment->integer > 2 )
				{
					G_Dismember( self, point, "cervical", "cranium", "head", "head_cap_torso_off", "torso_cap_head_off", "*head_cap_torso", "*torso_cap_head", BOTH_DISMEMBER_HEAD1, -1, -1, damage, hitLoc );
				}
				break;
			case HL_FOOT_RT:
			case HL_FOOT_LT:
			case HL_CHEST:
			case HL_BACK:
			default:
				break;
			}
		}
	}
}

static int G_CheckSpecialDeathAnim( gentity_t *self, vec3_t point, int damage, int mod, int hitLoc )
{
	int deathAnim = -1;

	if ( PM_InRoll( &self->client->ps ) )
	{
		deathAnim = BOTH_DEATH_ROLL;		//# Death anim from a roll
	}
	else if ( PM_FlippingAnim( self->client->ps.legsAnim ) )
	{
		deathAnim = BOTH_DEATH_FLIP;		//# Death anim from a flip
	}
	else if ( PM_SpinningAnim( self->client->ps.legsAnim ) )
	{
		float yawDiff = AngleNormalize180(AngleNormalize180(self->client->renderInfo.torsoAngles[YAW]) - AngleNormalize180(self->client->ps.viewangles[YAW]));
		if ( yawDiff > 135 || yawDiff < -135 )
		{
			deathAnim = BOTH_DEATH_SPIN_180;	//# Death anim when facing backwards
		}
		else if ( yawDiff < -60 )
		{
			deathAnim = BOTH_DEATH_SPIN_90_R;	//# Death anim when facing 90 degrees right
		}
		else if ( yawDiff > 60 )
		{
			deathAnim = BOTH_DEATH_SPIN_90_L;	//# Death anim when facing 90 degrees left
		}
	}
	else if ( PM_InOnGroundAnim( &self->client->ps ) )
	{
		if ( AngleNormalize180(self->client->renderInfo.torsoAngles[PITCH]) < 0 )
		{
			deathAnim = BOTH_DEATH_LYING_UP;	//# Death anim when lying on back
		}
		else
		{
			deathAnim = BOTH_DEATH_LYING_DN;	//# Death anim when lying on front
		}
	}
	else if ( PM_InKnockDown( &self->client->ps ) )
	{
		if ( AngleNormalize180(self->client->renderInfo.torsoAngles[PITCH]) < 0 )
		{
			deathAnim = BOTH_DEATH_FALLING_UP;	//# Death anim when falling on back
		}
		else
		{
			deathAnim = BOTH_DEATH_FALLING_DN;	//# Death anim when falling on face
		}
	}
	else if ( hitLoc == HL_HAND_RT )
	{
		if ( g_dismemberment->integer 
			&& self->client->dismemberable 
			&& mod == MOD_SABER )//only lightsaber
		{//FIXME: don't do strcmps here
			if ( G_StandardHumanoid( self->NPC_type ) )
			{
				if ( g_dismemberment->integer >= 4 
					|| g_realisticSaberDamage->integer 
					|| self->locationDamage[hitLoc] >= self->client->ps.stats[STAT_MAX_HEALTH]*hitLocHealthPercentage[hitLoc] )
				{//this location has taken enough damage to dismember
					deathAnim = BOTH_RIGHTHANDCHOPPEDOFF;
				}
			}
		}
	}
	else if ( PM_CrouchAnim( self->client->ps.legsAnim ) )
	{
		deathAnim = BOTH_DEATH_CROUCHED;	//# Death anim when crouched
	}

	return deathAnim;
}
extern qboolean PM_FinishedCurrentLegsAnim( gentity_t *self );
static int G_PickDeathAnim( gentity_t *self, vec3_t point, int damage, int mod, int hitLoc )
{//FIXME: play dead flop anims on body if in an appropriate _DEAD anim when this func is called
	int deathAnim = -1;
	if ( hitLoc == HL_NONE )
	{
		hitLoc = G_GetHitLocation( self, point );//self->hitLoc
	}
	//dead flops...if you are already playing a death animation, I guess it can just return directly
	switch( self->client->ps.legsAnim )
	{
	case BOTH_DEATH1:		//# First Death anim
	case BOTH_DEAD1:
	case BOTH_DEATH2:			//# Second Death anim
	case BOTH_DEAD2:
	case BOTH_DEATH8:			//# 
	case BOTH_DEAD8:
	case BOTH_DEATH13:			//# 
	case BOTH_DEAD13:
	case BOTH_DEATH14:			//# 
	case BOTH_DEAD14:
	case BOTH_DEATH16:			//# 
	case BOTH_DEAD16:
	case BOTH_DEADBACKWARD1:		//# First thrown backward death finished pose
	case BOTH_DEADBACKWARD2:		//# Second thrown backward death finished pose
		return -2;
		break;
		/*
		if ( PM_FinishedCurrentLegsAnim( self ) )
		{//done with the anim
			deathAnim = BOTH_DEADFLOP2;
		}
		else
		{
			deathAnim = -2;
		}
		break;
	case BOTH_DEADFLOP2:
		deathAnim = BOTH_DEADFLOP2;
		break;
		*/
	case BOTH_DEATH10:			//# 
	case BOTH_DEAD10:
	case BOTH_DEATH15:			//# 
	case BOTH_DEAD15:
	case BOTH_DEADFORWARD1:		//# First thrown forward death finished pose
	case BOTH_DEADFORWARD2:		//# Second thrown forward death finished pose
		return -2;
		break;
		/*
		if ( PM_FinishedCurrentLegsAnim( self ) )
		{//done with the anim
			deathAnim = BOTH_DEADFLOP1;
		}
		else
		{
			deathAnim = -2;
		}
		break;
		*/
	case BOTH_DEADFLOP1:
		//deathAnim = BOTH_DEADFLOP1;
		return -2;
		break;
	case BOTH_DEAD3:				//# Third Death finished pose
	case BOTH_DEAD4:				//# Fourth Death finished pose
	case BOTH_DEAD5:				//# Fifth Death finished pose
	case BOTH_DEAD6:				//# Sixth Death finished pose
	case BOTH_DEAD7:				//# Seventh Death finished pose
	case BOTH_DEAD9:				//# 
	case BOTH_DEAD11:			//#
	case BOTH_DEAD12:			//# 
	case BOTH_DEAD17:			//# 
	case BOTH_DEAD18:			//# 
	case BOTH_DEAD19:			//# 
	case BOTH_LYINGDEAD1:		//# Killed lying down death finished pose
	case BOTH_STUMBLEDEAD1:		//# Stumble forward death finished pose
	case BOTH_FALLDEAD1LAND:		//# Fall forward and splat death finished pose
	case BOTH_DEATH3:			//# Third Death anim
	case BOTH_DEATH4:			//# Fourth Death anim
	case BOTH_DEATH5:			//# Fifth Death anim
	case BOTH_DEATH6:			//# Sixth Death anim
	case BOTH_DEATH7:			//# Seventh Death anim
	case BOTH_DEATH9:			//# 
	case BOTH_DEATH11:			//#
	case BOTH_DEATH12:			//# 
	case BOTH_DEATH17:			//# 
	case BOTH_DEATH18:			//# 
	case BOTH_DEATH19:			//# 
	case BOTH_DEATHFORWARD1:		//# First Death in which they get thrown forward
	case BOTH_DEATHFORWARD2:		//# Second Death in which they get thrown forward
	case BOTH_DEATHBACKWARD1:	//# First Death in which they get thrown backward
	case BOTH_DEATHBACKWARD2:	//# Second Death in which they get thrown backward
	case BOTH_DEATH1IDLE:		//# Idle while close to death
	case BOTH_LYINGDEATH1:		//# Death to play when killed lying down
	case BOTH_STUMBLEDEATH1:		//# Stumble forward and fall face first death
	case BOTH_FALLDEATH1:		//# Fall forward off a high cliff and splat death - start
	case BOTH_FALLDEATH1INAIR:	//# Fall forward off a high cliff and splat death - loop
	case BOTH_FALLDEATH1LAND:	//# Fall forward off a high cliff and splat death - hit bottom
		return -2;
		break;
	case BOTH_DEATH_ROLL:		//# Death anim from a roll
	case BOTH_DEATH_FLIP:		//# Death anim from a flip
	case BOTH_DEATH_SPIN_90_R:	//# Death anim when facing 90 degrees right
	case BOTH_DEATH_SPIN_90_L:	//# Death anim when facing 90 degrees left
	case BOTH_DEATH_SPIN_180:	//# Death anim when facing backwards
	case BOTH_DEATH_LYING_UP:	//# Death anim when lying on back
	case BOTH_DEATH_LYING_DN:	//# Death anim when lying on front
	case BOTH_DEATH_FALLING_DN:	//# Death anim when falling on face
	case BOTH_DEATH_FALLING_UP:	//# Death anim when falling on back
	case BOTH_DEATH_CROUCHED:	//# Death anim when crouched
	case BOTH_RIGHTHANDCHOPPEDOFF:
		return -2;
		break;
	}
	// Not currently playing a death animation, so try and get an appropriate one now.
	if ( deathAnim == -1 )
	{
		deathAnim = G_CheckSpecialDeathAnim( self, point, damage, mod, hitLoc );

		if ( deathAnim == -1 )
		{//base on hitLoc
			//death anims
			switch( hitLoc )
			{
			case HL_FOOT_RT:
			case HL_FOOT_LT:
				if ( !Q_irand( 0, 2 ) )
				{
					deathAnim = BOTH_DEATH4;//back: forward
				}
				else if ( !Q_irand( 0, 1 ) )
				{
					deathAnim = BOTH_DEATH5;//same as 4
				}
				else
				{
					deathAnim = BOTH_DEATH15;//back: forward
				}
				break;
			case HL_LEG_RT:
				if ( !Q_irand( 0, 2 ) )
				{
					deathAnim = BOTH_DEATH4;//back: forward
				}
				else if ( !Q_irand( 0, 1 ) )
				{
					deathAnim = BOTH_DEATH5;//same as 4
				}
				else
				{
					deathAnim = BOTH_DEATH15;//back: forward
				}
				break;
			case HL_LEG_LT:
				if ( !Q_irand( 0, 2 ) )
				{
					deathAnim = BOTH_DEATH4;//back: forward
				}
				else if ( !Q_irand( 0, 1 ) )
				{
					deathAnim = BOTH_DEATH5;//same as 4
				}
				else
				{
					deathAnim = BOTH_DEATH15;//back: forward
				}
				break;
			case HL_BACK:
				if ( VectorLengthSquared( self->client->ps.velocity ) < 256 )
				{
					if ( Q_irand( 0, 1 ) )
					{
						deathAnim = BOTH_DEATH17;//head/back: croak
					}
					else
					{
						deathAnim = BOTH_DEATH10;//back: bend back, fall forward
					}
				}
				else
				{
					if ( !Q_irand( 0, 2 ) )
					{
						deathAnim = BOTH_DEATH4;//back: forward
					}
					else if ( !Q_irand( 0, 1 ) )
					{
						deathAnim = BOTH_DEATH5;//same as 4
					}
					else
					{
						deathAnim = BOTH_DEATH15;//back: forward
					}
				}
				break;
			case HL_HAND_RT:
				if ( g_dismemberment->integer && self->client->dismemberable && mod == MOD_SABER )//only lightsaber
				{//FIXME: don't do strcmps here
					if ( G_StandardHumanoid( self->NPC_type ) )
					{
						if ( g_dismemberment->integer >= 4 
							|| g_realisticSaberDamage->integer 
							|| self->locationDamage[hitLoc] >= self->client->ps.stats[STAT_MAX_HEALTH]*hitLocHealthPercentage[hitLoc] )
						{//this location has taken enough damage to dismember
							deathAnim = BOTH_RIGHTHANDCHOPPEDOFF;
						}
					}
				}
				//NOTE: falls through
			case HL_CHEST_RT:
			case HL_ARM_RT:
			case HL_BACK_RT:
				//NOTE: falls through from HL_HAND_RT
				if ( deathAnim == -1 )
				{
					if ( damage <= self->max_health*0.25 )
					{
						deathAnim = BOTH_DEATH9;//chest right: snap, fall forward
					}
					else if ( damage <= self->max_health*0.5 )
					{
						deathAnim = BOTH_DEATH3;//chest right: back
					}
					else if ( damage <= self->max_health*0.75 )
					{
						deathAnim = BOTH_DEATH6;//chest right: spin
					}
					else 
					{
						//TEMP HACK: play spinny deaths less often
						if ( Q_irand( 0, 1 ) )
						{
							deathAnim = BOTH_DEATH8;//chest right: spin high
						}
						else
						{
							switch ( Q_irand( 0, 2 ) )
							{
							default:
							case 0:
								deathAnim = BOTH_DEATH9;//chest right: snap, fall forward
								break;
							case 1:
								deathAnim = BOTH_DEATH3;//chest right: back
								break;
							case 2:
								deathAnim = BOTH_DEATH6;//chest right: spin
								break;
							}
						}
					}
				}
				break;
			case HL_CHEST_LT:
			case HL_ARM_LT:
			case HL_HAND_LT:
			case HL_BACK_LT:
				if ( damage <= self->max_health*0.25 )
				{
					deathAnim = BOTH_DEATH11;//chest left: snap, fall forward
				}
				else if ( damage <= self->max_health*0.5 )
				{
					deathAnim = BOTH_DEATH7;//chest left: back
				}
				else if ( damage <= self->max_health*0.75 )
				{
					deathAnim = BOTH_DEATH12;//chest left: spin
				}
				else
				{
					//TEMP HACK: play spinny deaths less often
					if ( Q_irand( 0, 1 ) )
					{
						deathAnim = BOTH_DEATH14;//chest left: spin high
					}
					else
					{
						switch ( Q_irand( 0, 2 ) )
						{
						default:
						case 0:
							deathAnim = BOTH_DEATH11;//chest left: snap, fall forward
							break;
						case 1:
							deathAnim = BOTH_DEATH7;//chest left: back
							break;
						case 2:
							deathAnim = BOTH_DEATH12;//chest left: spin
							break;
						}
					}
				}
				break;
			case HL_CHEST:
			case HL_WAIST:
				if ( damage <= self->max_health*0.25 || !VectorLengthSquared( self->client->ps.velocity ) )
				{
					if ( !Q_irand( 0, 1 ) )
					{
						deathAnim = BOTH_DEATH18;//gut: fall right
					}
					else
					{
						deathAnim = BOTH_DEATH19;//gut: fall left
					}
				}
				else if ( damage <= self->max_health*0.5 )
				{
					deathAnim = BOTH_DEATH2;//chest: backward short
				}
				else //if ( damage <= self->max_health*0.75 )
				{
					if ( !Q_irand( 0, 1 ) )
					{
						deathAnim = BOTH_DEATH1;//chest: backward med
					}
					else
					{
						deathAnim = BOTH_DEATH16;//same as 1
					}
				}
				break;
			case HL_HEAD:
				if ( damage <= self->max_health*0.5 )
				{
					deathAnim = BOTH_DEATH17;//head/back: croak
				}
				else
				{
					deathAnim = BOTH_DEATH13;//head: stumble, fall back
				}
				break;
			default:
				break;
			}
		}
	}

	// Validate.....
	if ( deathAnim == -1 || !PM_HasAnimation( self, deathAnim ))
	{
		// I guess we'll take what we can get.....
		deathAnim = PM_PickAnim( self, BOTH_DEATH1, BOTH_DEATH19 );
	}

	return deathAnim;
}

void G_FriendlyFireReaction( gentity_t *self, gentity_t *other, int dflags )
{
	if ( (!player->client->ps.viewEntity || other->s.number != player->client->ps.viewEntity)) 
	{//hit by a teammate
		if ( other != self->enemy && self != other->enemy )
		{//we weren't already enemies
			if ( self->enemy || other->enemy || (other->s.number&&other->s.number!=player->client->ps.viewEntity) || (!other->s.number&&Q_irand( 0, 3 )) )
			{//if one of us actually has an enemy already, it's okay, just an accident OR wasn't hit by player or someone controlled by player OR player hit ally and didn't get 25% chance of getting mad (FIXME:accumulate anger+base on diff?)
				return;
			}
			else if ( self->NPC && !other->s.number )//should be assumed, but...
			{//dammit, stop that!
				if ( !(dflags&DAMAGE_RADIUS) )
				{
					//if it's radius damage, ignore it
					if ( self->NPC->ffireDebounce < level.time )
					{
						//FIXME: way something?  NEED DIALOGUE
						self->NPC->ffireCount++;
						self->NPC->ffireDebounce = level.time + 500;
					}
				}
			}
		}
	}
}

float damageModifier[HL_MAX] = 
{
	1.0f,	//HL_NONE,
	0.25f,	//HL_FOOT_RT,
	0.25f,	//HL_FOOT_LT,
	0.75f,	//HL_LEG_RT,
	0.75f,	//HL_LEG_LT,
	1.0f,	//HL_WAIST,
	1.0f,	//HL_BACK_RT,
	1.0f,	//HL_BACK_LT,
	1.0f,	//HL_BACK,
	1.0f,	//HL_CHEST_RT,
	1.0f,	//HL_CHEST_LT,
	1.0f,	//HL_CHEST,
	0.5f,	//HL_ARM_RT,
	0.5f,	//HL_ARM_LT,
	0.25f,	//HL_HAND_RT,
	0.25f,	//HL_HAND_LT,
	2.0f,	//HL_HEAD,
	1.0f,	//HL_GENERIC1,
	1.0f,	//HL_GENERIC2,
	1.0f,	//HL_GENERIC3,
	1.0f,	//HL_GENERIC4,
	1.0f,	//HL_GENERIC5,
	1.0f,	//HL_GENERIC6,
};
/*

============
T_Damage

targ		entity that is being damaged
inflictor	entity that is causing the damage
attacker	entity that caused the inflictor to damage targ
	example: targ=monster, inflictor=rocket, attacker=player

dir			direction of the attack for knockback
point		point at which the damage is being inflicted, used for headshots
damage		amount of damage being inflicted
knockback	force to be applied against targ as a result of the damage

inflictor, attacker, dir, and point can be NULL for environmental effects

dflags		these flags are used to control how T_Damage works
	DAMAGE_RADIUS			damage was indirect (from a nearby explosion)
	DAMAGE_NO_ARMOR			armor does not protect from this damage
	DAMAGE_NO_KNOCKBACK		do not affect velocity, just view angles
	DAMAGE_NO_PROTECTION	kills godmode, armor, everything
	DAMAGE_NO_HIT_LOC		Damage not based on hit location
============
*/

void G_Damage( gentity_t *targ, gentity_t *inflictor, gentity_t *attacker, vec3_t dir, vec3_t point, int damage, int dflags, int mod, int hitLoc ) 
{
	gclient_t	*client;
	int			take;
	int			save;
	int			asave = 0;
	int			knockback;
	vec3_t		newDir;
	qboolean	alreadyDead = qfalse;

	if (!targ->takedamage) {
		return;
	}

	if ( targ->health <= 0 && !targ->client )
	{	// allow corpses to be disintegrated
		if( mod != MOD_SNIPER || (targ->flags & FL_DISINTEGRATED) )
		return;
	}

	// if we are the player and we are locked to an emplaced gun, we have to reroute damage to the gun....sigh.
	if ( targ->s.eFlags & EF_LOCKED_TO_WEAPON && targ->s.number == 0 && targ->owner )
	{
		// swapping the gun into our place to absorb our damage
		targ = targ->owner;
	}

	if ( (targ->flags&FL_SHIELDED) && mod != MOD_SABER  && !targ->client)
	{//magnetically protected, this thing can only be damaged by lightsabers
		return;
	}

	if ( (targ->flags&FL_DMG_BY_SABER_ONLY) && mod != MOD_SABER )
	{//can only be damaged by lightsabers (but no shield... yeah, it's redundant, but whattayagonnado?)
		return;
	}

	if (( targ->flags & FL_DMG_BY_HEAVY_WEAP_ONLY ) && !( dflags & DAMAGE_HEAVY_WEAP_CLASS ))
	{
		// can only be damaged by an heavy type weapon...but impacting missile was in the heavy weap class...so we just aren't taking damage from this missile
		return;
	}

	if ( targ->client && targ->client->NPC_class == CLASS_ATST )
	{
		// extra checks can be done here
		if ( mod == MOD_SNIPER || mod == MOD_DISRUPTOR )
		{
			// disruptor does not hurt an atst
			return;
		}
		if ( mod == MOD_SABER )
		{
			// I guess always do 5 points of damage...feel free to tweak as needed
			damage = 5;
		}
	}

	if ( !inflictor ) {
		inflictor = &g_entities[ENTITYNUM_WORLD];
	}
	if ( !attacker ) {
		attacker = &g_entities[ENTITYNUM_WORLD];
	}

	// no more weakling allies!
//	if ( attacker->s.number != 0 && damage >= 2 && targ->s.number != 0 && attacker->client && attacker->client->playerTeam == TEAM_PLAYER )
//	{//player-helpers do only half damage to enemies
//		damage = ceil((float)damage/2.0f);
//	}

	client = targ->client;

	if ( client ) {
		if ( client->noclip && !targ->s.number ) {
			return;
		}
	}

	if ( dflags&DAMAGE_NO_DAMAGE )
	{
		damage = 0;
	}

	if ( dir == NULL ) 
	{
		dflags |= DAMAGE_NO_KNOCKBACK;
	} 
	else 
	{
		VectorNormalize2( dir, newDir );
	}

	if ( targ->s.number != 0 )
	{//not the player
		if ( (targ->flags&FL_GODMODE) || (targ->flags&FL_UNDYING) )
		{//have god or undying on, so ignore no protection flag
			dflags &= ~DAMAGE_NO_PROTECTION;
		}
	}

	if ( client && PM_InOnGroundAnim( &client->ps ))
	{
		dflags |= DAMAGE_NO_KNOCKBACK;
	}

	if ( client && client->NPC_class == CLASS_GALAKMECH )
	{//hit Galak
		if ( client->ps.stats[STAT_ARMOR] > 0 )
		{//shields are up
			dflags &= ~DAMAGE_NO_ARMOR;//always affect armor
			if ( mod == MOD_ELECTROCUTE )
			{//shield protects us from this
				damage = 0;
			}
		}
		else
		{//shields down
			if ( mod == MOD_MELEE || (mod == MOD_CRUSH && attacker && attacker->client) )
			{//Galak takes no impact damage
				return;
			}
			if ( (dflags & DAMAGE_RADIUS) || mod == MOD_SABER )
			{//galak without shields takes quarter damage from explosives and lightsaber
				damage = ceil((float)damage/4.0f);
			}
		}
	}

	if ( client && ( mod == MOD_DEMP2 || mod == MOD_DEMP2_ALT ))
	{
		if ( client->NPC_class == CLASS_PROTOCOL || client->NPC_class == CLASS_SEEKER ||
			client->NPC_class == CLASS_R2D2 || client->NPC_class == CLASS_R5D2 ||
			client->NPC_class == CLASS_MOUSE || client->NPC_class == CLASS_GONK )
		{
			// DEMP2 does more damage to these guys.
			damage *= 2;
		}
		else if ( client->NPC_class == CLASS_PROBE || client->NPC_class == CLASS_INTERROGATOR ||
					client->NPC_class == CLASS_MARK1 || client->NPC_class == CLASS_MARK2 || client->NPC_class == CLASS_SENTRY )
		{
			// DEMP2 does way more damage to these guys.
			damage *= 5;
		}
	}
	knockback = damage;

	//Attempt to apply extra knockback
	if ( dflags & DAMAGE_EXTRA_KNOCKBACK )
	{
		knockback *= 2;
	}
	
	if ( knockback > 200 ) {
		knockback = 200;
	}

	if ( mod == MOD_CRUSH )
	{
		knockback = 0;
	}
	else if ( targ->flags & FL_NO_KNOCKBACK ) 
	{
		knockback = 0;
	}
	else if ( targ->client && attacker->client && targ->client->playerTeam == attacker->client->playerTeam )
	{
		knockback = 0;
	}
	else if ( dflags & DAMAGE_NO_KNOCKBACK )
	{
		knockback = 0;
	}

	// figure momentum add, even if the damage won't be taken
	if ( knockback && !(dflags&DAMAGE_DEATH_KNOCKBACK) ) //&& targ->client 
	{
		G_ApplyKnockback( targ, newDir, knockback );
	}

	// check for godmode, completely getting out of the damage
	if ( targ->flags & FL_GODMODE && !(dflags&DAMAGE_NO_PROTECTION) ) 
	{
		if ( targ->client && attacker->client && targ->client->playerTeam == attacker->client->playerTeam )
		{//complain, but don't turn on them
			G_FriendlyFireReaction( targ, attacker, dflags );
		}
		return;
	}

	// Check for team damage
	/*
	if ( targ != attacker && !(dflags&DAMAGE_IGNORE_TEAM) && OnSameTeam (targ, attacker)  ) 
	{//on same team
		if ( !targ->client ) 
		{//a non-player object should never take damage from an ent on the same team
			return;
		}

		if ( attacker->client && attacker->client->playerTeam == targ->noDamageTeam ) 
		{//NPC or player shot an object on his own team
			return;
		}

		if ( attacker->s.number != 0 && targ->s.number != 0 &&//player not involved in any way in this exchange
			attacker->client && targ->client &&//two NPCs
			attacker->client->playerTeam == targ->client->playerTeam ) //on the same team
		{//NPCs on same team don't hurt each other
			return;
		}

		if ( targ->s.number == 0 &&//player was hit
			attacker->client && targ->client &&//by an NPC
			attacker->client->playerTeam == TEAM_PLAYER ) //on the same team
		{
			if ( attacker->enemy != targ )//by accident
			{//do no damage, no armor loss, no reaction, run no scripts
				return;
			}
		}
	}
	*/

	if ( !(dflags&DAMAGE_NO_HIT_LOC) )
	{
		damage *= damageModifier[hitLoc];
	}

	// add to the attacker's hit counter
	if ( attacker->client && targ != attacker && targ->health > 0 ) {
		if ( OnSameTeam( targ, attacker ) ) {
//			attacker->client->ps.persistant[PERS_HITS] -= damage;
		} else {
//			attacker->client->ps.persistant[PERS_HITS] += damage;
		}
	}

	take = damage;
	save = 0;

	//FIXME: Do not use this method of difficulty changing
	// Scale the amount of damage given to the player based on the skill setting
	/*
	if ( targ->s.number == 0 && targ != attacker )
	{
		take *= ( g_spskill->integer + 1) * 0.75;
	}

	if ( take < 1 ) {
		take = 1;
	}
	*/
	if ( client )
	{
		//don't lose armor if on same team
		// save some from armor
		asave = CheckArmor (targ, take, dflags);
		if ( !asave )
		{//all out of armor
			targ->client->ps.powerups[PW_BATTLESUIT] = 0;
		}
		else
		{
			targ->client->ps.powerups[PW_BATTLESUIT] = level.time + ARMOR_EFFECT_TIME;
		}
		take -= asave;
	}

	if ( g_debugDamage->integer ) {
		gi.Printf( "[%d]client:%i health:%i damage:%i armor:%i\n", level.time, targ->s.number,
			targ->health, take, asave );
	}

	// add to the damage inflicted on a player this frame
	// the total will be turned into screen blends and view angle kicks
	// at the end of the frame
	if ( client ) {
		client->ps.persistant[PERS_ATTACKER] = attacker->s.number;	//attack can be the world ent
		client->damage_armor += asave;
		client->damage_blood += take;
		client->damage_knockback += knockback;
		if ( dir ) {	//can't check newdir since it's local, newdir is dir normalized
			VectorCopy ( newDir, client->damage_from );
			client->damage_fromWorld = qfalse;
		} else {
			VectorCopy ( targ->currentOrigin, client->damage_from );
			client->damage_fromWorld = qtrue;
		}
	}

	// do the damage
	if ( targ->health <= 0 )
	{
		alreadyDead = qtrue;
	}

	if ( attacker && attacker->client && !attacker->s.number )
	{
		if ( !alreadyDead )
		{
			int add;
			if ( take > targ->health )
			{
				add = targ->health;
			}
			else
			{
				add = take;
			}
			add += asave;
			//FIXME: need to check the MOD to find out what weapon (if *any*) actually did the killing
			attacker->client->sess.missionStats.weaponUsed[attacker->client->ps.weapon] += add;
		}
	}
	if ( take || (dflags&DAMAGE_NO_DAMAGE) ) 
	{
		if ( !targ->client || !attacker->client )
		{
			targ->health = targ->health - take;
			if (targ->health < 0)
			{
				targ->health = 0;
			}
			if ( !alreadyDead && ( ((targ->flags&FL_UNDYING) && !(dflags&DAMAGE_NO_PROTECTION)) || (dflags&DAMAGE_NO_KILL)) )
			{
				if(targ->health < 1)
				{
					G_ActivateBehavior( targ, BSET_DEATH );
					targ->health = 1;
				}
			}
		}
		else 
		{//two clients
			team_t		targTeam = TEAM_FREE;
			team_t		attackerTeam = TEAM_FREE;

			if ( player->client->ps.viewEntity && targ->s.number == player->client->ps.viewEntity )
			{
				targTeam = player->client->playerTeam;
			}
			else if ( targ->client ) {
				targTeam = targ->client->playerTeam;
			}
			else {
				targTeam = targ->noDamageTeam;
			}
		//	if ( targTeam == TEAM_DISGUISE ) {
		//		targTeam = TEAM_PLAYER;
		//	}
			if ( player->client->ps.viewEntity && attacker->s.number == player->client->ps.viewEntity )
			{
				attackerTeam = player->client->playerTeam;
			}
			else if ( attacker->client ) {
				attackerTeam = attacker->client->playerTeam;
			}
			else {
				attackerTeam = attacker->noDamageTeam;
			}
		//	if ( attackerTeam == TEAM_DISGUISE ) {
		//		attackerTeam = TEAM_PLAYER;
		//	}

			if ( targTeam != attackerTeam )
			{//on the same team
				targ->health = targ->health - take;

				//MCG - Falling should never kill player- only if a trigger_hurt does so.
				if ( mod == MOD_FALLING && targ->s.number == 0 && targ->health < 1 )
				{
					targ->health = 1;
				}
				else if (targ->health < 0)
				{
					targ->health = 0;
				}

				if ( !alreadyDead && ( ((targ->flags&FL_UNDYING) && !(dflags&DAMAGE_NO_PROTECTION)) || (dflags&DAMAGE_NO_KILL) ) )
				{
					if ( targ->health < 1 )
					{
						G_ActivateBehavior( targ, BSET_DEATH );
						targ->health = 1;
					}
				}
				else if ( targ->health < 1 && attacker->client )
				{	// The player or NPC just killed an enemy so increment the kills counter
					attacker->client->ps.persistant[PERS_ENEMIES_KILLED]++;
				}
			}
			else if ( targTeam == TEAM_PLAYER )
			{//on the same team, and target is an ally
				qboolean takeDamage = qtrue;
				qboolean yellAtAttacker = qtrue;

				//1) player doesn't take damage from teammates unless they're angry at him
				if ( targ->s.number == 0 )
				{//the player
					if ( attacker->enemy != targ && attacker != targ )
					{//an NPC shot the player by accident
						takeDamage = qfalse;
					}
				}
				//2) NPCs don't take any damage from player during combat
				else 
				{//an NPC
					if ( ((dflags & DAMAGE_RADIUS)) && !(dflags&DAMAGE_IGNORE_TEAM) )
					{//An NPC got hit by player and this is during combat or it was slash damage
						//NOTE: though it's not realistic to have teammates not take splash damage,
						//		even when not in combat, it feels really bad to have them able to
						//		actually be killed by the player's splash damage
						takeDamage = qfalse;
					}

					if ( (dflags & DAMAGE_RADIUS) )
					{//you're fighting and it's just radius damage, so don't even mention it
						yellAtAttacker = qfalse;
					}
				}

				if ( takeDamage )
				{
					targ->health = targ->health - take;
					if ( !alreadyDead && (((targ->flags&FL_UNDYING) && !(dflags&DAMAGE_NO_PROTECTION) && attacker->s.number != 0) || (dflags&DAMAGE_NO_KILL) ) )
					{//guy is marked undying and we're not the player or we're in combat
						if ( targ->health < 1 )
						{
							G_ActivateBehavior( targ, BSET_DEATH );
							
							targ->health = 1;
						}
					}
					else if (targ->health < 0)
					{
						targ->health = 0;
					}
				}
				
				if ( yellAtAttacker )
				{
					G_FriendlyFireReaction( targ, attacker, dflags );
				}
			}
		}

		if ( targ->client ) {
			targ->client->ps.stats[STAT_HEALTH] = targ->health;
			g_lastClientDamaged = targ;
		}
			
		//TEMP HACK FOR PLAYER LOOK AT ENEMY CODE
		//FIXME: move this to a player pain func?
		if ( targ->s.number == 0 )
		{
			if ( !targ->enemy //player does not have an enemy yet
				|| targ->enemy->s.weapon != WP_SABER //or player's enemy is not a jedi
				|| attacker->s.weapon == WP_SABER )//and attacker is a jedi
				//keep enemy jedi over shooters
			{
				if ( attacker->enemy == targ || !OnSameTeam( targ, attacker ) )
				{//don't set player's enemy to teammates that hit him by accident
					targ->enemy = attacker;
				}
				NPC_SetLookTarget( targ, attacker->s.number, level.time+1000 );
			}
		}
		else if ( attacker->s.number == 0 && (!targ->NPC || !targ->NPC->timeOfDeath) && (mod == MOD_SABER || attacker->s.weapon != WP_SABER || !attacker->enemy || attacker->enemy->s.weapon != WP_SABER) )//keep enemy jedi over shooters
		{//this looks dumb when they're on the ground and you keep hitting them, so only do this when first kill them
			if ( !OnSameTeam( targ, attacker ) )
			{//don't set player's enemy to teammates that he hits by accident
				attacker->enemy = targ;
			}
			NPC_SetLookTarget( attacker, targ->s.number, level.time+1000 );
		}
		//TEMP HACK FOR PLAYER LOOK AT ENEMY CODE

		//add up the damage to the location
		if ( targ->client )
		{
			targ->locationDamage[hitLoc] += take;
		}
		if ( targ->health > 0 && targ->NPC && targ->NPC->surrenderTime > level.time )
		{//he was surrendering, goes down with one hit
			targ->health = 0;
		}
		if ( targ->health <= 0 ) 
		{

			if ( knockback && (dflags&DAMAGE_DEATH_KNOCKBACK) )//&& targ->client 
			{//only do knockback on death
				if ( mod == MOD_FLECHETTE )
				{//special case because this is shotgun-ish damage, we need to multiply the knockback
					knockback *= 12;//*6 for 6 flechette shots
				}
				G_ApplyKnockback( targ, newDir, knockback );
			}

			if ( client )
				targ->flags |= FL_NO_KNOCKBACK;

			if (targ->health < -999)
				targ->health = -999;

			// If we are a breaking glass brush, store the damage point so we can do cool things with it.
			if ( targ->svFlags & SVF_GLASS_BRUSH )
			{
				VectorCopy( point, targ->pos1 );
				VectorCopy( dir, targ->pos2 );
			}
			if ( targ->client )
			{//HACK
				if ( point )
				{
					VectorCopy( point, targ->pos1 );
				}
				else
				{
					VectorCopy( targ->currentOrigin, targ->pos1 );
				}
			}
			if ( !alreadyDead || !targ->enemy )
			{//just killed or didn't have an enemy before
				targ->enemy = attacker;
			}
			GEntity_DieFunc (targ, inflictor, attacker, take, mod, hitLoc);
		}
		else 
		{
			GEntity_PainFunc( targ, inflictor, attacker, point, take, mod, hitLoc );
			if ( targ->s.number == 0 )
			{//player run painscript
				G_ActivateBehavior( targ, BSET_PAIN );
				if ( targ->health <= 25 )
				{
					G_ActivateBehavior( targ, BSET_FLEE );
				}
			}
		}
	}
}


/*
============
CanDamage

Returns qtrue if the inflictor can directly damage the target.  Used for
explosions and melee attacks.
============
*/
qboolean CanDamage (gentity_t *targ, vec3_t origin) {
	vec3_t	dest;
	trace_t	tr;
	vec3_t	midpoint;

	// use the midpoint of the bounds instead of the origin, because
	// bmodels may have their origin at 0,0,0
	VectorAdd (targ->absmin, targ->absmax, midpoint);
	VectorScale (midpoint, 0.5, midpoint);

	VectorCopy (midpoint, dest);
	gi.trace ( &tr, origin, vec3_origin, vec3_origin, dest, ENTITYNUM_NONE, MASK_SOLID);
	if (( tr.fraction == 1.0 && !(targ->contents & MASK_SOLID)) || tr.entityNum == targ->s.number ) // if we also test the entitynum's we can bust up bbrushes better!
		return qtrue;

	// this should probably check in the plane of projection, 
	// rather than in world coordinate, and also include Z
	VectorCopy (midpoint, dest);
	dest[0] += 15.0;
	dest[1] += 15.0;
	gi.trace ( &tr, origin, vec3_origin, vec3_origin, dest, ENTITYNUM_NONE, MASK_SOLID);
	if (( tr.fraction == 1.0 && !(targ->contents & MASK_SOLID)) || tr.entityNum == targ->s.number )
		return qtrue;

	VectorCopy (midpoint, dest);
	dest[0] += 15.0;
	dest[1] -= 15.0;
	gi.trace ( &tr, origin, vec3_origin, vec3_origin, dest, ENTITYNUM_NONE, MASK_SOLID);
	if (( tr.fraction == 1.0 && !(targ->contents & MASK_SOLID)) || tr.entityNum == targ->s.number )
		return qtrue;

	VectorCopy (midpoint, dest);
	dest[0] -= 15.0;
	dest[1] += 15.0;
	gi.trace ( &tr, origin, vec3_origin, vec3_origin, dest, ENTITYNUM_NONE, MASK_SOLID);
	if (( tr.fraction == 1.0 && !(targ->contents & MASK_SOLID)) || tr.entityNum == targ->s.number )
		return qtrue;

	VectorCopy (midpoint, dest);
	dest[0] -= 15.0;
	dest[1] -= 15.0;
	gi.trace ( &tr, origin, vec3_origin, vec3_origin, dest, ENTITYNUM_NONE, MASK_SOLID);
	if (( tr.fraction == 1.0 && !(targ->contents & MASK_SOLID)) || tr.entityNum == targ->s.number )
		return qtrue;


	return qfalse;
}


/*
============
G_RadiusDamage
============
*/
void G_RadiusDamage ( vec3_t origin, gentity_t *attacker, float damage, float radius,
					 gentity_t *ignore, int mod) {
	float		points, dist;
	gentity_t	*ent;
	gentity_t	*entityList[MAX_GENTITIES];
	int			numListedEntities;
	vec3_t		mins, maxs;
	vec3_t		v;
	vec3_t		dir;
	int			i, e;

	if ( radius < 1 ) {
		radius = 1;
	}

	for ( i = 0 ; i < 3 ; i++ ) {
		mins[i] = origin[i] - radius;
		maxs[i] = origin[i] + radius;
	}

	numListedEntities = gi.EntitiesInBox( mins, maxs, entityList, MAX_GENTITIES );

	for ( e = 0 ; e < numListedEntities ; e++ ) {
		ent = entityList[ e ];

		if ( ent == ignore )
			continue;
		if ( !ent->takedamage )
			continue;
		if ( !ent->contents )
			continue;

		// find the distance from the edge of the bounding box
		for ( i = 0 ; i < 3 ; i++ ) {
			if ( origin[i] < ent->absmin[i] ) {
				v[i] = ent->absmin[i] - origin[i];
			} else if ( origin[i] > ent->absmax[i] ) {
				v[i] = origin[i] - ent->absmax[i];
			} else {
				v[i] = 0;
			}
		}

		dist = VectorLength( v );
		if ( dist >= radius ) {
			continue;
		}

		points = damage * ( 1.0 - dist / radius );

		if (CanDamage (ent, origin)) 
		{//FIXME: still do a little damage in in PVS and close?
			if ( ent->svFlags & (SVF_GLASS_BRUSH|SVF_BBRUSH) )
			{
				VectorAdd( ent->absmin, ent->absmax, v );
				VectorScale( v, 0.5f, v );
			}
			else
			{
				VectorCopy( ent->currentOrigin, v );
			}

			VectorSubtract( v, origin, dir);
			// push the center of mass higher than the origin so players
			// get knocked into the air more
			dir[2] += 24;

			if ( ent->svFlags & SVF_GLASS_BRUSH )
			{
				if ( points > 1.0f )
				{
					// we want to cap this at some point, otherwise it just gets crazy
					if ( points > 6.0f )
					{
						VectorScale( dir, 6.0f, dir );
					}
					else
					{
						VectorScale( dir, points, dir );
					}
				}

				ent->splashRadius = radius;// * ( 1.0 - dist / radius );
			}

			G_Damage (ent, NULL, attacker, dir, origin, (int)points, DAMAGE_RADIUS, mod);
		}
	}
}
