#pragma once
//==========================================================================
//
// One wall segment in the draw list
//
//==========================================================================
#include "r_defs.h"
#include "r_data/renderstyle.h"
#include "textures/textures.h"
#include "r_data/colormaps.h"

#pragma warning(disable:4244)

struct GLHorizonInfo;
struct GLSkyInfo;
struct F3DFloor;
class FMaterial;
struct FTexCoordInfo;
struct FSectorPortalGroup;
struct FFlatVertex;
struct FLinePortalSpan;
struct FDynLightData;

enum WallTypes
{
	RENDERWALL_NONE,
	RENDERWALL_TOP,
	RENDERWALL_M1S,
	RENDERWALL_M2S,
	RENDERWALL_BOTTOM,
	RENDERWALL_FOGBOUNDARY,
	RENDERWALL_MIRRORSURFACE,
	RENDERWALL_M2SNF,
	RENDERWALL_COLOR,
	RENDERWALL_FFBLOCK,
	// Insert new types at the end!
};

enum PortalTypes
{
	PORTALTYPE_SKY,
	PORTALTYPE_HORIZON,
	PORTALTYPE_SKYBOX,
	PORTALTYPE_SECTORSTACK,
	PORTALTYPE_PLANEMIRROR,
	PORTALTYPE_MIRROR,
	PORTALTYPE_LINETOLINE,
};

//==========================================================================
//
// One sector plane, still in fixed point
//
//==========================================================================

struct GLSectorPlane
{
	FTextureID texture;
	secplane_t plane;
	float Texheight;
	float	Angle;
	FVector2 Offs;
	FVector2 Scale;

	void GetFromSector(sector_t * sec, int ceiling)
	{
		Offs.X = (float)sec->GetXOffset(ceiling);
		Offs.Y = (float)sec->GetYOffset(ceiling);
		Scale.X = (float)sec->GetXScale(ceiling);
		Scale.Y = (float)sec->GetYScale(ceiling);
		Angle = (float)sec->GetAngle(ceiling).Degrees;
		texture = sec->GetTexture(ceiling);
		plane = sec->GetSecPlane(ceiling);
		Texheight = (float)((ceiling == sector_t::ceiling)? plane.fD() : -plane.fD());
	}
};

struct GLSeg
{
	float x1,x2;
	float y1,y2;
	float fracleft, fracright;	// fractional offset of the 2 vertices on the linedef

	FVector3 Normal() const 
	{
		// we do not use the vector math inlines here because they are not optimized for speed but accuracy in the playsim and this is called quite frequently.
		float x = y2 - y1;
		float y = x1 - x2;
		float ilength = 1.f / sqrtf(x*x + y*y);
		return FVector3(x * ilength, 0, y * ilength);
	}
};

struct texcoord
{
	float u,v;
};

struct HWDrawInfo;

class GLWall
{
public:
	static const char passflag[];

	enum
	{
		GLWF_CLAMPX=1,
		GLWF_CLAMPY=2,
		GLWF_SKYHACK=4,
		GLWF_GLOW=8,		// illuminated by glowing flats
		GLWF_NOSPLITUPPER=16,
		GLWF_NOSPLITLOWER=32,
		GLWF_NOSPLIT=64,
		GLWF_TRANSLUCENT = 128
	};

	enum
	{
		RWF_BLANK = 0,
		RWF_TEXTURED = 1,	// actually not being used anymore because with buffers it's even less efficient not writing the texture coordinates - but leave it here
		RWF_NOSPLIT = 4,
		RWF_NORENDER = 8,
	};

	enum
	{
		LOLFT,
		UPLFT,
		UPRGT,
		LORGT,
	};

	friend struct GLDrawList;
	friend class GLPortal;

	vertex_t * vertexes[2];				// required for polygon splitting
	FMaterial *gltexture;
	TArray<lightlist_t> *lightlist;

	GLSeg glseg;
	float ztop[2],zbottom[2];
	texcoord tcs[4];
	float alpha;

	FColormap Colormap;
	ERenderStyle RenderStyle;
	
	float ViewDistance;

	int lightlevel;
	uint8_t type;
	uint8_t flags;
	short rellight;

	float topglowcolor[4];
	float bottomglowcolor[4];

	int dynlightindex;

	union
	{
		// it's either one of them but never more!
		FSectorPortal *secportal;	// sector portal (formerly skybox)
		GLSkyInfo * sky;			// for normal sky
		GLHorizonInfo * horizon;	// for horizon information
		FSectorPortalGroup * portal;			// stacked sector portals
		secplane_t * planemirror;	// for plane mirrors
		FLinePortalSpan *lineportal;	// line-to-line portals
	};


	secplane_t topplane, bottomplane;	// we need to save these to pass them to the shader for calculating glows.

	// these are not the same as ytop and ybottom!!!
	float zceil[2];
	float zfloor[2];

	unsigned int vertindex;
	unsigned int vertcount;

public:
	seg_t * seg;			// this gives the easiest access to all other structs involved
	subsector_t * sub;		// For polyobjects
//private:

	void PutWall(HWDrawInfo *di, bool translucent);
	void PutPortal(HWDrawInfo *di, int ptype);
	void CheckTexturePosition(FTexCoordInfo *tci);

	void Put3DWall(HWDrawInfo *di, lightlist_t * lightlist, bool translucent);
	bool SplitWallComplex(HWDrawInfo *di, sector_t * frontsector, bool translucent, float& maplightbottomleft, float& maplightbottomright);
	void SplitWall(HWDrawInfo *di, sector_t * frontsector, bool translucent);

	bool SetupLights(FDynLightData &lightdata);

	void MakeVertices(HWDrawInfo *di, bool nosplit);

	void SkyPlane(HWDrawInfo *di, sector_t *sector, int plane, bool allowmirror);
	void SkyLine(HWDrawInfo *di, sector_t *sec, line_t *line);
	void SkyNormal(HWDrawInfo *di, sector_t * fs,vertex_t * v1,vertex_t * v2);
	void SkyTop(HWDrawInfo *di, seg_t * seg,sector_t * fs,sector_t * bs,vertex_t * v1,vertex_t * v2);
	void SkyBottom(HWDrawInfo *di, seg_t * seg,sector_t * fs,sector_t * bs,vertex_t * v1,vertex_t * v2);

	bool DoHorizon(HWDrawInfo *di, seg_t * seg,sector_t * fs, vertex_t * v1,vertex_t * v2);

	bool SetWallCoordinates(seg_t * seg, FTexCoordInfo *tci, float ceilingrefheight,
		float topleft, float topright, float bottomleft, float bottomright, float t_ofs);

	void DoTexture(HWDrawInfo *di, int type,seg_t * seg,int peg,
						   float ceilingrefheight, float floorrefheight,
						   float CeilingHeightstart,float CeilingHeightend,
						   float FloorHeightstart,float FloorHeightend,
						   float v_offset);

	void DoMidTexture(HWDrawInfo *di, seg_t * seg, bool drawfogboundary,
					  sector_t * front, sector_t * back,
					  sector_t * realfront, sector_t * realback,
					  float fch1, float fch2, float ffh1, float ffh2,
					  float bch1, float bch2, float bfh1, float bfh2);

	void GetPlanePos(F3DFloor::planeref * planeref, float & left, float & right);

	void BuildFFBlock(HWDrawInfo *di, seg_t * seg, F3DFloor * rover,
					  float ff_topleft, float ff_topright, 
					  float ff_bottomleft, float ff_bottomright);
	void InverseFloors(HWDrawInfo *di, seg_t * seg, sector_t * frontsector,
					   float topleft, float topright, 
					   float bottomleft, float bottomright);
	void ClipFFloors(HWDrawInfo *di, seg_t * seg, F3DFloor * ffloor, sector_t * frontsector,
					float topleft, float topright, 
					float bottomleft, float bottomright);
	void DoFFloorBlocks(HWDrawInfo *di, seg_t * seg, sector_t * frontsector, sector_t * backsector,
					  float fch1, float fch2, float ffh1, float ffh2,
					  float bch1, float bch2, float bfh1, float bfh2);


	void CreateVertices(FFlatVertex *&ptr, bool nosplit);
	void SplitLeftEdge (FFlatVertex *&ptr);
	void SplitRightEdge(FFlatVertex *&ptr);
	void SplitUpperEdge(FFlatVertex *&ptr);
	void SplitLowerEdge(FFlatVertex *&ptr);

	int CountVertices();

public:
	GLWall() {}

	GLWall(const GLWall &other)
	{
		memcpy(this, &other, sizeof(GLWall));
	}

	GLWall & operator=(const GLWall &other)
	{
		memcpy(this, &other, sizeof(GLWall));
		return *this;
	}

	void Process(HWDrawInfo *di, seg_t *seg, sector_t *frontsector, sector_t *backsector);
	void ProcessLowerMiniseg(HWDrawInfo *di, seg_t *seg, sector_t *frontsector, sector_t *backsector);

	float PointOnSide(float x,float y)
	{
		return -((y-glseg.y1)*(glseg.x2-glseg.x1)-(x-glseg.x1)*(glseg.y2-glseg.y1));
	}

	// Lines start-end and fdiv must intersect.
	double CalcIntersectionVertex(GLWall * w2)
	{
		float ax = glseg.x1, ay=glseg.y1;
		float bx = glseg.x2, by=glseg.y2;
		float cx = w2->glseg.x1, cy=w2->glseg.y1;
		float dx = w2->glseg.x2, dy=w2->glseg.y2;
		return ((ay-cy)*(dx-cx)-(ax-cx)*(dy-cy)) / ((bx-ax)*(dy-cy)-(by-ay)*(dx-cx));
	}

};

inline float Dist2(float x1,float y1,float x2,float y2)
{
	return sqrtf((x1-x2)*(x1-x2)+(y1-y2)*(y1-y2));
}
