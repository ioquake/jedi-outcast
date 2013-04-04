// tr_shade.c

#include "tr_local.h"

#include "tr_quicksprite.h"

/*

  THIS ENTIRE FILE IS BACK END

  This file deals with applying shaders to surface data in the tess struct.
*/

color4ub_t	styleColors[MAX_LIGHT_STYLES];

/*
================
R_ArrayElementDiscrete

This is just for OpenGL conformance testing, it should never be the fastest
================
*/
static void APIENTRY R_ArrayElementDiscrete( GLint index ) {
	qglColor4ubv( tess.svars.colors[ index ] );
	if ( glState.currenttmu ) {
		qglMultiTexCoord2fARB( 0, tess.svars.texcoords[ 0 ][ index ][0], tess.svars.texcoords[ 0 ][ index ][1] );
		qglMultiTexCoord2fARB( 1, tess.svars.texcoords[ 1 ][ index ][0], tess.svars.texcoords[ 1 ][ index ][1] );
	} else {
		qglTexCoord2fv( tess.svars.texcoords[ 0 ][ index ] );
	}
	qglVertex3fv( tess.xyz[ index ] );
}

/*
===================
R_DrawStripElements

===================
*/
static int		c_vertexes;		// for seeing how long our average strips are
static int		c_begins;
static void R_DrawStripElements( int numIndexes, const glIndex_t *indexes, void ( APIENTRY *element )(GLint) ) {
	int i;
	int last[3] = { -1, -1, -1 };
	qboolean even;

	c_begins++;

	if ( numIndexes <= 0 ) {
		return;
	}

	qglBegin( GL_TRIANGLE_STRIP );

	// prime the strip
	element( indexes[0] );
	element( indexes[1] );
	element( indexes[2] );
	c_vertexes += 3;

	last[0] = indexes[0];
	last[1] = indexes[1];
	last[2] = indexes[2];

	even = qfalse;

	for ( i = 3; i < numIndexes; i += 3 )
	{
		// odd numbered triangle in potential strip
		if ( !even )
		{
			// check previous triangle to see if we're continuing a strip
			if ( ( indexes[i+0] == last[2] ) && ( indexes[i+1] == last[1] ) )
			{
				element( indexes[i+2] );
				c_vertexes++;
				assert( indexes[i+2] < tess.numVertexes );
				even = qtrue;
			}
			// otherwise we're done with this strip so finish it and start
			// a new one
			else
			{
				qglEnd();

				qglBegin( GL_TRIANGLE_STRIP );
				c_begins++;

				element( indexes[i+0] );
				element( indexes[i+1] );
				element( indexes[i+2] );

				c_vertexes += 3;

				even = qfalse;
			}
		}
		else
		{
			// check previous triangle to see if we're continuing a strip
			if ( ( last[2] == indexes[i+1] ) && ( last[0] == indexes[i+0] ) )
			{
				element( indexes[i+2] );
				c_vertexes++;

				even = qfalse;
			}
			// otherwise we're done with this strip so finish it and start
			// a new one
			else
			{
				qglEnd();

				qglBegin( GL_TRIANGLE_STRIP );
				c_begins++;

				element( indexes[i+0] );
				element( indexes[i+1] );
				element( indexes[i+2] );
				c_vertexes += 3;

				even = qfalse;
			}
		}

		// cache the last three vertices
		last[0] = indexes[i+0];
		last[1] = indexes[i+1];
		last[2] = indexes[i+2];
	}

	qglEnd();
}



/*
==================
R_DrawElements

Optionally performs our own glDrawElements that looks for strip conditions
instead of using the single glDrawElements call that may be inefficient
without compiled vertex arrays.
==================
*/
static void R_DrawElements( int numIndexes, const glIndex_t *indexes ) {
	int		primitives;

	primitives = r_primitives->integer;

	// default is to use triangles if compiled vertex arrays are present
	if ( primitives == 0 ) {
		if ( qglLockArraysEXT ) {
			primitives = 2;
		} else {
			primitives = 1;
		}
	}


	if ( primitives == 2 ) {
		qglDrawElements( GL_TRIANGLES, 
						numIndexes,
						GL_INDEX_TYPE,
						indexes );
		return;
	}

	if ( primitives == 1 ) {
		R_DrawStripElements( numIndexes,  indexes, qglArrayElement );
		return;
	}
	
	if ( primitives == 3 ) {
		R_DrawStripElements( numIndexes,  indexes, R_ArrayElementDiscrete );
		return;
	}

	// anything else will cause no drawing
}


unsigned char randomindex, randominterval;
float randomchart[256] = {
	0.6554, 0.6909, 0.4806, 0.6218, 0.5717, 0.3896, 0.0677, 0.7356, 
	0.8333, 0.1105, 0.4445, 0.8161, 0.4689, 0.0433, 0.7152, 0.0336, 
	0.0186, 0.9140, 0.1626, 0.6553, 0.8340, 0.7094, 0.2020, 0.8087, 
	0.9119, 0.8009, 0.1339, 0.8492, 0.9173, 0.5003, 0.6012, 0.6117, 
	0.5525, 0.5787, 0.1586, 0.3293, 0.9273, 0.7791, 0.8589, 0.4985, 
	0.0883, 0.8545, 0.2634, 0.4727, 0.3624, 0.1631, 0.7825, 0.0662, 
	0.6704, 0.3510, 0.7525, 0.9486, 0.4685, 0.1535, 0.1545, 0.1121, 
	0.4724, 0.8483, 0.3833, 0.1917, 0.8207, 0.3885, 0.9702, 0.9200, 
	0.8348, 0.7501, 0.6675, 0.4994, 0.0301, 0.5225, 0.8011, 0.1696, 
	0.5351, 0.2752, 0.2962, 0.7550, 0.5762, 0.7303, 0.2835, 0.4717, 
	0.1818, 0.2739, 0.6914, 0.7748, 0.7640, 0.8355, 0.7314, 0.5288, 
	0.7340, 0.6692, 0.6813, 0.2810, 0.8057, 0.0648, 0.8749, 0.9199, 
	0.1462, 0.5237, 0.3014, 0.4994, 0.0278, 0.4268, 0.7238, 0.5107, 
	0.1378, 0.7303, 0.7200, 0.3819, 0.2034, 0.7157, 0.5552, 0.4887, 
	0.0871, 0.3293, 0.2892, 0.4545, 0.0088, 0.1404, 0.0275, 0.0238, 
	0.0515, 0.4494, 0.7206, 0.2893, 0.6060, 0.5785, 0.4182, 0.5528, 
	0.9118, 0.8742, 0.3859, 0.6030, 0.3495, 0.4550, 0.9875, 0.6900, 
	0.6416, 0.2337, 0.7431, 0.9788, 0.6181, 0.2464, 0.4661, 0.7621, 
	0.7020, 0.8203, 0.8869, 0.2145, 0.7724, 0.6093, 0.6692, 0.9686, 
	0.5609, 0.0310, 0.2248, 0.2950, 0.2365, 0.1347, 0.2342, 0.1668, 
	0.3378, 0.4330, 0.2775, 0.9901, 0.7053, 0.7266, 0.4840, 0.2820, 
	0.5733, 0.4555, 0.6049, 0.0770, 0.4760, 0.6060, 0.4159, 0.3427, 
	0.1234, 0.7062, 0.8569, 0.1878, 0.9057, 0.9399, 0.8139, 0.1407, 
	0.1794, 0.9123, 0.9493, 0.2827, 0.9934, 0.0952, 0.4879, 0.5160, 
	0.4118, 0.4873, 0.3642, 0.7470, 0.0866, 0.5172, 0.6365, 0.2676, 
	0.2407, 0.7223, 0.5761, 0.1143, 0.7137, 0.2342, 0.3353, 0.6880, 
	0.2296, 0.6023, 0.6027, 0.4138, 0.5408, 0.9859, 0.1503, 0.7238, 
	0.6054, 0.2477, 0.6804, 0.1432, 0.4540, 0.9776, 0.8762, 0.7607, 
	0.9025, 0.9807, 0.0652, 0.8661, 0.7663, 0.2586, 0.3994, 0.0335, 
	0.7328, 0.0166, 0.9589, 0.4348, 0.5493, 0.7269, 0.6867, 0.6614, 
	0.6800, 0.7804, 0.5591, 0.8381, 0.0910, 0.7573, 0.8985, 0.3083, 
	0.3188, 0.8481, 0.2356, 0.6736, 0.4770, 0.4560, 0.6266, 0.4677
};

#define WIND_DAMP_INTERVAL 50
#define WIND_GUST_TIME 2500.0
#define WIND_GUST_DECAY (1.0 / WIND_GUST_TIME)
extern bool R_GetWindVector(vec3_t windVector);

int		lastWindTime = 0;
float	curWindSpeed=0;
vec3_t	curWindBlowVect={0,0,0}, targetWindBlowVect={0,0,0};
vec3_t	curWindGrassDir={0,0,0}, targetWindGrassDir={0,0,0};
int		totalsurfsprites=0, sssurfaces=0;

qboolean curWindPointActive=qfalse;
float curWindPointForce = 0;
vec3_t curWindPoint;
int nextGustTime=0;
float gustLeft=0;

static void R_UpdateWind(void)
{
	float dtime, dampfactor;	// Time since last update and damping time for wind changes
	float ratio;
	vec3_t ang, diff, retwindvec;
	float targetspeed;

	if (backEnd.refdef.time == lastWindTime)
		return;

	if (backEnd.refdef.time < lastWindTime)
	{
		curWindSpeed = r_windSpeed->value;
		nextGustTime = 0;
		gustLeft = 0;
	}

	targetspeed = r_windSpeed->value;	// Minimum gust delay, in seconds.
	
	if (targetspeed > 0 && r_windGust->value)
	{
		if (gustLeft > 0)
		{	// We are gusting
			// Add an amount to the target wind speed
			targetspeed *= 1.0 + gustLeft;

			gustLeft -= (float)(backEnd.refdef.time - lastWindTime)*WIND_GUST_DECAY;
			if (gustLeft <= 0)
			{
				nextGustTime = backEnd.refdef.time + (r_windGust->value*1000)*flrand(1.0,4.0);
			}
		}
		else if (backEnd.refdef.time >= nextGustTime)
		{	// See if there is another right now
			// Gust next time, mano
			gustLeft = flrand(0.75,1.5);
		}
	}

	// See if there is a weather system that will tell us a windspeed.
	if (R_GetWindVector(retwindvec))
	{
		retwindvec[2]=0;
		VectorScale(retwindvec, -1.0, retwindvec);
		vectoangles(retwindvec, ang);
	}
	else
	{	// Calculate the target wind vector based off cvars
		ang[YAW] = r_windAngle->value;
	}

	ang[PITCH] = -90.0 + targetspeed;
	if (ang[PITCH]>-45.0)
	{
		ang[PITCH] = -45.0;
	}
	ang[ROLL] = 0;

	if (targetspeed>0)
	{
//		ang[YAW] += cos(tr.refdef.time*0.01+flrand(-1.0,1.0))*targetspeed*0.5;
//		ang[PITCH] += sin(tr.refdef.time*0.01+flrand(-1.0,1.0))*targetspeed*0.5;
	}
	
	// Get the grass wind vector first
	AngleVectors(ang, targetWindGrassDir, NULL, NULL);
	targetWindGrassDir[2]-=1.0;
//		VectorScale(targetWindGrassDir, targetspeed, targetWindGrassDir);

	// Now get the general wind vector (no pitch)
	ang[PITCH]=0;
	AngleVectors(ang, targetWindBlowVect, NULL, NULL);

	// Start calculating a smoothing factor so wind doesn't change abruptly between speeds.
	dampfactor = 1.0-r_windDampFactor->value;	// We must exponent the amount LEFT rather than the amount bled off
	dtime = (float)(backEnd.refdef.time - lastWindTime) * (1.0/(float)WIND_DAMP_INTERVAL);	// Our dampfactor is geared towards a time interval equal to "1".

	// Note that since there are a finite number of "practical" delta millisecond values possible, 
	// the ratio should be initialized into a chart ultimately.
	ratio = pow(dampfactor, dtime);

	// Apply this ratio to the windspeed...
	curWindSpeed = targetspeed - (ratio * (targetspeed-curWindSpeed));

	// Use the curWindSpeed to calculate the final target wind vector (with speed)
	VectorScale(targetWindBlowVect, curWindSpeed, targetWindBlowVect);
	VectorSubtract(targetWindBlowVect, curWindBlowVect, diff);
	VectorMA(targetWindBlowVect, -ratio, diff, curWindBlowVect);

	// Update the grass vector now
	VectorSubtract(targetWindGrassDir, curWindGrassDir, diff);
	VectorMA(targetWindGrassDir, -ratio, diff, curWindGrassDir);

	lastWindTime = backEnd.refdef.time;

	curWindPointForce = r_windPointForce->value - (ratio * (r_windPointForce->value - curWindPointForce));
	if (curWindPointForce < 0.01)
	{
		curWindPointActive = qfalse;
	}
	else
	{
		curWindPointActive = qtrue;
		curWindPoint[0] = r_windPointX->value;
		curWindPoint[1] = r_windPointY->value;
		curWindPoint[2] = 0;
	}

	if (r_surfaceSprites->integer >= 2)
	{
		ri.Printf( PRINT_DEVELOPER, "Surfacesprites Drawn: %d, on %d surfaces\n", totalsurfsprites, sssurfaces);
	}

	totalsurfsprites=0;
	sssurfaces=0;
}



/////////////////////////////////////////////
// Surface sprite calculation and drawing.
/////////////////////////////////////////////

#define FADE_RANGE			250.0
#define WINDPOINT_RADIUS	750.0

float SSVertAlpha[SHADER_MAX_VERTEXES];
float SSVertWindForce[SHADER_MAX_VERTEXES];
vec2_t SSVertWindDir[SHADER_MAX_VERTEXES];

qboolean SSAdditiveTransparency=qfalse;
qboolean SSUsingFog=qfalse;


/////////////////////////////////////////////
// Vertical surface sprites

static void RB_VerticalSurfaceSprite(vec3_t loc, float width, float height, byte light, 
										byte alpha, float wind, float windidle, vec2_t fog, int hangdown, vec2_t skew)
{
	vec3_t loc2, right;
	float angle;
	float windsway;
	float points[16];
	color4ub_t color;

	angle = ((loc[0]+loc[1])*0.02+(tr.refdef.time*0.0015));

	if (windidle>0.0)
	{
		windsway = (height*windidle*0.075);
		loc2[0] = loc[0]+skew[0]+cos(angle)*windsway;
		loc2[1] = loc[1]+skew[1]+sin(angle)*windsway;

		if (hangdown)
		{
			loc2[2] = loc[2]-height;
		}
		else
		{
			loc2[2] = loc[2]+height;
		}
	}
	else
	{
		loc2[0] = loc[0]+skew[0];
		loc2[1] = loc[1]+skew[1];
		if (hangdown)
		{
			loc2[2] = loc[2]-height;
		}
		else
		{
			loc2[2] = loc[2]+height;
		}
	}

	if (wind>0.0 && curWindSpeed > 0.001)
	{
		windsway = (height*wind*0.075);

		// Add the angle
		VectorMA(loc2, height*wind, curWindGrassDir, loc2);
		// Bob up and down
		if (curWindSpeed < 40.0)
		{
			windsway *= curWindSpeed*(1.0/100.0);
		}
		else
		{
			windsway *= 0.4;
		}
		loc2[2] += sin(angle*2.5)*windsway;
	}

	VectorScale(backEnd.viewParms.or.axis[1], width*0.5, right);

	color[0]=light;
	color[1]=light;
	color[2]=light;
	color[3]=alpha;

	// Bottom right
//	VectorAdd(loc, right, point);
	points[0] = loc[0] + right[0];
	points[1] = loc[1] + right[1];
	points[2] = loc[2] + right[2];
	points[3] = 0;

	// Top right
//	VectorAdd(loc2, right, point);
	points[4] = loc2[0] + right[0];
	points[5] = loc2[1] + right[1];
	points[6] = loc2[2] + right[2];
	points[7] = 0;

	// Top left
//	VectorSubtract(loc2, right, point);
	points[8] = loc2[0] - right[0];
	points[9] = loc2[1] - right[1];
	points[10] = loc2[2] - right[2];
	points[11] = 0;

	// Bottom left
//	VectorSubtract(loc, right, point);
	points[12] = loc[0] - right[0];
	points[13] = loc[1] - right[1];
	points[14] = loc[2] - right[2];
	points[15] = 0;

	// Add the sprite to the render list.
	SQuickSprite.Add(points, color, fog);
}

static void RB_VerticalSurfaceSpriteWindPoint(vec3_t loc, float width, float height, byte light, 
												byte alpha, float wind, float windidle, vec2_t fog, 
												int hangdown, vec2_t skew, vec2_t winddiff, float windforce)
{
	vec3_t loc2, right;
	float angle;
	float windsway;
	float points[16];
	color4ub_t color;

	if (windforce > 1)
		windforce = 1;

//	wind += 1.0-windforce;

	angle = (loc[0]+loc[1])*0.02+(tr.refdef.time*0.0015);

	if (curWindSpeed <80.0)
	{
		windsway = (height*windidle*0.1)*(1.0+windforce);
		loc2[0] = loc[0]+skew[0]+cos(angle)*windsway;
		loc2[1] = loc[1]+skew[1]+sin(angle)*windsway;
	}
	else
	{
		loc2[0] = loc[0]+skew[0];
		loc2[1] = loc[1]+skew[1];
	}
	if (hangdown)
	{
		loc2[2] = loc[2]-height;
	}
	else
	{
		loc2[2] = loc[2]+height;
	}

	if (curWindSpeed > 0.001)
	{
		// Add the angle
		VectorMA(loc2, height*wind, curWindGrassDir, loc2);
	}

	loc2[0] += height*winddiff[0]*windforce;
	loc2[1] += height*winddiff[1]*windforce;
	loc2[2] -= height*windforce*(0.75 + 0.15*sin((tr.refdef.time + 500*windforce)*0.01));

	VectorScale(backEnd.viewParms.or.axis[1], width*0.5, right);

	color[0]=light;
	color[1]=light;
	color[2]=light;
	color[3]=alpha;

	// Bottom right
//	VectorAdd(loc, right, point);
	points[0] = loc[0] + right[0];
	points[1] = loc[1] + right[1];
	points[2] = loc[2] + right[2];
	points[3] = 0;

	// Top right
//	VectorAdd(loc2, right, point);
	points[4] = loc2[0] + right[0];
	points[5] = loc2[1] + right[1];
	points[6] = loc2[2] + right[2];
	points[7] = 0;

	// Top left
//	VectorSubtract(loc2, right, point);
	points[8] = loc2[0] - right[0];
	points[9] = loc2[1] - right[1];
	points[10] = loc2[2] - right[2];
	points[11] = 0;

	// Bottom left
//	VectorSubtract(loc, right, point);
	points[12] = loc[0] - right[0];
	points[13] = loc[1] - right[1];
	points[14] = loc[2] - right[2];
	points[15] = 0;

	// Add the sprite to the render list.
	SQuickSprite.Add(points, color, fog);
}

static void RB_DrawVerticalSurfaceSprites( shaderStage_t *stage, shaderCommands_t *input) 
{
	int curindex, curvert;
 	vec3_t dist;
	float triarea;
	vec2_t vec1to2, vec1to3;

	vec3_t v1,v2,v3;
	float a1,a2,a3;
	float l1,l2,l3;
	vec2_t fog1, fog2, fog3;
	vec2_t winddiff1, winddiff2, winddiff3;
	float  windforce1, windforce2, windforce3;

	float posi, posj;
	float step;
	float fa,fb,fc;

	vec3_t curpoint;
	float width, height;
	float alpha, alphapos, thisspritesfadestart, light;

	byte randomindex2;

	vec2_t skew={0,0};
	vec2_t fogv;
	vec2_t winddiffv;
	float windforce=0;
	qboolean usewindpoint = (qboolean) !!(curWindPointActive && stage->ss.wind > 0);

	float cutdist=stage->ss.fadeMax, cutdist2=cutdist*cutdist;
	float fadedist=stage->ss.fadeDist, fadedist2=fadedist*fadedist;

	assert(cutdist2 != fadedist2);
	float inv_fadediff = 1.0/(cutdist2-fadedist2);

	// The faderange is the fraction amount it takes for these sprites to fade out, assuming an ideal fade range of 250
	float faderange = FADE_RANGE/(cutdist-fadedist);

	if (faderange > 1.0)
	{	// Don't want to force a new fade_rand
		faderange = 1.0;
	}

	// Quickly calc all the alphas and windstuff for each vertex
	for (curvert=0; curvert<input->numVertexes; curvert++)
	{
		// Calc alpha at each point
		VectorSubtract(backEnd.viewParms.or.origin, input->xyz[curvert], dist);
		SSVertAlpha[curvert] = 1.0 - (VectorLengthSquared(dist) - fadedist2) * inv_fadediff;
	}

	// Wind only needs initialization once per tess.
	if (usewindpoint && !tess.SSInitializedWind)
	{
		for (curvert=0; curvert<input->numVertexes;curvert++)
		{	// Calc wind at each point
			dist[0]=input->xyz[curvert][0] - curWindPoint[0];
			dist[1]=input->xyz[curvert][1] - curWindPoint[1];
			step = (dist[0]*dist[0] + dist[1]*dist[1]);	// dist squared

			if (step >= (float)(WINDPOINT_RADIUS*WINDPOINT_RADIUS))
			{	// No wind
				SSVertWindDir[curvert][0] = 0;
				SSVertWindDir[curvert][1] = 0;
				SSVertWindForce[curvert]=0;		// Should be < 1
			}
			else
			{
				if (step<1)
				{	// Don't want to divide by zero
					SSVertWindDir[curvert][0] = 0;
					SSVertWindDir[curvert][1] = 0;
					SSVertWindForce[curvert] = curWindPointForce * stage->ss.wind;
				}
				else
				{
					step = Q_rsqrt(step);		// Equals 1 over the distance.
					SSVertWindDir[curvert][0] = dist[0] * step;
					SSVertWindDir[curvert][1] = dist[1] * step;
					step = 1.0 - (1.0 / (step * WINDPOINT_RADIUS));	// 1- (dist/maxradius) = a scale from 0 to 1 linearly dropping off
					SSVertWindForce[curvert] = curWindPointForce * stage->ss.wind * step;	// *step means divide by the distance.
				}
			}
		}
		tess.SSInitializedWind = qtrue;
	}
	
	for (curindex=0; curindex<input->numIndexes-2; curindex+=3)
	{
		curvert = input->indexes[curindex];
		VectorCopy(input->xyz[curvert], v1);
		if (stage->ss.facing)
		{	// Hang down
			if (input->normal[curvert][2] > -0.5)
			{
				continue;
			}
		}
		else
		{	// Point up
			if (input->normal[curvert][2] < 0.5)
			{
				continue;
			}
		}
		l1 = input->vertexColors[curvert][2];
		a1 = SSVertAlpha[curvert];
		fog1[0] = *((float *)(tess.svars.texcoords[0])+(curvert<<1));
		fog1[1] = *((float *)(tess.svars.texcoords[0])+(curvert<<1)+1);
		winddiff1[0] = SSVertWindDir[curvert][0];
		winddiff1[1] = SSVertWindDir[curvert][1];
		windforce1 = SSVertWindForce[curvert];

		curvert = input->indexes[curindex+1];
		VectorCopy(input->xyz[curvert], v2);
		if (stage->ss.facing)
		{	// Hang down
			if (input->normal[curvert][2] > -0.5)
			{
				continue;
			}
		}
		else
		{	// Point up
			if (input->normal[curvert][2] < 0.5)
			{
				continue;
			}
		}
		l2 = input->vertexColors[curvert][2];
		a2 = SSVertAlpha[curvert];
		fog2[0] = *((float *)(tess.svars.texcoords[0])+(curvert<<1));
		fog2[1] = *((float *)(tess.svars.texcoords[0])+(curvert<<1)+1);
		winddiff2[0] = SSVertWindDir[curvert][0];
		winddiff2[1] = SSVertWindDir[curvert][1];
		windforce2 = SSVertWindForce[curvert];

		curvert = input->indexes[curindex+2];
		VectorCopy(input->xyz[curvert], v3);
		if (stage->ss.facing)
		{	// Hang down
			if (input->normal[curvert][2] > -0.5)
			{
				continue;
			}
		}
		else
		{	// Point up
			if (input->normal[curvert][2] < 0.5)
			{
				continue;
			}
		}
		l3 = input->vertexColors[curvert][2];
		a3 = SSVertAlpha[curvert];
		fog3[0] = *((float *)(tess.svars.texcoords[0])+(curvert<<1));
		fog3[1] = *((float *)(tess.svars.texcoords[0])+(curvert<<1)+1);
		winddiff3[0] = SSVertWindDir[curvert][0];
		winddiff3[1] = SSVertWindDir[curvert][1];
		windforce3 = SSVertWindForce[curvert];

		if (a1 <= 0.0 && a2 <= 0.0 && a3 <= 0.0)
		{
			continue;
		}

		// Find the area in order to calculate the stepsize
		vec1to2[0] = v2[0] - v1[0];
		vec1to2[1] = v2[1] - v1[1];
		vec1to3[0] = v3[0] - v1[0];
		vec1to3[1] = v3[1] - v1[1];

		// Now get the cross product of this sum.
		triarea = vec1to3[0]*vec1to2[1] - vec1to3[1]*vec1to2[0];
		triarea=fabs(triarea);
		if (triarea <= 1.0)
		{	// Insanely small abhorrent triangle.
			continue;
		}
		step = stage->ss.density * Q_rsqrt(triarea);

		randomindex = (byte)(v1[0]+v1[1]+v2[0]+v2[1]+v3[0]+v3[1]);
		randominterval = (byte)(v1[0]+v2[1]+v3[2])|0x03;	// Make sure the interval is at least 3, and always odd

		for (posi=0; posi<1.0; posi+=step)
		{
			for (posj=0; posj<(1.0-posi); posj+=step)
			{
				fa=posi+randomchart[randomindex]*step;
				randomindex += randominterval;

				fb=posj+randomchart[randomindex]*step;
				randomindex += randominterval;

				if (fa>1.0)
					continue;

				if (fb>(1.0-fa))
					continue;

				fc = 1.0-fa-fb;

				// total alpha, minus random factor so some things fade out sooner.
				alphapos = a1*fa + a2*fb + a3*fc;
				
				// Note that the alpha at this point is a value from 1.0 to 0.0, but represents when to START fading
				thisspritesfadestart = faderange + (1.0-faderange) * randomchart[randomindex];
				randomindex += randominterval;

				// Find where the alpha is relative to the fadestart, and calc the real alpha to draw at.
				alpha = 1.0 - ((thisspritesfadestart-alphapos)/faderange);
				if (alpha > 0.0)
				{
					if (alpha > 1.0) 
						alpha=1.0;

					if (SSUsingFog)
					{
						fogv[0] = fog1[0]*fa + fog2[0]*fb + fog3[0]*fc;
						fogv[1] = fog1[1]*fa + fog2[1]*fb + fog3[1]*fc;
					}

					if (usewindpoint)
					{
						winddiffv[0] = winddiff1[0]*fa + winddiff2[0]*fb + winddiff3[0]*fc;
						winddiffv[1] = winddiff1[1]*fa + winddiff2[1]*fb + winddiff3[1]*fc;
						windforce = windforce1*fa + windforce2*fb + windforce3*fc;
					}
			
					VectorScale(v1, fa, curpoint);
					VectorMA(curpoint, fb, v2, curpoint);
					VectorMA(curpoint, fc, v3, curpoint);

					light = l1*fa + l2*fb + l3*fc;
					if (SSAdditiveTransparency)
					{	// Additive transparency, scale light value
						light *= alpha;
					}

					randomindex2 = randomindex;
					width = stage->ss.width*(1.0 + (stage->ss.variance[0]*randomchart[randomindex2]));
					height = stage->ss.height*(1.0 + (stage->ss.variance[1]*randomchart[randomindex2++]));
					if (randomchart[randomindex2++]>0.5)
					{
						width = -width;
					}
					if (stage->ss.fadeScale!=0 && alphapos < 1.0)
					{
						width *= 1.0 + (stage->ss.fadeScale*(1.0-alphapos));
					}

					if (stage->ss.vertSkew != 0)
					{	// flrand(-vertskew, vertskew)
						skew[0] = height * ((stage->ss.vertSkew*2.0f*randomchart[randomindex2++])-stage->ss.vertSkew);
						skew[1] = height * ((stage->ss.vertSkew*2.0f*randomchart[randomindex2++])-stage->ss.vertSkew);
					}

					if (usewindpoint && windforce > 0 && stage->ss.wind > 0.0)
					{
						if (SSUsingFog)
						{
							RB_VerticalSurfaceSpriteWindPoint(curpoint, width, height, (byte)light, (byte)(alpha*255.0), 
										stage->ss.wind, stage->ss.windIdle, fogv, stage->ss.facing, skew,
										winddiffv, windforce);
						}
						else
						{
							RB_VerticalSurfaceSpriteWindPoint(curpoint, width, height, (byte)light, (byte)(alpha*255.0), 
										stage->ss.wind, stage->ss.windIdle, NULL, stage->ss.facing, skew, 
										winddiffv, windforce);
						}
					}
					else
					{
						if (SSUsingFog)
						{
							RB_VerticalSurfaceSprite(curpoint, width, height, (byte)light, (byte)(alpha*255.0), 
										stage->ss.wind, stage->ss.windIdle, fogv, stage->ss.facing, skew);
						}
						else
						{
							RB_VerticalSurfaceSprite(curpoint, width, height, (byte)light, (byte)(alpha*255.0), 
										stage->ss.wind, stage->ss.windIdle, NULL, stage->ss.facing, skew);
						}
					}

					totalsurfsprites++;
				}
			}
		}
	}
}


/////////////////////////////////////////////
// Oriented surface sprites

static void RB_OrientedSurfaceSprite(vec3_t loc, float width, float height, byte light, byte alpha, vec2_t fog, int faceup)
{
	vec3_t loc2, right;
	float points[16];
	color4ub_t color;

	color[0]=light;
	color[1]=light;
	color[2]=light;
	color[3]=alpha;

	if (faceup)
	{
		width *= 0.5;
		height *= 0.5;

		// Bottom right
	//	VectorAdd(loc, right, point);
		points[0] = loc[0] + width;
		points[1] = loc[1] - width;
		points[2] = loc[2] + 1.0;
		points[3] = 0;

		// Top right
	//	VectorAdd(loc, right, point);
		points[4] = loc[0] + width;
		points[5] = loc[1] + width;
		points[6] = loc[2] + 1.0;
		points[7] = 0;

		// Top left
	//	VectorSubtract(loc, right, point);
		points[8] = loc[0] - width;
		points[9] = loc[1] + width;
		points[10] = loc[2] + 1.0;
		points[11] = 0;

		// Bottom left
	//	VectorSubtract(loc, right, point);
		points[12] = loc[0] - width;
		points[13] = loc[1] - width;
		points[14] = loc[2] + 1.0;
		points[15] = 0;
	}
	else
	{
		VectorMA(loc, height, backEnd.viewParms.or.axis[2], loc2);
		VectorScale(backEnd.viewParms.or.axis[1], width*0.5, right);

		// Bottom right
	//	VectorAdd(loc, right, point);
		points[0] = loc[0] + right[0];
		points[1] = loc[1] + right[1];
		points[2] = loc[2] + right[2];
		points[3] = 0;

		// Top right
	//	VectorAdd(loc2, right, point);
		points[4] = loc2[0] + right[0];
		points[5] = loc2[1] + right[1];
		points[6] = loc2[2] + right[2];
		points[7] = 0;

		// Top left
	//	VectorSubtract(loc2, right, point);
		points[8] = loc2[0] - right[0];
		points[9] = loc2[1] - right[1];
		points[10] = loc2[2] - right[2];
		points[11] = 0;

		// Bottom left
	//	VectorSubtract(loc, right, point);
		points[12] = loc[0] - right[0];
		points[13] = loc[1] - right[1];
		points[14] = loc[2] - right[2];
		points[15] = 0;
	}

	// Add the sprite to the render list.
	SQuickSprite.Add(points, color, fog);
}

static void RB_DrawOrientedSurfaceSprites( shaderStage_t *stage, shaderCommands_t *input) 
{
	int curindex, curvert;
 	vec3_t dist;
	float triarea, minnormal;
	vec2_t vec1to2, vec1to3;

	vec3_t v1,v2,v3;
	float a1,a2,a3;
	float l1,l2,l3;
	vec2_t fog1, fog2, fog3;

	float posi, posj;
	float step;
	float fa,fb,fc;

	vec3_t curpoint;
	float width, height;
	float alpha, alphapos, thisspritesfadestart, light;
	byte randomindex2;
	vec2_t fogv;
	
	float cutdist=stage->ss.fadeMax, cutdist2=cutdist*cutdist;
	float fadedist=stage->ss.fadeDist, fadedist2=fadedist*fadedist;

	assert(cutdist2 != fadedist2);
	float inv_fadediff = 1.0/(cutdist2-fadedist2);

	// The faderange is the fraction amount it takes for these sprites to fade out, assuming an ideal fade range of 250
	float faderange = FADE_RANGE/(cutdist-fadedist);

	if (faderange > 1.0)
	{	// Don't want to force a new fade_rand
		faderange = 1.0;
	}

	if (stage->ss.facing)
	{	// Faceup sprite.
		minnormal = 0.99;
	}
	else
	{	// Normal oriented sprite
		minnormal = 0.5;
	}

	// Quickly calc all the alphas for each vertex
	for (curvert=0; curvert<input->numVertexes; curvert++)
	{
		// Calc alpha at each point
		VectorSubtract(backEnd.viewParms.or.origin, input->xyz[curvert], dist);
		SSVertAlpha[curvert] = 1.0 - (VectorLengthSquared(dist) - fadedist2) * inv_fadediff;
	}

	for (curindex=0; curindex<input->numIndexes-2; curindex+=3)
	{
		curvert = input->indexes[curindex];
		VectorCopy(input->xyz[curvert], v1);
		if (input->normal[curvert][2] < minnormal)
		{
			continue;
		}
		l1 = input->vertexColors[curvert][2];
		a1 = SSVertAlpha[curvert];
		fog1[0] = *((float *)(tess.svars.texcoords[0])+(curvert<<1));
		fog1[1] = *((float *)(tess.svars.texcoords[0])+(curvert<<1)+1);

		curvert = input->indexes[curindex+1];
		VectorCopy(input->xyz[curvert], v2);
		if (input->normal[curvert][2] < minnormal)
		{
			continue;
		}
		l2 = input->vertexColors[curvert][2];
		a2 = SSVertAlpha[curvert];
		fog2[0] = *((float *)(tess.svars.texcoords[0])+(curvert<<1));
		fog2[1] = *((float *)(tess.svars.texcoords[0])+(curvert<<1)+1);

		curvert = input->indexes[curindex+2];
		VectorCopy(input->xyz[curvert], v3);
		if (input->normal[curvert][2] < minnormal)
		{
			continue;
		}
		l3 = input->vertexColors[curvert][2];
		a3 = SSVertAlpha[curvert];
		fog3[0] = *((float *)(tess.svars.texcoords[0])+(curvert<<1));
		fog3[1] = *((float *)(tess.svars.texcoords[0])+(curvert<<1)+1);

		if (a1 <= 0.0 && a2 <= 0.0 && a3 <= 0.0)
		{
			continue;
		}

		// Find the area in order to calculate the stepsize
		vec1to2[0] = v2[0] - v1[0];
		vec1to2[1] = v2[1] - v1[1];
		vec1to3[0] = v3[0] - v1[0];
		vec1to3[1] = v3[1] - v1[1];

		// Now get the cross product of this sum.
		triarea = vec1to3[0]*vec1to2[1] - vec1to3[1]*vec1to2[0];
		triarea=fabs(triarea);
		if (triarea <= 1.0)
		{	// Insanely small abhorrent triangle.
			continue;
		}
		step = stage->ss.density * Q_rsqrt(triarea);

		randomindex = (byte)(v1[0]+v1[1]+v2[0]+v2[1]+v3[0]+v3[1]);
		randominterval = (byte)(v1[0]+v2[1]+v3[2])|0x03;	// Make sure the interval is at least 3, and always odd

		for (posi=0; posi<1.0; posi+=step)
		{
			for (posj=0; posj<(1.0-posi); posj+=step)
			{
				fa=posi+randomchart[randomindex]*step;
				randomindex += randominterval;
				if (fa>1.0)
					continue;

				fb=posj+randomchart[randomindex]*step;
				randomindex += randominterval;
				if (fb>(1.0-fa))
					continue;

				fc = 1.0-fa-fb;

				// total alpha, minus random factor so some things fade out sooner.
				alphapos = a1*fa + a2*fb + a3*fc;
				
				// Note that the alpha at this point is a value from 1.0 to 0.0, but represents when to START fading
				thisspritesfadestart = faderange + (1.0-faderange) * randomchart[randomindex];
				randomindex += randominterval;

				// Find where the alpha is relative to the fadestart, and calc the real alpha to draw at.
				alpha = 1.0 - ((thisspritesfadestart-alphapos)/faderange);

				randomindex += randominterval;
				if (alpha > 0.0)
				{
					if (alpha > 1.0) 
						alpha=1.0;

					if (SSUsingFog)
					{
						fogv[0] = fog1[0]*fa + fog2[0]*fb + fog3[0]*fc;
						fogv[1] = fog1[1]*fa + fog2[1]*fb + fog3[1]*fc;
					}

					VectorScale(v1, fa, curpoint);
					VectorMA(curpoint, fb, v2, curpoint);
					VectorMA(curpoint, fc, v3, curpoint);

					light = l1*fa + l2*fb + l3*fc;
					if (SSAdditiveTransparency)
					{	// Additive transparency, scale light value
						light *= alpha;
					}

					randomindex2 = randomindex;
					width = stage->ss.width*(1.0 + (stage->ss.variance[0]*randomchart[randomindex2]));
					height = stage->ss.height*(1.0 + (stage->ss.variance[1]*randomchart[randomindex2++]));
					if (randomchart[randomindex2++]>0.5)
					{
						width = -width;
					}
					if (stage->ss.fadeScale!=0 && alphapos < 1.0)
					{
						width *= 1.0 + (stage->ss.fadeScale*(1.0-alphapos));
					}

					if (SSUsingFog)
					{
						RB_OrientedSurfaceSprite(curpoint, width, height, (byte)light, (byte)(alpha*255.0), fogv, stage->ss.facing);
					}
					else
					{
						RB_OrientedSurfaceSprite(curpoint, width, height, (byte)light, (byte)(alpha*255.0), NULL, stage->ss.facing);
					}

					totalsurfsprites++;
				}
			}
		}
	}
}


/////////////////////////////////////////////
// Effect surface sprites

static void RB_EffectSurfaceSprite(vec3_t loc, float width, float height, byte light, byte alpha, float life, int faceup)
{
	vec3_t loc2, right;
	float points[16];
	color4ub_t color;

	color[0]=light;
	color[1]=light;
	color[2]=light;
	color[3]=alpha;

	if (faceup)
	{
		width *= 0.5;
		height *= 0.5;

		// Bottom right
	//	VectorAdd(loc, right, point);
		points[0] = loc[0] + width;
		points[1] = loc[1] - width;
		points[2] = loc[2] + 1.0;
		points[3] = 0;

		// Top right
	//	VectorAdd(loc, right, point);
		points[4] = loc[0] + width;
		points[5] = loc[1] + width;
		points[6] = loc[2] + 1.0;
		points[7] = 0;

		// Top left
	//	VectorSubtract(loc, right, point);
		points[8] = loc[0] - width;
		points[9] = loc[1] + width;
		points[10] = loc[2] + 1.0;
		points[11] = 0;

		// Bottom left
	//	VectorSubtract(loc, right, point);
		points[12] = loc[0] - width;
		points[13] = loc[1] - width;
		points[14] = loc[2] + 1.0;
		points[15] = 0;
	}
	else
	{
		VectorMA(loc, height, backEnd.viewParms.or.axis[2], loc2);
		VectorScale(backEnd.viewParms.or.axis[1], width*0.5, right);

		// Bottom right
	//	VectorAdd(loc, right, point);
		points[0] = loc[0] + right[0];
		points[1] = loc[1] + right[1];
		points[2] = loc[2] + right[2];
		points[3] = 0;

		// Top right
	//	VectorAdd(loc2, right, point);
		points[4] = loc2[0] + right[0];
		points[5] = loc2[1] + right[1];
		points[6] = loc2[2] + right[2];
		points[7] = 0;

		// Top left
	//	VectorSubtract(loc2, right, point);
		points[8] = loc2[0] - right[0];
		points[9] = loc2[1] - right[1];
		points[10] = loc2[2] - right[2];
		points[11] = 0;

		// Bottom left
	//	VectorSubtract(loc, right, point);
		points[12] = loc[0] - right[0];
		points[13] = loc[1] - right[1];
		points[14] = loc[2] - right[2];
		points[15] = 0;
	}

	// Add the sprite to the render list.
	SQuickSprite.Add(points, color, NULL);
}

static void RB_DrawEffectSurfaceSprites( shaderStage_t *stage, shaderCommands_t *input) 
{
	int curindex, curvert;
 	vec3_t dist;
	float triarea, minnormal;
	vec2_t vec1to2, vec1to3;

	vec3_t v1,v2,v3;
	float a1,a2,a3;
	float l1,l2,l3;

	float posi, posj;
	float step;
	float fa,fb,fc;
	float effecttime, effectpos;
	float density;

	vec3_t curpoint;
	float width, height;
	float alpha, alphapos, thisspritesfadestart, light;
	byte randomindex2;
	
	float cutdist=stage->ss.fadeMax, cutdist2=cutdist*cutdist;
	float fadedist=stage->ss.fadeDist, fadedist2=fadedist*fadedist;

	float fxalpha = stage->ss.fxAlphaEnd - stage->ss.fxAlphaStart;

	assert(cutdist2 != fadedist2);
	float inv_fadediff = 1.0/(cutdist2-fadedist2);

	// The faderange is the fraction amount it takes for these sprites to fade out, assuming an ideal fade range of 250
	float faderange = FADE_RANGE/(cutdist-fadedist);
	if (faderange > 1.0)
	{	// Don't want to force a new fade_rand
		faderange = 1.0;
	}

	if (stage->ss.facing)
	{	// Faceup sprite.
		minnormal = 0.99;
	}
	else
	{	// Normal oriented sprite
		minnormal = 0.5;
	}

	if (stage->ss.surfaceSpriteType == SURFSPRITE_WEATHERFX)
	{	// This effect is affected by weather settings.
		if (r_surfaceWeather->value < 0.01)
		{	// Don't show these effects
			return;
		}
		else
		{
			density = stage->ss.density / r_surfaceWeather->value;
		}
	}
	else
	{
		density = stage->ss.density;
	}

	// Quickly calc all the alphas for each vertex
	for (curvert=0; curvert<input->numVertexes; curvert++)
	{
		// Calc alpha at each point
		VectorSubtract(backEnd.viewParms.or.origin, input->xyz[curvert], dist);
		SSVertAlpha[curvert] = 1.0 - (VectorLengthSquared(dist) - fadedist2) * inv_fadediff;

	// Note this is the proper equation, but isn't used right now because it would be just a tad slower.
		// Formula for alpha is 1.0 - ((len-fade)/(cut-fade))
		// Which is equal to (1.0+fade/(cut-fade)) - (len/(cut-fade))
		// So mult=1/(cut-fade), and base=(1+fade*mult).
	//	SSVertAlpha[curvert] = fadebase - (VectorLength(dist) * fademult);

	}

	for (curindex=0; curindex<input->numIndexes-2; curindex+=3)
	{
		curvert = input->indexes[curindex];
		VectorCopy(input->xyz[curvert], v1);
		if (input->normal[curvert][2] < minnormal)
		{
			continue;
		}
		l1 = input->vertexColors[curvert][2];
		a1 = SSVertAlpha[curvert];

		curvert = input->indexes[curindex+1];
		VectorCopy(input->xyz[curvert], v2);
		if (input->normal[curvert][2] < minnormal)
		{
			continue;
		}
		l2 = input->vertexColors[curvert][2];
		a2 = SSVertAlpha[curvert];

		curvert = input->indexes[curindex+2];
		VectorCopy(input->xyz[curvert], v3);
		if (input->normal[curvert][2] < minnormal)
		{
			continue;
		}
		l3 = input->vertexColors[curvert][2];
		a3 = SSVertAlpha[curvert];

		if (a1 <= 0.0 && a2 <= 0.0 && a3 <= 0.0)
		{
			continue;
		}

		// Find the area in order to calculate the stepsize
		vec1to2[0] = v2[0] - v1[0];
		vec1to2[1] = v2[1] - v1[1];
		vec1to3[0] = v3[0] - v1[0];
		vec1to3[1] = v3[1] - v1[1];

		// Now get the cross product of this sum.
		triarea = vec1to3[0]*vec1to2[1] - vec1to3[1]*vec1to2[0];
		triarea=fabs(triarea);
		if (triarea <= 1.0)
		{	// Insanely small abhorrent triangle.
			continue;
		}
		step = density * Q_rsqrt(triarea);

		randomindex = (byte)(v1[0]+v1[1]+v2[0]+v2[1]+v3[0]+v3[1]);
		randominterval = (byte)(v1[0]+v2[1]+v3[2])|0x03;	// Make sure the interval is at least 3, and always odd

		for (posi=0; posi<1.0; posi+=step)
		{
			for (posj=0; posj<(1.0-posi); posj+=step)
			{
				effecttime = (tr.refdef.time+10000.0*randomchart[randomindex])/stage->ss.fxDuration;
				effectpos = (float)effecttime - (int)effecttime;

				randomindex2 = randomindex+effecttime;
				randomindex += randominterval;
				fa=posi+randomchart[randomindex2++]*step;
				if (fa>1.0)
					continue;

				fb=posj+randomchart[randomindex2++]*step;
				if (fb>(1.0-fa))
					continue;

				fc = 1.0-fa-fb;

				// total alpha, minus random factor so some things fade out sooner.
				alphapos = a1*fa + a2*fb + a3*fc;
				
				// Note that the alpha at this point is a value from 1.0 to 0.0, but represents when to START fading
				thisspritesfadestart = faderange + (1.0-faderange) * randomchart[randomindex2];
				randomindex2 += randominterval;

				// Find where the alpha is relative to the fadestart, and calc the real alpha to draw at.
				alpha = 1.0 - ((thisspritesfadestart-alphapos)/faderange);
				if (alpha > 0.0)
				{
					if (alpha > 1.0) 
						alpha=1.0;

					VectorScale(v1, fa, curpoint);
					VectorMA(curpoint, fb, v2, curpoint);
					VectorMA(curpoint, fc, v3, curpoint);

					light = l1*fa + l2*fb + l3*fc;
					randomindex2 = randomindex;
					width = stage->ss.width*(1.0 + (stage->ss.variance[0]*randomchart[randomindex2]));
					height = stage->ss.height*(1.0 + (stage->ss.variance[1]*randomchart[randomindex2++]));

					width = width + (effectpos*stage->ss.fxGrow[0]*width);
					height = height + (effectpos*stage->ss.fxGrow[1]*height);
					alpha = alpha*(stage->ss.fxAlphaStart+(fxalpha*effectpos));

					if (SSAdditiveTransparency)
					{	// Additive transparency, scale light value
						light *= alpha;
					}

					if (randomchart[randomindex2]>0.5)
					{
						width = -width;
					}
					if (stage->ss.fadeScale!=0 && alphapos < 1.0)
					{
						width *= 1.0 + (stage->ss.fadeScale*(1.0-alphapos));
					}

					if (stage->ss.wind>0.0 && curWindSpeed > 0.001)
					{
						vec3_t drawpoint;

						VectorMA(curpoint, effectpos*stage->ss.wind, curWindBlowVect, drawpoint);
						RB_EffectSurfaceSprite(drawpoint, width, height, (byte)light, (byte)(alpha*255.0), stage->ss.fxDuration, stage->ss.facing);
					}
					else
					{
						RB_EffectSurfaceSprite(curpoint, width, height, (byte)light, (byte)(alpha*255.0), stage->ss.fxDuration, stage->ss.facing);
					}

					totalsurfsprites++;
				}
			}
		}
	}
}


static void RB_DrawSurfaceSprites( shaderStage_t *stage, shaderCommands_t *input) 
{
	fog_t			*fog;
	unsigned long	glbits=stage->stateBits;
	
	R_UpdateWind();

	//
	// Check fog
	//
	if ( tess.fogNum && tess.shader->fogPass)// && r_drawfog->value) 
	{
		fog = tr.world->fogs + tess.fogNum;
		SSUsingFog = qtrue;
		SQuickSprite.StartGroup(&stage->bundle[0], glbits, fog->colorInt);
	}
	else
	{
		SSUsingFog = qfalse;
		SQuickSprite.StartGroup(&stage->bundle[0], glbits);
	}

	// Special provision in case the transparency is additive.
	if (glbits & GLS_SRCBLEND_ONE)
	{	// Additive transparency, scale light value
		SSAdditiveTransparency=qtrue;
	}
	else
	{
		SSAdditiveTransparency=qfalse;
	}

	switch(stage->ss.surfaceSpriteType)
	{
	case SURFSPRITE_VERTICAL:
		RB_DrawVerticalSurfaceSprites(stage, input);
		break;
	case SURFSPRITE_ORIENTED:
		RB_DrawOrientedSurfaceSprites(stage, input);
		break;
	case SURFSPRITE_EFFECT:
	case SURFSPRITE_WEATHERFX:
		RB_DrawEffectSurfaceSprites(stage, input);
		break;
	}

	SQuickSprite.EndGroup();

	sssurfaces++;
}





/*
=============================================================

SURFACE SHADERS

=============================================================
*/

shaderCommands_t	tess;
static qboolean	setArraysOnce;

/*
=================
R_BindAnimatedImage

=================
*/
// de-static'd because tr_quicksprite wants it
void R_BindAnimatedImage( textureBundle_t *bundle ) {
	int		index;

	if ( bundle->isVideoMap ) {
		ri.CIN_RunCinematic(bundle->videoMapHandle);
		ri.CIN_UploadCinematic(bundle->videoMapHandle);
		return;
	}

	if ((r_fullbright->value /*|| tr.refdef.doFullbright */) && bundle->isLightmap)
	{
		GL_Bind( tr.whiteImage );
		return;
	}

	if ( bundle->numImageAnimations <= 1 ) {
		GL_Bind( bundle->image[0] );
		return;
	}

	// it is necessary to do this messy calc to make sure animations line up
	// exactly with waveforms of the same frequency
	index = myftol( tess.shaderTime * bundle->imageAnimationSpeed * FUNCTABLE_SIZE );
	index >>= FUNCTABLE_SIZE2;

	if ( index < 0 ) {
		index = 0;	// may happen with shader time offsets
	}
	if ( bundle->oneShotAnimMap )
	{
		if ( index >= bundle->numImageAnimations )
		{
			// stick on last frame
			index = bundle->numImageAnimations - 1;
		}
	}
	else
	{
		// loop
		index %= bundle->numImageAnimations;
	}

	GL_Bind( bundle->image[ index ] );
}

/*
================
DrawTris

Draws triangle outlines for debugging
================
*/
static void DrawTris (shaderCommands_t *input) {
	GL_Bind( tr.whiteImage );
	qglColor3f (1,1,1);

	GL_State( GLS_POLYMODE_LINE | GLS_DEPTHMASK_TRUE );
	qglDepthRange( 0, 0 );

	qglDisableClientState (GL_COLOR_ARRAY);
	qglDisableClientState (GL_TEXTURE_COORD_ARRAY);

	qglVertexPointer (3, GL_FLOAT, 16, input->xyz);	// padded for SIMD

	if (qglLockArraysEXT) {
		qglLockArraysEXT(0, input->numVertexes);
		GLimp_LogComment( "glLockArraysEXT\n" );
	}

	R_DrawElements( input->numIndexes, input->indexes );

	if (qglUnlockArraysEXT) {
		qglUnlockArraysEXT();
		GLimp_LogComment( "glUnlockArraysEXT\n" );
	}
	qglDepthRange( 0, 1 );
}


/*
================
DrawNormals

Draws vertex normals for debugging
================
*/
static void DrawNormals (shaderCommands_t *input) {
	int		i;
	vec3_t	temp;

	GL_Bind( tr.whiteImage );
	qglColor3f (1,1,1);
	qglDepthRange( 0, 0 );	// never occluded
	GL_State( GLS_POLYMODE_LINE | GLS_DEPTHMASK_TRUE );

	qglBegin (GL_LINES);
	for (i = 0 ; i < input->numVertexes ; i++) {
		qglVertex3fv (input->xyz[i]);
		VectorMA (input->xyz[i], 2, input->normal[i], temp);
		qglVertex3fv (temp);
	}
	qglEnd ();

	qglDepthRange( 0, 1 );
}

/*
==============
RB_BeginSurface

We must set some things up before beginning any tesselation,
because a surface may be forced to perform a RB_End due
to overflow.
==============
*/
void RB_BeginSurface( shader_t *shader, int fogNum ) {

	shader_t *state = (shader->remappedShader) ? shader->remappedShader : shader;

	tess.numIndexes = 0;
	tess.numVertexes = 0;
	tess.shader = state;
	tess.fogNum = fogNum;
	tess.dlightBits = 0;		// will be OR'd in by surface functions
	tess.xstages = state->stages;
	tess.numPasses = state->numUnfoggedPasses;
	tess.currentStageIteratorFunc = state->optimalStageIteratorFunc;

	tess.shaderTime = backEnd.refdef.floatTime - tess.shader->timeOffset;
	if (tess.shader->clampTime && tess.shaderTime >= tess.shader->clampTime) {
		tess.shaderTime = tess.shader->clampTime;
	}


}

/*
===================
DrawMultitextured

output = t0 * t1 or t0 + t1

t0 = most upstream according to spec
t1 = most downstream according to spec
===================
*/
static void DrawMultitextured( shaderCommands_t *input, int stage ) {
	shaderStage_t	*pStage;

	pStage = tess.xstages[stage];

	GL_State( pStage->stateBits );

	// this is an ugly hack to work around a GeForce driver
	// bug with multitexture and clip planes
	if ( backEnd.viewParms.isPortal ) {
		qglPolygonMode( GL_FRONT_AND_BACK, GL_FILL );
	}

	//
	// base
	//
	GL_SelectTexture( 0 );
	qglTexCoordPointer( 2, GL_FLOAT, 0, input->svars.texcoords[0] );
	R_BindAnimatedImage( &pStage->bundle[0] );

	//
	// lightmap/secondary pass
	//
	GL_SelectTexture( 1 );
	qglEnable( GL_TEXTURE_2D );
	qglEnableClientState( GL_TEXTURE_COORD_ARRAY );

	if ( r_lightmap->integer ) {
		GL_TexEnv( GL_REPLACE );
	} else {
		GL_TexEnv( tess.shader->multitextureEnv );
	}

	qglTexCoordPointer( 2, GL_FLOAT, 0, input->svars.texcoords[1] );

	R_BindAnimatedImage( &pStage->bundle[1] );

	R_DrawElements( input->numIndexes, input->indexes );

	//
	// disable texturing on TEXTURE1, then select TEXTURE0
	//
	//qglDisableClientState( GL_TEXTURE_COORD_ARRAY );
	qglDisable( GL_TEXTURE_2D );

	GL_SelectTexture( 0 );
}



/*
===================
ProjectDlightTexture

Perform dynamic lighting with another rendering pass
===================
*/
static void ProjectDlightTexture( void ) {
	int		i, l;
	vec3_t	origin;
	float	*texCoords;
	byte	*colors;
	byte	clipBits[SHADER_MAX_VERTEXES];
	MAC_STATIC float	texCoordsArray[SHADER_MAX_VERTEXES][2];
	byte	colorArray[SHADER_MAX_VERTEXES][4];
	unsigned	hitIndexes[SHADER_MAX_INDEXES];
	int		numIndexes;
	float	scale;
	float	radius;
	vec3_t	floatColor;

	if ( !backEnd.refdef.num_dlights ) {
		return;
	}

	for ( l = 0 ; l < backEnd.refdef.num_dlights ; l++ ) {
		dlight_t	*dl;

		if ( !( tess.dlightBits & ( 1 << l ) ) ) {
			continue;	// this surface definately doesn't have any of this light
		}
		texCoords = texCoordsArray[0];
		colors = colorArray[0];

		dl = &backEnd.refdef.dlights[l];
		VectorCopy( dl->transformed, origin );
		radius = dl->radius;
		scale = 1.0f / radius;
		floatColor[0] = dl->color[0] * 255.0f;
		floatColor[1] = dl->color[1] * 255.0f;
		floatColor[2] = dl->color[2] * 255.0f;

		for ( i = 0 ; i < tess.numVertexes ; i++, texCoords += 2, colors += 4 ) {
			vec3_t	dist;
			int		clip;
			float	modulate;

			backEnd.pc.c_dlightVertexes++;

			VectorSubtract( origin, tess.xyz[i], dist );
			texCoords[0] = 0.5f + dist[0] * scale;
			texCoords[1] = 0.5f + dist[1] * scale;

			clip = 0;
			if ( texCoords[0] < 0.0f ) {
				clip |= 1;
			} else if ( texCoords[0] > 1.0f ) {
				clip |= 2;
			}
			if ( texCoords[1] < 0.0f ) {
				clip |= 4;
			} else if ( texCoords[1] > 1.0f ) {
				clip |= 8;
			}
			// modulate the strength based on the height and color
			if ( dist[2] > radius ) {
				clip |= 16;
				modulate = 0.0f;
			} else if ( dist[2] < -radius ) {
				clip |= 32;
				modulate = 0.0f;
			} else {
				dist[2] = Q_fabs(dist[2]);
				if ( dist[2] < radius * 0.5f ) {
					modulate = 1.0f;
				} else {
					modulate = 2.0f * (radius - dist[2]) * scale;
				}
			}
			clipBits[i] = clip;

			colors[0] = myftol(floatColor[0] * modulate);
			colors[1] = myftol(floatColor[1] * modulate);
			colors[2] = myftol(floatColor[2] * modulate);
			colors[3] = 255;
		}

		// build a list of triangles that need light
		numIndexes = 0;
		for ( i = 0 ; i < tess.numIndexes ; i += 3 ) {
			int		a, b, c;

			a = tess.indexes[i];
			b = tess.indexes[i+1];
			c = tess.indexes[i+2];
			if ( clipBits[a] & clipBits[b] & clipBits[c] ) {
				continue;	// not lighted
			}
			hitIndexes[numIndexes] = a;
			hitIndexes[numIndexes+1] = b;
			hitIndexes[numIndexes+2] = c;
			numIndexes += 3;
		}

		if ( !numIndexes ) {
			continue;
		}

		qglEnableClientState( GL_TEXTURE_COORD_ARRAY );
		qglTexCoordPointer( 2, GL_FLOAT, 0, texCoordsArray[0] );

		qglEnableClientState( GL_COLOR_ARRAY );
		qglColorPointer( 4, GL_UNSIGNED_BYTE, 0, colorArray );

		GL_Bind( tr.dlightImage );
		// include GLS_DEPTHFUNC_EQUAL so alpha tested surfaces don't add light
		// where they aren't rendered
		if ( dl->additive ) {
			GL_State( GLS_SRCBLEND_ONE | GLS_DSTBLEND_ONE | GLS_DEPTHFUNC_EQUAL );
		}
		else {
			GL_State( GLS_SRCBLEND_DST_COLOR | GLS_DSTBLEND_ONE | GLS_DEPTHFUNC_EQUAL );
		}
		R_DrawElements( numIndexes, hitIndexes );
		backEnd.pc.c_totalIndexes += numIndexes;
		backEnd.pc.c_dlightIndexes += numIndexes;
	}
}


/*
===================
RB_FogPass

Blends a fog texture on top of everything else
===================
*/
static void RB_FogPass( void ) {
	fog_t		*fog;
	int			i;

	qglEnableClientState( GL_COLOR_ARRAY );
	qglColorPointer( 4, GL_UNSIGNED_BYTE, 0, tess.svars.colors );

	qglEnableClientState( GL_TEXTURE_COORD_ARRAY);
	qglTexCoordPointer( 2, GL_FLOAT, 0, tess.svars.texcoords[0] );

	fog = tr.world->fogs + tess.fogNum;

	for ( i = 0; i < tess.numVertexes; i++ ) {
		* ( int * )&tess.svars.colors[i] = fog->colorInt;
	}

	RB_CalcFogTexCoords( ( float * ) tess.svars.texcoords[0] );

	GL_Bind( tr.fogImage );

	if ( tess.shader->fogPass == FP_EQUAL ) {
		GL_State( GLS_SRCBLEND_SRC_ALPHA | GLS_DSTBLEND_ONE_MINUS_SRC_ALPHA | GLS_DEPTHFUNC_EQUAL );
	} else {
		GL_State( GLS_SRCBLEND_SRC_ALPHA | GLS_DSTBLEND_ONE_MINUS_SRC_ALPHA );
	}

	R_DrawElements( tess.numIndexes, tess.indexes );
}

/*
===============
ComputeColors
===============
*/
static void ComputeColors( shaderStage_t *pStage, int forceRGBGen )
{
	int			i;
	color4ub_t	*colors = tess.svars.colors;
	qboolean killGen = qfalse;

	if ( tess.shader != tr.projectionShadowShader && tess.shader != tr.shadowShader && 
			( backEnd.currentEntity->e.renderfx & (RF_DISINTEGRATE1|RF_DISINTEGRATE2)))
	{
		RB_CalcDisintegrateColors( (unsigned char *)tess.svars.colors );
		RB_CalcDisintegrateVertDeform();

		// We've done some custom alpha and color stuff, so we can skip the rest.  Let it do fog though
		killGen = qtrue;
	}

	//
	// rgbGen
	//
	if ( !forceRGBGen )
	{
		forceRGBGen = pStage->rgbGen;
	}

	if ( backEnd.currentEntity->e.renderfx & RF_VOLUMETRIC ) // does not work for rotated models, technically, this should also be a CGEN type, but that would entail adding new shader commands....which is too much work for one thing
	{
		int			i;
		float		*normal, dot;
		unsigned char *color;
		int			numVertexes;

		normal = tess.normal[0];
		color = tess.svars.colors[0];

		numVertexes = tess.numVertexes;

		for ( i = 0 ; i < numVertexes ; i++, normal += 4, color += 4) 
		{
			dot = DotProduct( normal, backEnd.refdef.viewaxis[0] );

			dot *= dot * dot * dot;

			if ( dot < 0.2f ) // so low, so just clamp it
			{
				dot = 0.0f;
			}

			color[0] = color[1] = color[2] = color[3] = myftol( backEnd.currentEntity->e.shaderRGBA[0] * (1-dot) );

		}

		killGen = qtrue;
	}

	if (killGen)
	{
		goto avoidGen;
	}

	//
	// rgbGen
	//
	switch ( forceRGBGen )
	{
		case CGEN_IDENTITY:
			Com_Memset( tess.svars.colors, 0xff, tess.numVertexes * 4 );
			break;
		default:
		case CGEN_IDENTITY_LIGHTING:
			Com_Memset( tess.svars.colors, tr.identityLightByte, tess.numVertexes * 4 );
			break;
		case CGEN_LIGHTING_DIFFUSE:
			RB_CalcDiffuseColor( ( unsigned char * ) tess.svars.colors );
			break;
		case CGEN_EXACT_VERTEX:
			Com_Memcpy( tess.svars.colors, tess.vertexColors, tess.numVertexes * sizeof( tess.vertexColors[0] ) );
			break;
		case CGEN_CONST:
			for ( i = 0; i < tess.numVertexes; i++ ) {
				*(int *)tess.svars.colors[i] = *(int *)pStage->constantColor;
			}
			break;
		case CGEN_VERTEX:
			if ( tr.identityLight == 1 )
			{
				Com_Memcpy( tess.svars.colors, tess.vertexColors, tess.numVertexes * sizeof( tess.vertexColors[0] ) );
			}
			else
			{
				for ( i = 0; i < tess.numVertexes; i++ )
				{
					tess.svars.colors[i][0] = tess.vertexColors[i][0] * tr.identityLight;
					tess.svars.colors[i][1] = tess.vertexColors[i][1] * tr.identityLight;
					tess.svars.colors[i][2] = tess.vertexColors[i][2] * tr.identityLight;
					tess.svars.colors[i][3] = tess.vertexColors[i][3];
				}
			}
			break;
		case CGEN_ONE_MINUS_VERTEX:
			if ( tr.identityLight == 1 )
			{
				for ( i = 0; i < tess.numVertexes; i++ )
				{
					tess.svars.colors[i][0] = 255 - tess.vertexColors[i][0];
					tess.svars.colors[i][1] = 255 - tess.vertexColors[i][1];
					tess.svars.colors[i][2] = 255 - tess.vertexColors[i][2];
				}
			}
			else
			{
				for ( i = 0; i < tess.numVertexes; i++ )
				{
					tess.svars.colors[i][0] = ( 255 - tess.vertexColors[i][0] ) * tr.identityLight;
					tess.svars.colors[i][1] = ( 255 - tess.vertexColors[i][1] ) * tr.identityLight;
					tess.svars.colors[i][2] = ( 255 - tess.vertexColors[i][2] ) * tr.identityLight;
				}
			}
			break;
		case CGEN_FOG:
			{
				fog_t		*fog;

				fog = tr.world->fogs + tess.fogNum;

				for ( i = 0; i < tess.numVertexes; i++ ) {
					* ( int * )&tess.svars.colors[i] = fog->colorInt;
				}
			}
			break;
		case CGEN_WAVEFORM:
			RB_CalcWaveColor( &pStage->rgbWave, ( unsigned char * ) tess.svars.colors );
			break;
		case CGEN_ENTITY:
			RB_CalcColorFromEntity( ( unsigned char * ) tess.svars.colors );
			break;
		case CGEN_ONE_MINUS_ENTITY:
			RB_CalcColorFromOneMinusEntity( ( unsigned char * ) tess.svars.colors );
			break;
		case CGEN_LIGHTMAP0:
			memset( colors, 0xff, tess.numVertexes * 4 );
			break;
		case CGEN_LIGHTMAP1:
			for ( i = 0; i < tess.numVertexes; i++ ) 
			{
				*(unsigned *)&colors[i] = *(unsigned *)styleColors[pStage->lightmapStyle];
			}
			break;
		case CGEN_LIGHTMAP2:
			for ( i = 0; i < tess.numVertexes; i++ ) 
			{
				*(unsigned *)&colors[i] = *(unsigned *)styleColors[pStage->lightmapStyle];
			}
			break;
		case CGEN_LIGHTMAP3:
			for ( i = 0; i < tess.numVertexes; i++ ) 
			{
				*(unsigned *)&colors[i] = *(unsigned *)styleColors[pStage->lightmapStyle];
			}
			break;

	}

	//
	// alphaGen
	//
	switch ( pStage->alphaGen )
	{
	case AGEN_SKIP:
		break;
	case AGEN_IDENTITY:
		if ( forceRGBGen != CGEN_IDENTITY ) {
			if ( ( forceRGBGen == CGEN_VERTEX && tr.identityLight != 1 ) ||
				 forceRGBGen != CGEN_VERTEX ) {
				for ( i = 0; i < tess.numVertexes; i++ ) {
					tess.svars.colors[i][3] = 0xff;
				}
			}
		}
		break;
	case AGEN_CONST:
		if ( forceRGBGen != CGEN_CONST ) {
			for ( i = 0; i < tess.numVertexes; i++ ) {
				tess.svars.colors[i][3] = pStage->constantColor[3];
			}
		}
		break;
	case AGEN_WAVEFORM:
		RB_CalcWaveAlpha( &pStage->alphaWave, ( unsigned char * ) tess.svars.colors );
		break;
	case AGEN_LIGHTING_SPECULAR:
		RB_CalcSpecularAlpha( ( unsigned char * ) tess.svars.colors );
		break;
	case AGEN_ENTITY:
		RB_CalcAlphaFromEntity( ( unsigned char * ) tess.svars.colors );
		break;
	case AGEN_ONE_MINUS_ENTITY:
		RB_CalcAlphaFromOneMinusEntity( ( unsigned char * ) tess.svars.colors );
		break;
    case AGEN_VERTEX:
		if ( forceRGBGen != CGEN_VERTEX ) {
			for ( i = 0; i < tess.numVertexes; i++ ) {
				tess.svars.colors[i][3] = tess.vertexColors[i][3];
			}
		}
        break;
    case AGEN_ONE_MINUS_VERTEX:
        for ( i = 0; i < tess.numVertexes; i++ )
        {
			tess.svars.colors[i][3] = 255 - tess.vertexColors[i][3];
        }
        break;
	case AGEN_PORTAL:
		{
			unsigned char alpha;

			for ( i = 0; i < tess.numVertexes; i++ )
			{
				float len;
				vec3_t v;

				VectorSubtract( tess.xyz[i], backEnd.viewParms.or.origin, v );
				len = VectorLength( v );

				len /= tess.shader->portalRange;

				if ( len < 0 )
				{
					alpha = 0;
				}
				else if ( len > 1 )
				{
					alpha = 0xff;
				}
				else
				{
					alpha = len * 0xff;
				}

				tess.svars.colors[i][3] = alpha;
			}
		}
		break;
	case AGEN_BLEND:
		if ( forceRGBGen != CGEN_VERTEX ) 
		{
			for ( i = 0; i < tess.numVertexes; i++ ) 
			{
				//colors[i][3] = tess.vertexAlphas[i][pStage->index];	// only used on SOF2, needs implementing if you want it
			}
		}
		break;
	}
avoidGen:
	//
	// fog adjustment for colors to fade out as fog increases
	//
	if ( tess.fogNum )
	{
		switch ( pStage->adjustColorsForFog )
		{
		case ACFF_MODULATE_RGB:
			RB_CalcModulateColorsByFog( ( unsigned char * ) tess.svars.colors );
			break;
		case ACFF_MODULATE_ALPHA:
			RB_CalcModulateAlphasByFog( ( unsigned char * ) tess.svars.colors );
			break;
		case ACFF_MODULATE_RGBA:
			RB_CalcModulateRGBAsByFog( ( unsigned char * ) tess.svars.colors );
			break;
		case ACFF_NONE:
			break;
		}
	}
}

/*
===============
ComputeTexCoords
===============
*/
static void ComputeTexCoords( shaderStage_t *pStage ) {
	int		i;
	int		b;
    float	*texcoords;

	for ( b = 0; b < NUM_TEXTURE_BUNDLES; b++ ) {
		int tm;

        texcoords = (float *)tess.svars.texcoords[b];
		//
		// generate the texture coordinates
		//
		switch ( pStage->bundle[b].tcGen )
		{
		case TCGEN_IDENTITY:
			Com_Memset( tess.svars.texcoords[b], 0, sizeof( float ) * 2 * tess.numVertexes );
			break;
		case TCGEN_TEXTURE:
			for ( i = 0 ; i < tess.numVertexes ; i++ ) {
				tess.svars.texcoords[b][i][0] = tess.texCoords[i][0][0];
				tess.svars.texcoords[b][i][1] = tess.texCoords[i][0][1];
			}
			break;
		case TCGEN_LIGHTMAP:
			for ( i = 0 ; i < tess.numVertexes ; i++,texcoords+=2 ) {
				texcoords[0] = tess.texCoords[i][1][0];
				texcoords[1] = tess.texCoords[i][1][1];
			}
			break;
		case TCGEN_LIGHTMAP1:
			for ( i = 0 ; i < tess.numVertexes ; i++,texcoords+=2 ) {
				texcoords[0] = tess.texCoords[i][2][0];
				texcoords[1] = tess.texCoords[i][2][1];
			}
			break;
		case TCGEN_LIGHTMAP2:
			for ( i = 0 ; i < tess.numVertexes ; i++,texcoords+=2 ) {
				texcoords[0] = tess.texCoords[i][3][0];
				texcoords[1] = tess.texCoords[i][3][1];
			}
			break;
		case TCGEN_LIGHTMAP3:
			for ( i = 0 ; i < tess.numVertexes ; i++,texcoords+=2 ) {
				texcoords[0] = tess.texCoords[i][4][0];
				texcoords[1] = tess.texCoords[i][4][1];
			}
			break;
		case TCGEN_VECTOR:
			for ( i = 0 ; i < tess.numVertexes ; i++ ) {
				tess.svars.texcoords[b][i][0] = DotProduct( tess.xyz[i], pStage->bundle[b].tcGenVectors[0] );
				tess.svars.texcoords[b][i][1] = DotProduct( tess.xyz[i], pStage->bundle[b].tcGenVectors[1] );
			}
			break;
		case TCGEN_FOG:
			RB_CalcFogTexCoords( ( float * ) tess.svars.texcoords[b] );
			break;
		case TCGEN_ENVIRONMENT_MAPPED:
			RB_CalcEnvironmentTexCoords( ( float * ) tess.svars.texcoords[b] );
			break;
		case TCGEN_BAD:
			return;
		}

		//
		// alter texture coordinates
		//
		for ( tm = 0; tm < pStage->bundle[b].numTexMods ; tm++ ) {
			switch ( pStage->bundle[b].texMods[tm].type )
			{
			case TMOD_NONE:
				tm = TR_MAX_TEXMODS;		// break out of for loop
				break;

			case TMOD_TURBULENT:
				RB_CalcTurbulentTexCoords( &pStage->bundle[b].texMods[tm].wave, 
						                 ( float * ) tess.svars.texcoords[b] );
				break;

			case TMOD_ENTITY_TRANSLATE:
				RB_CalcScrollTexCoords( backEnd.currentEntity->e.shaderTexCoord,
									 ( float * ) tess.svars.texcoords[b] );
				break;

			case TMOD_SCROLL:
				RB_CalcScrollTexCoords( pStage->bundle[b].texMods[tm].scroll,
										 ( float * ) tess.svars.texcoords[b] );
				break;

			case TMOD_SCALE:
				RB_CalcScaleTexCoords( pStage->bundle[b].texMods[tm].scale,
									 ( float * ) tess.svars.texcoords[b] );
				break;
			
			case TMOD_STRETCH:
				RB_CalcStretchTexCoords( &pStage->bundle[b].texMods[tm].wave, 
						               ( float * ) tess.svars.texcoords[b] );
				break;

			case TMOD_TRANSFORM:
				RB_CalcTransformTexCoords( &pStage->bundle[b].texMods[tm],
						                 ( float * ) tess.svars.texcoords[b] );
				break;

			case TMOD_ROTATE:
				RB_CalcRotateTexCoords( pStage->bundle[b].texMods[tm].rotateSpeed,
										( float * ) tess.svars.texcoords[b] );
				break;

			default:
				ri.Error( ERR_DROP, "ERROR: unknown texmod '%d' in shader '%s'\n", pStage->bundle[b].texMods[tm].type, tess.shader->name );
				break;
			}
		}
	}
}

void ForceAlpha(unsigned char *dstColors, int TR_ForceEntAlpha)
{
	int	i;

	dstColors += 3;

	for ( i = 0; i < tess.numVertexes; i++, dstColors += 4 )
	{
		*dstColors = TR_ForceEntAlpha;
	}
}

/*
** RB_IterateStagesGeneric
*/
static void RB_IterateStagesGeneric( shaderCommands_t *input )
{
	int stage;

	for ( stage = 0; stage < MAX_SHADER_STAGES; stage++ )
	{
		shaderStage_t *pStage = tess.xstages[stage];
		int forceRGBGen = 0;
		int stateBits = 0;

		if ( !pStage )
		{
			break;
		}

		if ( stage && r_lightmap->integer && !( pStage->bundle[0].isLightmap || pStage->bundle[1].isLightmap || pStage->bundle[0].vertexLightmap ) )
		{
			break;
		}

		stateBits = pStage->stateBits;

		if ( backEnd.currentEntity )
		{
			if ( backEnd.currentEntity->e.renderfx & RF_DISINTEGRATE1 )
			{
				// we want to be able to rip a hole in the thing being disintegrated, and by doing the depth-testing it avoids some kinds of artefacts, but will probably introduce others?
				stateBits = GLS_SRCBLEND_SRC_ALPHA | GLS_DSTBLEND_ONE_MINUS_SRC_ALPHA | GLS_DEPTHMASK_TRUE | GLS_DEPTHTEST_DISABLE;
			}

			if ( backEnd.currentEntity->e.renderfx & RF_RGB_TINT )
			{//want to use RGBGen from ent
				forceRGBGen = CGEN_ENTITY;
			}
		}

		if (pStage->ss.surfaceSpriteType)
		{
			// We check for surfacesprites AFTER drawing everything else
			continue;
		}

		ComputeColors( pStage, forceRGBGen );
		ComputeTexCoords( pStage );

		if ( !setArraysOnce )
		{
			qglEnableClientState( GL_COLOR_ARRAY );
			qglColorPointer( 4, GL_UNSIGNED_BYTE, 0, input->svars.colors );
		}

		//
		// do multitexture
		//
		if ( pStage->bundle[1].image[0] != 0 )
		{
			DrawMultitextured( input, stage );
		}
		else
		{
			if ( !setArraysOnce )
			{
				qglTexCoordPointer( 2, GL_FLOAT, 0, input->svars.texcoords[0] );
			}

			//
			// set state
			//
			if ( pStage->bundle[0].vertexLightmap && ( r_vertexLight->integer && !r_uiFullScreen->integer ) && r_lightmap->integer )
			{
				GL_Bind( tr.whiteImage );
			}
			else 
				R_BindAnimatedImage( &pStage->bundle[0] );

			if (backEnd.currentEntity && (backEnd.currentEntity->e.renderfx & RF_FORCE_ENT_ALPHA))
			{
				ForceAlpha((unsigned char *) tess.svars.colors, backEnd.currentEntity->e.shaderRGBA[3]);
				GL_State(GLS_SRCBLEND_SRC_ALPHA | GLS_DSTBLEND_ONE_MINUS_SRC_ALPHA);
			}
			else
			{
				GL_State( stateBits );
			}

			//
			// draw
			//
			R_DrawElements( input->numIndexes, input->indexes );
		}
	}
}


/*
** RB_StageIteratorGeneric
*/
void RB_StageIteratorGeneric( void )
{
	shaderCommands_t *input;
	int stage;

	input = &tess;

	RB_DeformTessGeometry();

	//
	// log this call
	//
	if ( r_logFile->integer ) 
	{
		// don't just call LogComment, or we will get
		// a call to va() every frame!
		GLimp_LogComment( va("--- RB_StageIteratorGeneric( %s ) ---\n", tess.shader->name) );
	}

	//
	// set face culling appropriately
	//
	GL_Cull( input->shader->cullType );

	// set polygon offset if necessary
	if ( input->shader->polygonOffset )
	{
		qglEnable( GL_POLYGON_OFFSET_FILL );
		qglPolygonOffset( r_offsetFactor->value, r_offsetUnits->value );
	}

	//
	// if there is only a single pass then we can enable color
	// and texture arrays before we compile, otherwise we need
	// to avoid compiling those arrays since they will change
	// during multipass rendering
	//
	if ( tess.numPasses > 1 || input->shader->multitextureEnv )
	{
		setArraysOnce = qfalse;
		qglDisableClientState (GL_COLOR_ARRAY);
		qglDisableClientState (GL_TEXTURE_COORD_ARRAY);
	}
	else
	{
		setArraysOnce = qtrue;

		qglEnableClientState( GL_COLOR_ARRAY);
		qglColorPointer( 4, GL_UNSIGNED_BYTE, 0, tess.svars.colors );

		qglEnableClientState( GL_TEXTURE_COORD_ARRAY);
		qglTexCoordPointer( 2, GL_FLOAT, 0, tess.svars.texcoords[0] );
	}

	//
	// lock XYZ
	//
	qglVertexPointer (3, GL_FLOAT, 16, input->xyz);	// padded for SIMD
	if (qglLockArraysEXT)
	{
		qglLockArraysEXT(0, input->numVertexes);
		GLimp_LogComment( "glLockArraysEXT\n" );
	}

	//
	// enable color and texcoord arrays after the lock if necessary
	//
	if ( !setArraysOnce )
	{
		qglEnableClientState( GL_TEXTURE_COORD_ARRAY );
		qglEnableClientState( GL_COLOR_ARRAY );
	}

	//
	// call shader function
	//
	RB_IterateStagesGeneric( input );

	// 
	// now do any dynamic lighting needed
	//
	if ( tess.dlightBits && tess.shader->sort <= SS_OPAQUE
		&& !(tess.shader->surfaceFlags & (SURF_NODLIGHT | SURF_SKY) ) ) {
		ProjectDlightTexture();
	}

	//
	// now do fog
	//
	if ( tess.fogNum && tess.shader->fogPass ) {
		RB_FogPass();
	}

	// 
	// unlock arrays
	//
	if (qglUnlockArraysEXT) 
	{
		qglUnlockArraysEXT();
		GLimp_LogComment( "glUnlockArraysEXT\n" );
	}

	//
	// reset polygon offset
	//
	if ( input->shader->polygonOffset )
	{
		qglDisable( GL_POLYGON_OFFSET_FILL );
	}

	// Now check for surfacesprites.
	if (r_surfaceSprites->integer)
	{
		for ( stage = 1; stage < MAX_SHADER_STAGES; stage++ )
		{
			if (!tess.xstages[stage])
			{
				break;
			}
			if (tess.xstages[stage]->ss.surfaceSpriteType)
			{	// Draw the surfacesprite
				RB_DrawSurfaceSprites(tess.xstages[stage], input);
			}
		}
	}
}


/*
** RB_StageIteratorVertexLitTexture
*/
void RB_StageIteratorVertexLitTexture( void )
{
	shaderCommands_t *input;
	shader_t		*shader;
	int stage;

	input = &tess;

	shader = input->shader;

	//
	// compute colors
	//
	RB_CalcDiffuseColor( ( unsigned char * ) tess.svars.colors );

	//
	// log this call
	//
	if ( r_logFile->integer ) 
	{
		// don't just call LogComment, or we will get
		// a call to va() every frame!
		GLimp_LogComment( va("--- RB_StageIteratorVertexLitTexturedUnfogged( %s ) ---\n", tess.shader->name) );
	}

	//
	// set face culling appropriately
	//
	GL_Cull( input->shader->cullType );

	//
	// set arrays and lock
	//
	qglEnableClientState( GL_COLOR_ARRAY);
	qglEnableClientState( GL_TEXTURE_COORD_ARRAY);

	qglColorPointer( 4, GL_UNSIGNED_BYTE, 0, tess.svars.colors );
	qglTexCoordPointer( 2, GL_FLOAT, 16, tess.texCoords[0][0] );
	qglVertexPointer (3, GL_FLOAT, 16, input->xyz);

	if ( qglLockArraysEXT )
	{
		qglLockArraysEXT(0, input->numVertexes);
		GLimp_LogComment( "glLockArraysEXT\n" );
	}

	//
	// call special shade routine
	//
	R_BindAnimatedImage( &tess.xstages[0]->bundle[0] );
	GL_State( tess.xstages[0]->stateBits );
	R_DrawElements( input->numIndexes, input->indexes );

	// 
	// now do any dynamic lighting needed
	//
	if ( tess.dlightBits && tess.shader->sort <= SS_OPAQUE ) {
		ProjectDlightTexture();
	}

	//
	// now do fog
	//
	if ( tess.fogNum && tess.shader->fogPass ) {
		RB_FogPass();
	}

	// 
	// unlock arrays
	//
	if (qglUnlockArraysEXT) 
	{
		qglUnlockArraysEXT();
		GLimp_LogComment( "glUnlockArraysEXT\n" );
	}

	// Now check for surfacesprites.
	if (r_surfaceSprites->integer)
	{
		for ( stage = 1; stage < MAX_SHADER_STAGES; stage++ )
		{
			if (!tess.xstages[stage])
			{
				break;
			}
			if (tess.xstages[stage]->ss.surfaceSpriteType)
			{	// Draw the surfacesprite
				RB_DrawSurfaceSprites(tess.xstages[stage], input);
			}
		}
	}
}

//define	REPLACE_MODE

void RB_StageIteratorLightmappedMultitexture( void ) {
	shaderCommands_t *input;
	int stage;

	input = &tess;

	//
	// log this call
	//
	if ( r_logFile->integer ) {
		// don't just call LogComment, or we will get
		// a call to va() every frame!
		GLimp_LogComment( va("--- RB_StageIteratorLightmappedMultitexture( %s ) ---\n", tess.shader->name) );
	}

	//
	// set face culling appropriately
	//
	GL_Cull( input->shader->cullType );

	//
	// set color, pointers, and lock
	//
	GL_State( GLS_DEFAULT );
	qglVertexPointer( 3, GL_FLOAT, 16, input->xyz );

#ifdef REPLACE_MODE
	qglDisableClientState( GL_COLOR_ARRAY );
	qglColor3f( 1, 1, 1 );
	qglShadeModel( GL_FLAT );
#else
	qglEnableClientState( GL_COLOR_ARRAY );
	qglColorPointer( 4, GL_UNSIGNED_BYTE, 0, tess.constantColor255 );
#endif

	//
	// select base stage
	//
	GL_SelectTexture( 0 );

	qglEnableClientState( GL_TEXTURE_COORD_ARRAY );
	R_BindAnimatedImage( &tess.xstages[0]->bundle[0] );
	qglTexCoordPointer( 2, GL_FLOAT, 16, tess.texCoords[0][0] );

	//
	// configure second stage
	//
	GL_SelectTexture( 1 );
	qglEnable( GL_TEXTURE_2D );
	if ( r_lightmap->integer ) {
		GL_TexEnv( GL_REPLACE );
	} else {
		GL_TexEnv( GL_MODULATE );
	}
	R_BindAnimatedImage( &tess.xstages[0]->bundle[1] );
	qglEnableClientState( GL_TEXTURE_COORD_ARRAY );
	qglTexCoordPointer( 2, GL_FLOAT, 16, tess.texCoords[0][1] );

	//
	// lock arrays
	//
	if ( qglLockArraysEXT ) {
		qglLockArraysEXT(0, input->numVertexes);
		GLimp_LogComment( "glLockArraysEXT\n" );
	}

	R_DrawElements( input->numIndexes, input->indexes );

	//
	// disable texturing on TEXTURE1, then select TEXTURE0
	//
	qglDisable( GL_TEXTURE_2D );
	qglDisableClientState( GL_TEXTURE_COORD_ARRAY );

	GL_SelectTexture( 0 );
#ifdef REPLACE_MODE
	GL_TexEnv( GL_MODULATE );
	qglShadeModel( GL_SMOOTH );
#endif

	// 
	// now do any dynamic lighting needed
	//
	if ( tess.dlightBits && tess.shader->sort <= SS_OPAQUE ) {
		ProjectDlightTexture();
	}

	//
	// now do fog
	//
	if ( tess.fogNum && tess.shader->fogPass ) {
		RB_FogPass();
	}

	//
	// unlock arrays
	//
	if ( qglUnlockArraysEXT ) {
		qglUnlockArraysEXT();
		GLimp_LogComment( "glUnlockArraysEXT\n" );
	}

	// Now check for surfacesprites.
	if (r_surfaceSprites->integer)
	{
		for ( stage = 1; stage < MAX_SHADER_STAGES; stage++ )
		{
			if (!tess.xstages[stage])
			{
				break;
			}
			if (tess.xstages[stage]->ss.surfaceSpriteType)
			{	// Draw the surfacesprite
				RB_DrawSurfaceSprites(tess.xstages[stage], input);
			}
		}
	}
}

/*
** RB_EndSurface
*/
void RB_EndSurface( void ) {
	shaderCommands_t *input;

	input = &tess;

	if (input->numIndexes == 0) {
		return;
	}

	if (input->indexes[SHADER_MAX_INDEXES-1] != 0) {
		ri.Error (ERR_DROP, "RB_EndSurface() - SHADER_MAX_INDEXES hit");
	}	
	if (input->xyz[SHADER_MAX_VERTEXES-1][0] != 0) {
		ri.Error (ERR_DROP, "RB_EndSurface() - SHADER_MAX_VERTEXES hit");
	}

	if ( tess.shader == tr.shadowShader ) {
		RB_ShadowTessEnd();
		return;
	}

	// for debugging of sort order issues, stop rendering after a given sort value
	if ( r_debugSort->integer && r_debugSort->integer < tess.shader->sort ) {
		return;
	}

	//
	// update performance counters
	//
	backEnd.pc.c_shaders++;
	backEnd.pc.c_vertexes += tess.numVertexes;
	backEnd.pc.c_indexes += tess.numIndexes;
	backEnd.pc.c_totalIndexes += tess.numIndexes * tess.numPasses;
	if (tess.fogNum && tess.shader->fogPass > FP_NONE && tess.shader->fogPass < FP_GLFOG)// && r_drawfog->value)
	{	
		backEnd.pc.c_totalIndexes += tess.numIndexes;
	}

	//
	// call off to shader specific tess end function
	//
	tess.currentStageIteratorFunc();

	//
	// draw debugging stuff
	//
	if ( r_showtris->integer && com_developer->integer ) {
		DrawTris (input);
	}
	if ( r_shownormals->integer && com_developer->integer ) {
		DrawNormals (input);
	}
	// clear shader so we can tell we don't have any unclosed surfaces
	tess.numIndexes = 0;

	GLimp_LogComment( "----------\n" );
}

