//NPC_reactions.cpp

// leave this line at the top for all NPC_xxxx.cpp files...
#include "g_headers.h"




#include "b_local.h"
#include "anims.h"
#include "g_functions.h"
#include "characters.h"
#include "wp_saber.h"

extern qboolean G_CheckForStrongAttackMomentum( gentity_t *self );
extern void G_AddVoiceEvent( gentity_t *self, int event, int speakDebounceTime );
extern void G_SoundOnEnt( gentity_t *ent, soundChannel_t channel, const char *soundPath );
float g_crosshairEntDist = Q3_INFINITE;
int g_crosshairSameEntTime = 0;
int g_crosshairEntNum = ENTITYNUM_NONE;
int g_crosshairEntTime = 0;
extern int	teamLastEnemyTime[];
extern	cvar_t	*g_spskill;
extern int PM_AnimLength( int index, animNumber_t anim );
extern void cgi_S_StartSound( vec3_t origin, int entityNum, int entchannel, sfxHandle_t sfx );
extern qboolean Q3_TaskIDPending( gentity_t *ent, taskID_t taskType );
extern int PM_PickAnim( gentity_t *self, int minAnim, int maxAnim );
extern qboolean NPC_CheckLookTarget( gentity_t *self );
extern void NPC_SetLookTarget( gentity_t *self, int entNum, int clearTime );
extern qboolean Jedi_WaitingAmbush( gentity_t *self );
extern void Jedi_Ambush( gentity_t *self );

extern qboolean PM_SaberInSpecialAttack( int anim );
extern qboolean PM_SpinningSaberAnim( int anim );
extern qboolean PM_SpinningAnim( int anim );
extern qboolean PM_InKnockDown( playerState_t *ps );
extern qboolean PM_CrouchAnim( int anim );
extern qboolean PM_FlippingAnim( int anim );
extern qboolean PM_RollingAnim( int anim );
extern qboolean PM_InCartwheel( int anim );

extern qboolean	stop_icarus;
/*
-------------------------
NPC_CheckAttacker
-------------------------
*/

static void NPC_CheckAttacker( gentity_t *other, int mod )
{
	//FIXME: I don't see anything in here that would stop teammates from taking a teammate
	//			as an enemy.  Ideally, there would be code before this to prevent that from
	//			happening, but that is presumptuous.
	
	//valid ent - FIXME: a VALIDENT macro would be nice here
	if ( !other )
		return;

	if ( !other->inuse )
		return;

	//Don't take a target that doesn't want to be
	if ( other->flags & FL_NOTARGET ) 
		return;

	if ( NPC->svFlags & SVF_LOCKEDENEMY )
	{//IF LOCKED, CANNOT CHANGE ENEMY!!!!!
		return;
	}

	//If we haven't taken a target, just get mad
	if ( NPC->enemy == NULL )//was using "other", fixed to NPC
	{
		G_SetEnemy( NPC, other );
		return;
	}

	//we have an enemy, see if he's dead
	if ( NPC->enemy->health <= 0 )
	{
		G_ClearEnemy( NPC );
		G_SetEnemy( NPC, other );
		return;
	}

	//Don't take the same enemy again
	if ( other == NPC->enemy )
		return;

	if ( NPC->client->ps.weapon == WP_SABER )
	{//I'm a jedi
		if ( mod == MOD_SABER )
		{//I was hit by a saber  FIXME: what if this was a thrown saber?
			//always switch to this enemy if I'm a jedi and hit by another saber
			G_ClearEnemy( NPC );
			G_SetEnemy( NPC, other );
			return;
		}
	}
	//Special case player interactions
	if ( other == &g_entities[0] )
	{
		//Account for the skill level to skew the results
		float	luckThreshold;

		switch ( g_spskill->integer )
		{
		//Easiest difficulty, mild chance of picking up the player
		case 0:
			luckThreshold = 0.9f;
			break;

		//Medium difficulty, half-half chance of picking up the player
		case 1:
			luckThreshold = 0.5f;
			break;

		//Hardest difficulty, always turn on attacking player
		case 2:
		default:
			luckThreshold = 0.0f;
			break;
		}

		//Randomly pick up the target
		if ( random() > luckThreshold )
		{
			G_ClearEnemy( other );
			other->enemy = NPC;
		}

		return;
	}
}

void NPC_SetPainEvent( gentity_t *self )
{
	if ( !self->NPC || !(self->NPC->aiFlags&NPCAI_DIE_ON_IMPACT) )
	{
	// no more borg
	//	if( self->client->playerTeam != TEAM_BORG )
	//	{
			if ( !Q3_TaskIDPending( self, TID_CHAN_VOICE ) )
			{
				G_AddEvent( self, EV_PAIN, floor((float)self->health/self->max_health*100.0f) );
			}
	//	}
	}
}

/*
-------------------------
NPC_GetPainChance
-------------------------
*/

#define	MIN_FLINCH_DAMAGE	50

float NPC_GetPainChance( gentity_t *self, int damage )
{
	if ( damage > self->max_health/2.0f )//MIN_FLINCH_DAMAGE )
		return 1.0f;

	switch ( g_spskill->integer )
	{
	case 0:	//easy
		return 0.75f;
		break;

	case 1://med
		return 0.35f;
		break;

	case 2://hard
	default:
		return 0.05f;
		break;
	}
}

/*
-------------------------
NPC_ChoosePainAnimation
-------------------------
*/

#define	MIN_PAIN_TIME	200

extern int G_PickPainAnim( gentity_t *self, vec3_t point, int damage, int hitLoc );
void NPC_ChoosePainAnimation( gentity_t *self, gentity_t *other, vec3_t point, int damage, int mod, int hitLoc )
{
	//If we've already taken pain, then don't take it again
	if ( level.time < self->painDebounceTime && mod != MOD_ELECTROCUTE && mod != MOD_MELEE )
	{//FIXME: if hit while recoving from losing a saber lock, we should still play a pain anim?
		return;
	}

	int		pain_anim = -1;
	float	pain_chance;
	
	if ( self->client->NPC_class == CLASS_GALAKMECH )
	{
		if ( self->client->ps.powerups[PW_GALAK_SHIELD] )
		{//shield up
			return;
		}
		else if ( self->health > 200 && damage < 100 )
		{//have a *lot* of health
			pain_chance = 0.05f;
		}
		else
		{//the lower my health and greater the damage, the more likely I am to play a pain anim
			pain_chance = (200.0f-self->health)/100.0f + damage/50.0f;
		}
	}
	else if ( self->client && self->client->playerTeam == TEAM_PLAYER && other && !other->s.number )
	{//ally shot by player always complains
		pain_chance = 1.1f;
	}
	else 
	{
		if ( other && other->s.weapon == WP_SABER || mod == MOD_ELECTROCUTE || mod == MOD_CRUSH/*FIXME:MOD_FORCE_GRIP*/ )
		{
			pain_chance = 1.0f;//always take pain from saber
		}
		else if ( mod == MOD_MELEE )
		{//higher in rank (skill) we are, less likely we are to be fazed by a punch
			pain_chance = 1.0f - ((RANK_CAPTAIN-self->NPC->rank)/(float)RANK_CAPTAIN);
		}
		else
		{
			pain_chance = NPC_GetPainChance( self, damage );
		}
		if ( self->client->NPC_class == CLASS_DESANN )
		{
			pain_chance *= 0.5f;
		}
	}

	//See if we're going to flinch
	if ( random() < pain_chance )
	{
		//Pick and play our animation
		if ( !(self->client->ps.eFlags&EF_FORCE_GRIPPED) )
		{//not being force-gripped
			if ( G_CheckForStrongAttackMomentum( self )
				|| PM_SpinningAnim( self->client->ps.legsAnim )
				|| PM_SaberInSpecialAttack( self->client->ps.torsoAnim )
				|| PM_InKnockDown( &self->client->ps )
				|| PM_RollingAnim( self->client->ps.legsAnim )
				|| (PM_FlippingAnim( self->client->ps.legsAnim )&&!PM_InCartwheel( self->client->ps.legsAnim )) )
			{//strong attacks, rolls, knockdowns, flips and spins cannot be interrupted by pain
			}
			else
			{//play an anim
				if ( self->client->NPC_class == CLASS_GALAKMECH )
				{//only has 1 for now
					//FIXME: never plays this, it seems...
					pain_anim = BOTH_PAIN1;
				}
				else if ( mod == MOD_MELEE )
				{
					pain_anim = PM_PickAnim( self, BOTH_PAIN2, BOTH_PAIN3 );
				}
				else if ( self->s.weapon == WP_SABER )
				{//temp HACK: these are the only 2 pain anims that look good when holding a saber
					pain_anim = PM_PickAnim( self, BOTH_PAIN2, BOTH_PAIN3 );
				}
				else if ( mod != MOD_ELECTROCUTE )
				{
					pain_anim = G_PickPainAnim( self, point, damage, hitLoc );
				}

				if ( pain_anim == -1 )
				{
					pain_anim = PM_PickAnim( self, BOTH_PAIN1, BOTH_PAIN19 );
				}
				self->client->ps.saberAnimLevel = FORCE_LEVEL_1;//next attack must be a quick attack
				self->client->ps.saberMove = LS_READY;//don't finish whatever saber move you may have been in
				int parts = SETANIM_BOTH;
				if ( PM_CrouchAnim( self->client->ps.legsAnim ) || PM_InCartwheel( self->client->ps.legsAnim ) )
				{
					parts = SETANIM_LEGS;
				}
				NPC_SetAnim( self, parts, pain_anim, SETANIM_FLAG_OVERRIDE|SETANIM_FLAG_HOLD );
			}
			NPC_SetPainEvent( self );
		}
		else
		{
			G_AddVoiceEvent( self, Q_irand(EV_CHOKE1, EV_CHOKE3), 0 );
		}
		
		//Setup the timing for it
		if ( mod == MOD_ELECTROCUTE )
		{
			self->painDebounceTime = level.time + 4000;
		}
		self->painDebounceTime = level.time + PM_AnimLength( self->client->clientInfo.animFileIndex, (animNumber_t) pain_anim );
		self->client->fireDelay = 0;
	}
}

/*
===============
NPC_Pain
===============
*/
void NPC_Pain( gentity_t *self, gentity_t *inflictor, gentity_t *other, vec3_t point, int damage, int mod, int hitLoc ) 
{
	team_t otherTeam = TEAM_FREE;

	if ( self->NPC == NULL ) 
		return;

	if ( other == NULL ) 
		return;

	//or just remove ->pain in player_die?
	if ( self->client->ps.pm_type == PM_DEAD )
		return;

	if ( other == self ) 
		return;

	//MCG: Ignore damage from your own team for now
	if ( other->client )
	{
		otherTeam = other->client->playerTeam;
	//	if ( otherTeam == TEAM_DISGUISE )
	//	{
	//		otherTeam = TEAM_PLAYER;
	//	}
	}

	if ( self->client->playerTeam && other->client && otherTeam == self->client->playerTeam && (!player->client->ps.viewEntity || other->s.number != player->client->ps.viewEntity)) 
	{//hit by a teammate
		if ( other != self->enemy && self != other->enemy )
		{//we weren't already enemies
			if ( self->enemy || other->enemy 
				|| (other->s.number&&other->s.number!=player->client->ps.viewEntity) 
				/*|| (!other->s.number&&Q_irand( 0, 3 ))*/ )
			{//if one of us actually has an enemy already, it's okay, just an accident OR wasn't hit by player or someone controlled by player OR player hit ally and didn't get 25% chance of getting mad (FIXME:accumulate anger+base on diff?)
				//FIXME: player should have to do a certain amount of damage to ally or hit them several times to make them mad
				//Still run pain and flee scripts
				if ( self->client && self->NPC )
				{//Run any pain instructions
					if ( self->health <= (self->max_health/3) && G_ActivateBehavior(self, BSET_FLEE) )
					{
						
					}
					else// if( VALIDSTRING( self->behaviorSet[BSET_PAIN] ) )
					{
						G_ActivateBehavior(self, BSET_PAIN);
					}
				}
				if ( damage != -1 )
				{//-1 == don't play pain anim
					//Set our proper pain animation
					NPC_ChoosePainAnimation( self, other, point, damage, mod, hitLoc );
				}
				return;
			}
			else if ( self->NPC && !other->s.number )//should be assumed, but...
			{//dammit, stop that!
				if ( self->NPC->ffireCount < 3+((2-g_spskill->integer)*2) )
				{//not mad enough yet
					//Com_Printf( "chck: %d < %d\n", self->NPC->ffireCount, 3+((2-g_spskill->integer)*2) );
					if ( damage != -1 )
					{//-1 == don't play pain anim
						//Set our proper pain animation
						NPC_ChoosePainAnimation( self, other, point, damage, mod, hitLoc );
					}
					return;
				}
				else
				{//okay, we're going to turn on our ally, we need to 
					self->NPC->behaviorState = self->NPC->tempBehavior = self->NPC->defaultBehavior = BS_DEFAULT;
					other->flags &= ~FL_NOTARGET;
					self->svFlags &= ~(SVF_IGNORE_ENEMIES|SVF_ICARUS_FREEZE|SVF_NO_COMBAT_SOUNDS);
					G_SetEnemy( self, other );
					self->svFlags |= SVF_LOCKEDENEMY;
					self->NPC->scriptFlags &= ~(SCF_DONT_FIRE|SCF_CROUCHED|SCF_WALKING|SCF_NO_COMBAT_TALK|SCF_FORCED_MARCH);
					self->NPC->scriptFlags |= (SCF_CHASE_ENEMIES|SCF_NO_MIND_TRICK);
					stop_icarus = qtrue;
				}
			}
		}
	}

	SaveNPCGlobals();
	SetNPCGlobals( self );

	//Do extra bits
	if ( NPCInfo->ignorePain == qfalse )
	{
		if ( NPCInfo->charmedTime && NPCInfo->charmedTime < level.time && NPC->client )
		{//charmed enemy
			team_t	saveTeam = NPC->client->enemyTeam;
			NPC->client->enemyTeam = NPC->client->playerTeam;
			NPC->client->playerTeam = saveTeam;
			NPC->client->leader = NULL;
			if ( NPCInfo->tempBehavior == BS_FOLLOW_LEADER )
			{
				NPCInfo->tempBehavior = BS_DEFAULT;
			}
			G_ClearEnemy( NPC );
		}
		NPCInfo->charmedTime = NPCInfo->confusionTime = 0;//clear any charm or confusion, regardless
		//Check to take a new enemy
		if ( NPC->enemy != other )
		{//not already mad at them
			NPC_CheckAttacker( other, mod );
		}

		if ( damage != -1 )
		{//-1 == don't play pain anim
			//Set our proper pain animation
			NPC_ChoosePainAnimation( self, other, point, damage, mod, hitLoc );
		}
	}

	//Attempt to run any pain instructions
	if(self->client && self->NPC)
	{
		//FIXME: This needs better heuristics perhaps
		if(self->health <= (self->max_health/3) && G_ActivateBehavior(self, BSET_FLEE) )
		{
		}
		else //if( VALIDSTRING( self->behaviorSet[BSET_PAIN] ) )
		{
			G_ActivateBehavior(self, BSET_PAIN);
		}
	}

	//Attempt to fire any paintargets we might have
	if( self->paintarget && self->paintarget[0] )
	{
		G_UseTargets2(self, other, self->paintarget);
	}

	RestoreNPCGlobals();
}

/*
-------------------------
NPC_Touch
-------------------------
*/
extern qboolean INV_SecurityKeyGive( gentity_t *target, const char *keyname );
void NPC_Touch(gentity_t *self, gentity_t *other, trace_t *trace) 
{
	if(!self->NPC)
		return;

	SaveNPCGlobals();
	SetNPCGlobals( self );

	if ( self->message && self->health <= 0 )
	{//I am dead and carrying a key
		if ( other && player && player->health > 0 && other == player )
		{//player touched me
			char *text;
			qboolean	keyTaken;
			//give him my key
			if ( Q_stricmp( "goodie", self->message ) == 0 )
			{//a goodie key
				if ( (keyTaken = INV_GoodieKeyGive( other )) == qtrue )
				{
					text = "cp @INGAME_TOOK_IMPERIAL_GOODIE_KEY";
				}
				else
				{
					text = "cp @INGAME_CANT_CARRY_GOODIE_KEY";
				}
			}
			else
			{//a named security key
				if ( (keyTaken = INV_SecurityKeyGive( player, self->message )) == qtrue )
				{
					text = "cp @INGAME_TOOK_IMPERIAL_SECURITY_KEY";
				}
				else
				{
					text = "cp @INGAME_CANT_CARRY_SECURITY_KEY";
				}
			}
			if ( keyTaken )
			{//remove my key
				gi.G2API_SetSurfaceOnOff( &self->ghoul2[self->playerModel], "l_arm_key", 0x00000002 );
				self->message = NULL;
				//FIXME: temp pickup sound
				G_Sound( player, G_SoundIndex( "sound/weapons/key_pkup.wav" ) );
				//FIXME: need some event to pass to cgame for sound/graphic/message?
			}
			//FIXME: temp message
			gi.SendServerCommand( NULL, text );
		}
	}

	if ( other->client ) 
	{//FIXME:  if pushing against another bot, both ucmd.rightmove = 127???
		//Except if not facing one another...
		if ( other->health > 0 ) 
		{
			NPCInfo->touchedByPlayer = other;
		}

		if ( other == NPCInfo->goalEntity ) 
		{
			NPCInfo->aiFlags |= NPCAI_TOUCHED_GOAL;
		}

		if( !(self->svFlags&SVF_LOCKEDENEMY) && !(self->svFlags&SVF_IGNORE_ENEMIES) && !(other->flags & FL_NOTARGET) )
		{
			if ( self->client->enemyTeam )
			{//See if we bumped into an enemy
				if ( other->client->playerTeam == self->client->enemyTeam )
				{//bumped into an enemy
					if( NPCInfo->behaviorState != BS_HUNT_AND_KILL && !NPCInfo->tempBehavior )
					{//MCG - Begin: checking specific BS mode here, this is bad, a HACK
						//FIXME: not medics?
						if ( NPC->enemy != other )
						{//not already mad at them
							G_SetEnemy( NPC, other );
						}
		//				NPCInfo->tempBehavior = BS_HUNT_AND_KILL;
					}
				}
			}
		}

		//FIXME: do this if player is moving toward me and with a certain dist?
		/*
		if ( other->s.number == 0 && self->client->playerTeam == other->client->playerTeam )
		{
			VectorAdd( self->s.pushVec, other->client->ps.velocity, self->s.pushVec );
		}
		*/
	}
	else 
	{//FIXME: check for SVF_NONNPC_ENEMY flag here?
		if ( other == NPCInfo->goalEntity ) 
		{
			NPCInfo->aiFlags |= NPCAI_TOUCHED_GOAL;
		}
	}

	RestoreNPCGlobals();
}

/*
-------------------------
NPC_TempLookTarget
-------------------------
*/

void NPC_TempLookTarget( gentity_t *self, int lookEntNum, int minLookTime, int maxLookTime )
{
	if ( !self->client )
	{
		return;
	}

	if ( !minLookTime )
	{
		minLookTime = 1000;
	}

	if ( !maxLookTime )
	{
		maxLookTime = 1000;
	}

	if ( !NPC_CheckLookTarget( self ) )
	{//Not already looking at something else
		//Look at him for 1 to 3 seconds
		NPC_SetLookTarget( self, lookEntNum, level.time + Q_irand( minLookTime, maxLookTime ) );
	}
}

void NPC_Respond( gentity_t *self, int userNum )
{
	/*
	int event;

	if ( Q_irand( 0, 1 ) )
	{
		event = Q_irand(EV_RESPOND1, EV_RESPOND3);
	}
	else
	{
		event = Q_irand(EV_BUSY1, EV_BUSY3);
	}
	*/

	if ( !Q_irand( 0, 1 ) )
	{//set looktarget to them for a second or two
		NPC_TempLookTarget( self, userNum, 1000, 3000 );
	}

	//G_AddVoiceEvent( self, event, 3000 );
}

void WaitNPCRespond ( gentity_t *self )
{
	//make sure the responding ent is still valid
	if ( !self->enemy || !self->enemy->client || !self->enemy->NPC )
	{
		G_FreeEntity( self );
		return;
	}

	if ( gi.VoiceVolume[0] )
	{//player is still talking
		self->nextthink = level.time + 500;
		//set enemy to not respond for a bit longer
		self->enemy->NPC->blockedSpeechDebounceTime = level.time + 1000;
		return;
	}

	if ( self->enemy->health <= 0 || (!self->alt_fire && (self->enemy->NPC->scriptFlags&SCF_NO_RESPONSE)) )
	{
		G_FreeEntity( self );
		return;
	}

	//set enemy to be ready to respond
	self->enemy->NPC->blockedSpeechDebounceTime = 0;

	if ( self->alt_fire )
	{//Run the NPC's usescript
		G_ActivateBehavior( self->enemy, BSET_USE );
	}
	else
	{//make them respond generically
		NPC_Respond( self->enemy, 0 );
	}
}

/*
-------------------------
NPC_UseResponse
-------------------------
*/

void NPC_UseResponse( gentity_t *self, gentity_t *user, qboolean useWhenDone )
{
	if ( !self->NPC || !self->client )
	{
		return;
	}

	if ( user->s.number != 0 )
	{//not used by the player
		if ( useWhenDone )
		{
			G_ActivateBehavior( self, BSET_USE );
		}
		return;
	}

	if ( user->client && self->client->playerTeam != user->client->playerTeam )
	{//only those on the same team react
		if ( useWhenDone )
		{
			G_ActivateBehavior( self, BSET_USE );
		}
		return;
	}

	if ( self->NPC->blockedSpeechDebounceTime > level.time )
	{//I'm not responding right now
		return;
	}

	if ( gi.VoiceVolume[self->s.number] )
	{//I'm talking already
		if ( !useWhenDone )
		{//you're not trying to use me
			return;
		}
	}

	if ( useWhenDone )
	{
		G_ActivateBehavior( self, BSET_USE );
	}
	else
	{
		NPC_Respond( self, user->s.number );
	}
}

/*
-------------------------
NPC_Use
-------------------------
*/
extern void Add_Batteries( gentity_t *ent, int *count );

void NPC_Use( gentity_t *self, gentity_t *other, gentity_t *activator ) 
{
	if (self->client->ps.pm_type == PM_DEAD)
	{//or just remove ->pain in player_die?
		return;
	}

	SaveNPCGlobals();
	SetNPCGlobals( self );

	if(self->client && self->NPC)
	{
		if ( Jedi_WaitingAmbush( NPC ) )
		{
			Jedi_Ambush( NPC );
		}
		//Run any use instructions
		if ( activator && activator->s.number == 0 && self->client->NPC_class == CLASS_GONK )
		{
			// must be using the gonk, so attempt to give battery power.
			// NOTE: this will steal up to MAX_BATTERIES for the activator, leaving the residual on the gonk for potential later use.
			Add_Batteries( activator, &self->client->ps.batteryCharge );
		}
		// Not using MEDICs anymore
/*
		if ( self->NPC->behaviorState == BS_MEDIC_HIDE && activator->client )
		{//Heal me NOW, dammit!
			if ( activator->health < activator->max_health )
			{//person needs help
				if ( self->NPC->eventualGoal != activator )
				{//not my current patient already
					NPC_TakePatient( activator );
					G_ActivateBehavior( self, BSET_USE );
				}
			}
			else if ( !self->enemy && activator->s.number == 0 && !gi.VoiceVolume[self->s.number] && !(self->NPC->scriptFlags&SCF_NO_RESPONSE) )
			{//I don't have an enemy and I'm not talking and I was used by the player
				NPC_UseResponse( self, other, qfalse );
			}
		}
*/
//		else if ( self->behaviorSet[BSET_USE] )
		if ( self->behaviorSet[BSET_USE] )
		{
			NPC_UseResponse( self, other, qtrue );
		}
//		else if ( isMedic( self ) )
//		{//Heal me NOW, dammit!
//			NPC_TakePatient( activator );
//		}
		else if ( !self->enemy && activator->s.number == 0 && !gi.VoiceVolume[self->s.number] && !(self->NPC->scriptFlags&SCF_NO_RESPONSE) )
		{//I don't have an enemy and I'm not talking and I was used by the player
			NPC_UseResponse( self, other, qfalse );
		}
	}

	RestoreNPCGlobals();
}

void NPC_CheckPlayerAim( void )
{
	//FIXME: need appropriate dialogue
	/*
	gentity_t *player = &g_entities[0];

	if ( player && player->client && player->client->ps.weapon > (int)(WP_NONE) && player->client->ps.weapon < (int)(WP_TRICORDER) )
	{//player has a weapon ready
		if ( g_crosshairEntNum == NPC->s.number && level.time - g_crosshairEntTime < 200 
			&& g_crosshairSameEntTime >= 3000 && g_crosshairEntDist < 256 )
		{//if the player holds the crosshair on you for a few seconds
			//ask them what the fuck they're doing
			G_AddVoiceEvent( NPC, Q_irand( EV_FF_1A, EV_FF_1C ), 0 );
		}
	}
	*/
}

void NPC_CheckAllClear( void )
{
	//FIXME: need to make this happen only once after losing enemies, not over and over again
	/*
	if ( NPC->client && !NPC->enemy && level.time - teamLastEnemyTime[NPC->client->playerTeam] > 10000 )
	{//Team hasn't seen an enemy in 10 seconds
		if ( !Q_irand( 0, 2 ) )
		{
			G_AddVoiceEvent( NPC, Q_irand(EV_SETTLE1, EV_SETTLE3), 3000 );
		}
	}
	*/
}