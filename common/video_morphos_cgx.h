#ifndef VIDEO_MORPHOS_CGX_H
#define VIDEO_MORPHOS_CGX_H

#include <exec/types.h>
#include <graphics/rastport.h>

#include "video_morphos.h"

class VideoMorphOSCGX : public VideoMorphOS
{
public:
    VideoMorphOSCGX(int width, int height);
    virtual ~VideoMorphOSCGX();

    virtual bool Initialise();
    virtual void RefreshMouseCursor();
    virtual void Render_Frame(const void* displaybuffer);
    virtual void Set_Palette(void* newpaletteptr);
    virtual void Shutdown();

private:
    void Mouse_Cursor_To_ARGB8888();

    unsigned int current_screen_buffer;
    struct BitMap* mouse_cursor_bitmap;
    APTR mouse_cursor_object;
    struct RastPort mouse_cursor_rastport;
    bool mouse_cursor_visible;

    unsigned int palette_cursor[256];
    unsigned char palette_cgx[256 * 4];
    struct MsgPort* port_screen_buffer_display;
    struct MsgPort* port_screen_buffer_write;
    struct RastPort rastport;
    struct ScreenBuffer* screen_buffers[3];
    unsigned int screen_buffers_swappable;
    unsigned int screen_buffers_writable;
    ULONG screen_depth;
    ULONG screen_palette[1 + (256 * 3) + 1];
};

#endif /* VIDEO_MORPHOS_CGX_H */
