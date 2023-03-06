#ifndef VIDEO_MORPHOS_TINYGL_H
#define VIDEO_MORPHOS_TINYGL_H

#include "video_morphos.h"

#include <GL/gl.h>

struct Coordinate2
{
    float X;
    float Y;
};

class VideoMorphOSTinyGL : public VideoMorphOS
{
public:
    VideoMorphOSTinyGL(int width, int height);
    virtual ~VideoMorphOSTinyGL();

    virtual bool Initialise();
    virtual void Recalculate_Window();
    virtual void RefreshMouseCursor();
    virtual void Render_Frame(const void* displaybuffer);
    virtual void Set_Palette(void* newpaletteptr);
    virtual void Shutdown();

private:
    void* ConvertImageToTinyGL(const void* data, unsigned int width, unsigned int height);
    void* ConvertTransparentImageToTinyGL(const void* data, unsigned int width, unsigned int height);

    unsigned short palette_tinygl_16[256];
    unsigned int palette_tinygl_32[256];
    GLuint texture_id_black;
    GLuint texture_id_display;
    GLuint texture_id_mouse_cursor;
    struct Library* TinyGLBase;
    GLContext* __tglContext;
    bool use_15bpp;
    struct Coordinate2 vertex_coords_display[4];
    struct Coordinate2 vertex_coords_left_bar[4];
    struct Coordinate2 vertex_coords_right_bar[4];
};

#endif /* VIDEO_MORPHOS_TINYGL_H */
