#undef _NO_PPCINLINE

#include <intuition/pointerclass.h>

#include <proto/cybergraphics.h>
#include <proto/exec.h>
#include <proto/intuition.h>
#include <proto/keymap.h>

#include <string.h>
#include <math.h>

#include "settings.h"
#include "video.h"
#include "video_morphos.h"
#include "video_morphos_cgx.h"
#include "video_morphos_tinygl.h"

extern "C" {
void dprintf(const char*, ...);
}

#if 0
#define DEBUG(x, y...) dprintf("Video: " x, ##y)
#else
#define DEBUG(x, y...)                                                                                                 \
    do {                                                                                                               \
    } while (0)
#endif

VideoMorphOS* videomorphos;

static int screen_width;
static int screen_height;
static void* visible_membuf;
static bool use_tinygl = 1;

/****************************************
 ****************************************
 ****************************************/

VideoMorphOS::VideoMorphOS(int width, int height)
{
    display_width = 0;
    display_height = 0;
    game_width = width;
    game_height = height;
    mouse_cursor_width = 0;
    mouse_cursor_height = 0;
    mouse_cursor_hotspot_x = 0;
    mouse_cursor_hotspot_y = 0;
    mouse_cursor_pixels = 0;
    scaled_width = 0;
    scaled_height = 0;
    scaling_enabled = 0;
    screen = 0;
    morphos_window_xoffset = 0;
    morphos_window_yoffset = 0;
    morphos_window_xscale = 0;
    morphos_window_yscale = 0;
    morphos_window = 0;

    use_fullscreen = !Settings.Video.Windowed;
}

VideoMorphOS::~VideoMorphOS()
{
    FreeVec(mouse_cursor_pixels);
}

void VideoMorphOS::Clear_Display()
{
    FillPixelArray(
        morphos_window->RPort, morphos_window->BorderLeft, morphos_window->BorderTop, screen_width, screen_height, 0);
}

void VideoMorphOS::Get_Video_Mouse_Raw(int& x, int& y)
{
    x = morphos_window->MouseX - morphos_window->BorderLeft;
    y = morphos_window->MouseY - morphos_window->BorderTop;
}

struct Window* VideoMorphOS::Get_Window()
{
    return morphos_window;
}

void VideoMorphOS::Hide_Mouse_Cursor()
{
    SetWindowPointer(videomorphos->Get_Window(), WA_PointerType, POINTERTYPE_INVISIBLE, TAG_DONE);
}

void VideoMorphOS::Recalculate_Window()
{
    if (morphos_window) {
        display_width = morphos_window->Width - morphos_window->BorderLeft - morphos_window->BorderRight;
        display_height = morphos_window->Height - morphos_window->BorderTop - morphos_window->BorderBottom;

        if (scaling_enabled) {
            if (display_width * 3 / 4 > display_height) {
                scaled_width = round((double)display_height * 4 / 3);
                scaled_height = display_height;
            } else {
                scaled_width = display_width;
                scaled_height = round((double)display_width * 3 / 4);
            }
        } else {
            scaled_width = game_width;
            scaled_height = game_height;
        }

        morphos_window_xscale = (double)scaled_width / game_width;
        morphos_window_yscale = (double)scaled_height / game_height;

        morphos_window_xoffset = (display_width - scaled_width) / 2;
        morphos_window_yoffset = (display_height - scaled_height) / 2;
    }

    if (screen) {
        screen_width = screen->Width;
        screen_height = screen->Height;
    } else {
        screen_width = display_width;
        screen_height = display_height;
    }

    DEBUG("Game: %d x %d\n", game_width, game_height);
    DEBUG("Screen: %d x %d\n", screen_width, screen_height);
    DEBUG("Display: %d x %d\n", display_width, display_height);
    DEBUG("Scaled: %d x %d\n", scaled_width, scaled_height);
    DEBUG("Offset: %d x %d\n", morphos_window_xoffset, morphos_window_yoffset);
}

void VideoMorphOS::Refresh_Settings()
{
    DEBUG("Applying new display settings\n");

    Shutdown();

    use_fullscreen = !Settings.Video.Windowed;

    Initialise();
    Recalculate_Window();
    RefreshMouseCursor();

    if (visible_membuf) {
        videomorphos->Render_Frame(visible_membuf);
    }
}

void VideoMorphOS::Set_Cursor(const void* cursor, int width, int height, int hotx, int hoty)
{
    if (!mouse_cursor_pixels || width != mouse_cursor_width || height != mouse_cursor_height) {
        FreeVec(mouse_cursor_pixels);
        mouse_cursor_pixels = AllocVec(width * height, MEMF_ANY);
    }
    if (mouse_cursor_pixels) {
        CopyMem((void*)cursor, mouse_cursor_pixels, width * height);
        mouse_cursor_width = width;
        mouse_cursor_height = height;
        mouse_cursor_hotspot_x = hotx;
        mouse_cursor_hotspot_y = hoty;
    }
}

bool VideoMorphOS::Translate_Mouse_Coordinates(int* x, int* y)
{
    int newx, newy;
    bool ret;

    ret = true;

    newx = *x;
    newy = *y;

    newx -= (morphos_window->BorderLeft + morphos_window_xoffset);
    newy -= (morphos_window->BorderTop + morphos_window_yoffset);

    newx /= morphos_window_xscale;
    newy /= morphos_window_yscale;

    if (newx < 0) {
        newx = 0;
        ret = false;
    }

    if (newy < 0) {
        newy = 0;
        ret = false;
    }

    if (newx >= game_width) {
        newx = game_width - 1;
        ret = false;
    }

    if (newy >= game_height) {
        newy = game_height - 1;
        ret = false;
    }

    *x = newx;
    *y = newy;

    return ret;
}

/****************************************
 ****************************************
 ****************************************/

class SurfaceMonitorClassMorphOS : public SurfaceMonitorClass
{

public:
    SurfaceMonitorClassMorphOS()
    {
    }

    virtual void Restore_Surfaces()
    {
    }

    virtual void Set_Surface_Focus(bool in_focus)
    {
    }

    virtual void Release()
    {
    }
};

SurfaceMonitorClassMorphOS AllSurfacesMorphOS;
SurfaceMonitorClass& AllSurfaces = AllSurfacesMorphOS;

SurfaceMonitorClass::SurfaceMonitorClass()
{
    SurfacesRestored = false;
}

/****************************************
 ****************************************
 ****************************************/

class VideoSurfaceMorphOS : public VideoSurface
{
public:
    VideoSurfaceMorphOS(int w, int h, GBC_Enum flags);
    virtual ~VideoSurfaceMorphOS();

    virtual void* GetData() const;
    virtual int GetPitch() const;
    virtual bool IsAllocated() const;
    virtual void AddAttachedSurface(VideoSurface* surface);
    virtual bool IsReadyToBlit();
    virtual bool LockWait();
    virtual bool Unlock();
    virtual void Blt(const Rect& destRect, VideoSurface* src, const Rect& srcRect, bool mask);
    virtual void FillRect(const Rect& rect, unsigned char color);

private:
    unsigned int width;
    unsigned int height;
    void* membuf;
};

VideoSurfaceMorphOS::VideoSurfaceMorphOS(int w, int h, GBC_Enum flags)
{
    width = w;
    height = h;

#warning Probably too much memory;
    membuf = AllocVec(w * h * 4, MEMF_ANY | MEMF_CLEAR);
    if (membuf) {
        if ((flags & GBC_VISIBLE)) {
            visible_membuf = membuf;
        }
    }
}

VideoSurfaceMorphOS::~VideoSurfaceMorphOS()
{
    if (membuf) {
        if (visible_membuf == membuf) {
            visible_membuf = 0;
        }

        FreeVec(membuf);
    }
}

void* VideoSurfaceMorphOS::GetData() const
{
    return membuf;
}

int VideoSurfaceMorphOS::GetPitch() const
{
    return width;
}

bool VideoSurfaceMorphOS::IsAllocated() const
{
    return false;
}

void VideoSurfaceMorphOS::AddAttachedSurface(VideoSurface* surface)
{
}

bool VideoSurfaceMorphOS::IsReadyToBlit()
{
    return true;
}

bool VideoSurfaceMorphOS::LockWait()
{
    return true;
}

bool VideoSurfaceMorphOS::Unlock()
{
    return true;
}

void VideoSurfaceMorphOS::Blt(const Rect& destRect, VideoSurface* src, const Rect& srcRect, bool mask)
{
    const unsigned char* srcmem;
    unsigned char* dstmem;
    int i;

    srcmem = (const unsigned char*)((VideoSurfaceMorphOS*)src)->membuf;
    srcmem += srcRect.X + srcRect.Y * ((VideoSurfaceMorphOS*)src)->width;

    dstmem = (unsigned char*)membuf;
    dstmem += destRect.X + destRect.Y * width;

    if (srcRect.Y > destRect.Y || srcRect.X >= destRect.X + destRect.Width || destRect.X >= srcRect.X + srcRect.Width) {
        for (i = 0; i < destRect.Height; i++) {
            memcpy(dstmem, srcmem, destRect.Width);
            srcmem += width;
            dstmem += width;
        }
    } else if (srcRect.Y < destRect.Y) {
        srcmem += width * destRect.Height;
        dstmem += width * destRect.Height;
        for (i = 0; i < destRect.Height; i++) {
            srcmem -= width;
            dstmem -= width;
            memcpy(dstmem, srcmem, destRect.Width);
        }
    } else {
        for (i = 0; i < destRect.Height; i++) {
            memmove(dstmem, srcmem, destRect.Width);
            srcmem += width;
            dstmem += width;
        }
    }
}

void VideoSurfaceMorphOS::FillRect(const Rect& rect, unsigned char color)
{
    unsigned int fillwidth;
    unsigned int fillheight;
    unsigned char* dstmem;
    int i;

    if (rect.X >= width || rect.Y >= width) {
        DEBUG("Out of bounds FillRect (x %d, y %d, width %d, height %d)\n", rect.X, rect.Y, rect.Width, rect.Height);
        return;
    }

    fillwidth = rect.Width;
    fillheight = rect.Height;

    if (rect.X + fillwidth > width) {
        DEBUG("Out of bounds FillRect (x %d, y %d, width %d, height %d)\n", rect.X, rect.Y, rect.Width, rect.Height);
        fillwidth = width - rect.X;
    }

    if (rect.Y + fillheight > height) {
        DEBUG("Out of bounds FillRect (x %d, y %d, width %d, height %d)\n", rect.X, rect.Y, rect.Width, rect.Height);
        fillheight = height - rect.Y;
    }

    dstmem = (unsigned char*)membuf;
    dstmem += rect.X + rect.Y * width;

    for (i = 0; i < rect.Height; i++) {
        memset(dstmem, color, rect.Width);
        dstmem += width;
    }
}

/****************************************
 ****************************************
 ****************************************/

Video::Video()
{
}

Video::~Video()
{
}

Video& Video::Shared()
{
    static Video video;
    return video;
}

VideoSurface* Video::CreateSurface(int w, int h, GBC_Enum flags)
{
    return new VideoSurfaceMorphOS(w, h, flags);
}

/****************************************
 ****************************************
 ****************************************/

void Get_Video_Mouse(int& x, int& y)
{
    struct Window* window;

    window = videomorphos->Get_Window();

    x = window->MouseX;
    y = window->MouseY;

    videomorphos->Translate_Mouse_Coordinates(&x, &y);
}

unsigned int Get_Free_Video_Memory(void)
{
    return 1000000000;
}

unsigned Get_Video_Hardware_Capabilities(void)
{
    return 0;
    return VIDEO_BLITTER;
}

void Refresh_Video_Mode()
{
    if (videomorphos) {
        videomorphos->Refresh_Settings();
    }
}

static void Reopen_Display(int width, int height)
{
    DEBUG("%s: width %d, height %d\n", __func__, width, height);

    if (videomorphos) {
        DEBUG("Shutting down existing display\n");
        videomorphos->Shutdown();
        delete videomorphos;
        videomorphos = 0;
    }

    if (use_tinygl) {
        DEBUG("Attempting to create TinyGL display\n");
        videomorphos = new VideoMorphOSTinyGL(width, height);
        if (videomorphos->Initialise()) {
            DEBUG("%s:%d/%s(): Using TinyGL\n");
        } else {
            delete videomorphos;
            videomorphos = 0;
        }

        if (!videomorphos) {
            DEBUG("Failed to initialise TinyGL\n");
        }
    }

    if (!videomorphos) {
        videomorphos = new VideoMorphOSCGX(width, height);
        if (!videomorphos->Initialise()) {
            delete videomorphos;
            videomorphos = 0;
        }
    }

    if (videomorphos) {
        videomorphos->Recalculate_Window();
        videomorphos->Clear_Display();
    }
}

void Reset_Video_Mode(void)
{
    if (videomorphos) {
        videomorphos->Shutdown();
        delete videomorphos;
        videomorphos = 0;
    }
}

void Set_DD_Palette(void* newpaletteptr)
{
    videomorphos->Set_Palette(newpaletteptr);
    videomorphos->RefreshMouseCursor();
}

void Set_Video_Cursor(void* cursor, int w, int h, int hotx, int hoty)
{
    videomorphos->Set_Cursor(cursor, w, h, hotx, hoty);
    videomorphos->RefreshMouseCursor();
}

void Set_Video_Cursor_Clip(bool clipped)
{
}

bool Set_Video_Mode(int w, int h, int bits_per_pixel)
{
    DEBUG("%s: w %d, h %d, bits_per_pixel 0x%08x\n", __func__);

    Reopen_Display(w, h);

    return true;
}

void Video_Render_Frame(void)
{
    if (visible_membuf) {
        videomorphos->Render_Frame(visible_membuf);
    }
}

void Wait_Blit(void)
{
}

void Wait_Vert_Blank(void)
{
}
