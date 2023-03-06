#undef _NO_PPCINLINE

#include <proto/exec.h>
#include <proto/intuition.h>

#include <GL/gl.h>
#include <tgl/gla.h>

#include "settings.h"
#include "video_morphos_tinygl.h"
#include "wwmouse.h"

extern "C" {
void dprintf(const char*, ...);
}

#if 0
#define DEBUG(x, y...) dprintf("TinyGL: " x, ##y)
#else
#define DEBUG(x, y...)                                                                                                 \
    do {                                                                                                               \
    } while (0)
#endif

static const float gltexcoords[2 * 4] = {0, 1, 0, 0, 1, 1, 1, 0};

VideoMorphOSTinyGL::VideoMorphOSTinyGL(int width, int height)
    : VideoMorphOS(width, height)
{
    memset(palette_tinygl_16, 0, sizeof(palette_tinygl_16));
    memset(palette_tinygl_32, 0, sizeof(palette_tinygl_32));
    texture_id_black = 0;
    texture_id_display = 0;
    texture_id_mouse_cursor = 0;
    TinyGLBase = 0;
    __tglContext = 0;
    memset(vertex_coords_display, 0, sizeof(vertex_coords_display));
    memset(vertex_coords_left_bar, 0, sizeof(vertex_coords_left_bar));
    memset(vertex_coords_right_bar, 0, sizeof(vertex_coords_right_bar));

    use_15bpp = !Settings.Video.PixelDepth24BPP;
}

VideoMorphOSTinyGL::~VideoMorphOSTinyGL()
{
}

bool VideoMorphOSTinyGL::Initialise()
{
    static const unsigned int blackpixel = 0;
    unsigned int screenwidth;
    unsigned int screenheight;
    bool ret;

    DEBUG("%s:\n", __func__);

    ret = false;

    TinyGLBase = OpenLibrary("tinygl.library", 0);
    if (!TinyGLBase) {
        DEBUG("Unable to open tinygl.library\n");
    } else {
        if (use_fullscreen) {
            screen = OpenScreenTags(
                0, SA_LikeWorkbench, TRUE, SA_Depth, use_15bpp ? 15 : 24, SA_Quiet, TRUE, SA_AdaptSize, TRUE, TAG_DONE);
        } else {
            screen = 0;
        }

        if (screen) {
            DEBUG("Opened screen\n");
            screenwidth = screen->Width;
            screenheight = screen->Height;
        } else {
            screenwidth = game_width;
            screenheight = game_height * 12 / 10;
        }

        morphos_window = OpenWindowTags(0,
                                        screen ? TAG_IGNORE : WA_Title,
                                        "Vanilla Conquer",
                                        WA_IDCMP,
                                        IDCMP_MOUSEBUTTONS | IDCMP_RAWKEY,
                                        WA_InnerWidth,
                                        screenwidth,
                                        WA_InnerHeight,
                                        screenheight,
                                        WA_DragBar,
                                        screen ? FALSE : TRUE,
                                        WA_DepthGadget,
                                        screen ? FALSE : TRUE,
                                        WA_Borderless,
                                        screen ? TRUE : FALSE,
                                        WA_RMBTrap,
                                        TRUE,
                                        screen ? WA_PubScreen : TAG_IGNORE,
                                        (ULONG)screen,
                                        WA_Activate,
                                        TRUE,
                                        WA_ReportMouse,
                                        TRUE,
                                        TAG_DONE);

        if (morphos_window) {
            DEBUG("Opened window\n");
            __tglContext = GLInit();
            if (__tglContext) {
                struct TagItem tinygltags[] = {
                    {0, 0},
                    {TGL_CONTEXT_NODEPTH, TRUE},
                    {TAG_DONE},
                };

                if (screen) {
                    DEBUG("Screen context\n");
                    tinygltags[0].ti_Tag = TGL_CONTEXT_SCREEN;
                    tinygltags[0].ti_Data = (ULONG)screen;
                } else {
                    DEBUG("Window context\n");
                    tinygltags[0].ti_Tag = TGL_CONTEXT_WINDOW;
                    tinygltags[0].ti_Data = (ULONG)morphos_window;
                }

                if (GLAInitializeContext(__tglContext, tinygltags)) {
                    DEBUG("Created TinyGL context\n");

                    glMatrixMode(GL_MODELVIEW);
                    glLoadIdentity();
                    glMatrixMode(GL_PROJECTION);
                    glLoadIdentity();
                    glGenTextures(1, &texture_id_display);
                    glGenTextures(1, &texture_id_mouse_cursor);
                    glGenTextures(1, &texture_id_black);

                    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

                    glBindTexture(GL_TEXTURE_2D, texture_id_mouse_cursor);
                    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
                    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
                    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
                    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

                    glBindTexture(GL_TEXTURE_2D, texture_id_display);
                    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
                    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
                    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
                    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

                    glBindTexture(GL_TEXTURE_2D, texture_id_black);
                    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
                    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
                    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
                    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
                    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, &blackpixel);

                    glEnable(GL_TEXTURE_2D);
                    glVertexPointer(2, GL_FLOAT, 0, vertex_coords_display);
                    glTexCoordPointer(2, GL_FLOAT, 0, gltexcoords);
                    glEnableClientState(GL_VERTEX_ARRAY);
                    glEnableClientState(GL_TEXTURE_COORD_ARRAY);
                    glClearColor(0, 0, 0, 0);
                    glClear(GL_COLOR_BUFFER_BIT);

                    scaling_enabled = true;

                    DEBUG("Setting mouse pointer\n");

                    Hide_Mouse_Cursor();

                    DEBUG("TinyGL display opened\n");

                    ret = true;
                }

                if (!ret) {
                    GLClose(__tglContext);
                    __tglContext = 0;
                }
            }

            if (!ret) {
                CloseWindow(morphos_window);
                morphos_window = 0;
            }
        }

        if (!ret) {
            if (screen) {
                CloseScreen(screen);
                screen = 0;
            }
        }

        if (!ret) {
            CloseLibrary(TinyGLBase);
            TinyGLBase = 0;
        }
    }

    return ret;
}

void* VideoMorphOSTinyGL::ConvertImageToTinyGL(const void* data, unsigned int width, unsigned int height)
{
    void* convertedpixels;
    unsigned int pixelcount;

    pixelcount = game_width * game_height;

    if (use_15bpp) {
        convertedpixels = AllocVec(pixelcount * 2, MEMF_ANY);
        if (convertedpixels) {
            const unsigned char* src;
            unsigned short* dst;
            unsigned int i;

            src = (const unsigned char*)data;
            dst = (unsigned short*)convertedpixels;

            for (i = 0; i < pixelcount; i++) {
                dst[i] = palette_tinygl_16[src[i]];
            }
        }
    } else {
        convertedpixels = AllocVec(pixelcount * 4, MEMF_ANY);
        if (convertedpixels) {
            const unsigned char* src;
            unsigned int* dst;
            unsigned int i;

            src = (const unsigned char*)data;
            dst = (unsigned int*)convertedpixels;

            for (i = 0; i < pixelcount; i++) {
                dst[i] = palette_tinygl_32[src[i]];
            }
        }
    }

    return convertedpixels;
}

void* VideoMorphOSTinyGL::ConvertTransparentImageToTinyGL(const void* data, unsigned int width, unsigned int height)
{
    void* convertedpixels;
    unsigned int pixelcount;

    pixelcount = width * height;

    if (use_15bpp) {
        convertedpixels = AllocVec(pixelcount * 2, MEMF_ANY);
        if (convertedpixels) {
            const unsigned char* src;
            unsigned char pixel;
            unsigned short* dst;
            unsigned int i;

            src = (const unsigned char*)data;
            dst = (unsigned short*)convertedpixels;

            for (i = 0; i < pixelcount; i++) {
                pixel = src[i];
                if (pixel) {
                    dst[i] = palette_tinygl_16[pixel];
                } else {
                    dst[i] = 0;
                }
            }
        }
    } else {
        convertedpixels = AllocVec(pixelcount * 4, MEMF_ANY);
        if (convertedpixels) {
            const unsigned char* src;
            unsigned char pixel;
            unsigned int* dst;
            unsigned int i;

            src = (const unsigned char*)data;
            dst = (unsigned int*)convertedpixels;

            for (i = 0; i < pixelcount; i++) {
                pixel = src[i];
                if (pixel) {
                    dst[i] = palette_tinygl_32[pixel];
                } else {
                    dst[i] = 0;
                }
            }
        }
    }

    return convertedpixels;
}

void VideoMorphOSTinyGL::Recalculate_Window()
{
    float gl_scale_x;
    float gl_scale_y;

    VideoMorphOS::Recalculate_Window();

    gl_scale_x = (double)scaled_width / display_width;
    gl_scale_y = (double)scaled_height / display_height;

    dprintf("%s:%d/%s(): gl_scale_x %f, gl_scale_y %f\n", __FILE__, __LINE__, __func__, gl_scale_x, gl_scale_y);

    vertex_coords_display[0].X = -gl_scale_x;
    vertex_coords_display[0].Y = -gl_scale_y;
    vertex_coords_display[1].X = -gl_scale_x;
    vertex_coords_display[1].Y = gl_scale_y;
    vertex_coords_display[2].X = gl_scale_x;
    vertex_coords_display[2].Y = -gl_scale_y;
    vertex_coords_display[3].X = gl_scale_x;
    vertex_coords_display[3].Y = gl_scale_y;

    vertex_coords_left_bar[0].X = -1;
    vertex_coords_left_bar[0].Y = -1;
    vertex_coords_left_bar[1].X = -1;
    vertex_coords_left_bar[1].Y = 1;
    vertex_coords_left_bar[2].X = -gl_scale_x;
    vertex_coords_left_bar[2].Y = -1;
    vertex_coords_left_bar[3].X = -gl_scale_x;
    vertex_coords_left_bar[3].Y = 1;

    vertex_coords_right_bar[0].X = gl_scale_x;
    vertex_coords_right_bar[0].Y = -1;
    vertex_coords_right_bar[1].X = gl_scale_x;
    vertex_coords_right_bar[1].Y = 1;
    vertex_coords_right_bar[2].X = 1;
    vertex_coords_right_bar[2].Y = -1;
    vertex_coords_right_bar[3].X = 1;
    vertex_coords_right_bar[3].Y = 1;
}

void VideoMorphOSTinyGL::RefreshMouseCursor(void)
{
    void* convertedpixels;

    if (mouse_cursor_pixels) {
        convertedpixels = ConvertTransparentImageToTinyGL(mouse_cursor_pixels, mouse_cursor_width, mouse_cursor_height);
        if (convertedpixels) {
            glBindTexture(GL_TEXTURE_2D, texture_id_mouse_cursor);
            glTexImage2D(GL_TEXTURE_2D,
                         0,
                         use_15bpp ? GL_RGB5_A1 : GL_RGBA8,
                         mouse_cursor_width,
                         mouse_cursor_height,
                         0,
                         GL_RGBA,
                         use_15bpp ? GL_UNSIGNED_SHORT_5_5_5_1 : GL_UNSIGNED_BYTE,
                         convertedpixels);

            FreeVec(convertedpixels);
        }
    }
}

void VideoMorphOSTinyGL::Render_Frame(const void* displaybuffer)
{
    struct Coordinate2 mouse_cursor_gl_coords[4];
    int mouse_x;
    int mouse_y;
    float mouse_gl_width;
    float mouse_gl_height;
    float mouse_gl_position_x;
    float mouse_gl_position_y;
    void* convertedpixels;

    Get_Video_Mouse_Raw(mouse_x, mouse_y);

    convertedpixels = ConvertImageToTinyGL(displaybuffer, game_width, game_height);
    if (convertedpixels) {
        glDisable(GL_BLEND);

        /* Erase the area around where the display is drawn, as the mouse cursor might previously have been drawing there and left a trail. */
        glBindTexture(GL_TEXTURE_2D, texture_id_black);

        glVertexPointer(2, GL_FLOAT, 0, vertex_coords_left_bar);
        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

        glVertexPointer(2, GL_FLOAT, 0, vertex_coords_right_bar);
        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

        glBindTexture(GL_TEXTURE_2D, texture_id_display);
        glTexImage2D(GL_TEXTURE_2D,
                     0,
                     use_15bpp ? GL_RGB5_A1 : GL_RGBA8,
                     game_width,
                     game_height,
                     0,
                     GL_RGBA,
                     use_15bpp ? GL_UNSIGNED_SHORT_5_5_5_1 : GL_UNSIGNED_BYTE,
                     convertedpixels);

        FreeVec(convertedpixels);

        glVertexPointer(2, GL_FLOAT, 0, vertex_coords_display);
        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

        if (!Get_Mouse_State()) {

            glBindTexture(GL_TEXTURE_2D, texture_id_mouse_cursor);
            mouse_gl_position_x =
                (2.0 / display_width * (mouse_x - (mouse_cursor_hotspot_x * morphos_window_xscale))) - 1;
            mouse_gl_position_y =
                1 - (2.0 / display_height * (mouse_y - (mouse_cursor_hotspot_y * morphos_window_yscale)));
            mouse_gl_width = (2.0 / display_width) * mouse_cursor_width * morphos_window_xscale;
            mouse_gl_height = (2.0 / display_height) * mouse_cursor_height * morphos_window_yscale;

            mouse_cursor_gl_coords[0].X = mouse_gl_position_x;
            mouse_cursor_gl_coords[0].Y = mouse_gl_position_y - mouse_gl_height;
            mouse_cursor_gl_coords[1].X = mouse_gl_position_x;
            mouse_cursor_gl_coords[1].Y = mouse_gl_position_y;
            mouse_cursor_gl_coords[2].X = mouse_gl_position_x + mouse_gl_width;
            mouse_cursor_gl_coords[2].Y = mouse_gl_position_y - mouse_gl_height;
            mouse_cursor_gl_coords[3].X = mouse_gl_position_x + mouse_gl_width;
            mouse_cursor_gl_coords[3].Y = mouse_gl_position_y;

            glEnable(GL_BLEND);
            glVertexPointer(2, GL_FLOAT, 0, mouse_cursor_gl_coords);
            glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
        }

        GLASwapBuffers(__tglContext);
    }
}

void VideoMorphOSTinyGL::Set_Palette(void* newpaletteptr)
{
    unsigned char* newpalette;
    unsigned int i;

    newpalette = (unsigned char*)newpaletteptr;

    if (use_15bpp) {
        for (i = 0; i < 256; i++) {
            palette_tinygl_16[i] = ((newpalette[i * 3 + 0] & 0x3e) << 10) | ((newpalette[i * 3 + 1] & 0x3e) << 5)
                                   | ((newpalette[i * 3 + 2] & 0x3d) << 0) | 1;
        }
    } else {
        for (i = 0; i < 256; i++) {
            palette_tinygl_32[i] =
                (newpalette[i * 3 + 0] << 26) | (newpalette[i * 3 + 1] << 18) | (newpalette[i * 3 + 2] << 10) | 0xff;
        }
    }
}

void VideoMorphOSTinyGL::Shutdown()
{
    if (__tglContext) {
        GLADestroyContext(__tglContext);
        GLClose(__tglContext);
        __tglContext = 0;
    }

    if (morphos_window) {
        CloseWindow(morphos_window);
        morphos_window = 0;
    }

    if (screen) {
        CloseScreen(screen);
        screen = 0;
    }

    if (TinyGLBase) {
        CloseLibrary(TinyGLBase);
        TinyGLBase = 0;
    }
}
