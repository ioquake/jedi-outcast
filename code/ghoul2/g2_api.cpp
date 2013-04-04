// leave this as first line for PCH reasons...
//
#include "../server/exe_headers.h"

#include <list>

#ifndef __Q_SHARED_H
	#include "../game/q_shared.h"
#endif

#if !defined(TR_LOCAL_H)
	#include "../renderer/tr_local.h"
#endif

#if !defined(G2_H_INC)
	#include "G2.h"
#endif

#if !defined(MINIHEAP_H_INC)
	#include "..\qcommon\MiniHeap.h"
#endif

extern mdxaBone_t		worldMatrix;
extern mdxaBone_t		worldMatrixInv;

// must be a power of two
#define MAX_G2_MODELS (512)
#define G2_MODEL_BITS (9)
#define G2_INDEX_MASK (MAX_G2_MODELS-1)



class Ghoul2InfoArray : public IGhoul2InfoArray
{
	vector<CGhoul2Info>	mInfos[MAX_G2_MODELS];
	int					mIds[MAX_G2_MODELS];
	list<int>			mFreeIndecies;
	void DeleteLow(int idx)
	{
		mInfos[idx].clear();
		if ((mIds[idx]>>G2_MODEL_BITS)>(1<<(31-G2_MODEL_BITS)))
		{
			mIds[idx]=MAX_G2_MODELS+idx; //rollover reset id to minimum value
			mFreeIndecies.push_back(idx);
		}
		else
		{
			mIds[idx]+=MAX_G2_MODELS;
			mFreeIndecies.push_front(idx);
		}
	}
public:
	Ghoul2InfoArray()
	{
		int i;
		for (i=0;i<MAX_G2_MODELS;i++)
		{
			mIds[i]=MAX_G2_MODELS+i;
			mFreeIndecies.push_back(i);
		}
	}
#if _DEBUG
	~Ghoul2InfoArray()
	{
		char mess[1000];
		if (mFreeIndecies.size()<MAX_G2_MODELS)
		{
			sprintf(mess,"************************\nLeaked %d ghoul2info slots\n", MAX_G2_MODELS - mFreeIndecies.size());
			OutputDebugString(mess);
			int i;
			for (i=0;i<MAX_G2_MODELS;i++)
			{
				list<int>::iterator j;
				for (j=mFreeIndecies.begin();j!=mFreeIndecies.end();j++)
				{
					if (*j==i)
						break;
				}
				if (j==mFreeIndecies.end())
				{
					sprintf(mess,"Leaked Info idx=%d id=%d sz=%d\n", i, mIds[i], mInfos[i].size());
					OutputDebugString(mess);
					if (mInfos[i].size())
					{
						sprintf(mess,"%s\n", mInfos[i][0].mFileName);
						OutputDebugString(mess);
					}
				}
			}
		}
		else
		{
			OutputDebugString("No ghoul2 info slots leaked\n");
		}
	}
#endif
	void VidRestart()
	{
		int j;
		for (j=0;j<MAX_G2_MODELS;j++)
		{
			vector<CGhoul2Info> &ghoul2=mInfos[j];
			int i;

			for (i = 0; i < ghoul2.size(); i++)
			{
				// if we have a name, re-register us so the model thinks it's in the correct place.
				if (ghoul2[i].mFileName[0])
				{
					ghoul2[i].mModel = RE_RegisterModel(ghoul2[i].mFileName); 
					assert(ghoul2[i].mModel);
				}
			}
		}
	}
	int New()
	{
		if (mFreeIndecies.empty())
		{
			assert(0);
			Com_Error(ERR_FATAL, "Out of ghoul2 info slots");

		}
		// gonna pull from the front, doing a 
		int idx=*mFreeIndecies.begin();
		mFreeIndecies.erase(mFreeIndecies.begin());
		return mIds[idx];
	}
	bool IsValid(int handle) const
	{
		if (!handle)
		{
			return false;
		}
		assert(handle>0); //negative handle???
		assert((handle&G2_INDEX_MASK)>=0&&(handle&G2_INDEX_MASK)<MAX_G2_MODELS); //junk handle
		if (mIds[handle&G2_INDEX_MASK]!=handle) // not a valid handle, could be old
		{
			return false;
		}
		return true;
	}
	void Delete(int handle)
	{
		if (!handle)
		{
			return;
		}
		assert(handle>0); //null handle
		assert((handle&G2_INDEX_MASK)>=0&&(handle&G2_INDEX_MASK)<MAX_G2_MODELS); //junk handle
		assert(mIds[handle&G2_INDEX_MASK]==handle); // not a valid handle, could be old or garbage
		if (mIds[handle&G2_INDEX_MASK]==handle)
		{
			DeleteLow(handle&G2_INDEX_MASK);
		}
	}
	vector<CGhoul2Info> &Get(int handle)
	{
		static vector<CGhoul2Info> null;
		assert(handle>0); //null handle
		assert((handle&G2_INDEX_MASK)>=0&&(handle&G2_INDEX_MASK)<MAX_G2_MODELS); //junk handle
		assert(mIds[handle&G2_INDEX_MASK]==handle); // not a valid handle, could be old or garbage
		if (handle<=0||(handle&G2_INDEX_MASK)<0||(handle&G2_INDEX_MASK)>=MAX_G2_MODELS||mIds[handle&G2_INDEX_MASK]!=handle)
		{
			null.clear();
			return null;
		}
		return mInfos[handle&G2_INDEX_MASK];
	}
	const vector<CGhoul2Info> &Get(int handle) const
	{
		assert(handle>0);
		assert(mIds[handle&G2_INDEX_MASK]==handle); // not a valid handle, could be old or garbage
		return mInfos[handle&G2_INDEX_MASK];
	}

};

IGhoul2InfoArray &TheGhoul2InfoArray()
{
	static Ghoul2InfoArray singleton;
	return singleton;
}


// this is the ONLY function to read entity states directly
void G2API_CleanGhoul2Models(CGhoul2Info_v &ghoul2)
{
	ghoul2.~CGhoul2Info_v();	
}

qhandle_t G2API_PrecacheGhoul2Model(const char *fileName)
{
	return RE_RegisterModel((char *)fileName);
}

// initialise all that needs to be on a new Ghoul II model
int G2API_InitGhoul2Model(CGhoul2Info_v &ghoul2, const char *fileName, int modelIndex, qhandle_t customSkin,
						  qhandle_t customShader, int modelFlags, int lodBias)
{
	int				model = -1;
	CGhoul2Info		newModel;


	if (!(fileName[0]))
	{
		assert(fileName[0]);
		return -1;
	}

	// find a free spot in the list
	for (model=0; model< ghoul2.size(); model++)
	{
		if (ghoul2[model].mModelindex == -1)
		{
			// this is only valid and used on the game side. Client side ignores this
			ghoul2[model].mModelindex = modelIndex;
				// on the game side this is valid. On the client side it is valid only after it has been filled in by trap_G2_SetGhoul2ModelIndexes 
			ghoul2[model].mModel = RE_RegisterModel((char *)fileName); 
		  	model_t		*mod_m = R_GetModelByHandle(ghoul2[model].mModel);
			if (mod_m->type == MOD_BAD)
			{
				return -1;
			}
			
			// init what is necessary for this ghoul2 model
			G2_Init_Bone_List(ghoul2[model].mBlist);
			G2_Init_Bolt_List(ghoul2[model].mBltlist);
			ghoul2[model].mCustomShader = customShader;
			ghoul2[model].mCustomSkin = customSkin;
			strcpy(ghoul2[model].mFileName, fileName);
			ghoul2[model].mCreationID = modelFlags;
			ghoul2[model].mLodBias = lodBias;
			ghoul2[model].mAnimFrameDefault = 0;
			ghoul2[model].mFlags = 0;

			// we aren't attached to anyone upfront
			ghoul2[model].mModelBoltLink = -1;
			return model;
		}
	}

	// if we got this far, then we didn't find a spare position, so lets insert a new one
	newModel.mModelindex = modelIndex;
	// on the game side this is valid. On the client side it is valid only after it has been filled in by trap_G2_SetGhoul2ModelIndexes 
	newModel.mModel = RE_RegisterModel((char *)fileName);
  	model_t		*mod_m = R_GetModelByHandle(newModel.mModel);
	if (mod_m->type == MOD_BAD)
	{
		return -1;
	}
			
	// init what is necessary for this ghoul2 model
	G2_Init_Bone_List(newModel.mBlist);
	G2_Init_Bolt_List(newModel.mBltlist);
	newModel.mCustomShader = customShader;
	newModel.mCustomSkin = customSkin;
	strcpy(newModel.mFileName, fileName);
	newModel.mCreationID = modelFlags;
	newModel.mLodBias = lodBias;
	newModel.mAnimFrameDefault = 0;
	newModel.mFlags = 0;

	// we aren't attached to anyone upfront
	newModel.mModelBoltLink = -1;

	// insert into list.
	model = ghoul2.size();
	ghoul2.push_back(newModel);

	return model;
}

qboolean G2API_SetLodBias(CGhoul2Info *ghlInfo, int lodBias)
{
	if (ghlInfo)
	{
		ghlInfo->mLodBias = lodBias;
		return qtrue;
	}
	return qfalse;
}

qboolean G2API_SetSkin(CGhoul2Info *ghlInfo, qhandle_t customSkin)
{
	if (ghlInfo)
	{
		ghlInfo->mCustomSkin = customSkin;
		return qtrue;
	}
	return qfalse;
}

qboolean G2API_SetShader(CGhoul2Info *ghlInfo, qhandle_t customShader)
{
	if (ghlInfo)
	{
		ghlInfo->mCustomShader = customShader;
		return qtrue;
	}
	return qfalse;
}

qboolean G2API_SetSurfaceOnOff(CGhoul2Info *ghlInfo, const char *surfaceName, const int flags)
{
	if (ghlInfo)
	{
		// ensure we flush the cache
		ghlInfo->mMeshFrameNum = 0;
		return G2_SetSurfaceOnOff(ghlInfo->mFileName, ghlInfo->mSlist, surfaceName, flags);
	}
	return qfalse;
}

int G2API_GetSurfaceOnOff(CGhoul2Info *ghlInfo, const char *surfaceName)
{
	if (ghlInfo)
	{
		return G2_IsSurfaceOff(ghlInfo->mFileName, ghlInfo->mSlist, surfaceName);
	}
	return -1;
}

qboolean G2API_SetRootSurface(CGhoul2Info_v &ghlInfo, const int modelIndex, const char *surfaceName)
{
	return G2_SetRootSurface(ghlInfo, modelIndex, ghlInfo[modelIndex].mFileName,  surfaceName);
}

int G2API_AddSurface(CGhoul2Info *ghlInfo, int surfaceNumber, int polyNumber, float BarycentricI, float BarycentricJ, int lod )
{

	if (ghlInfo)
	{
		// ensure we flush the cache
		ghlInfo->mMeshFrameNum = 0;
		return G2_AddSurface(ghlInfo, surfaceNumber, polyNumber, BarycentricI, BarycentricJ, lod);
	}
	return -1;
}

qboolean G2API_RemoveSurface(CGhoul2Info *ghlInfo, const int index)
{
	if (ghlInfo)
	{
		// ensure we flush the cache
		ghlInfo->mMeshFrameNum = 0;
		return G2_RemoveSurface(ghlInfo->mSlist, index);
	}
	return qfalse;
}

int G2API_GetParentSurface(CGhoul2Info *ghlInfo, const int index)
{
	if (ghlInfo)
	{
		return G2_GetParentSurface(ghlInfo->mFileName, index);
	}
	return -1;
}

int G2API_GetSurfaceRenderStatus(CGhoul2Info *ghlInfo, const char *surfaceName)
{
	if (ghlInfo)
	{
		return G2_IsSurfaceRendered(ghlInfo->mFileName, surfaceName, ghlInfo->mSlist);
	}
	return -1;
}

qboolean G2API_RemoveGhoul2Model(CGhoul2Info_v &ghlInfo, const int modelIndex)
{
	// sanity check
	if (!ghlInfo.size() || (ghlInfo.size() <= modelIndex) || (ghlInfo[modelIndex].mModelindex == -1))
	{
		// if we hit this assert then we are trying to delete a ghoul2 model on a ghoul2 instance that
		// one way or another is already gone.
		assert(0 && "remove non existing model");
		return qfalse;
	}

	// clear out the vectors this model used.
	ghlInfo[modelIndex].mBlist.clear();
	ghlInfo[modelIndex].mBltlist.clear();
	ghlInfo[modelIndex].mSlist.clear();

	 // set us to be the 'not active' state
	ghlInfo[modelIndex].mModelindex = -1;

	int newSize = ghlInfo.size();
	// now look through the list from the back and see if there is a block of -1's we can resize off the end of the list
	for (int i=ghlInfo.size()-1; i>-1; i--)
	{
		if (ghlInfo[i].mModelindex == -1)
		{
			newSize = i;
		}
		// once we hit one that isn't a -1, we are done.
		else
		{
			break;
		}
	}
	// do we need to resize?
	if (newSize != ghlInfo.size())
	{
		// yes, so lets do it
		ghlInfo.resize(newSize);
	}
	
	return qtrue;
}

qboolean G2API_SetBoneAnimIndex(CGhoul2Info *ghlInfo, const int index, const int startFrame, const int endFrame, const int flags, const float animSpeed, const int currentTime, const float setFrame, const int blendTime)
{
	if (ghlInfo)
	{
		// ensure we flush the cache
		ghlInfo->mSkelFrameNum = 0;
 		return G2_Set_Bone_Anim_Index(ghlInfo->mBlist, index, startFrame, endFrame, flags, animSpeed, currentTime, setFrame, blendTime);
	}
	return qfalse;
}

qboolean G2API_SetBoneAnim(CGhoul2Info *ghlInfo, const char *boneName, const int startFrame, const int endFrame, const int flags, const float animSpeed, const int currentTime, const float setFrame, const int blendTime)
{
	if (ghlInfo)
	{
		// ensure we flush the cache
		ghlInfo->mSkelFrameNum = 0;
 		return G2_Set_Bone_Anim(ghlInfo->mFileName, ghlInfo->mBlist, boneName, startFrame, endFrame, flags, animSpeed, currentTime, setFrame, blendTime);
	}
	return qfalse;
}

qboolean G2API_GetBoneAnim(CGhoul2Info *ghlInfo, const char *boneName, const int currentTime, float *currentFrame,
						   int *startFrame, int *endFrame, int *flags, float *animSpeed, int *modelList)
{
	if (ghlInfo)
	{
 		return G2_Get_Bone_Anim(ghlInfo->mFileName, ghlInfo->mBlist, boneName, currentTime, currentFrame,
			startFrame, endFrame, flags, animSpeed, modelList, ghlInfo->mModelindex);
	}
	return qfalse;
}

qboolean G2API_GetBoneAnimIndex(CGhoul2Info *ghlInfo, const int iBoneIndex, const int currentTime, float *currentFrame,
						   int *startFrame, int *endFrame, int *flags, float *animSpeed, int *modelList)
{
	if (ghlInfo)
	{
		return G2_Get_Bone_Anim_Index(	ghlInfo->mBlist,// boneInfo_v &blist, 
										iBoneIndex,		// const int index, 
										currentTime,	// const int currentTime, 
										currentFrame,	// float *currentFrame,
										startFrame,		// int *startFrame, 
										endFrame,		// int *endFrame, 
										flags,			// int *flags, 
										animSpeed,		// float *retAnimSpeed,
										modelList,		// qhandle_t *modelList, 
										ghlInfo->mModelindex	// int modelIndex
										);
	}
	return qfalse;
}

qboolean G2API_GetAnimRange(CGhoul2Info *ghlInfo, const char *boneName,	int *startFrame, int *endFrame)
{
	if (ghlInfo)
	{
 		return G2_Get_Bone_Anim_Range(ghlInfo->mFileName, ghlInfo->mBlist, boneName, startFrame, endFrame);
	}
	return qfalse;
}

qboolean G2API_GetAnimRangeIndex(CGhoul2Info *ghlInfo, const int boneIndex, int *startFrame, int *endFrame)
{
	if (ghlInfo)
	{
 		return G2_Get_Bone_Anim_Range_Index(ghlInfo->mBlist, boneIndex, startFrame, endFrame);
	}
	return qfalse;
}

qboolean G2API_PauseBoneAnim(CGhoul2Info *ghlInfo, const char *boneName, const int currentTime)
{
	if (ghlInfo)
	{
 		return G2_Pause_Bone_Anim(ghlInfo->mFileName, ghlInfo->mBlist, boneName, currentTime);
	}
	return qfalse;
}

qboolean G2API_PauseBoneAnimIndex(CGhoul2Info *ghlInfo, const int boneIndex, const int currentTime)
{
	if (ghlInfo)
	{
 		return G2_Pause_Bone_Anim_Index(ghlInfo->mBlist, boneIndex, currentTime);
	}
	return qfalse;
}

qboolean	G2API_IsPaused(CGhoul2Info *ghlInfo, const char *boneName)
{
	if (ghlInfo)
	{
 		return G2_IsPaused(ghlInfo->mFileName, ghlInfo->mBlist, boneName);
	}
	return qfalse;
}

qboolean G2API_StopBoneAnimIndex(CGhoul2Info *ghlInfo, const int index)
{
	if (ghlInfo)
	{
 		return G2_Stop_Bone_Anim_Index(ghlInfo->mBlist, index);
	}
	return qfalse;
}

qboolean G2API_StopBoneAnim(CGhoul2Info *ghlInfo, const char *boneName)
{
	if (ghlInfo)
	{
 		return G2_Stop_Bone_Anim(ghlInfo->mFileName, ghlInfo->mBlist, boneName);
	}
	return qfalse;
}

qboolean G2API_SetBoneAnglesIndex(CGhoul2Info *ghlInfo, const int index, const vec3_t angles, const int flags,
							 const Eorientations yaw, const Eorientations pitch, const Eorientations roll,
							 qhandle_t *modelList, int blendTime, int currentTime)
{
	if (ghlInfo)
	{
		// ensure we flush the cache
		ghlInfo->mSkelFrameNum = 0;
		return G2_Set_Bone_Angles_Index(ghlInfo->mFileName, ghlInfo->mBlist, index, angles, flags, yaw, pitch, roll, modelList, ghlInfo->mModelindex, blendTime, currentTime);
	}
	return qfalse;
}

qboolean G2API_SetBoneAngles(CGhoul2Info *ghlInfo, const char *boneName, const vec3_t angles, const int flags,
							 const Eorientations up, const Eorientations left, const Eorientations forward,
							 qhandle_t *modelList, int blendTime, int currentTime )
{
	if (ghlInfo)
	{
			// ensure we flush the cache
		ghlInfo->mSkelFrameNum = 0;
		return G2_Set_Bone_Angles(ghlInfo->mFileName, ghlInfo->mBlist, boneName, angles, flags, up, left, forward, modelList, ghlInfo->mModelindex, blendTime, currentTime);
	}
	return qfalse;
}

qboolean G2API_SetBoneAnglesMatrixIndex(CGhoul2Info *ghlInfo, const int index, const mdxaBone_t &matrix,
								   const int flags, qhandle_t *modelList, int blendTime, int currentTime)
{
	if (ghlInfo)
	{
		// ensure we flush the cache
		ghlInfo->mSkelFrameNum = 0;
		return G2_Set_Bone_Angles_Matrix_Index(ghlInfo->mBlist, index, matrix, flags, modelList, ghlInfo->mModelindex, blendTime, currentTime);
	}
	return qfalse;
}

qboolean G2API_SetBoneAnglesMatrix(CGhoul2Info *ghlInfo, const char *boneName, const mdxaBone_t &matrix,
								   const int flags, qhandle_t *modelList, int blendTime, int currentTime)
{
	if (ghlInfo)
	{
		// ensure we flush the cache
		ghlInfo->mSkelFrameNum = 0;
		return G2_Set_Bone_Angles_Matrix(ghlInfo->mFileName, ghlInfo->mBlist, boneName, matrix, flags, modelList, ghlInfo->mModelindex, blendTime, currentTime);
	}
	return qfalse;
}

qboolean G2API_StopBoneAnglesIndex(CGhoul2Info *ghlInfo, const int index)
{
	if (ghlInfo)
	{
		// ensure we flush the cache
		ghlInfo->mSkelFrameNum = 0;
 		return G2_Stop_Bone_Angles_Index(ghlInfo->mBlist, index);
	}
	return qfalse;
}

qboolean G2API_StopBoneAngles(CGhoul2Info *ghlInfo, const char *boneName)
{
	if (ghlInfo)
	{
		// ensure we flush the cache
		ghlInfo->mSkelFrameNum = 0;
 		return G2_Stop_Bone_Angles(ghlInfo->mFileName, ghlInfo->mBlist, boneName);
	}
	return qfalse;
}

qboolean G2API_RemoveBone(CGhoul2Info *ghlInfo, const char *boneName)
{
	if (ghlInfo)
	{
		// ensure we flush the cache
		ghlInfo->mSkelFrameNum = 0;
 		return G2_Remove_Bone(ghlInfo->mFileName, ghlInfo->mBlist, boneName);
	}
	return qfalse;
}

void G2API_AnimateG2Models(CGhoul2Info_v &ghoul2, float speedVar)
{
	int model;

	// Walk the list and find all models that are active
	for (model=0; model< ghoul2.size(); model++)
	{
		if (ghoul2[model].mModel)
		{
			G2_Animate_Bone_List(ghoul2, speedVar, model);	
		}
	}
}

qboolean G2API_RemoveBolt(CGhoul2Info *ghlInfo, const int index)
{
	if (ghlInfo)
	{
 		return G2_Remove_Bolt( ghlInfo->mBltlist, index);
	}
	return qfalse;
}

int G2API_AddBolt(CGhoul2Info *ghlInfo, const char *boneName)
{
	if (ghlInfo)
	{
		return G2_Add_Bolt(ghlInfo->mFileName, ghlInfo->mBltlist, ghlInfo->mSlist, boneName);
	}
	return -1;
}

int G2API_AddBoltSurfNum(CGhoul2Info *ghlInfo, const int surfIndex)
{
	if (ghlInfo)
	{
		return G2_Add_Bolt_Surf_Num(ghlInfo->mFileName, ghlInfo->mBltlist, ghlInfo->mSlist, surfIndex);
	}
	return -1;
}


qboolean G2API_AttachG2Model(CGhoul2Info *ghlInfo, CGhoul2Info *ghlInfoTo, int toBoltIndex, int toModel)
{
	assert( toBoltIndex >= 0 );
	if ( toBoltIndex < 0 )
	{
		return qfalse;
	}
	// make sure we have a model to attach, a model to attach to, and a bolt on that model
	if ((ghlInfo) && (ghlInfoTo) && ((ghlInfoTo->mBltlist[toBoltIndex].boneNumber != -1) || (ghlInfoTo->mBltlist[toBoltIndex].surfaceNumber != -1)))
	{
		// encode the bolt address into the model bolt link
	   toModel &= MODEL_AND;
	   toBoltIndex &= BOLT_AND;
	   ghlInfo->mModelBoltLink = (toModel << MODEL_SHIFT)  | (toBoltIndex << BOLT_SHIFT);
	   return qtrue;
	}
	return qfalse;
}


qboolean G2API_DetachG2Model(CGhoul2Info *ghlInfo)
{
	if (ghlInfo)
	{
	   ghlInfo->mModelBoltLink = -1;
	   return qtrue;
	}
	return qfalse;
}

qboolean G2API_AttachEnt(int *boltInfo, CGhoul2Info *ghlInfoTo, int toBoltIndex, int entNum, int toModelNum)
{  	
	// make sure we have a model to attach, a model to attach to, and a bolt on that model
	if ((ghlInfoTo) && ((ghlInfoTo->mBltlist[toBoltIndex].boneNumber != -1) || (ghlInfoTo->mBltlist[toBoltIndex].surfaceNumber != -1)))
	{
		// encode the bolt address into the model bolt link
	   toModelNum &= MODEL_AND;	
	   toBoltIndex &= BOLT_AND;
	   entNum &= ENTITY_AND;
	   *boltInfo =  (toBoltIndex << BOLT_SHIFT) | (toModelNum << MODEL_SHIFT) | (entNum << ENTITY_SHIFT);
	   return qtrue;
	}
	return qfalse;

}

void G2API_DetachEnt(int *boltInfo)
{
   *boltInfo = 0;
}


qboolean G2API_GetBoltMatrix(CGhoul2Info_v &ghoul2, const int modelIndex, const int boltIndex, mdxaBone_t *matrix, const vec3_t angles, 
							 const vec3_t position, const int frameNum, qhandle_t *modelList, const vec3_t scale )
{
	CGhoul2Info *ghlInfo = &ghoul2[modelIndex];

	//assert(boltIndex < ghlInfo->mBltlist.size());

	if (boltIndex >= 0 && ghlInfo && (boltIndex < ghlInfo->mBltlist.size()) )
	{
		// make sure we have transformed the skeleton
		G2_ConstructGhoulSkeleton(ghoul2, frameNum, modelList, true, position, scale, false);

		// scale the bolt position by the scale factor for this model since at this point its still in model space
		if (scale[0])
		{
			ghlInfo->mBltlist[boltIndex].position.matrix[0][3] *= scale[0];
		}
		if (scale[1])
		{
			ghlInfo->mBltlist[boltIndex].position.matrix[1][3] *= scale[1];
		}
		if (scale[2])
		{
			ghlInfo->mBltlist[boltIndex].position.matrix[2][3] *= scale[2];
		}

		// pre generate the world matrix
		G2_GenerateWorldMatrix(angles, position);

		// make the model space matrix we have for this bolt into a world matrix
		Multiply_3x4Matrix(matrix, &worldMatrix, &ghlInfo->mBltlist[boltIndex].position );
												
		return qtrue;
		
	}

	return qfalse;
}

void G2API_ListSurfaces(CGhoul2Info *ghlInfo)
{
	if (ghlInfo)
	{
		G2_List_Model_Surfaces(ghlInfo->mFileName);
	}
}

void G2API_ListBones(CGhoul2Info *ghlInfo, int frame)
{
	if (ghlInfo)
	{
		G2_List_Model_Bones(ghlInfo->mFileName, frame);
	}
}

// decide if we have Ghoul2 models associated with this ghoul list or not
qboolean G2API_HaveWeGhoul2Models(CGhoul2Info_v &ghoul2)
{
	return !!ghoul2.IsValid();
}

// run through the Ghoul2 models and set each of the mModel values to the correct one from the cgs.gameModel offset lsit
void G2API_SetGhoul2ModelIndexes(CGhoul2Info_v &ghoul2, qhandle_t *modelList, qhandle_t *skinList)
{
	int i;
	for (i=0; i<ghoul2.size(); i++)
	{
		if (ghoul2[i].mModelindex != -1)
		{
			// broken into 3 lines for debugging, STL is a pain to view...
			//
			const int iModelIndex  = ghoul2[i].mModelindex;
			const qhandle_t mModel = modelList[iModelIndex];
//we have to call this func to clean up the bad mModels from before the renderer started, but this won't be filled in the very next frame
//so, for that frame it will actually zero out the real model, but i don't know which is valid, so i'll just fix it in the renderer.
//			assert(mModel);

			ghoul2[i].mModel = mModel;
			ghoul2[i].mSkin = skinList[ghoul2[i].mCustomSkin];
		}
	}
}


char *G2API_GetAnimFileNameIndex(qhandle_t modelIndex)
{
	model_t		*mod_m = R_GetModelByHandle(modelIndex);
	return mod_m->mdxm->animName;
}

/************************************************************************************************
 * G2API_GetAnimFileName
 *    obtains the name of a model's .gla (animation) file 
 *
 * Input
 *    pointer to list of CGhoul2Info's, WraithID of specific model in that list
 *
 * Output
 *    true if a good filename was obtained, false otherwise
 *
 ************************************************************************************************/
qboolean G2API_GetAnimFileName(CGhoul2Info *ghlInfo, char **filename)
{
	if (ghlInfo)
	{
		return G2_GetAnimFileName(ghlInfo->mFileName, filename);
	}
	return qfalse;
}

/*
=======================
SV_QsortEntityNumbers
=======================
*/
static int QDECL QsortDistance( const void *a, const void *b ) {
	const float	&ea = ((CCollisionRecord*)a)->mDistance;
	const float	&eb = ((CCollisionRecord*)b)->mDistance;

	if ( ea < eb ) {
		return -1;
	}
	return 1;
}


void G2API_CollisionDetect(CCollisionRecord *collRecMap, CGhoul2Info_v &ghoul2, const vec3_t angles, const vec3_t position,
							int frameNumber, int entNum, vec3_t rayStart, vec3_t rayEnd, vec3_t scale, CMiniHeap *,
							EG2_Collision eG2TraceType, int useLod, float fRadius)
{

	vec3_t	transRayStart, transRayEnd;

	//let's keep our timer use consistent
	frameNumber = tr.refdef.time;

	// make sure we have transformed the whole skeletons for each model
	G2_ConstructGhoulSkeleton(ghoul2, frameNumber, NULL, true, position, scale, false);

	// pre generate the world matrix - used to transform the incoming ray
	G2_GenerateWorldMatrix(angles, position);

	G2VertSpaceServer->ResetHeap();

	// now having done that, time to build the model
	G2_TransformModel(ghoul2, frameNumber, scale,G2VertSpaceServer, useLod);

	// model is built. Lets check to see if any triangles are actually hit.
	// first up, translate the ray to model space
	TransformAndTranslatePoint(rayStart, transRayStart, &worldMatrixInv);
	TransformAndTranslatePoint(rayEnd, transRayEnd, &worldMatrixInv);

	// now walk each model and check the ray against each poly - sigh, this is SO expensive. I wish there was a better way to do this.
	G2_TraceModels(ghoul2, transRayStart, transRayEnd, collRecMap, entNum, eG2TraceType, useLod, fRadius);

	G2VertSpaceServer->ResetHeap();
	// now sort the resulting array of collision records so they are distance ordered
	qsort( collRecMap, MAX_G2_COLLISIONS, 
		sizeof( CCollisionRecord ), QsortDistance );


}

qboolean G2API_SetGhoul2ModelFlags(CGhoul2Info *ghlInfo, const int flags)
{
  	if (ghlInfo)
	{
		ghlInfo->mFlags &= GHOUL2_NEWORIGIN;
		ghlInfo->mFlags |= flags;
		return qtrue;
	}
	return qfalse;
}

int G2API_GetGhoul2ModelFlags(CGhoul2Info *ghlInfo)
{
  	if (ghlInfo)
	{
		return (ghlInfo->mFlags & ~GHOUL2_NEWORIGIN);
	}
	return 0;
}

// given a boltmatrix, return in vec a normalised vector for the axis requested in flags
void G2API_GiveMeVectorFromMatrix(mdxaBone_t &boltMatrix, Eorientations flags, vec3_t &vec)
{
	switch (flags)
	{
	case ORIGIN:
		vec[0] = boltMatrix.matrix[0][3];
		vec[1] = boltMatrix.matrix[1][3];
		vec[2] = boltMatrix.matrix[2][3];
		break;
	case POSITIVE_Y:
		vec[0] = boltMatrix.matrix[0][1];
		vec[1] = boltMatrix.matrix[1][1];
		vec[2] = boltMatrix.matrix[2][1];
 		break;
	case POSITIVE_X:
		vec[0] = boltMatrix.matrix[0][0];
		vec[1] = boltMatrix.matrix[1][0];
		vec[2] = boltMatrix.matrix[2][0];
		break;
	case POSITIVE_Z:
		vec[0] = boltMatrix.matrix[0][2];
		vec[1] = boltMatrix.matrix[1][2];
		vec[2] = boltMatrix.matrix[2][2];
		break;
	case NEGATIVE_Y:
		vec[0] = -boltMatrix.matrix[0][1];
		vec[1] = -boltMatrix.matrix[1][1];
		vec[2] = -boltMatrix.matrix[2][1];
		break;
	case NEGATIVE_X:
		vec[0] = -boltMatrix.matrix[0][0];
		vec[1] = -boltMatrix.matrix[1][0];
		vec[2] = -boltMatrix.matrix[2][0];
		break;
	case NEGATIVE_Z:
		vec[0] = -boltMatrix.matrix[0][2];
		vec[1] = -boltMatrix.matrix[1][2];
		vec[2] = -boltMatrix.matrix[2][2];
		break;
	}
}

// copy a model from one ghoul2 instance to another, and reset the root surface on the new model if need be
// NOTE if modelIndex = -1 then copy all the models
void G2API_CopyGhoul2Instance(CGhoul2Info_v &ghoul2From, CGhoul2Info_v &ghoul2To, int modelIndex)
{
	int	i, model;
	int	from = 0;
	int	to = ghoul2From.size();

	// determing if we are only copying one model or not
	if (modelIndex != -1)
	{
		from = modelIndex;
		to = modelIndex + 1;
	}

	// now copy the models
	for (i=from; i<to; i++)
	{
		// find a free spot in the list
		for (model=0; model< ghoul2To.size(); model++)
		{
			if (ghoul2To[model].mModelindex == -1)
			{
				// Copy model to clear position
				ghoul2To[model] = ghoul2From[i];
				break;
			}
		}
		// we didn't find one. Resize us up one and copy across what we need
		model = ghoul2To.size();
		ghoul2To.resize(model + 1);
		ghoul2To[model] = ghoul2From[i];
	}

}

char *G2API_GetSurfaceName(CGhoul2Info *ghlInfo, int surfNumber)
{
	static char noSurface[1] = "";
	model_t	*mod = R_GetModelByHandle(RE_RegisterModel(ghlInfo->mFileName));
	mdxmSurface_t		*surf = 0;
	mdxmSurfHierarchy_t	*surfInfo = 0;

	surf = (mdxmSurface_t *)G2_FindSurface((void *)mod, surfNumber, 0);
	if (surf)
	{
		mdxmHierarchyOffsets_t	*surfIndexes = (mdxmHierarchyOffsets_t *)((byte *)mod->mdxm + sizeof(mdxmHeader_t));
		surfInfo = (mdxmSurfHierarchy_t *)((byte *)surfIndexes + surfIndexes->offsets[surf->thisSurfaceIndex]);
		return surfInfo->name;
	}

	return noSurface;
}


int	G2API_GetSurfaceIndex(CGhoul2Info *ghlInfo, const char *surfaceName)
{
  	if (ghlInfo)
	{
		return G2_GetSurfaceIndex(ghlInfo->mFileName, surfaceName);
	}
	return -1;
}

char *G2API_GetGLAName(CGhoul2Info *ghlInfo)
{
	model_t	*mod = R_GetModelByHandle(RE_RegisterModel(ghlInfo->mFileName));
	return mod->mdxm->animName;
}

qboolean G2API_SetNewOrigin(CGhoul2Info *ghlInfo, const int boltIndex)
{
	if (ghlInfo)
	{
		ghlInfo->mNewOrigin = boltIndex;
		ghlInfo->mFlags |= GHOUL2_NEWORIGIN;
		return qtrue;
	}
	return qfalse;
}

int G2API_GetBoneIndex(CGhoul2Info *ghlInfo, const char *boneName, qboolean bAddIfNotFound)
{
	if (ghlInfo)
	{			
		return G2_Get_Bone_Index(ghlInfo, boneName, bAddIfNotFound);
	}
	return -1;
}

qboolean G2API_SaveGhoul2Models(CGhoul2Info_v &ghoul2, char **buffer, int *size)
{
	return G2_SaveGhoul2Models(ghoul2, buffer, size);
}

void G2API_LoadGhoul2Models(CGhoul2Info_v &ghoul2, char *buffer)
{
	G2_LoadGhoul2Model(ghoul2, buffer);
}

void G2API_FreeSaveBuffer(char *buffer)
{
	Z_Free(buffer);
}

// this is kinda sad, but I need to call the destructor in this module (exe), not the game.dll...
//
void G2API_LoadSaveCodeDestructGhoul2Info(CGhoul2Info_v &ghoul2)
{
	ghoul2.~CGhoul2Info_v();	// so I can load junk over it then memset to 0 without orphaning
}