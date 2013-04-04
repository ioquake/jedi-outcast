//NPC_formation.cpp

//Question- possible to make one formation one unit of another larger formation?
//#define NPC_FORMATION_CPP

// leave this line at the top for all NPC_xxxx.cpp files...
#include "g_headers.h"





#include "b_local.h"
#include "anims.h"
#include "g_nav.h"
#include "g_squad.h"

#include "g_navigator.h"

extern	CNavigator	navigator;

#define MAX_INTEREST_DIST	( 256 * 256 )
extern qboolean G_ClearLineOfSight( const vec3_t point1, const vec3_t point2, int ignore, int clipmask );
extern void G_AddVoiceEvent( gentity_t *self, int event, int speakDebounceTime );
extern void NPC_AimWiggle( vec3_t enemy_org );
extern int PM_PickAnim( gentity_t *self, int minAnim, int maxAnim );
extern qboolean PM_HasAnimation( gentity_t *ent, int animation );
//=========================================================================================
/*
-------------------------
NPC_SquadIdle
-------------------------
*/

//FIXME: This is a lovely legacy solution...
extern int teamLastEnemyTime[];
static int squadSpeechDebounceTime = 0;
extern int teamEnemyCount[TEAM_NUM_TEAMS];
void NPC_SquadIdle( void )
{
	//say something when we've finished fighting for about 3 seconds and it's all clear
	if ( ( level.time - teamLastEnemyTime[TEAM_STARFLEET] > 3000 ) && 
		 ( level.time - teamLastEnemyTime[TEAM_STARFLEET] < 3500 ) )
	{
		if ( Q_irand(0, 3) == 0 )
		{//indicate all clear
			if ( squadSpeechDebounceTime < level.time )
			{
				G_AddVoiceEvent( NPC, Q_irand( EV_SETTLE1, EV_SETTLE3 ), 3000 );
				squadSpeechDebounceTime = level.time + 2000;
			}
		}
		else
		{//don't say it this time
			squadSpeechDebounceTime = level.time + 2000;
		}
	}

	//regenerate 10 points of health per second when not in combat
	if ( NPC->health < NPC->max_health && (NPC->flags & FL_UNDYING) )
	{
		NPC->health++;
	}
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#define	FORMATION_GAP_DIAG			32
#define	FORMATION_GAP_FBLR			64
#define MAX_DIST_TO_LEADER_SQUARED	(128 * 128)
#define	MAX_IDLE_TIME				7

static vec3_t	form_forward;
static vec3_t	form_right;
static float	form_speed;
static float	form_forward_speed;
static vec3_t	form_dir;
static vec3_t	form_angles;
extern int		form_shot_traces;
extern int		form_updateseg_traces;
extern int		form_leaderseg_traces;
extern int		form_closestSP_traces;
extern int		form_clearpath_traces;

extern qboolean NAV_ClearPathToPoint( gentity_t *self, vec3_t pmins, vec3_t pmaxs, vec3_t point, int clipmask );
extern lookMode_t NPC_FindLocalInterestPoint( gentity_t *self, vec3_t lookVec );
extern int PM_AnimLength( int index, animNumber_t anim );
extern void NPC_UpdateLastSquadPoint ( gentity_t *self );
extern void G_UpdateFormationGoals( gentity_t *self );
extern qboolean ValidAnimFileIndex( int index );
extern qboolean NPC_BSPointCombat( void );
extern void G_AddVoiceEvent( gentity_t *self, int event, int speakDebounceTime );

/*
-------------------------
NPC_BlockingPlayer
-------------------------
*/

static qboolean NPC_BlockingPlayer( vec3_t blockDir )
{
	vec3_t	playerDir, velocityDir;

	//Get the player's move direction and speed
	if ( VectorNormalize2( g_entities[0].client->ps.velocity, velocityDir ) == 0 )
		return qfalse;

	//Get our direction to the player
	VectorSubtract( NPC->currentOrigin, g_entities[0].currentOrigin, playerDir );
	float distance = VectorNormalize( playerDir );

	//Getting really close, let's back off
	if ( distance < 32 )
	{
		VectorCopy( playerDir, blockDir );
		return qtrue;
	}

	//Too far, forget it
	if ( distance > 128 )
		return qfalse;

	//See if it's even feasible
	float dot = DotProduct( playerDir, velocityDir );

	if ( dot < 0.85 )
		return qfalse;

	vec3_t	testPos;
	vec3_t	ptmins, ptmaxs, tmins, tmaxs;

	VectorMA( g_entities[0].currentOrigin, distance, velocityDir, testPos );

	VectorAdd( NPC->currentOrigin, NPC->mins, tmins );
	VectorAdd( NPC->currentOrigin, NPC->maxs, tmaxs );

	VectorAdd( testPos, g_entities[0].mins, ptmins );
	VectorAdd( testPos, g_entities[0].maxs, ptmaxs );

	if ( G_BoundsOverlap( ptmins, ptmaxs, tmins, tmaxs ) )
	{
		VectorCopy( velocityDir, blockDir );
		return qtrue;
	}

	return qfalse;
}

/*
-------------------------
NPC_Unobstruct
-------------------------
*/

#define	AVOID_ITERATIONS	5

void NPC_Unobstruct( vec3_t dir )
{
	vec3_t		moveAngles, testPos;
	trace_t		tr;

	vectoangles( dir, moveAngles );

	moveAngles[YAW] = AngleNormalize360( moveAngles[YAW] - 90 );

	for ( int i = 0; i < AVOID_ITERATIONS; i++ )
	{
		AngleVectors( moveAngles, testPos, NULL, NULL );
		VectorMA( NPC->currentOrigin, 64, testPos, testPos );

		if ( NAV_CheckAhead( NPC, testPos, tr, NPC->clipmask|CONTENTS_BOTCLIP ) )
		{
			NPCInfo->combatMove = qtrue;
			ucmd.buttons |= BUTTON_WALKING;
			NPC_SetMoveGoal( NPC, testPos, 4, qtrue );
			NPC_SlideMoveToGoal();
			NPC_UpdateAngles( qtrue, qtrue );
			return;
		}

		moveAngles[YAW] = AngleNormalize360( moveAngles[YAW] + 45 );
	}
}

/*
-------------------------
G_CreateFormation
-------------------------
*/

void G_CreateFormation( gentity_t *self )
{
	gentity_t	*found = NULL, *last;
	int			num = 0, i, j;

	//Must be a valid target
	if( ( self == NULL ) || ( self->client == NULL ) )
		return;

	//Must have a squad name
	if( VALIDSTRING( self->client->squadname ) == false )
		return;

	//assign all the followers:
	last = self;
	for ( found = &g_entities[1], j = 1; j < ENTITYNUM_WORLD; j++, found++ )
	{
		//found = &g_entities[j];

		//Don't need to assign self
		if ( found == self )
			continue;

		if ( !found  || !found->client )
			continue;

		//Can't assign a dead NPC or non-NPC
		if ( !found->NPC || found->health<=0 )
			continue;

		//Must have a valid squad name
		if ( VALIDSTRING( found->client->squadname ) == false )
			continue;

		//Must share our squadname
		if ( Q_stricmp( self->client->squadname, found->client->squadname ) != 0 )
			continue;

		//Can't possibly give him a path to follow
		if ( VALIDSTRING( found->script_targetname ) == false )
			continue;

		//Go through each squadPath and give them theirs
		for ( i = 0; i < num_squad_paths; i++ )
		{
			if ( Q_stricmp( found->script_targetname, squadPaths[i].ownername ) == 0 )
			{
				found->NPC->iSquadPathIndex = i;
				found->NPC->iSquadRouteIndex= i;
				break;
			}
		}

		//See if we succeeded putting them on a path
		if ( found->NPC->iSquadPathIndex == -1 )
		{
			//FIXME: What to do if they don't have a squadpath?  Just follow player and keep a distance?
			//FIXME: Emit this warning properly
#ifndef FINAL_BUILD
			gi.Printf("No squadPath for %s\n", found->script_targetname);
#endif//FINAL_BUILD
		}
		else
		{
			//New member added
			num++;

			//link them up
			last->client->follower = found;
			found->client->leader = last;
			
			//make self the leader
			found->client->team_leader = self;

			//Put them in BS_BEHAVIOR
			found->NPC->behaviorState = BS_FORMATION;
			found->NPC->tempBehavior = BS_DEFAULT;

			//we need to make them head to their first squadPoint
			found->NPC->sPCurSegPoint1 = found->NPC->sPCurSegPoint2 = -1;
			found->NPC->sPDestSegPoint1 = found->NPC->sPDestSegPoint2 = 0;//head for the first one
			VectorCopy(squadPaths[found->NPC->iSquadPathIndex].waypoints[0].origin, found->NPC->sPDestPos);
			found->NPC->aiFlags |= NPCAI_OFF_PATH;
			found->NPC->aiFlags &= ~NPCAI_FORM_TELE_NAV;
			found->NPC->lastSquadPoint = -1;
//			found->NPC->scriptFlags |= SCF_CAREFUL;//Set them to always use walk2/stand2/run2
			VectorClear(found->NPC->leaderTeleportSpot);
		}
		
		last = found;
	}

	//Set ourself as the team leader
	self->client->team_leader = self;

	//Debug warning
	if ( num == 0 )
	{
		//FIXME: Or... this means that no one was around to form up
#ifndef FINAL_BUILD
		gi.Printf( S_COLOR_RED"ERROR: formation %s could not be formed!\n", self->client->squadname );
#endif//FINAL_BUILD
	}
}

/*
-------------------------
G_FindClosestPointOnLineSegment
-------------------------
*/

qboolean G_FindClosestPointOnLineSegment( const vec3_t start, const vec3_t end, const vec3_t from, vec3_t result )
{
	vec3_t	vecStart2From, vecStart2End, vecEnd2Start, vecEnd2From;
	float	distEnd2From, distEnd2Result, theta, cos_theta;

	//Find the perpendicular vector to vec from start to end
	VectorSubtract( from, start, vecStart2From);
	VectorSubtract( end, start, vecStart2End);

	float dot = DotProductNormalize( vecStart2From, vecStart2End );

	if ( dot <= 0 )
	{
		//The perpendicular would be beyond or through the start point
		VectorCopy( start, result );
		return qfalse;
	}

	if ( dot == 1 )
	{
		//parallel, closer of 2 points will be the target
		if( (VectorLengthSquared( vecStart2From )) < (VectorLengthSquared( vecStart2End )) )
		{
			VectorCopy( from, result );
		}
		else
		{
			VectorCopy( end, result );
		}
		return qfalse;
	}

	//Try other end
	VectorSubtract( from, end, vecEnd2From);
	VectorSubtract( start, end, vecEnd2Start);

	dot = DotProductNormalize( vecEnd2From, vecEnd2Start );

	if ( dot <= 0 )
	{//The perpendicular would be beyond or through the start point
		VectorCopy( end, result );
		return qfalse;
	}

	if ( dot == 1 )
	{//parallel, closer of 2 points will be the target
		if( (VectorLengthSquared( vecEnd2From )) < (VectorLengthSquared( vecEnd2Start )))
		{
			VectorCopy( from, result );
		}
		else
		{
			VectorCopy( end, result );
		}
		return qfalse;
	}

	//		      /|
	//		  c  / |
	//		    /  |a
	//	theta  /)__|    
	//		      b
	//cos(theta) = b / c
	//solve for b
	//b = cos(theta) * c

	//angle between vecs end2from and end2start, should be between 0 and 90
	theta = 90 * (1 - dot);//theta
	
	//Get length of side from End2Result using sine of theta
	distEnd2From = VectorLength( vecEnd2From );//c
	cos_theta = cos(DEG2RAD(theta));//cos(theta)
	distEnd2Result = cos_theta * distEnd2From;//b

	//Extrapolate to find result
	VectorNormalize( vecEnd2Start );
	VectorMA( end, distEnd2Result, vecEnd2Start, result );
	
	//perpendicular intersection is between the 2 endpoints
	return qtrue;
}

float G_PointDistFromLineSegment( const vec3_t start, const vec3_t end, const vec3_t from )
{
	vec3_t	vecStart2From, vecStart2End, vecEnd2Start, vecEnd2From, intersection;
	float	distEnd2From, distStart2From, distEnd2Result, theta, cos_theta;

	//Find the perpendicular vector to vec from start to end
	VectorSubtract( from, start, vecStart2From);
	VectorSubtract( end, start, vecStart2End);
	VectorSubtract( from, end, vecEnd2From);
	VectorSubtract( start, end, vecEnd2Start);

	float dot = DotProductNormalize( vecStart2From, vecStart2End );

	distStart2From = Distance( start, from );
	distEnd2From = Distance( end, from );

	if ( dot <= 0 )
	{
		//The perpendicular would be beyond or through the start point
		return distStart2From;
	}

	if ( dot == 1 )
	{
		//parallel, closer of 2 points will be the target
		return ((distStart2From<distEnd2From)?distStart2From:distEnd2From);
	}

	//Try other end

	dot = DotProductNormalize( vecEnd2From, vecEnd2Start );

	if ( dot <= 0 )
	{//The perpendicular would be beyond or through the end point
		return distEnd2From;
	}

	if ( dot == 1 )
	{//parallel, closer of 2 points will be the target
		return ((distStart2From<distEnd2From)?distStart2From:distEnd2From);
	}

	//		      /|
	//		  c  / |
	//		    /  |a
	//	theta  /)__|    
	//		      b
	//cos(theta) = b / c
	//solve for b
	//b = cos(theta) * c

	//angle between vecs end2from and end2start, should be between 0 and 90
	theta = 90 * (1 - dot);//theta
	
	//Get length of side from End2Result using sine of theta
	cos_theta = cos(DEG2RAD(theta));//cos(theta)
	distEnd2Result = cos_theta * distEnd2From;//b

	//Extrapolate to find result
	VectorNormalize( vecEnd2Start );
	VectorMA( end, distEnd2Result, vecEnd2Start, intersection );
	
	//perpendicular intersection is between the 2 endpoints, return dist to it from from
	return Distance( intersection, from );
}
/*
-------------------------
NPC_BuildSquadPointDistances
-------------------------
*/

int NPC_BuildSquadPointDistances( gentity_t *self, const vec3_t center, squadPath_t *squadPath, int keyWp )
{
	int	keyWpIndex = -1;
#if 1	//NOTENOTE: Optimized version

	unsigned long	dists[MAX_WAYPOINTS_IN_PATH];
	unsigned long	minDist, bestDist;
	qboolean		distAlreadyRanked[MAX_WAYPOINTS_IN_PATH];
	vec3_t			vec;

	//If we've already calc'd from this position, don't bother doing it again
	if ( VectorCompare( center, self->NPC->lastSPCalcedOrg ) )
		return keyWpIndex;

	//Save this as our last calc spot
	VectorCopy( center, self->NPC->lastSPCalcedOrg );

	//calc relative distances for all
	for ( int i = 0; i < squadPath->numWaypoints; i++ )
	{
		VectorSubtract( squadPath->waypoints[i].origin, center, vec );
		
		//Exaggerate the z diff to weight the cost towards level connections
		vec[2] *= 10;

		dists[i] = (unsigned long) VectorLengthSquared( vec );
	}

	//Clear the double check
	memset( &distAlreadyRanked, qfalse, sizeof( distAlreadyRanked ) );

	minDist = 0;
	
	//Rank all waypoints
	for ( i = 0; i < squadPath->numWaypoints; i++ )
	{
		bestDist = (unsigned long) -1;

		for ( int j = 0; j < squadPath->numWaypoints; j++ )
		{
			if ( distAlreadyRanked[j] )
				continue;

			if ( dists[j] <= bestDist && dists[j] >= minDist )
			{
				bestDist = dists[j];
				squadPath->closestWaypoints[i] = j;
				if ( keyWp != -1 )
				{
					if ( keyWp == j )
					{
						keyWpIndex = i;
					}
				}
			}
		}
		
		distAlreadyRanked[squadPath->closestWaypoints[i]] = qtrue;
		minDist = bestDist;
	}

	return keyWpIndex;

#else	//NOTENOTE: Old version

	float		dists[MAX_WAYPOINTS_IN_PATH];
	float		minDist, bestDist;
	qboolean	distAlreadyRanked[MAX_WAYPOINTS_IN_PATH];
	int			i, j;
	vec3_t		vec;

	if ( VectorCompare( center, self->NPC->lastSPCalcedOrg ) )
	{//We calced distances for this position last time, just return;
		return -1;
	}

	VectorCopy( center, self->NPC->lastSPCalcedOrg );

	//FIXME: init these to -1 each frame???
	//calc relative distances for all
	for ( i = 0; i < squadPath->numWaypoints; i++ )
	{
		VectorSubtract( squadPath->waypoints[i].origin, center, vec );
		//Ranges are too large (> Q3_INFINITE), we have to sqrt them
		//Exaggerate the z diff
		vec[2] *= 10;

		dists[i] = VectorLength( vec );
	}

	//rank them
	for ( j = 0; j < squadPath->numWaypoints; j++ )
	{
		distAlreadyRanked[j] = qfalse;
	}

	minDist = -1;
	for ( i = 0; i < squadPath->numWaypoints; i++ )
	{
		bestDist = Q3_INFINITE;
		for ( j = 0; j < squadPath->numWaypoints; j++ )
		{
			if ( distAlreadyRanked[j] )
			{
				continue;
			}

			if ( dists[j] <= bestDist && dists[j] >= minDist )
			{
				bestDist = dists[j];
				squadPath->closestWaypoints[i] = j;
				if ( keyWp != -1 )
				{
					if ( keyWp == j )
					{
						keyWpIndex = i;
					}
				}
			}
		}
		distAlreadyRanked[squadPath->closestWaypoints[i]] = qtrue;
		minDist = bestDist;
	}

	return keyWpIndex;
#endif

}


/*
-------------------------
NPC_SetRelativeGoalOrgOnPath

  FIXME: Try to push NPC ahead or back along path if in the player's line of fire.

  ALSO: If being pushed out of someone's way, apply it here.

  ALSO: If blocked by another NPC, ask him to move (in main routine)

  Currently, with the "pushing" of other NPCs out of our way removed, NPCs wiggle
	back and forth endlessly when trying to get around someone.
-------------------------
*/

void NPC_SetRelativeGoalOrgOnPath(gentity_t *self, vec3_t goalOrg)
{
	squadPath_t	*squadPath = &squadPaths[self->NPC->iSquadPathIndex];
	vec3_t		pathDir, vec, segVec, goalVec;
	float		toNextWpDist, lead, point1valuemult, point2valuemult, segLength, goalLength;
	int			nextWp, lastWp, i;
	qboolean	backwards = qfalse;

	//For the segment passed in, see how far ahead/behind we should be
	//dest segment dir/length
	VectorSubtract( squadPath->waypoints[self->NPC->sPDestSegPoint1].origin, squadPath->waypoints[self->NPC->sPDestSegPoint2].origin, segVec );
	segLength = VectorNormalize( segVec );
	//dir/dist from leader to dest segment corner
	VectorSubtract( squadPath->waypoints[self->NPC->sPDestSegPoint1].origin, goalOrg, goalVec );
	goalLength = VectorNormalize( goalVec );

	//GoalLength should never be larger than segLength!  That would mean leader is
	//farther away from one of the segment points than the other end of the segment!
	assert(goalLength <= segLength);

	point1valuemult = (segLength - goalLength) / segLength;//1 if close to point 1
	point2valuemult = 1 - point1valuemult;//1 if far from point 1

	lead = ( (squadPath->waypoints[self->NPC->sPDestSegPoint1].leadDist * point1valuemult) + (squadPath->waypoints[self->NPC->sPDestSegPoint2].leadDist * point2valuemult) );
	//This way it gradually changes, never snaps
	
	//IDEA: If in the player's way, move!
	if ( self->NPC->goalDistToPathSeg < DEFAULT_PLAYER_RADIUS*2 )
	{//Leader is very close to our path
		if ( (lead == 0) || (lead < 0 && lead > -64) )
		{//Back us up
			lead = -128 + self->NPC->goalDistToPathSeg;
		}
		else if ( lead > 0 && lead < 64 )
		{//move us ahead
			lead = 128 - self->NPC->goalDistToPathSeg;
		}
	}

	//In the correct spot?
	if ( lead == 0 )
		return;

	if ( lead < 0.0f )
	{
		backwards = qtrue;
		lead = fabs( lead );
	}

	//What is the general forward direction of our path?
	if ( self->NPC->sPDestSegPoint1 < self->NPC->sPDestSegPoint2 )
	{
		if ( !backwards )
		{
			nextWp = self->NPC->sPDestSegPoint2;
			lastWp = self->NPC->sPDestSegPoint1;
			VectorSubtract( squadPath->waypoints[self->NPC->sPDestSegPoint2].origin, squadPath->waypoints[self->NPC->sPDestSegPoint1].origin, pathDir );
		}
		else
		{
			nextWp = self->NPC->sPDestSegPoint1;
			lastWp = self->NPC->sPDestSegPoint2;
			VectorSubtract( squadPath->waypoints[self->NPC->sPDestSegPoint1].origin, squadPath->waypoints[self->NPC->sPDestSegPoint2].origin, pathDir );
		}
	}
	else
	{
		if ( !backwards )
		{
			nextWp = self->NPC->sPDestSegPoint1;
			lastWp = self->NPC->sPDestSegPoint2;
			VectorSubtract( squadPath->waypoints[self->NPC->sPDestSegPoint1].origin, squadPath->waypoints[self->NPC->sPDestSegPoint2].origin, pathDir );
		}
		else
		{
			nextWp = self->NPC->sPDestSegPoint2;
			lastWp = self->NPC->sPDestSegPoint1;
			VectorSubtract( squadPath->waypoints[self->NPC->sPDestSegPoint2].origin, squadPath->waypoints[self->NPC->sPDestSegPoint1].origin, pathDir );
		}
	}

	VectorNormalize( pathDir );

	while ( lead > 0.0f )
	{//FIXME: do we need to calc toNextWpDist?  Shouldn't it be the cost of the route between them?
		VectorSubtract( squadPath->waypoints[nextWp].origin, goalOrg, vec );
		toNextWpDist = VectorLength( vec );
		if ( toNextWpDist == lead )
		{
			VectorCopy( squadPath->waypoints[nextWp].origin, goalOrg );
			lead = 0.0f;
		}
		else if ( toNextWpDist > lead )
		{//We want to stop somewhere before this next Wp
			VectorMA( goalOrg, lead, pathDir, goalOrg );
			lead = 0.0f;
		}
		else
		{//We have more to go
			VectorCopy( squadPath->waypoints[nextWp].origin, goalOrg );
			if ( squadPath->waypoints[nextWp].flags & SPF_BRANCH )
			{//This one is a branch, we don't want to go past it until told to
				//IDEA: Maybe pick the branch they're closest to?  Bad thing here is if
				//		the player is standing on this point, we don't want to crowd him, ever
				lead = 0.0f;
			}
			else
			{//Only has one possible nextWp
				for ( i = 0; i < MAX_PATH_BRANCHES; i++ )
				{
					if ( squadPath->waypoints[nextWp].nextWp[i] != -1 && squadPath->waypoints[nextWp].nextWp[i] != lastWp )
					{//Don't double back, we do this because one of the neighbors is going to be
						//the one we just came from!
						lastWp = nextWp;
						nextWp = squadPath->waypoints[nextWp].nextWp[i];
						break;
					}
				}

				if ( i == MAX_PATH_BRANCHES )
				{//checked all branches, no nextwp, stop here
					lead = 0.0f;
				}
				else
				{//Follow the branch
					//Set the dir of the next branch
					VectorSubtract( squadPath->waypoints[nextWp].origin, squadPath->waypoints[lastWp].origin, pathDir );
					VectorNormalize( pathDir );
					//subtract the lead dist
					lead -= toNextWpDist;
				}
			}
		}
	}

	self->NPC->sPDestSegPoint1 = lastWp;
	self->NPC->sPDestSegPoint2 = nextWp;
	return;
}

/*
-------------------------
NPC_FindClosestSquadPoint
-------------------------
*/

int NPC_FindClosestSquadPoint( gentity_t *self )
{
	int	maxToCheck;

	//Build our distances
	NPC_BuildSquadPointDistances( self, self->currentOrigin, &squadPaths[self->NPC->iSquadPathIndex], WAYPOINT_NONE );

	//Only check the closest eight
	//FIXME: This is an ugly implementation
	if ( squadPaths[self->NPC->iSquadPathIndex].numWaypoints < 8 )
	{
		maxToCheck = squadPaths[self->NPC->iSquadPathIndex].numWaypoints;
	}
	else
	{
		maxToCheck = 8;
	}

	//Check those points
	for ( int i = 0; i < maxToCheck; i++ )
	{
		//FIXME: Remove these in release
		form_closestSP_traces++;

		//If the point is good, go there
		if ( NAV_ClearPathToPoint(self, self->mins, self->maxs, squadPaths[self->NPC->iSquadPathIndex].waypoints[squadPaths[self->NPC->iSquadPathIndex].closestWaypoints[i]].origin, self->clipmask) )//MASK_DEADSOLID))
			return squadPaths[self->NPC->iSquadPathIndex].closestWaypoints[i];
	}
	
	return -1;
}

/*
-------------------------
NPC_UpdatePathSegment

  NOTE: any bad side effects of not calling this every formation movement frame?
-------------------------
*/

void NPC_UpdatePathSegment( gentity_t *self )
{
	squadPath_t	*squadPath = &squadPaths[self->NPC->iSquadPathIndex];
	int			wp1, wp2, nextWp, firstPoint = -1, firstPointIndex = -1;
	vec3_t		point, vec, pathPoint, mins, maxs;
	float		newSegClearPath;
	float		newSegPathDist;
	float		newSegDist;
	float		newSegCost, oldSegCost;
	float		newSegActualCost, oldSegActualCost;
	float		wp1Dist, wp2Dist;
	trace_t		trace;
	int			numsegschecked = 0;
	qboolean	foundOne = qfalse;

	VectorCopy( self->currentOrigin, point );

	//=== TESTING ===
	if ( self->NPC->sPCurSegPoint1 != -1 && self->NPC->sPCurSegPoint2 != -1 )
	{//test our last first, see if we've gotten farther away, if not, keep that
		qboolean forceFullCheck = qfalse;

		//First, see if we got to the end of our current segment, if so, need to move on
		if ( self->NPC->sPCurSegPoint1 == self->NPC->curSegNextWp )
		{
			firstPoint = self->NPC->sPCurSegPoint1;
		}
		else if ( self->NPC->sPCurSegPoint2 == self->NPC->curSegNextWp )
		{
			firstPoint = self->NPC->sPCurSegPoint2;
		}

		if ( firstPoint != -1 )
		{
			VectorSubtract( squadPath->waypoints[firstPoint].origin, point, vec );
			newSegDist = VectorLengthSquared(vec);

			if ( newSegDist <= self->NPC->sPLastSegDist || (fabs(vec[0])+fabs(vec[1]))/2 <= (self->maxs[0]*self->maxs[1])/2 )
			{//at the end of the segment, must do full check, but we know one of the points
				forceFullCheck = qtrue;
			}
		}

		if ( !forceFullCheck )
		{
			G_FindClosestPointOnLineSegment( squadPath->waypoints[self->NPC->sPCurSegPoint1].origin, squadPath->waypoints[self->NPC->sPCurSegPoint2].origin, point, pathPoint );

			VectorSubtract(pathPoint, point, vec);
			newSegDist = VectorLengthSquared(vec);

			/*if ( gi.inPVSIgnorePortals( point, pathPoint ) )
			{//make sure we're in PVS
				form_updateseg_traces++;
				//see if we have a clear path to it
				gi.trace( &trace, point, mins, maxs, pathPoint, self->s.number, CONTENTS_SOLID|CONTENTS_MONSTERCLIP );
				newSegClearPath = trace.fraction;

				//Calculate final overall cost
				newSegDist *= (1.1f - newSegClearPath);*/

				if ( newSegDist <= self->NPC->sPLastSegDist || (fabs(vec[0])+fabs(vec[1]))/2 <= (self->maxs[0]*self->maxs[1])/2 )
				{//haven't gotten any farther from the segment we were heading to or we're physically on our segment
					self->NPC->sPLastSegDist = newSegDist;
					return;
				}
			//}
		}
	}
	//===============

	VectorCopy( self->mins, mins );
	VectorCopy( self->maxs, maxs );
	mins[2] += STEPSIZE;

	firstPointIndex = NPC_BuildSquadPointDistances( self, point, &squadPaths[self->NPC->iSquadPathIndex], firstPoint );
	
	//Which squadPathPoint, if any, are we in?
	NPC_UpdateLastSquadPoint( NPC );

	self->NPC->sPCurSegPoint1 = self->NPC->sPCurSegPoint2 = -1;
	
	oldSegCost = oldSegActualCost = Q3_INFINITE;
	
	//Go through all waypoint pairs, starting with 2 closest and see if you can find any 2 that are neighbors
	for( wp1 = (firstPointIndex == -1) ? 0 : firstPointIndex; wp1 < squadPath->numWaypoints && wp1 != -1; wp1 = (firstPointIndex == -1) ? (wp1+1) : -1 )
	{
		for( wp2 = 0; wp2 < squadPath->numWaypoints; wp2++ )
		{
			if(wp2 == wp1)
				continue;

			//NOTE: using this array (assuming it's saved and loaded) would
			//		eliminate some of this looping... or just make a list
			//		of neighbors and iterate through that?  Must go from
			//		closest out, though
			//if ( branchFound[wp1][wp2] || branchFound[wp2][wp1] )

			for(nextWp = 0; nextWp < MAX_PATH_BRANCHES; nextWp++)
			{
				if((squadPath->waypoints[squadPath->closestWaypoints[wp1]].nextWp[nextWp] == squadPath->closestWaypoints[wp2])||
					(squadPath->waypoints[squadPath->closestWaypoints[wp2]].nextWp[nextWp] == squadPath->closestWaypoints[wp1]))
				{//They are neighbors!  Woo!
					newSegClearPath = 0.0f;
					newSegPathDist = Q3_INFINITE;
					newSegDist = Q3_INFINITE;
					newSegCost = newSegActualCost = Q3_INFINITE;
					wp1Dist = wp2Dist = Q3_INFINITE;
					
					numsegschecked++;
					//Criteria #1 : Segment has a route to dest - note there should ALWAYS be a route
					if(self->NPC->destSegLastWp != -1)
					{//goal here is to find the path segment closest to our dest segment (by leader)
						if(squadPath->closestWaypoints[wp1] == self->NPC->destSegLastWp)
						{
							wp1Dist = 0;
						}
						else if(squadRoutes[self->NPC->iSquadRouteIndex].nextSquadPoint[squadPath->closestWaypoints[wp1]][self->NPC->destSegLastWp] != -1)
						{//find dist from this seg to our dest seg
							wp1Dist = squadRoutes[self->NPC->iSquadRouteIndex].cost[squadPath->closestWaypoints[wp1]][self->NPC->destSegLastWp];
						}
						else
						{
							wp1Dist = Q3_INFINITE;
						}

						if(squadPath->closestWaypoints[wp2] == self->NPC->destSegLastWp)
						{
							wp2Dist = 0;
						}
						else if(squadRoutes[self->NPC->iSquadRouteIndex].nextSquadPoint[squadPath->closestWaypoints[wp2]][self->NPC->destSegLastWp] != -1)
						{
							wp2Dist = squadRoutes[self->NPC->iSquadRouteIndex].cost[squadPath->closestWaypoints[wp2]][self->NPC->destSegLastWp];
						}
						else
						{
							wp2Dist = Q3_INFINITE;
						}
						if(wp2Dist < wp1Dist)
						{
							newSegPathDist = wp2Dist;
						}
						else
						{
							newSegPathDist = wp2Dist;
						}
					}
					else
					{
						newSegPathDist = Q3_INFINITE;
					}

					if ( newSegPathDist < Q3_INFINITE )
					{
						//Criteria #2 : Segment's distance from us
						G_FindClosestPointOnLineSegment( squadPath->waypoints[squadPath->closestWaypoints[wp1]].origin, squadPath->waypoints[squadPath->closestWaypoints[wp2]].origin, point, pathPoint );
						
						VectorSubtract(pathPoint, point, vec);
						newSegDist = VectorLengthSquared(vec);
						
						//Calculate cost
						newSegCost = (newSegPathDist + newSegDist);// * (1.1f - newSegClearPath);

						//Compare value - what if they're equal...?
						if ( newSegCost < oldSegCost )
						{//This one is better
							//Criteria #3 : Can get to the segment
							if ( gi.inPVSIgnorePortals( point, pathPoint ) )//FIXME: IgnorePortals?
							{
								form_updateseg_traces++;
								//gi.trace( &trace, point, NULL, NULL, pathPoint, self->s.number, CONTENTS_SOLID|CONTENTS_MONSTERCLIP );
								//NOTE: A point trace would be faster here, but to be sure,
								//		this really should be a full size trace, otherwise 
								//		we can possibly pick a closer segment we can see but
								//		not get to!  Plus, we don't even check for ledges
								//		here...
								gi.trace( &trace, point, mins, maxs, pathPoint, self->s.number, CONTENTS_SOLID|CONTENTS_MONSTERCLIP|CONTENTS_BOTCLIP );
								newSegClearPath = trace.fraction;

								//Calculate final overall cost
								newSegActualCost = newSegCost * (1.1f - newSegClearPath);

								if ( newSegActualCost < oldSegActualCost )
								{
									foundOne = qtrue;
									oldSegCost = newSegCost;
									oldSegActualCost = newSegActualCost;
									self->NPC->sPCurSegPoint1 = squadPath->closestWaypoints[wp1];
									self->NPC->sPCurSegPoint2 = squadPath->closestWaypoints[wp2];
									self->NPC->sPLastSegDist = newSegDist;// * (1.1f - newSegClearPath);
								}
							}
						}
					}
						
					if ( numsegschecked > 16 && foundOne )
					{//Only check the 16 nearest segments before settling for our best so far
						return;
					}
					//Don't need to look at rest of neighbors
					nextWp = MAX_PATH_BRANCHES;
				}
			}
		}
	}
}

/*
qboolean NPC_FindClosestGoalPathPointToEnt(gentity_t *self, gentity_t *targEnt, vec3_t goalOrg, qboolean inclusive, qboolean clearPath)

  Finds 2 closest adjacent squadpoints to the targEnt
*/

qboolean NPC_SetSegDest( gentity_t *self, float bestSegDist, int closest1, int closest2 )
{
	self->NPC->goalDistToPathSeg = bestSegDist;
	self->NPC->sPDestSegPoint1 = closest1;
	self->NPC->sPDestSegPoint2 = closest2;
	return qtrue;
}

qboolean NPC_FindClosestGoalPathPointToEnt(gentity_t *self, gentity_t *targEnt, vec3_t goalOrg, float maxDist, qboolean inclusive, qboolean clearPath)
{
	squadPath_t	*squadPath = &squadPaths[self->NPC->iSquadPathIndex];
	int			wp1, wp2, nextWp;
	int			closest1 = -1;
	int			closest2 = -1;
	int			numTraces = 0;
	int			maxTraces = 8;
	float		segDist;
	float		bestSegDist = 128;
	vec3_t		point, vec;

	//=== TESTING ===
	//test our last first, see if leader has gotten farther away, if not, keep current dest seg
	if ( self->NPC->sPDestSegPoint1 != -1 && self->NPC->sPDestSegPoint2 != -1 )
	{
		G_FindClosestPointOnLineSegment( squadPath->waypoints[self->NPC->sPDestSegPoint1].origin, squadPath->waypoints[self->NPC->sPDestSegPoint2].origin, targEnt->currentOrigin, goalOrg );

		VectorSubtract( goalOrg, targEnt->currentOrigin, vec );
		segDist = VectorLengthSquared(vec);

		if ( segDist <= self->NPC->goalDistToPathSeg || (fabs(vec[0])+fabs(vec[1]))/2 <= (targEnt->maxs[0]+targEnt->maxs[1])/2 )
		{//leader hasn't gotten any farther from the segment we were heading to or they're physically on our segment
			//FIXME: do we need to check PVS and trace too?
			self->NPC->goalDistToPathSeg = segDist;
			return qtrue;
			//FIXME: If this does fail, don't check segment again below?
		}
	}
	//===============

	VectorCopy( targEnt->currentOrigin, point );
	
	//If in BS_FORMATION, do this once at the beginning of the frame
	//so other places can use it without having to rebuild it
	NPC_BuildSquadPointDistances( self, point, squadPath, WAYPOINT_NONE );

	self->NPC->sPDestSegPoint1 = self->NPC->sPDestSegPoint2 = -1;
	//Go through all waypoint pairs, starting with 2 closest and see if you can
	//find any 2 that are neighbors
	
	for ( wp1 = 0; wp1 < squadPath->numWaypoints && numTraces < maxTraces; wp1++ )
	{
		for ( wp2 = 0; wp2 < squadPath->numWaypoints && numTraces < maxTraces; wp2++ )
		{
			if ( wp2 == wp1 )
			{
				continue;
			}

			for ( nextWp = 0; nextWp < MAX_PATH_BRANCHES; nextWp++ )
			{
				if ( (squadPath->waypoints[squadPath->closestWaypoints[wp1]].nextWp[nextWp] == squadPath->closestWaypoints[wp2])||
					 (squadPath->waypoints[squadPath->closestWaypoints[wp2]].nextWp[nextWp] == squadPath->closestWaypoints[wp1]) )
				{//They are neighbors!  Woo!
					//FIXME: sometimes we want just the closest segment... when?
					if ( inclusive )
					{//Does the perp intersection from point have to be within the line segment?
						//Save these in case we don't find anything else
						if ( closest1 == -1 && closest2 == -1 )
						{//Only save the first (best), theoretically
							closest1 = squadPath->closestWaypoints[wp1];
							closest2 = squadPath->closestWaypoints[wp2];
						}

						//Here, goalOrg is closest point on that segment to point passed in,
						//segment does not necc contains his perp intersection!  This is bad
						//because we have no idea which of the two segments that come off of
						//this point are the ones we want, we may want a totally different
						//segment altogether?
						//G_FindClosestPointOnLineSegment( squadPath->waypoints[squadPath->closestWaypoints[wp1]].origin, squadPath->waypoints[squadPath->closestWaypoints[wp2]].origin, point, goalOrg );

						if ( G_FindClosestPointOnLineSegment( squadPath->waypoints[squadPath->closestWaypoints[wp1]].origin, squadPath->waypoints[squadPath->closestWaypoints[wp2]].origin, point, goalOrg ) )
							//found a segment that contains leader's perpendicular intersection
						{
							VectorSubtract( goalOrg, point, vec );
							segDist = VectorLengthSquared( vec );
							if ( segDist <= maxDist )
							{//It's close enough to the leader to consider (<=128)
								if ( clearPath )
								{
									//FIXME: without IgnorePortals this may fail if a door closes on a squadPath segment...
									if ( gi.inPVSIgnorePortals( point, squadPath->waypoints[squadPath->closestWaypoints[wp1]].origin ) &&
										 gi.inPVSIgnorePortals( point, squadPath->waypoints[squadPath->closestWaypoints[wp2]].origin ) )
									{
										//FIXME: could optimize by keeping a list, then just tracing to the closest ones first until we find a clear one?
										numTraces++;
										form_leaderseg_traces++;
										if ( NAV_ClearPathToPoint( targEnt, self->mins, self->maxs, goalOrg, MASK_DEADSOLID ) )
										{//Can the leader get there?
											//NOTE: this doesn't necc find the closest, 
											//		just the first!
											bestSegDist = segDist;
											return NPC_SetSegDest( self, bestSegDist, squadPath->closestWaypoints[wp1], squadPath->closestWaypoints[wp2] );
										}
									}
								}
								else
								{
									bestSegDist = segDist;
									return NPC_SetSegDest( self, bestSegDist, squadPath->closestWaypoints[wp1], squadPath->closestWaypoints[wp2] );
								}
							}
						}
					}
					else
					{
						return NPC_SetSegDest( self, bestSegDist, squadPath->closestWaypoints[wp1], squadPath->closestWaypoints[wp2] );
					}
				}
			}
		}
	}
	
	if ( closest1 != -1 && closest2 != -1 )
	{//Okay, none inclusive, just return the closest, if any
		G_FindClosestPointOnLineSegment( squadPath->waypoints[closest1].origin, squadPath->waypoints[closest2].origin, point, goalOrg );
		form_leaderseg_traces++;
		if ( NAV_ClearPathToPoint( targEnt, self->mins, self->maxs, goalOrg, MASK_DEADSOLID ) )
		{
			//FIXME: should we make sure it's close enough first???
			return NPC_SetSegDest( self, bestSegDist, closest1, closest2 );
		}
	}

	return qfalse;
}

/*
void NPC_FindClosestFormationSpot (gentity_t *self)

*/
void NPC_FindsPDestSegPoint(gentity_t *self)
{
	vec3_t		goalOrg, bottom;
	squadPath_t	*squadPath;
	trace_t		trace;
	int			wpNum;
	gentity_t	*targEnt;
	float		maxDist = MAX_SEGMENT_DIST_FROM_LEADER_SQUARED;

	if ( !self->NPC )
	{
		return;
	}

	if ( self->NPC->aiFlags&NPCAI_FORM_TELE_NAV )
	{//Head for teleport spot
		if ( NPCInfo->touchedByPlayer == self->client->team_leader )
		{//Hey, bumped into our leader, follow him now
			VectorClear( NPCInfo->leaderTeleportSpot );
			NPCInfo->aiFlags &= ~NPCAI_FORM_TELE_NAV;
		}
		else
		{
			VectorCopy( self->NPC->leaderTeleportSpot, self->NPC->sPDestPos);
			return;
		}
	}

	targEnt = self->client->team_leader;

	if ( self->NPC->aiFlags & NPCAI_OFF_PATH )
	{//Need to get on our path first
		//FIXME: see if we can get to our last sPDestPos and go there?
		wpNum = NPC_FindClosestSquadPoint( self );
		if ( wpNum != -1 )
		{
			self->NPC->sPDestSegPoint1 = self->NPC->sPDestSegPoint2 = wpNum;
			VectorCopy( squadPaths[self->NPC->iSquadPathIndex].waypoints[wpNum].origin, self->NPC->sPDestPos );
			return;
		}
		else
		{
#ifdef _DEBUG
			//gi.Printf("%s: Hold on, I'm a little lost here...\n", self->script_targetname);
#endif
			targEnt = self;
			maxDist = 500*500;
		}
	}

	//FIXME: this is not really a good idea, visibility is not reliable
	//and can be cut unnaturally (by area portals, etc.) - we might want
	//an NPC to be able to rejoin the squad from outside visibility...
	/*
	if(targEnt != self && !gi.inPVSIgnorePortals(self->currentOrigin, targEnt->currentOrigin))//FIXME: IgnorePortals bad?
	{//Don't know where my leader actually is!  What if we were following him?  Should be a flag to not follow in for in PVS
		//Don't try to get close to him/her
		return;
	}
	*/
	squadPath = &squadPaths[self->NPC->iSquadPathIndex];	// note: this isn't actually used past here at the moment, but stuff might get unREM'd...

/*
	if(form_forward_speed <= 0)
	{//Leader moving backwards
		VectorCopy(self->team_leader->currentOrigin, leaderSpot);
	}
	else
	{//leader moving forwards at least a little
		VectorMA(self->team_leader->currentOrigin, form_forward_speed/2, form_dir, leaderSpot);
	}
*/
	//if(!NPC_UpdatePathSegmentForEnt(self, targEnt, goalOrg)) - no, no, no
	if ( !NPC_FindClosestGoalPathPointToEnt( self, targEnt, goalOrg, maxDist, qtrue, qtrue ) )
	{//Can't find a segment close to leader???
		if ( self->NPC->aiFlags & NPCAI_OFF_PATH )
		{
			if ( self->NPC->blockedSpeechDebounceTime < level.time )
			{
#ifdef _DEBUG
				//gi.Printf("%s: Help!  I'm stuck!!!\n", self->script_targetname);
#endif
				self->NPC->blockedSpeechDebounceTime = level.time + 3000;//FIXME: make a define
				//FIXME: What do we do now?
				//Maybe we should try to use the waypoint network-
				//	Find goal's closest waypoint, then see if we have a waypoint we can
				//	get to that has a route to that waypoint, if so, use it.
				//Blind bump and turn?  Pick a direction toward your goal...?
				//	Trace that and outward until you find a trace.fraction of 1
				//	If don't find a trace fraction of 1, choose longest?
				//Or just move toward goal and slide along walls Heretic II style?
			}
		}
		else
		{//Will this screw us up when he gets back on it?
			VectorCopy( self->currentOrigin, self->NPC->sPDestPos );
		}
		//Maybe we should try to use the waypoint network-
		//	Find goal's closest waypoint, then see if we have a waypoint we can
		//	get to that has a route to that waypoint, if so, use it?  Or just wait?
		return;
	}

	//Now we have the closest point on our path to our leader
	//	We now have to determine how far ahead or behind the leader we should be
	if ( !(self->NPC->aiFlags & NPCAI_OFF_PATH) )
	{//if we're trying to get on our path, skip this step
		NPC_SetRelativeGoalOrgOnPath( self, goalOrg );
	}
	else
	{
		NPCInfo->aiFlags |= NPCAI_STRAIGHT_TO_DESTPOS;
	}
	//Will set the two wp's of the segment our goalOrg is on

	//Now trace-test this spot and see where exactly (z-wise) we should be heading.
	VectorCopy( goalOrg, bottom );
	bottom[2] -= 1024;
	//FIXME: should we not choose this spot if it's got someone in it?  Or let
	//	collision avoidance handle this?
	form_clearpath_traces++;
	gi.trace( &trace, goalOrg, self->mins, self->maxs, bottom, self->s.number, CONTENTS_SOLID|CONTENTS_MONSTERCLIP|CONTENTS_BOTCLIP );
	
	if ( trace.startsolid || trace.allsolid )
	{//Position is in solid, move up
		goalOrg[2] += STEPSIZE;
		form_clearpath_traces++;
		gi.trace( &trace, goalOrg, self->mins, self->maxs, bottom, self->s.number, CONTENTS_SOLID|CONTENTS_MONSTERCLIP|CONTENTS_BOTCLIP );
		if ( trace.startsolid || trace.allsolid )
		{//Position is in solid, move up again
			goalOrg[2] += STEPSIZE;
			form_clearpath_traces++;
			gi.trace( &trace, goalOrg, self->mins, self->maxs, bottom, self->s.number, CONTENTS_SOLID|CONTENTS_MONSTERCLIP|CONTENTS_BOTCLIP );
			if ( trace.startsolid || trace.allsolid )
			{//Position is in solid
#ifdef _DEBUG
				//gi.Printf("Warning: squadPath %s between %s and %s has in-solid point at %s\n",
				//	squadPath->ownername, vtos(squadPath->waypoints[self->NPC->sPDestSegPoint1].origin), vtos(squadPath->waypoints[self->NPC->sPDestSegPoint2].origin), vtos(goalOrg));
#endif
				return;
			}
		}
	}

	if ( trace.fraction == 1.0 )
	{//No bottom!
#ifdef _DEBUG
		//gi.Printf("Warning: squadPath %s between %s and %s has no floor at %s\n",
		//	squadPath->ownername, vtos(squadPath->waypoints[self->NPC->sPDestSegPoint1].origin), vtos(squadPath->waypoints[self->NPC->sPDestSegPoint2].origin), vtos(goalOrg));
#endif
		return;
	}

	//Ok, we found a point along our path to head to.

	//FIXME: pick the proper squadPoint on the path and figure out which way
	//	up or down the path you should head.  Follow the path(don't head straight for formGoalPos)

	VectorCopy( trace.endpos, self->NPC->sPDestPos );
	//if this fails, should we just try to follow our leader or stay where we are?
}

/*
void G_UpdateFormationGoals (gentity_t *self)

//Only do this if we have moved... changed surfaces?

//loop through all NPCs in formation

//Step 1: Search for the closest waypoint with a ownername == NPC's script_targetname
//When create formation first time, put the waypoints into a linked list on each NPC?

//Step 2: Find surface of player, NPC & wp.

//Step 3: See if there is a valid route between player & wp and NPC & wp.  If not, continue...

//Step 4: Set NPC's formationGoal to the new wp

//Step 5: Repeat for all NPCs in formation.

*/
void G_MaintainFormations (gentity_t *self)
{
	//vec3_t		angles;

	//DISABLED FOR NOW
	return;

	/*
	if(!self)
	{
		return;
	}
	
	if(self->team_leader != self)
	{//ONLY team_leaders call this function
		return;
	}

	//FIXME: make this just set whether or not NPCs should try to catch up or stop?

	angles[PITCH] = angles[ROLL] = 0;
	angles[YAW] = self->client->ps.viewangles[YAW];
	AngleVectors(angles, form_forward, form_right, NULL);

	form_speed = VectorNormalize2( self->client->ps.velocity, form_dir );
	form_forward_speed = DotProduct(form_dir, form_forward) * form_speed;
	vectoangles( form_dir, form_angles );*/
}

//================================================================================================

//OLD:

void NPC_DropFormation (gentity_t *self)
{
	//remove leader and team_leader from all and self
	//unassign all the followers
	//unnumber them
	//unlink them
}

void NPC_AppendOntoFormation (gentity_t *self, gentity_t *leader)
{//Append self onto the leader's list
	gentity_t	*ent = self->client->team_leader;

	while(ent->client->follower)
	{
		ent = ent->client->follower;
	}
	ent->client->follower = self;
	self->client->team_leader = leader;
	//leader->numFollowers++;
}

void NPC_DeleteFromFormation (gentity_t *self)
{
	gentity_t	*ent = self->client->team_leader;
	//Remove self from the list
	//relink them
	while(ent->client->follower)
	{
		if(ent->client->follower == self)
		{
			ent->client->follower = self->client->follower;
			//self->team_leader->numFollowers--;
			return;
		}
		ent = ent->client->follower;
	}
}

//===Actual NPC BState funcs============================================================

void NPC_UpdateLastSquadPoint (gentity_t *self)
{
	vec3_t	vec;
	int		i;
	float	dist, bestDist;
	//	Check which point you're closest to, keep track of last and next.
	//	When your next is your target point, start checking for proximity to
	//	formationGoalPos- when close to it, start to slow down, when there, stop and look around or do
	//	whatever your formation idle behavior is.
	//	When you get to a waypoint, set it as your last and set your next to it's next, if any.
	//	When you set your next waypoint, see if it has special instructions, like to wait
	//		for others to go through it first.  When it's your turn, go through and flag it.
	//		While you're waiting, where do you stand?  128 away?  Coded into it?  Waitscript?
	//	When get to a branch, wait there until leader is >= 128 from your point AND
	//		leader's range to one of the next points is less than yours- so if he's
	//		closer to the right branch than you, go right, etc.  Instead of branch,
	//		could come to a dead end and have to pick from a new one???

	//See what waypoint, if any, we're in/by
	bestDist = MAX_WAYPOINT_REACHED_DIST_SQUARED;
	self->NPC->aiFlags &= ~NPCAI_IN_SQUADPOINT;
	self->NPC->currentSquadPoint = -1;

	//This presumes that buildsquadpointdistances was called on our origin!
	for(i = 0; i < squadPaths[self->NPC->iSquadPathIndex].numWaypoints; i++)
	{
		VectorSubtract(squadPaths[self->NPC->iSquadPathIndex].waypoints[squadPaths[self->NPC->iSquadPathIndex].closestWaypoints[i]].origin, self->currentOrigin, vec);
		dist = VectorLengthSquared(vec);
		if(dist < bestDist)
		{
			bestDist = dist;
			self->NPC->currentSquadPoint = self->NPC->lastSquadPoint = squadPaths[self->NPC->iSquadPathIndex].closestWaypoints[i];
			self->NPC->aiFlags |= NPCAI_IN_SQUADPOINT;
			self->NPC->aiFlags &= ~NPCAI_OFF_PATH;
		}
	}

	//FIXME: Avoidance can make him miss this squadpoint, maybe we
	//		should do this when he finds his 2 closest and use the
	//		target of the squadpoint behind us on our path?
	/*
	if ( self->NPC->aiFlags & NPCAI_IN_SQUADPOINT )
	{
		if ( squadPaths[self->NPC->iSquadPathIndex].waypoints[self->NPC->lastSquadPoint].target && squadPaths[self->NPC->iSquadPathIndex].waypoints[self->NPC->lastSquadPoint].target[0] )
		{//Fire the target
			G_UseTargets2( NPC, NPC, squadPaths[self->NPC->iSquadPathIndex].waypoints[self->NPC->lastSquadPoint].target );
			//For now, does it only once
			squadPaths[self->NPC->iSquadPathIndex].waypoints[self->NPC->lastSquadPoint].target = NULL;
		}
	}
	*/
}

/*
void NPC_SetPathDistToGoalPos( gentity_t *self )
How far along our path are we from where we want to be?

Sets my next squadpoint, my last one and my goalPos' next and last
*/
qboolean NPC_FindPathToGoalPos( gentity_t *self )
{
	squadPath_t		*path	= &squadPaths [self->NPC->iSquadPathIndex];
	squadRoute_t	*routes = &squadRoutes[self->NPC->iSquadRouteIndex];
	float		cost[2][2], distToCurNext, distToDestLast;
	float		bestCost = Q3_INFINITE;
	qboolean	foundRoute = qfalse;
	qboolean	onSameSeg = qfalse;
	int			i, j, bestCurNext = -1, bestDestLast = -1;
	vec3_t		vecToCurNext, vecToDestLast;

	self->NPC->curSegLastWp = -1;
	self->NPC->curSegNextWp = -1;
	self->NPC->destSegLastWp = -1;
	self->NPC->destSegNextWp = -1;

	//if we're on the same segment as our destPos, we do something different
	if(self->NPC->sPCurSegPoint1 == self->NPC->sPDestSegPoint1)
	{
		if(self->NPC->sPCurSegPoint2 == self->NPC->sPDestSegPoint2)
		{
			onSameSeg = qtrue;
		}
	}

	if(self->NPC->sPCurSegPoint1 == self->NPC->sPDestSegPoint2)
	{
		if(self->NPC->sPCurSegPoint2 == self->NPC->sPDestSegPoint1)
		{
			onSameSeg = qtrue;
		}
	}

	if(onSameSeg)
	{//We're on the same segment as our destPos
		//head right for it!
		self->NPC->aiFlags |= NPCAI_STRAIGHT_TO_DESTPOS;
		//How do we set these?  Do we need to?
		self->NPC->destSegNextWp = self->NPC->curSegLastWp = self->NPC->sPDestSegPoint2;
		self->NPC->destSegLastWp = self->NPC->curSegNextWp = self->NPC->sPDestSegPoint1;//one we want to head to

		//dist is our dist to our destPos
		VectorSubtract(self->NPC->sPDestPos, self->currentOrigin, vecToCurNext);
		self->NPC->sPDestPosPathDist = VectorLength(vecToCurNext);
		return qtrue;
	}

	//Ok, we're on different segs, could be adjacent, so let's see
	if(self->NPC->sPCurSegPoint1 == self->NPC->sPDestSegPoint1)
	{//One of our segment points is the same as thiers
		cost[0][0] = 0;
		foundRoute = qtrue;
	}
	else if(routes->nextSquadPoint[self->NPC->sPCurSegPoint1][self->NPC->sPDestSegPoint1] != -1)
	{
		cost[0][0] = routes->cost[self->NPC->sPCurSegPoint1][self->NPC->sPDestSegPoint1];
		foundRoute = qtrue;
	}
	else
	{
		cost[0][0] = Q3_INFINITE;
	}

	if(self->NPC->sPCurSegPoint1 == self->NPC->sPDestSegPoint2)
	{//One of our segment points is the same as thiers
		cost[0][1] = 0;
		foundRoute = qtrue;
	}
	else if(routes->nextSquadPoint[self->NPC->sPCurSegPoint1][self->NPC->sPDestSegPoint2] != -1)
	{
		cost[0][1] = routes->cost[self->NPC->sPCurSegPoint1][self->NPC->sPDestSegPoint2];
		foundRoute = qtrue;
	}
	else
	{
		cost[0][1] = Q3_INFINITE;
	}

	if(self->NPC->sPCurSegPoint2 == self->NPC->sPDestSegPoint1)
	{//One of our segment points is the same as thiers
		cost[1][0] = 0;
		foundRoute = qtrue;
	}
	else if(routes->nextSquadPoint[self->NPC->sPCurSegPoint2][self->NPC->sPDestSegPoint1] != -1)
	{
		cost[1][0] = routes->cost[self->NPC->sPCurSegPoint2][self->NPC->sPDestSegPoint1];
		foundRoute = qtrue;
	}
	else
	{
		cost[1][0] = Q3_INFINITE;
	}

	if(self->NPC->sPCurSegPoint2 == self->NPC->sPDestSegPoint2)
	{//One of our segment points is the same as thiers
		cost[1][1] = 0;
		foundRoute = qtrue;
	}
	else if(routes->nextSquadPoint[self->NPC->sPCurSegPoint2][self->NPC->sPDestSegPoint2] != -1)
	{
		cost[1][1] = routes->cost[self->NPC->sPCurSegPoint2][self->NPC->sPDestSegPoint2];
		foundRoute = qtrue;
	}
	else
	{
		cost[1][1] = Q3_INFINITE;
	}

	if(!foundRoute)
	{//No route?!! Impossible!
		return qfalse;
	}

	for(i = 0; i < 2; i++)
	{
		for(j = 0; j < 2; j++)
		{
			if(cost[i][j] < bestCost)
			{
				bestCost = cost[i][j];
				bestCurNext = i;
				bestDestLast = j;
			}
		}
	}

	if(bestCurNext == 0)
	{
		self->NPC->curSegNextWp = self->NPC->sPCurSegPoint1;
		self->NPC->curSegLastWp = self->NPC->sPCurSegPoint2;
	}
	else//bestCurNext = 1
	{
		self->NPC->curSegNextWp = self->NPC->sPCurSegPoint2;
		self->NPC->curSegLastWp = self->NPC->sPCurSegPoint1;
	}

	if(bestDestLast == 0)
	{
		self->NPC->destSegLastWp = self->NPC->sPDestSegPoint1;
		self->NPC->destSegNextWp = self->NPC->sPDestSegPoint2;
	}
	else//bestDestLast = 1
	{
		self->NPC->destSegLastWp = self->NPC->sPDestSegPoint2;
		self->NPC->destSegNextWp = self->NPC->sPDestSegPoint1;
	}

	VectorSubtract(path->waypoints[self->NPC->curSegNextWp].origin, self->currentOrigin, vecToCurNext);
	distToCurNext = VectorLength(vecToCurNext);

	VectorSubtract(path->waypoints[self->NPC->destSegLastWp].origin, self->NPC->sPDestPos, vecToDestLast);
	distToDestLast = VectorLength(vecToDestLast);

	self->NPC->sPDestPosPathDist = distToCurNext + bestCost + distToDestLast;
	
	//It's as easy as that!
	return qtrue;
}

/*
-------------------------
NPC_FindLocalEnemies
-------------------------
*/

qboolean NPC_FindLocalEnemies( gentity_t *self, vec3_t lookVec )
{//FIXME: calc this and store as a lookTarg and redo it only if no lookTarg?
	gentity_t	*newenemy = NULL;
	int			entNum;
	vec3_t		diff, eyes, enemySpot, bestSpot;
	float		relDist;
	float		bestDist = Q3_INFINITE;

	VectorClear( bestSpot );
	CalcEntitySpot( self, SPOT_HEAD_LEAN, eyes );
	for ( entNum = 0; entNum < globals.num_entities; entNum++ )
	{
		newenemy = &g_entities[entNum];

		if ( !gi.inPVS( newenemy->currentOrigin, NPC->currentOrigin ) )//FIXME: IgnorePortals bad?
		{
			continue;
		}

		if ( newenemy->client && !(newenemy->flags & FL_NOTARGET) )
		{
			if ( newenemy->client->playerTeam == self->client->enemyTeam )
			{//FIXME:  check for range and FOV or vis?
				VectorSubtract( NPC->currentOrigin, newenemy->currentOrigin, diff );
				relDist = VectorLengthSquared( diff );
				if ( relDist < bestDist )
				{
					if( NPC_CheckVisibility ( newenemy, CHECK_VISRANGE ) >= VIS_360 )//CHECK_360|
					{
						if(newenemy->health > 0)
						{
							CalcEntitySpot( newenemy, SPOT_HEAD, enemySpot );
						}
						else
						{//SPOT_GROUND?
							VectorCopy( newenemy->currentOrigin, enemySpot );
						}

						if( G_ClearLineOfSight( eyes, enemySpot, self->s.number, MASK_OPAQUE ) )
						{
							bestDist = relDist;
							VectorCopy( enemySpot, bestSpot );
						}
					}
				}
			}
		}
	}

	if(VectorLengthSquared(bestSpot))
	{
		VectorSubtract(bestSpot, eyes, lookVec);
		return qtrue;
	}

	return qfalse;
}


/*
-------------------------
NPC_BSFormation
-------------------------
*/

void NPC_BSFormation(void)
{
	vec3_t	weapspot, enemyorg, aimdir, desiredAngles, diff, dir;
	qboolean	faced = qfalse;

	//See if we need to move, first and foremost
	vec3_t	collisionDir;

	if ( NPC_BlockingPlayer( collisionDir ) )
	{
		NPC_Unobstruct( collisionDir );
		//Com_Printf("Blocking player\n");
		return;
	}

	NPCInfo->aiFlags &= ~NPCAI_STRAIGHT_TO_DESTPOS;
	ucmd.buttons |= BUTTON_CAREFUL;//always use careful walk, stand, run, etc.

	if ( !NPC->enemy )
	{
		gentity_t	*newenemy;
		newenemy = NPC_PickEnemy(NPC, NPC->client->enemyTeam, qtrue, qfalse, qtrue);
		if(newenemy)
		{//Just acquired one
			G_SetEnemy( NPC, newenemy );
		}
		else
		{
			NPC_SquadIdle();
		}
	}
	else
	{
		NPC_CheckEnemy( qfalse, qfalse );
	}

	if ( NPC->enemy )
	{
		/*
		if(NPC_BSPointCombat())
			return;
		*/

		//Just stand still and fire at targets
		//NPC_BSStandAndShoot();//NOTE:  It is not valid to call this from here, it breaks stuff
		
		//if(NPC->enemy)
		{//Old style behavior
			//Keeping in formation, but fighting
			//FIXME: should we check for VIS_PVS and VIS_360 before looking at enemy?
			CalcEntitySpot(NPC, SPOT_WEAPON, weapspot);
			CalcEntitySpot(NPC->enemy, SPOT_HEAD, enemyorg);
			NPC_AimWiggle( enemyorg );
			
			//FIXME - SPOT_ORIGIN looks down when they're close!
			VectorSubtract(enemyorg, weapspot, aimdir);
			VectorNormalize(aimdir);
			vectoangles(aimdir, desiredAngles);
			NPCInfo->desiredYaw = desiredAngles[YAW];
			NPCInfo->desiredPitch = desiredAngles[PITCH];

			faced = qtrue;
			NPC_UpdateAngles(qtrue, qtrue);
			//FIXME: these guys shoot each other!  
			//Need to check for a clear shot...
			//Maybe tell the person in the way to duck or sidestep?
			//If in combat, should probably script to run to actual hiding points, etc.
			if ( NPC_CheckAttack( 1.0 ) )
			{
				form_shot_traces++;
				enemyVisibility = NPC_CheckVisibility ( NPC->enemy, CHECK_FOV|CHECK_SHOOT|CHECK_VISRANGE );//CHECK_PVS|CHECK_360|
				if(enemyVisibility == VIS_SHOOT)
				{
					WeaponThink(qtrue);
				}
			}

			NPC->aimDebounceTime = 0;
		}
	}
	else if ( NPCInfo->combatPoint != -1 )
	{//FIXME: make a func call to clear your point
		level.combatPoints[NPCInfo->combatPoint].occupied = qfalse;
		NPCInfo->combatPoint = -1;
	}

	if ( NPCInfo->iSquadPathIndex != -1 )
	{//We have a path
		vec3_t	angles, forward, right, pathAngles, goalPos, vec;
		float	fDot, rDot;
		int		move = 127;
		qboolean	lost = qfalse;
		qboolean	reachedDest = qfalse;
		qboolean	blocked = qfalse;
		int			goalWp = -1;
		qboolean	dontCatchUp = qfalse;
		qboolean	checkedClearPath = qfalse;
		//FIXME: less likely to update further we are from player?
		//FIXME: still start/stop too suddenly and in synch with player, 
		//	make them keep walkinga few steps after the player stops and
		//	not start right away when player moves.  Also, make them move
		//	slower more, does that mean slow the player down?  Or just better
		//	anims?  Need better transitioning anims.  Also, altering fps of
		//	your anim a little may help too.
		//FIXME: only do this if player has moved more than 32 from the last spot we checked him at (when we got to our dest)

		NPCInfo->distToGoal = 0;

		if(VectorLengthSquared(NPC->client->ps.velocity) < 1.0f &&
			!(NPCInfo->aiFlags & NPCAI_OFF_PATH) && !(NPCInfo->aiFlags & NPCAI_FORM_TELE_NAV))
		{//We're not really moving and we're not trying to get on to our path
			VectorSubtract(NPC->client->team_leader->currentOrigin, NPC->currentOrigin, vec);
			if(VectorLengthSquared(vec) > 10000)//100 squared
			{//We're more than 100 away from leader
				VectorSubtract(NPC->client->team_leader->currentOrigin, NPCInfo->lastLeaderPoint, vec);
				if(VectorLengthSquared(vec) < 10000)//100 squared
				{//Leader has moved less than 100 since last we caught up
					dontCatchUp = qtrue;
				}
			}
		}

		if(dontCatchUp)
		{
			reachedDest = qtrue;
		}
		else
		{
			/*int	old1, old2, new1, new2;
			if(NPCInfo->sPCurSegPoint1 < NPCInfo->sPCurSegPoint2)
			{
				old1 = NPCInfo->sPCurSegPoint1;
				old2 = NPCInfo->sPCurSegPoint2;
			}
			else
			{
				old2 = NPCInfo->sPCurSegPoint1;
				old1 = NPCInfo->sPCurSegPoint2;
			}*/
			NPC_UpdatePathSegment( NPC );//Where on our path are we?
			/*if(NPCInfo->sPCurSegPoint1 < NPCInfo->sPCurSegPoint2)
			{
				new1 = NPCInfo->sPCurSegPoint1;
				new2 = NPCInfo->sPCurSegPoint2;
			}
			else
			{
				new2 = NPCInfo->sPCurSegPoint1;
				new1 = NPCInfo->sPCurSegPoint2;
			}

			if(old1!=new1 && old2 != new2)
			{
				gi.Printf("%s NewSeg: %d : %d\n", NPC->script_targetname, NPCInfo->sPCurSegPoint1, NPCInfo->sPCurSegPoint2);
			}*/

			//FIXME:  On Stasis levels, chug occurs when the leader starts moving around...
			//		Need to figure out what exactly is so expensive...
			//		Is it "ClearPathTo" traces?
			//		Is it collision avoidance?
			//		Is it enemy visibility/check shoot traces?
			//		Is it inPVS checks?
			//		Is it math overhead from processing the path and picking our destPos?
			//		Or is it just movement physics?
			//		Whatever it is, it accentuates the fact that patch traces are much,
			//			much more expensive than regular traces.
			NPC_FindsPDestSegPoint( NPC );//Where on our path do we want to be

			if ( NPCInfo->aiFlags & NPCAI_STRAIGHT_TO_DESTPOS )
			{//We're heading straight for our sPDestPos
				VectorSubtract( NPCInfo->sPDestPos, NPC->currentOrigin, diff );
				NPCInfo->sPDestPosPathDist = VectorLength( diff );
			}
			else
			{//we're trying to use our path to get somewhere
				NPC_FindPathToGoalPos( NPC );//How far along our path are we from where we want to be?
				if ( NPCInfo->curSegNextWp == -1 )
				{//We don't have a path to our goalPos, go to our closest one
					goalWp = NPC_FindClosestSquadPoint( NPC );//NPCInfo->sPDestSegPoint1;
					if ( goalWp != -1 )
					{
						VectorCopy( squadPaths[NPCInfo->iSquadPathIndex].waypoints[goalWp].origin, NPCInfo->sPDestPos );
						VectorSubtract( NPCInfo->sPDestPos, NPC->currentOrigin, diff );
						NPCInfo->sPDestPosPathDist = VectorLength( diff );
					}
				}
				else
				{//head for our next
					goalWp = NPCInfo->curSegNextWp;
				}
			}

			angles[PITCH] = angles[ROLL] = 0;
			angles[YAW] = NPC->client->ps.viewangles[YAW];
			AngleVectors(angles, forward, right, NULL);

			if ( goalWp != -1 || NPCInfo->aiFlags & NPCAI_STRAIGHT_TO_DESTPOS )
			{//We have a next wp or heading straight to our sPDestPos, start moving
				int	neededDistSquared;
				//Are we close to our sPDestPos?
				if ( NPCInfo->aiFlags & NPCAI_FORM_TELE_NAV )
				{//We're trying to go through a teleporter, get right up to this point
					neededDistSquared = 0;
				}
				else if ( NPCInfo->aiFlags & NPCAI_STRAIGHT_TO_DESTPOS )
				{//We're trying to reacquire our path, so require us to get much closer to it point
					neededDistSquared = 64;//8 squared
				}
				else
				{
					neededDistSquared = MAX_WAYPOINT_REACHED_DIST_SQUARED;
				}

				if ( NPCInfo->sPDestPosPathDist*NPCInfo->sPDestPosPathDist > neededDistSquared )
				{//Still on our way there
					//FIXME: If use our clipmask: when someone is blocking our goalPos,
					//this makes us run to the waypoint unnecessarily, so using MASK_DEADSOLID
					if ( NPCInfo->aiFlags & NPCAI_STRAIGHT_TO_DESTPOS )
					{//head straight there
						//gi.Printf("%s heading straight for dest\n", NPC->script_targetname);
						VectorCopy( NPCInfo->sPDestPos, goalPos );
					}
					else if ( NPCInfo->currentSquadPoint == goalWp && NPCInfo->destSegLastWp == goalWp )
					{//head straight there
						//gi.Printf("%s hit dest lastwp, heading straight for dest\n", NPC->script_targetname);
						VectorCopy( NPCInfo->sPDestPos, goalPos );
					}
					else
					{//using squadPoints to get there
						//gi.Printf("%s heading forWp %d\n", NPC->script_targetname, goalWp);
						//Head to the goalWp
						VectorCopy( squadPaths[NPCInfo->iSquadPathIndex].waypoints[goalWp].origin, goalPos );

						//Now let's see if we can head straight for our sPDestPos
						if ( goalWp == NPCInfo->destSegLastWp || (NPCInfo->destSegLastWp == -1 && goalWp == NPCInfo->curSegNextWp) )
						{//we're en route to our goalWp
							if ( gi.inPVS( NPC->currentOrigin, NPCInfo->sPDestPos ) )//FIXME: IgnorePortals?
							{//In PVS of our goalPos
								form_clearpath_traces++;
								if ( NAV_ClearPathToPoint( NPC, NPC->mins, NPC->maxs, NPCInfo->sPDestPos, MASK_DEADSOLID ) )
								{//Nothing blocking us, head right for it
									//gi.Printf("%s can get to dest wp, heading straight for it\n", NPC->script_targetname);
									VectorCopy( NPCInfo->sPDestPos, goalPos );
									checkedClearPath = qtrue;
								}
							}
						}
					}
				}
				else
				{//We're there, stop
					reachedDest = qtrue;
					//VectorCopy(NPCInfo->formGoalPos, goalPos);
					//We're now on our path and ready to roll
					NPCInfo->aiFlags &= ~NPCAI_OFF_PATH;
					NPCInfo->aiFlags &= ~NPCAI_STRAIGHT_TO_DESTPOS;
					//This should make us wait here
					VectorCopy( NPC->currentOrigin, goalPos );
					VectorCopy( NPC->client->team_leader->currentOrigin, NPCInfo->lastLeaderPoint );
				}
			}
			else
			{//Can't get there, now what?  For now, stop.
				//This should make us wait here
				VectorCopy( NPC->currentOrigin, goalPos );
				lost = qtrue;
				//Ok, we're fucked, time to do a more expensive search...
				//Find the closest point to ourself on our path and head there
				NPCInfo->aiFlags |= NPCAI_OFF_PATH;
			}

			ucmd.forwardmove = 0;
			ucmd.rightmove = 0;
			NPCInfo->distToGoal = 0;

			//FIXME:  if the path will go through a teleporter, don't go through
			//until leader does (vectorlength of leaderTeleportSpot is non-zero)

			if ( !reachedDest && !lost )
			{//Moving
				if ( NPCInfo->aiFlags & NPCAI_FORM_TELE_NAV )
				{//NOTE:  This could be made more generic...?
					//Nav to goalPos
					qboolean	clearPath = qfalse;

					/*
					VectorCopy( goalPos, NPCInfo->tempGoal->currentOrigin );
					NPCInfo->goalEntity = NPCInfo->tempGoal;
					*/

					NPC_SetMoveGoal( NPC, goalPos, 16, qtrue );
					
					if ( !checkedClearPath )
					{
						clearPath = NPC_ClearPathToGoal( dir, NPCInfo->goalEntity );
					}

					if ( !clearPath )
					{//need to Nav there
						vec3_t		dir;
						navInfo_t	info;

						if ( NAV_MoveToGoal( NPC, info ) == WAYPOINT_NONE )
						{
							//Try to follow them for 10 seconds, then see if they're close,
							//if not, try for another 10 seconds
							if ( NPCInfo->navTime > level.time )
							{
								//head straight there anyway
								VectorSubtract( goalPos, NPC->currentOrigin, dir );
							}
							else
							{
								vec3_t	leaderVec;
								
								//if leader right here with me,
								//	forget about teleport thing...
								VectorSubtract( NPC->client->team_leader->currentOrigin, NPC->currentOrigin, leaderVec );
								if ( VectorLengthSquared( leaderVec ) < 128*128 )
								{//FIXME: do some more expensive, intelligent check?
									//Can't nav there, so... screw it
									//Back into normal formation
									VectorClear( NPCInfo->leaderTeleportSpot );
									NPCInfo->aiFlags &= ~NPCAI_FORM_TELE_NAV;
									//Just stand around this time
									VectorClear( dir );
								}
								else
								{//Tryagain for another 10 seconds
									NPCInfo->navTime = level.time + 10000;
								}
							}
						}
					}
				}
				else
				{
					VectorSubtract( goalPos, NPC->currentOrigin, dir );
					
					if ( fabs(dir[0]) <= NPC->maxs[0] && fabs(dir[1]) <= NPC->maxs[1] )
					{//If we're close to it on X & Y, and Z is within reasonable distance, eliminate Z diff
						if ( dir[2] < 0 )
						{
							if ( dir[2] > NPC->mins[2] )
							{
								dir[2] = 0;
							}
						}
						else if ( dir[2] < 64 )
						{
							dir[2] = 0;
						}
					}
				}

				if ( ( NPCInfo->distToGoal = VectorNormalize( dir ) ) )//We're actually trying to move
				{
					navInfo_t	info;
					
					VectorCopy( dir, info.direction );
					VectorCopy( dir, info.pathDirection );
					info.distance = NPCInfo->distToGoal;

					if ( NAV_AvoidCollision( NPC, NPCInfo->goalEntity, info ) == qfalse )
					{//Blocked
						//FIXME: if totally blocked, STOP!!!!
						if ( NPCInfo->consecutiveBlockedMoves >= 100 )
						{//Blocked for a whole second straight
							if ( NPCInfo->blockingEntNum > -1 && NPCInfo->blockingEntNum < ENTITYNUM_NONE )
							{
								gentity_t *blocker = &g_entities[NPCInfo->blockingEntNum];
								if ( NPCInfo->blockingEntNum < ENTITYNUM_WORLD || (blocker && !blocker->client) )
								{//We're being blocked by a non-player
									//We need to reacquire our path
									NPCInfo->aiFlags |= NPCAI_OFF_PATH;
								}
							}
						}
						blocked = qtrue;
						VectorClear( dir );
					}
				}

				dir[2] = 0;
				vectoangles( dir, pathAngles );
				if ( !blocked )
				{
					if ( NPCInfo->distToGoal == 0 ) 
						NPCInfo->distToGoal = VectorNormalize( dir );

					//compare dir to angles to get proper move commands for left and right
					fDot = DotProduct( forward, dir );
					rDot = DotProduct( right, dir );
					if ( fabs( fDot ) < 0.01 )
					{
						fDot = 0;
					}

					if ( fabs( rDot ) < 0.01 )
					{
						rDot = 0;
					}
					//FIXME: should this always be a run?  If we're close should we slow down?
					//	Or should we actually modify our speed field to speed up and slow down?
					ucmd.forwardmove = floor( move * fDot );
					ucmd.rightmove = floor( move * rDot );
				}
			}
		}

		if ( !lost )
		{
			//FIXME: base speed on speed of leader?
			
			//SPEED
			
			//Now calculate the desired speed
			if ( blocked )
			{//Sudden stop
				NPCInfo->desiredSpeed = NPCInfo->currentSpeed = 0;
				NPCInfo->aiFlags &= ~NPCAI_STRAIGHT_TO_DESTPOS;
			}
			else
			{
				if ( reachedDest )
				{//we're there, full stop (don't overshoot)
					NPCInfo->desiredSpeed = 0;
					NPCInfo->currentSpeed = 0;
				}
				else if ( NPCInfo->sPDestPosPathDist > 200 && NPCInfo->distToGoal > 64 )
				{//We're far from our destPos
					NPCInfo->desiredSpeed = NPCInfo->stats.runSpeed + 10;
				}
				else if ( NPCInfo->sPDestPosPathDist > 64 )
				{//We're closer to our destPos
					NPCInfo->desiredSpeed = NPCInfo->stats.walkSpeed - 10;
				}
				else
				{//We want to slow to a stop
					if ( NPCInfo->sPDestPosPathDist > 32 )
					{//We're still a bit from our dest keep moving
						NPCInfo->desiredSpeed = 64;
					}
					else if ( NPCInfo->currentSpeed <= 0 )
					{//we're close but not there yet, what are we stopped for?  Get moving!
						NPCInfo->desiredSpeed = 32;
					}
					/*
					else
					{//We're still moving but very close, stop!
						//Full stop
						NPCInfo->aiFlags &= ~NPCAI_OFF_PATH;
						NPCInfo->aiFlags &= ~NPCAI_STRAIGHT_TO_DESTPOS;
						NPCInfo->desiredSpeed = 0;
						NPCInfo->currentSpeed = 0;
						ucmd.forwardmove = ucmd.rightmove = 0;
					}*/
				}
			}

			if ( ucmd.forwardmove == 0 && ucmd.rightmove == 0 && !NPC->enemy )
			{//Waiting around at sPDestPos
				if ( NPC->aimDebounceTime < level.time )
				{//FIXME: need to remember last lookMode
					vec3_t		lookVec;
					lookMode_t	lookMode = LT_NONE;

					//FIXME: maybe look for dead buddies' bodies first...
					if ( NPC_FindLocalEnemies( NPC, lookVec) )
					{//Found an enemy to look at
						lookMode = LT_AIMSOFT;
					}
					else
					{
						//ANGLES
						NPCInfo->idleCounter++;
						if ( NPCInfo->idleCounter > Q_irand( 5, 10 ) )
						{//See if there are any interest spots to look at
							NPCInfo->idleCounter = 11;//No jitter
							lookMode = (lookMode_t)NPC_FindLocalInterestPoint( NPC, lookVec );
						}
					}

					if ( lookMode != LT_NONE )
					{//FIXME: based on intensity of lookMode, 
						//look away (down path) every now and then?
						vectoangles( lookVec, pathAngles );
						NPCInfo->lookMode = lookMode;
					}
					else
					{//If not, look in the dir of our path
						VectorCopy( NPCInfo->lastPathAngles, pathAngles );
					}
					
					//face toward front of formation and look around.
					//FIXME: randomly play guard idle (occais) and guard_lookaround (rare)
					int	changelook = Q_irand( 0, 100 );
					int torsoAnim = (NPC->client->ps.torsoAnim&~ANIM_TOGGLEBIT);

					//FIXME: make these transition ONLY when finished with last frame, otherwise they snap
					if ( ( changelook < 20 || torsoAnim == BOTH_STAND2TO4 || torsoAnim == BOTH_STAND4 || torsoAnim == BOTH_STAND4TO2 ) && 
						NPC->client->ps.weapon != WP_NONE && NPC->client->ps.weapon != WP_SABER && 
						torsoAnim != BOTH_GUARD_LOOKAROUND1 &&
						torsoAnim != BOTH_GUARD_IDLE1 && 
						torsoAnim != BOTH_STAND2_RANDOM1 &&
						torsoAnim != BOTH_STAND2_RANDOM2 &&
						torsoAnim != BOTH_STAND2_RANDOM3 &&
						torsoAnim != BOTH_STAND2_RANDOM4 )
					{//Play one of the lookarounds instead
						//NPCInfo->lookMode = LT_NONE;

						NPCInfo->desiredYaw = AngleNormalize360(pathAngles[YAW]);
						NPCInfo->desiredPitch = AngleNormalize360(pathAngles[PITCH]);
						if ( torsoAnim == BOTH_STAND2TO4 )
						{
							if ( PM_HasAnimation( NPC, BOTH_STAND4 ) )
							{
								NPC_SetAnim( NPC, SETANIM_BOTH, BOTH_STAND4, SETANIM_FLAG_NORMAL );
								NPC->aimDebounceTime = level.time + PM_AnimLength( NPC->client->clientInfo.animFileIndex, BOTH_STAND4 ) - FRAMETIME;
							}
						}
						else if ( torsoAnim == BOTH_STAND4TO2 )
						{
							if ( PM_HasAnimation( NPC, BOTH_STAND2 ) )
							{
								NPC_SetAnim( NPC, SETANIM_BOTH, BOTH_STAND2, SETANIM_FLAG_NORMAL );
								NPC->aimDebounceTime = level.time + PM_AnimLength( NPC->client->clientInfo.animFileIndex, BOTH_STAND2 );
							}
						}
						else if ( torsoAnim == BOTH_STAND4 )
						{
							if ( PM_HasAnimation( NPC, BOTH_STAND4TO2 ) )
							{
								NPC_SetAnim( NPC, SETANIM_BOTH, BOTH_STAND4TO2, SETANIM_FLAG_NORMAL );
								NPC->aimDebounceTime = level.time + PM_AnimLength( NPC->client->clientInfo.animFileIndex, BOTH_STAND4TO2 ) - FRAMETIME;
							}
						}
						else if ( changelook < 4 && (torsoAnim != BOTH_GUARD_LOOKAROUND1) )
						{//guard look around
							NPC_SetAnim( NPC, SETANIM_BOTH, BOTH_GUARD_LOOKAROUND1, SETANIM_FLAG_NORMAL );
							if ( (NPC->client->ps.torsoAnim&~ANIM_TOGGLEBIT) == BOTH_GUARD_LOOKAROUND1 )
							{
								NPC->aimDebounceTime = level.time + PM_AnimLength( NPC->client->clientInfo.animFileIndex, BOTH_GUARD_LOOKAROUND1 );
							}
						}
						else if ( changelook < 10 && torsoAnim != BOTH_GUARD_IDLE1 )
						{//guard idle
							NPC_SetAnim( NPC, SETANIM_BOTH, BOTH_GUARD_IDLE1, SETANIM_FLAG_NORMAL );
							if ( (NPC->client->ps.torsoAnim&~ANIM_TOGGLEBIT) == BOTH_GUARD_IDLE1 )
							{
								NPC->aimDebounceTime = level.time + PM_AnimLength( NPC->client->clientInfo.animFileIndex, BOTH_GUARD_IDLE1 );
							}						
						}
						else if ( changelook < 18 ) 
						{//random stand2 anim
							int standAnim = PM_PickAnim( NPC, BOTH_STAND2_RANDOM1, BOTH_STAND2_RANDOM4 );
							if ( standAnim != -1 )
							{
								NPC_SetAnim( NPC, SETANIM_BOTH, standAnim, SETANIM_FLAG_NORMAL );
								NPC->aimDebounceTime = level.time + PM_AnimLength( NPC->client->clientInfo.animFileIndex, (animNumber_t)standAnim );
							}//else???
						}
						else
						{//transition to stand4
							if ( PM_HasAnimation( NPC, BOTH_STAND2TO4 ) )
							{
								NPC_SetAnim( NPC, SETANIM_BOTH, BOTH_STAND2TO4, SETANIM_FLAG_NORMAL );
								NPC->aimDebounceTime = level.time + PM_AnimLength( NPC->client->clientInfo.animFileIndex, BOTH_STAND2TO4 ) - FRAMETIME;
							}
						}

					}
					else
					{//FIXME: do this also when walking around?
						//FIXME: only glance
						NPC_SetAnim( NPC, SETANIM_BOTH, BOTH_STAND2, SETANIM_FLAG_NORMAL );
						if ( changelook > 90 )
						{//FIXME: this turning should be torso only...
							NPCInfo->desiredYaw = AngleNormalize360( pathAngles[YAW] + Q_irand( 10, 30 ) );
							NPC->aimDebounceTime = level.time + Q_irand( 500, 2000 );
						}
						else if ( changelook > 80 )
						{
							NPCInfo->desiredYaw = AngleNormalize360( pathAngles[YAW] - Q_irand( 10, 30 ) );
							NPC->aimDebounceTime = level.time + Q_irand( 500, 2000 );
						}
						else if ( changelook > 40 )
						{
							NPCInfo->desiredYaw = AngleNormalize360( pathAngles[YAW] );
							NPC->aimDebounceTime = level.time + Q_irand( 500, 2000 );
						}
						
						if ( lookMode != LT_NONE )
						{
							changelook = Q_irand( 0, 100 );
							if ( changelook > 90 )
							{//FIXME: this turning should be torso only...
								NPCInfo->desiredPitch = AngleNormalize360( pathAngles[PITCH] + Q_irand( 1, 20 ) );
								NPC->aimDebounceTime = level.time + Q_irand( 500, 2000 );
							}
							else if ( changelook > 80 )
							{
								NPCInfo->desiredPitch = AngleNormalize360( pathAngles[PITCH] - Q_irand( 1, 20 ) );
								NPC->aimDebounceTime = level.time + Q_irand( 500, 2000 );
							}
							else if ( changelook > 40 )
							{
								NPCInfo->desiredPitch = AngleNormalize360( pathAngles[PITCH] );
								NPC->aimDebounceTime = level.time + Q_irand( 500, 2000 );
							}
						}
					}
				}
			}
			else
			{
				//ANGLES
				VectorCopy( pathAngles, NPCInfo->lastPathAngles );

				NPCInfo->idleCounter = 0;
				if ( ucmd.forwardmove || ucmd.rightmove )
				{//When moving, always face forward
					NPCInfo->lookMode = LT_NONE;
				}
				else if ( NPC->enemy )
				{//Angles set at beginning of func, turn off lookmode
					NPCInfo->lookMode = LT_AIM;
				}

				if ( !NPC->enemy )
				{//Moving toward goal without an enemy
					if ( NPC->aimDebounceTime > level.time )
					{					
						NPCInfo->desiredYaw = pathAngles[YAW];
						NPCInfo->desiredPitch = 0;
					}
					else
					{//Play one of the idles
						int	changelook = Q_irand(0, 100);

						//FIXME: make these transition ONLY when finished with last frame, otherwise they snap
						if ( changelook < 10 &&
							((NPC->client->ps.torsoAnim&~ANIM_TOGGLEBIT) != BOTH_GUARD_IDLE1) )
						{//Play one of the lookarounds instead
							//NPCInfo->lookMode = LT_NONE;

							NPCInfo->desiredYaw = AngleNormalize360( pathAngles[YAW] );
							NPCInfo->desiredPitch = AngleNormalize360( pathAngles[PITCH] );
							NPC_SetAnim( NPC, SETANIM_TORSO, BOTH_GUARD_IDLE1, SETANIM_FLAG_NORMAL );
							if ( (NPC->client->ps.torsoAnim&~ANIM_TOGGLEBIT) == BOTH_GUARD_IDLE1 )
							{
								NPC->aimDebounceTime = level.time + PM_AnimLength( NPC->client->clientInfo.animFileIndex, BOTH_GUARD_IDLE1 );
							}
						}//else the normal upper should take over...
					}
				}
			}
		}
		//Should we make them strafe more, such as looking around a corner?
		//	Maybe we should always face them at the NEXT point in the path
		//	AFTER the one they're heading to.
	}
	else
	{//Stand here or just follow my leader at a distance?
	}

	if ( !faced )
	{
		NPC_UpdateAngles( qtrue, qtrue );
	}
	//FIXME: non-combat squadmates shouldn't do this...
	/*
	if(ucmd.forwardmove > 0 || ucmd.rightmove > 0)
	{
		//Moving anims
		if(!(ucmd.buttons & BUTTON_ATTACK))
		{//not shooting
			if(ucmd.forwardmove <= 64 || (ucmd.buttons & BUTTON_WALKING) || (ucmd.upmove < 0) || NPCInfo->distToGoal < 100)
			{//walking or crouching or close to goal
				NPC_SetAnim(NPC,SETANIM_TORSO,TORSO_WEAPONIDLE1,SETANIM_FLAG_NORMAL);
			}
		}
	}
	else
	{//standing around
		if(NPC->s.weapon == WP_SABER)
		{//one handed weapon
			NPC_SetAnim(NPC,SETANIM_TORSO,TORSO_WEAPONREADY1,SETANIM_FLAG_NORMAL);
		}
		else
		{//two handed weapon
			NPC_SetAnim(NPC,SETANIM_TORSO,TORSO_WEAPONREADY3,SETANIM_FLAG_NORMAL);
		}

		NPC_SetAnim(NPC,SETANIM_LEGS,BOTH_ATTACK2,SETANIM_FLAG_NORMAL);

	}
	*/
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

/*
-------------------------
NPC_FindLocalInterestPoint 
-------------------------
*/

lookMode_t NPC_FindLocalInterestPoint (gentity_t *self, vec3_t lookVec)
{
	int		i, bestPoint = -1;
	float	dist, bestDist = Q3_INFINITE;
	vec3_t	diffVec, eyes;

	CalcEntitySpot(self, SPOT_HEAD_LEAN, eyes);
	for(i = 0; i < level.numInterestPoints; i++)
	{
		//Don't ignore portals?  If through a portal, need to look at portal!
		if( gi.inPVS(level.interestPoints[i].origin, eyes) )
		{
			VectorSubtract(level.interestPoints[i].origin, eyes, diffVec);
			if((fabs(diffVec[0]) + fabs(diffVec[1])) / 2 < 48 &&
				fabs(diffVec[2]) > (fabs(diffVec[0]) + fabs(diffVec[1])) / 2 )
			{//Too close to look so far up or down
				continue;
			}
			dist = VectorLengthSquared(diffVec);
			//Some priority to more interesting points
			dist -= ((int)level.interestPoints[i].lookMode * 5) * ((int)level.interestPoints[i].lookMode * 5);
			if( dist < MAX_INTEREST_DIST && dist < bestDist )
			{
				if(G_ClearLineOfSight(eyes, level.interestPoints[i].origin, self->s.number, MASK_OPAQUE))
				{
					bestDist = dist;
					bestPoint = i;
				}
			}
		}
	}

	if(bestPoint != -1)
	{
		VectorSubtract(level.interestPoints[bestPoint].origin, eyes, lookVec);
		return level.interestPoints[bestPoint].lookMode;
	}

	return LT_NONE;
}

/*QUAKED target_interest (1 0.8 0.5) (-4 -4 -4) (4 4 4)
A point that a squadmate will look at if standing still

target - thing to fire when someone looks at this thing

interest:
	0 = just glance at it (default)
	1 = look at it, kind of aim at it, keep feet forward
	2 = look at it, kind of aim at it, turn feet some
	3 = aim fully at it, keep feet forward
	4 = aim fully at it, turn feet some
	5 = fully face it with whole body
*/

void SP_target_interest( gentity_t *self )
{//FIXME: rename point_interest
	if(level.numInterestPoints >= MAX_INTEREST_POINTS)
	{
		gi.Printf("ERROR:  Too many interest points, limit is %d\n", MAX_INTEREST_POINTS);
		G_FreeEntity(self);
		return;
	}

	VectorCopy(self->currentOrigin, level.interestPoints[level.numInterestPoints].origin);
	self->health += 1;
	if(self->health > LT_FULLFACE)
	{
		level.interestPoints[level.numInterestPoints].lookMode = LT_FULLFACE;
	}
	else if(self->health < LT_GLANCE)
	{
		level.interestPoints[level.numInterestPoints].lookMode = LT_GLANCE;
	}
	else
	{
		level.interestPoints[level.numInterestPoints].lookMode = (lookMode_t)self->health;
	}

	if(self->target && self->target[0])
	{
		level.interestPoints[level.numInterestPoints].target = G_NewString( self->target );
	}

	level.numInterestPoints++;

	G_FreeEntity(self);
}
