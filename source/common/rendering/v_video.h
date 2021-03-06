/*
** v_video.h
**
**---------------------------------------------------------------------------
** Copyright 1998-2008 Randy Heit
** All rights reserved.
**
** Redistribution and use in source and binary forms, with or without
** modification, are permitted provided that the following conditions
** are met:
**
** 1. Redistributions of source code must retain the above copyright
**    notice, this list of conditions and the following disclaimer.
** 2. Redistributions in binary form must reproduce the above copyright
**    notice, this list of conditions and the following disclaimer in the
**    documentation and/or other materials provided with the distribution.
** 3. The name of the author may not be used to endorse or promote products
**    derived from this software without specific prior written permission.
**
** THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
** IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
** OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
** IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
** INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
** NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
** DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
** THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
** (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
** THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
**---------------------------------------------------------------------------
**
*/

#ifndef __V_VIDEO_H__
#define __V_VIDEO_H__

#include <functional>
#include "basics.h"
#include "vectors.h"
#include "m_png.h"

//#include "doomdef.h"
//#include "dobject.h"
#include "renderstyle.h"
#include "c_cvars.h"
#include "v_2ddrawer.h"
//#include "hwrenderer/dynlights/hw_shadowmap.h"

static const int VID_MIN_WIDTH = 640;
static const int VID_MIN_HEIGHT = 400;

static const int VID_MIN_UI_WIDTH = 640;
static const int VID_MIN_UI_HEIGHT = 400;

struct sector_t;
class FTexture;
struct FPortalSceneState;
class FSkyVertexBuffer;
class IIndexBuffer;
class IVertexBuffer;
class IDataBuffer;
class FFlatVertexBuffer;
class HWViewpointBuffer;
class FLightBuffer;
struct HWDrawInfo;

enum EHWCaps
{
	// [BB] Added texture compression flags.
	RFL_TEXTURE_COMPRESSION = 1,
	RFL_TEXTURE_COMPRESSION_S3TC = 2,

	RFL_SHADER_STORAGE_BUFFER = 4,
	RFL_BUFFER_STORAGE = 8,

	RFL_NO_CLIP_PLANES = 32,

	RFL_INVALIDATE_BUFFER = 64,
	RFL_DEBUG = 128,
};


struct IntRect
{
	int left, top;
	int width, height;


	void Offset(int xofs, int yofs)
	{
		left += xofs;
		top += yofs;
	}

	void AddToRect(int x, int y)
	{
		if (x < left)
			left = x;
		if (x > left + width)
			width = x - left;

		if (y < top)
			top = y;
		if (y > top + height)
			height = y - top;
	}


};





extern int CleanWidth, CleanHeight, CleanXfac, CleanYfac;
extern int CleanWidth_1, CleanHeight_1, CleanXfac_1, CleanYfac_1;
extern int DisplayWidth, DisplayHeight;

void V_UpdateModeSize (int width, int height);
void V_OutputResized (int width, int height);
void V_CalcCleanFacs (int designwidth, int designheight, int realwidth, int realheight, int *cleanx, int *cleany, int *cx1=NULL, int *cx2=NULL);

EXTERN_CVAR(Int, vid_rendermode)
EXTERN_CVAR(Bool, vid_fullscreen)
EXTERN_CVAR(Int, win_x)
EXTERN_CVAR(Int, win_y)
EXTERN_CVAR(Int, win_w)
EXTERN_CVAR(Int, win_h)
EXTERN_CVAR(Bool, win_maximized)


inline bool V_IsHardwareRenderer()
{
	return vid_rendermode == 4;
}

inline bool V_IsSoftwareRenderer()
{
	return vid_rendermode < 2;
}

inline bool V_IsPolyRenderer()
{
	return vid_rendermode == 2 || vid_rendermode == 3;
}

inline bool V_IsTrueColor()
{
	return vid_rendermode == 1 || vid_rendermode == 3 || vid_rendermode == 4;
}


class FTexture;
struct FColormap;
class FileWriter;
enum FTextureFormat : uint32_t;
class FModelRenderer;
struct SamplerUniform;

// TagItem definitions for DrawTexture. As far as I know, tag lists
// originated on the Amiga.
//
// Think of TagItems as an array of the following structure:
//
// struct TagItem {
//     uint32_t ti_Tag;
//     uint32_t ti_Data;
// };

#define TAG_DONE	(0)  /* Used to indicate the end of the Tag list */
#define TAG_END		(0)  /* Ditto									*/
						 /* list pointed to in ti_Data 				*/

#define TAG_USER	((uint32_t)(1u<<30))


class FFont;
struct FRemapTable;
class player_t;
typedef uint32_t angle_t;
struct RenderScene;


//
// VIDEO
//
//
class DCanvas
{
public:
	DCanvas (int width, int height, bool bgra);
	~DCanvas ();
	void Resize(int width, int height, bool optimizepitch = true);

	// Member variable access
	inline uint8_t *GetPixels () const { return Pixels.Data(); }
	inline int GetWidth () const { return Width; }
	inline int GetHeight () const { return Height; }
	inline int GetPitch () const { return Pitch; }
	inline bool IsBgra() const { return Bgra; }

protected:
	TArray<uint8_t> Pixels;
	int Width;
	int Height;
	int Pitch;
	bool Bgra;
};

class FUniquePalette;
class IHardwareTexture;
class FTexture;


class DFrameBuffer
{
protected:

	F2DDrawer m2DDrawer;
private:
	int Width = 0;
	int Height = 0;
	int BufferLock = 0;
protected:
	int clipleft = 0, cliptop = 0, clipwidth = -1, clipheight = -1;

public:
	// Hardware render state that needs to be exposed to the API independent part of the renderer. For ease of access this is stored in the base class.
	int hwcaps = 0;								// Capability flags
	float glslversion = 0;						// This is here so that the differences between old OpenGL and new OpenGL/Vulkan can be handled by platform independent code.
	int instack[2] = { 0,0 };					// this is globally maintained state for portal recursion avoidance.
	int stencilValue = 0;						// Global stencil test value
	unsigned int uniformblockalignment = 256;	// Hardware dependent uniform buffer alignment.
	unsigned int maxuniformblock = 65536;
	const char *vendorstring;					// We have to account for some issues with particular vendors.
	//FPortalSceneState *mPortalState;			// global portal state.
	//FSkyVertexBuffer *mSkyData = nullptr;		// the sky vertex buffer
	FFlatVertexBuffer *mVertexData = nullptr;	// Global vertex data
	//HWViewpointBuffer *mViewpoints = nullptr;	// Viewpoint render data.
	//FLightBuffer *mLights = nullptr;			// Dynamic lights
	//IShadowMap mShadowMap;

	IntRect mScreenViewport;
	IntRect mSceneViewport;
	IntRect mOutputLetterbox;
	float mSceneClearColor[4];


public:
	DFrameBuffer (int width=1, int height=1);
	virtual ~DFrameBuffer();
	virtual void InitializeState() = 0;	// For stuff that needs 'screen' set.
	virtual bool IsVulkan() { return false; }
	virtual bool IsPoly() { return false; }

	virtual DCanvas* GetCanvas() { return nullptr; }

	void SetSize(int width, int height);
	void SetVirtualSize(int width, int height)
	{
		Width = width;
		Height = height;
	}
	inline int GetWidth() const { return Width; }
	inline int GetHeight() const { return Height; }

	FVector2 SceneScale() const
	{
		return { mSceneViewport.width / (float)mScreenViewport.width, mSceneViewport.height / (float)mScreenViewport.height };
	}

	FVector2 SceneOffset() const
	{
		return { mSceneViewport.left / (float)mScreenViewport.width, mSceneViewport.top / (float)mScreenViewport.height };
	}

	// Make the surface visible.
	virtual void Update ();

	// Stores the palette with flash blended in into 256 dwords
	// Mark the palette as changed. It will be updated on the next Update().
	virtual void UpdatePalette() {}

	virtual void SetGamma() {}

	// Returns true if running fullscreen.
	virtual bool IsFullscreen () = 0;
	virtual void ToggleFullscreen(bool yes) {}

	// Changes the vsync setting, if supported by the device.
	virtual void SetVSync (bool vsync);

	// Delete any resources that need to be deleted after restarting with a different IWAD
	virtual void CleanForRestart() {}
	virtual void SetTextureFilterMode() {}
	virtual IHardwareTexture *CreateHardwareTexture() { return nullptr; }
	virtual void PrecacheMaterial(FMaterial *mat, int translation) {}
	virtual FModelRenderer *CreateModelRenderer(int mli) { return nullptr; }
	virtual void TextureFilterChanged() {}
	virtual void BeginFrame() {}
	virtual void SetWindowSize(int w, int h) {}
	virtual void StartPrecaching() {}

	virtual int GetClientWidth() = 0;
	virtual int GetClientHeight() = 0;
	virtual void BlurScene(float amount) {}
    
    // Interface to hardware rendering resources
	virtual IVertexBuffer *CreateVertexBuffer() { return nullptr; }
	virtual IIndexBuffer *CreateIndexBuffer() { return nullptr; }
	virtual IDataBuffer *CreateDataBuffer(int bindingpoint, bool ssbo, bool needsresize) { return nullptr; }
	bool BuffersArePersistent() { return !!(hwcaps & RFL_BUFFER_STORAGE); }

	// Begin/End 2D drawing operations.
	void Begin2D() { isIn2D = true; }
	void End2D() { isIn2D = false; }

	void BeginScene();
	void FinishScene();

	void End2DAndUpdate()
	{
		DrawRateStuff();
		End2D();
		Update();
	}


	// Returns true if Begin2D has been called and 2D drawing is now active
	bool HasBegun2D() { return isIn2D; }

	// This is overridable in case Vulkan does it differently.
	virtual bool RenderTextureIsFlipped() const
	{
		return true;
	}

	// Report a game restart
	virtual int Backend() { return 0; }
	virtual const char* DeviceName() const { return "Unknown"; }
	virtual void Draw2D() {}
	virtual void WriteSavePic(FileWriter *file, int width, int height);

	// Screen wiping
	virtual FTexture *WipeStartScreen();
	virtual FTexture *WipeEndScreen();

	virtual void PostProcessScene(int fixedcm, const std::function<void()> &afterBloomDrawEndScene2D) { if (afterBloomDrawEndScene2D) afterBloomDrawEndScene2D(); }

	void ScaleCoordsFromWindow(int16_t &x, int16_t &y);

	uint64_t GetLastFPS() const { return LastCount; }

	// 2D Texture drawing
	void ClearClipRect() { clipleft = cliptop = 0; clipwidth = clipheight = -1; }
	void SetClipRect(int x, int y, int w, int h);
	void GetClipRect(int *x, int *y, int *w, int *h);

	void VirtualToRealCoords(double &x, double &y, double &w, double &h, double vwidth, double vheight, bool vbottom = false, bool handleaspect = true) const;

	// Code that uses these (i.e. SBARINFO) should probably be evaluated for using doubles all around instead.
	void VirtualToRealCoordsInt(int &x, int &y, int &w, int &h, int vwidth, int vheight, bool vbottom = false, bool handleaspect = true) const;

	// Text drawing functions -----------------------------------------------


	// Calculate gamma table
	void CalcGamma(float gamma, uint8_t gammalookup[256]);

	virtual void SetViewportRects(IntRect *bounds);
	int ScreenToWindowX(int x);
	int ScreenToWindowY(int y);

	void FPSLimit();

	// Retrieves a buffer containing image data for a screenshot.
	// Hint: Pitch can be negative for upside-down images, in which case buffer
	// points to the last row in the buffer, which will be the first row output.
	virtual TArray<uint8_t> GetScreenshotBuffer(int &pitch, ESSType &color_type, float &gamma) { return TArray<uint8_t>(); }

	static float GetZNear() { return 5.f; }
	static float GetZFar() { return 65536.f; }

	// The original size of the framebuffer as selected in the video menu.
	uint64_t FrameTime = 0;

protected:
	void DrawRateStuff ();

private:
	uint64_t fpsLimitTime = 0;

	uint64_t LastMS = 0, LastSec = 0, FrameCount = 0, LastCount = 0, LastTic = 0;

	bool isIn2D = false;
};


// This is the screen updated by I_FinishUpdate.
extern DFrameBuffer *screen;

#define SCREENWIDTH (screen->GetWidth ())
#define SCREENHEIGHT (screen->GetHeight ())
#define SCREENPITCH (screen->GetPitch ())


// Allocates buffer screens, call before R_Init.
void V_InitScreenSize();
void V_InitScreen();

// Initializes graphics mode for the first time.
void V_Init2 ();

void V_Shutdown ();

class FScanner;
struct FScriptPosition;
// Returns the closest color to the one desired. String
// should be of the form "rr gg bb".
int V_GetColorFromString (const uint32_t *palette, const char *colorstring, FScriptPosition *sc = nullptr);
// Scans through the X11R6RGB lump for a matching color
// and returns a color string suitable for V_GetColorFromString.
FString V_GetColorStringByName (const char *name, FScriptPosition *sc = nullptr);

// Tries to get color by name, then by string
int V_GetColor (const uint32_t *palette, const char *str, FScriptPosition *sc = nullptr);
int V_GetColor(const uint32_t *palette, FScanner &sc);

int CheckRatio (int width, int height, int *trueratio=NULL);
static inline int CheckRatio (double width, double height) { return CheckRatio(int(width), int(height)); }
inline bool IsRatioWidescreen(int ratio) { return (ratio & 3) != 0; }

float ActiveRatio (int width, int height, float *trueratio = NULL);
static inline double ActiveRatio (double width, double height) { return ActiveRatio(int(width), int(height)); }

int AspectBaseWidth(float aspect);
int AspectBaseHeight(float aspect);
double AspectPspriteOffset(float aspect);
int AspectMultiplier(float aspect);
bool AspectTallerThanWide(float aspect);
void ScaleWithAspect(int &w, int &h, int Width, int Height);

int GetUIScale(int altval);
int GetConScale(int altval);

extern bool setsizeneeded, setmodeneeded;

EXTERN_CVAR(Int, uiscale);
EXTERN_CVAR(Int, con_scaletext);
EXTERN_CVAR(Int, con_scale);

inline int active_con_scaletext(bool newconfont = false)
{
	return newconfont? GetConScale(con_scaletext) : GetUIScale(con_scaletext);
}

inline int active_con_scale()
{
	return GetConScale(con_scale);
}


#endif // __V_VIDEO_H__
