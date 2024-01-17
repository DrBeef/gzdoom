// 
//---------------------------------------------------------------------------
//
// Copyright(C) 2009-2016 Christoph Oelckers
// All rights reserved.
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU Lesser General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Lesser General Public License for more details.
//
// You should have received a copy of the GNU Lesser General Public License
// along with this program.  If not, see http://www.gnu.org/licenses/
//
//--------------------------------------------------------------------------
//

#ifndef __GL_RENDERSTATE_H
#define __GL_RENDERSTATE_H

#include <string.h>
#include "gl_load/gl_system.h"
#include "gl_load/gl_interface.h"
#include "gl/renderer/gl_renderer.h"
#include "r_data/matrix.h"
#include "hwrenderer/scene//hw_drawstructs.h"
#include "hwrenderer/scene//hw_renderstate.h"
#include "hwrenderer/textures/hw_material.h"
#include "c_cvars.h"
#include "r_defs.h"
#include "r_data/r_translate.h"
#include "v_palette.h"
#include "g_levellocals.h"

class FVertexBuffer;
class FShader;
struct GLSectorPlane;
extern TArray<VSMatrix> gl_MatrixStack;

EXTERN_CVAR(Bool, gl_global_fade)
EXTERN_CVAR(Color, gl_global_fade_color)
EXTERN_CVAR(Bool, gl_enhanced_nightvision)

enum EPassType
{
	NORMAL_PASS,
	GBUFFER_PASS,
	MAX_PASS_TYPES
};


class FGLRenderState : public FRenderState
{
	uint8_t mLastDepthClamp : 1;

	int mGlobalFadeMode;
	float mGlossiness, mSpecularLevel;
	float mShaderTimer;

	float mInterpolationFactor;

	FVertexBuffer *mVertexBuffer, *mCurrentVertexBuffer;
	float mClipSplit[2];

	int mEffectState;
	int mTempTM = TM_NORMAL;

	FRenderStyle stRenderStyle;
	int stSrcBlend, stDstBlend;
	bool stAlphaTest;
	bool stSplitEnabled;
	int stBlendEquation;

	FShader *activeShader;

	EPassType mPassType = NORMAL_PASS;
	int mNumDrawBuffers = 1;

	bool ApplyShader();

	// Texture binding state
	FMaterial *lastMaterial = nullptr;
	int lastClamp = 0;
	int lastTranslation = 0;
	int maxBoundMaterial = -1;


public:

	FGLRenderState()
	{
		Reset();
	}

	void Reset();

	void ClearLastMaterial()
	{
		lastMaterial = nullptr;
	}

	void ApplyMaterial(FMaterial *mat, int clampmode, int translation, int overrideshader);

	void Apply();
	void ApplyLightIndex(int index);
	void ApplyBlendMode();

	void SetVertexBuffer(FVertexBuffer *vb)
	{
		mVertexBuffer = vb;
	}

	void ResetVertexBuffer()
	{
		// forces rebinding with the next 'apply' call.
		mCurrentVertexBuffer = NULL;
	}

	int SetGlobalFadeMode(int fadeMode)
	{
		int fademode = mGlobalFadeMode;
		mGlobalFadeMode = fadeMode;
		return fademode;
	}

	void SetSpecular(float glossiness, float specularLevel)
	{
		mGlossiness = glossiness;
		mSpecularLevel = specularLevel;
	}

	void InitSceneClearColor()
	{
		float r, g, b;
		if (gl_global_fade)
		{
			mSceneColor = mFadeColor;
		}
		r = g = b = 1.f;
		GLRenderer->mSceneClearColor[0] = mSceneColor.r * r / 255.f;
		GLRenderer->mSceneClearColor[1] = mSceneColor.g * g / 255.f;
		GLRenderer->mSceneClearColor[2] = mSceneColor.b * b / 255.f;
	}

	void ResetFadeColor()
	{
		mFadeColor = gl_global_fade_color;
	}

	void SetClipSplit(float bottom, float top)
	{
		mClipSplit[0] = bottom;
		mClipSplit[1] = top;
	}

	void SetClipSplit(float *vals)
	{
		memcpy(mClipSplit, vals, 2 * sizeof(float));
	}

	void GetClipSplit(float *out)
	{
		memcpy(out, mClipSplit, 2 * sizeof(float));
	}

	void ClearClipSplit()
	{
		mClipSplit[0] = -1000000.f;
		mClipSplit[1] = 1000000.f;
	}

	// This wraps the depth clamp setting because we frequently need to read it which OpenGL is not particularly performant at...
	bool SetDepthClamp(bool on)
	{
		bool res = mLastDepthClamp;
		if (!on) glDisable(GL_DEPTH_CLAMP);
		else glEnable(GL_DEPTH_CLAMP);
		mLastDepthClamp = on;
		return res;
	}

	void SetInterpolationFactor(float fac)
	{
		mInterpolationFactor = fac;
	}

	float GetInterpolationFactor()
	{
		return mInterpolationFactor;
	}

	void SetPassType(EPassType passType)
	{
		mPassType = passType;
	}

	EPassType GetPassType()
	{
		return mPassType;
	}

	void EnableDrawBuffers(int count)
	{
		count = MIN(count, 3);
		if (mNumDrawBuffers != count)
		{
			static GLenum buffers[] = { GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1, GL_COLOR_ATTACHMENT2 };
			glDrawBuffers(count, buffers);
			mNumDrawBuffers = count;
		}
	}

	int GetPassDrawBufferCount()
	{
		return mPassType == GBUFFER_PASS ? 3 : 1;
	}


	void SetVertexBuffer(int which) override;
};

extern FGLRenderState gl_RenderState;

#endif
