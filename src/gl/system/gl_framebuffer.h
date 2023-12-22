#ifndef __GL_FRAMEBUFFER
#define __GL_FRAMEBUFFER

#ifdef _WIN32
#include "win32gliface.h"
#else
#include "sdlglvideo.h"
#endif

#include <memory>

class FHardwareTexture;
class FSimpleVertexBuffer;
class FGLDebug;

#ifdef _WIN32
class OpenGLFrameBuffer : public Win32GLFrameBuffer
{
	typedef Win32GLFrameBuffer Super;
#else
//#include "sdlglvideo.h"
class OpenGLFrameBuffer : public SDLGLFB
{
	typedef SDLGLFB Super;	//[C]commented, DECLARE_CLASS defines this in linux
#endif


public:

	explicit OpenGLFrameBuffer() {}
	OpenGLFrameBuffer(void *hMonitor, int width, int height, int bits, int refreshHz, bool fullscreen) ;
	~OpenGLFrameBuffer();

	void InitializeState();
	void Update();

	// Color correction
	bool SetGamma (float gamma);
	bool SetBrightness(float bright);
	bool SetContrast(float contrast);
	void DoSetGamma();

	void UpdatePalette();
	void GetFlashedPalette (PalEntry pal[256]);
	PalEntry *GetPalette ();
	bool SetFlash(PalEntry rgb, int amount);
	void GetFlash(PalEntry &rgb, int &amount);
	int GetPageCount();
	bool Begin2D(bool copy3d);
	void GameRestart();

	// Retrieves a buffer containing image data for a screenshot.
	// Hint: Pitch can be negative for upside-down images, in which case buffer
	// points to the last row in the buffer, which will be the first row output.
	virtual void GetScreenshotBuffer(const uint8_t *&buffer, int &pitch, ESSType &color_type, float &gamma) override;

	// Releases the screenshot buffer.
	virtual void ReleaseScreenshotBuffer();

	FNativePalette *CreatePalette(FRemapTable *remap);

	bool WipeStartScreen(int type);
	void WipeEndScreen();
	bool WipeDo(int ticks);
	void WipeCleanup();
	void Swap();
	bool IsHWGammaActive() const { return HWGammaActive; }

	void SetVSync(bool vsync);

	void ScaleCoordsFromWindow(int16_t &x, int16_t &y) override;
	void Draw2D() override;

	bool HWGammaActive = false;			// Are we using hardware or software gamma?
	std::shared_ptr<FGLDebug> mDebug;	// Debug API
private:
	PalEntry Flash;						// Only needed to support some cruft in the interface that only makes sense for the software renderer
	PalEntry SourcePalette[256];		// This is where unpaletted textures get their palette from
	uint8_t *ScreenshotBuffer;			// What the name says. This must be maintained because the software renderer can return a locked canvas surface which the caller cannot release.

	class Wiper
	{

	protected:
		FSimpleVertexBuffer *mVertexBuf;

		void MakeVBO(OpenGLFrameBuffer *fb);

	public:
		Wiper();
		virtual ~Wiper();
		virtual bool Run(int ticks, OpenGLFrameBuffer *fb) = 0;
	};

	class Wiper_Melt;			friend class Wiper_Melt;
	class Wiper_Burn;			friend class Wiper_Burn;
	class Wiper_Crossfade;		friend class Wiper_Crossfade;

	Wiper *ScreenWipe;
	FHardwareTexture *wipestartscreen;
	FHardwareTexture *wipeendscreen;


public:
};


#endif //__GL_FRAMEBUFFER
