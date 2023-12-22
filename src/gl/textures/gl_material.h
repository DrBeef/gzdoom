
#ifndef __GL_MATERIAL_H
#define __GL_MATERIAL_H

#include "m_fixed.h"
#include "textures/textures.h"
#include "gl/textures/gl_hwtexture.h"
#include "gl/renderer/gl_colormap.h"
#include "i_system.h"
#include "r_defs.h"

EXTERN_CVAR(Bool, gl_precache)

struct FRemapTable;
class FTextureShader;

enum
{
	CLAMP_NONE = 0,
	CLAMP_X = 1,
	CLAMP_Y = 2,
	CLAMP_XY = 3,
	CLAMP_XY_NOMIP = 4,
	CLAMP_NOFILTER = 5,
	CLAMP_CAMTEX = 6,
};


//===========================================================================
// 
// this is the texture maintenance class for OpenGL. 
//
//===========================================================================
class FMaterial;

enum ESpecialTranslations : uint32_t
{
	STRange_Min = 0x10000000,
	STRange_Desaturate = 0x10000000,
	STRange_Specialcolormap = 0x20000000,
};

class FGLTexture
{
	friend class FMaterial;
public:
	FTexture * tex;
	FTexture * hirestexture;
	int8_t bIsTransparent;
	int HiresLump;

private:
	FHardwareTexture *mHwTexture;

	bool bHasColorkey;		// only for hires
	bool bExpandFlag;
	uint8_t lastSampler;
	int lastTranslation;

	unsigned char * LoadHiresTexture(FTexture *hirescheck, int *width, int *height);

	FHardwareTexture *CreateHwTexture();

	const FHardwareTexture *Bind(int texunit, int clamp, int translation, FTexture *hirescheck);
	
public:
	FGLTexture(FTexture * tx, bool expandpatches);
	~FGLTexture();

	unsigned char * CreateTexBuffer(int translation, int & w, int & h, FTexture *hirescheck, bool createexpanded = true, bool alphatrans = false);

	void Clean(bool all);
	void CleanUnused(SpriteHits &usedtranslations);
	int Dump(int i);

};

//===========================================================================
// 
// this is the material class for OpenGL. 
//
//===========================================================================

class FMaterial
{
	friend class FRenderState;

	struct FTextureLayer
	{
		FTexture *texture;
		bool animated;
	};

	// This array is needed because not all textures are managed by the texture manager
	// but some code needs to discard all hardware dependent data attached to any created texture.
	// Font characters are not, for example.
	static TArray<FMaterial *> mMaterials;
	static int mMaxBound;

	FGLTexture *mBaseLayer;	
	TArray<FTextureLayer> mTextureLayers;
	int mShaderIndex;
	int mLayerFlags = 0;

	short mLeftOffset;
	short mTopOffset;
	short mWidth;
	short mHeight;
	short mRenderWidth;
	short mRenderHeight;
	bool mExpanded;
	bool mTrimResult;
	uint16_t trim[4];

	float mSpriteU[2], mSpriteV[2];
	FloatRect mSpriteRect;

	FGLTexture * ValidateSysTexture(FTexture * tex, bool expand);
	bool TrimBorders(uint16_t *rect);

public:
	FTexture *tex;
	
	FMaterial(FTexture *tex, bool forceexpand);
	~FMaterial();
	int GetLayerFlags() const { return mLayerFlags; }
	void SetSpriteRect();
	void Precache();
	void PrecacheList(SpriteHits &translations);
	bool isMasked() const
	{
		return mBaseLayer->tex->bMasked;
	}

	int GetLayers() const
	{
		return mTextureLayers.Size() + 1;
	}

	void Bind(int clamp, int translation);

	unsigned char * CreateTexBuffer(int translation, int & w, int & h, bool allowhires=true, bool createexpanded = true) const
	{
		return mBaseLayer->CreateTexBuffer(translation, w, h, allowhires? tex : NULL, createexpanded);
	}

	void Clean(bool f)
	{
		mBaseLayer->Clean(f);
	}

	void BindToFrameBuffer();
	// Patch drawing utilities

	void GetSpriteRect(FloatRect * r) const
	{
		*r = mSpriteRect;
	}

	void GetTexCoordInfo(FTexCoordInfo *tci, float x, float y) const
	{
		tci->GetFromTexture(tex, x, y);
	}

	void GetTexCoordInfo(FTexCoordInfo *tci, side_t *side, int texpos) const
	{
		GetTexCoordInfo(tci, (float)side->GetTextureXScale(texpos), (float)side->GetTextureYScale(texpos));
	}

	// This is scaled size in integer units as needed by walls and flats
	int TextureHeight() const { return mRenderHeight; }
	int TextureWidth() const { return mRenderWidth; }

	int GetAreas(FloatRect **pAreas) const;

	int GetWidth() const
	{
		return mWidth;
	}

	int GetHeight() const
	{
		return mHeight;
	}

	int GetLeftOffset() const
	{
		return mLeftOffset;
	}

	int GetTopOffset() const
	{
		return mTopOffset;
	}

	// Get right/bottom UV coordinates for patch drawing
	float GetUL() const { return 0; }
	float GetVT() const { return 0; }
	float GetUR() const { return 1; }
	float GetVB() const { return 1; }
	float GetU(float upix) const { return upix/(float)mWidth; }
	float GetV(float vpix) const { return vpix/(float)mHeight; }

	float GetSpriteUL() const { return mSpriteU[0]; }
	float GetSpriteVT() const { return mSpriteV[0]; }
	float GetSpriteUR() const { return mSpriteU[1]; }
	float GetSpriteVB() const { return mSpriteV[1]; }



	bool GetTransparent() const
	{
		if (mBaseLayer->bIsTransparent == -1) 
		{
			if (!mBaseLayer->tex->bHasCanvas)
			{
				int w, h;
				unsigned char *buffer = CreateTexBuffer(0, w, h);
				delete [] buffer;
			}
			else
			{
				mBaseLayer->bIsTransparent = 0;
			}
		}
		return !!mBaseLayer->bIsTransparent;
	}

	static void DeleteAll();
	static void FlushAll();
	static FMaterial *ValidateTexture(FTexture * tex, bool expand);
	static FMaterial *ValidateTexture(FTextureID no, bool expand, bool trans);
	static void ClearLastTexture();

	static void InitGlobalState();
};

#endif


