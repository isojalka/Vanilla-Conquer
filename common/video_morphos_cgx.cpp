#undef _NO_PPCINLINE

#include <cybergraphx/cybergraphics.h>
#include <intuition/pointerclass.h>

#include <proto/cybergraphics.h>
#include <proto/exec.h>
#include <proto/graphics.h>
#include <proto/intuition.h>

#include <string.h>

#include "video_morphos_cgx.h"
#include "wwmouse.h"

#define ENABLE_DEBUG 0

#if ENABLE_DEBUG
extern "C" {
void dprintf(const char*, ...);
}

#define DEBUG(x, y...) dprintf("CGX: " x, ##y)
#else
#define DEBUG(x, y...)                                                                                                 \
    do {                                                                                                               \
    } while (0)
#endif

VideoMorphOSCGX::VideoMorphOSCGX(int width, int height)
    : VideoMorphOS(width, height)
{
    current_screen_buffer = 0;
    mouse_cursor_bitmap = 0;
    mouse_cursor_visible = false;
    memset(palette_cgx, 0, sizeof(palette_cgx));
    port_screen_buffer_display = 0;
    port_screen_buffer_write = 0;
    screen_buffers[0] = 0;
    screen_buffers[1] = 0;
    screen_buffers[2] = 0;
    screen_buffers_swappable = 0;
    screen_buffers_writable = 0;
    screen_depth = 0;
    screen_palette[0] = 256 << 16;
    screen_palette[1 + (3 * 256)] = 0;
}

VideoMorphOSCGX::~VideoMorphOSCGX()
{
}

bool VideoMorphOSCGX::Initialise()
{
    unsigned int screendepth;
    unsigned int screenwidth;
    unsigned int screenheight;
    APTR handle;
    struct DimensionInfo diminfo;
    ULONG modeid;
    unsigned int i;
    bool ret;

    ret = false;

    screendepth = 8;
    screenwidth = game_width;
    screenheight = game_height;

    if (use_fullscreen) {
        modeid = BestCModeIDTags(CYBRBIDTG_Depth,
                                 screendepth,
                                 CYBRBIDTG_NominalWidth,
                                 screenwidth,
                                 CYBRBIDTG_NominalHeight,
                                 screenheight,
                                 TAG_DONE);
        if (modeid != INVALID_ID) {
            DEBUG("Got mode ID 0x%08x for %dx%d\n", modeid, screenwidth, screenheight);
        } else {
            modeid = BestCModeIDTags(CYBRBIDTG_Depth,
                                     screendepth,
                                     CYBRBIDTG_NominalWidth,
                                     screenwidth,
                                     CYBRBIDTG_NominalHeight,
                                     screenwidth * 3 / 4,
                                     TAG_DONE);
            if (modeid != INVALID_ID) {
                DEBUG("Got mode ID 0x%08x for 4:3-adjusted %dx%d\n", modeid, screenwidth, screenwidth * 3 / 4);
            } else {
                DEBUG("Failed to get mode ID for %dx%d\n", screenwidth, screenheight);
            }
        }

        if (modeid != INVALID_ID) {
            if ((handle = FindDisplayInfo(modeid))
                && GetDisplayInfoData(handle, &diminfo, sizeof(diminfo), DTAG_DIMS, 0)) {
                screenwidth = (diminfo.StdOScan.MaxX - diminfo.StdOScan.MinX) + 1;
                screenheight = (diminfo.StdOScan.MaxY - diminfo.StdOScan.MinY) + 1;

                DEBUG("Mode is %d x %d\n", screenwidth, screenheight);
            }
        }

        screen = OpenScreenTags(0,
                                SA_Width,
                                screenwidth,
                                SA_Height,
                                screenheight,
                                SA_Depth,
                                screendepth,
                                SA_Quiet,
                                TRUE,
                                SA_AdaptSize,
                                TRUE,
                                TAG_DONE);
        if (screen) {
            GetAttr(SA_Depth, screen, &screen_depth);
            if (screen_depth == 8) {
                LoadRGB32(&screen->ViewPort, screen_palette);
            }
            screenwidth = screen->Width;
            screenheight = screen->Height;
        }
    } else {
        screen = 0;
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
        if (screen) {
            i = 0;

            port_screen_buffer_display = CreateMsgPort();
            if (port_screen_buffer_display) {
                port_screen_buffer_write = CreateMsgPort();
                if (port_screen_buffer_write) {
                    for (i = 0; i < 3; i++) {
                        screen_buffers[i] = AllocScreenBuffer(screen, 0, i ? SB_COPY_BITMAP : SB_SCREEN_BITMAP);
                        if (screen_buffers[i] == 0)
                            break;

                        screen_buffers[i]->sb_DBufInfo->dbi_DispMessage.mn_ReplyPort = port_screen_buffer_display;
                        screen_buffers[i]->sb_DBufInfo->dbi_SafeMessage.mn_ReplyPort = port_screen_buffer_write;
                    }

                    if (i != 3) {
                        DeleteMsgPort(port_screen_buffer_write);
                        port_screen_buffer_write = 0;
                    }
                }

                if (i != 3) {
                    DeleteMsgPort(port_screen_buffer_display);
                    port_screen_buffer_display = 0;
                }
            }
        }

        if (!screen || i == 3) {
            if (screen) {
                current_screen_buffer = 1;
                screen_buffers_swappable = 1;
                screen_buffers_writable = 2;

                for (i = 0; i < 3; i++) {
                    while (GetMsg(screen_buffers[i]->sb_DBufInfo->dbi_SafeMessage.mn_ReplyPort)) {
                        DEBUG("Screen buffer %d is safe to write\n", i);
                    }
                    while (GetMsg(screen_buffers[i]->sb_DBufInfo->dbi_DispMessage.mn_ReplyPort)) {
                        DEBUG("Screen buffer %d has been displayed\n", i);
                    }
                }
            }

            InitRastPort(&mouse_cursor_rastport);
            InitRastPort(&rastport);
            scaling_enabled = false;

            if (screen) {
                for (i = 0; i < 3; i++) {
                    rastport.BitMap = screen_buffers[i]->sb_BitMap;

                    FillPixelArray(&rastport, 0, 0, screen->Width, screen->Height, 0);
                }
            }

            Hide_Mouse_Cursor();

            mouse_cursor_visible = false;

            ret = true;
        }

        if (!ret) {
            if (screen) {
                for (i = 0; i < 3; i++) {
                    if (screen_buffers[i]) {
                        screen_buffers[i]->sb_DBufInfo->dbi_DispMessage.mn_ReplyPort = 0;
                        screen_buffers[i]->sb_DBufInfo->dbi_SafeMessage.mn_ReplyPort = 0;
                        FreeScreenBuffer(screen, screen_buffers[i]);
                    }
                }
            }

            CloseWindow(morphos_window);
        }
    }

    if (!ret) {
        if (screen) {
            CloseScreen(screen);
            screen = 0;
        }
    }

    return ret;
}

void VideoMorphOSCGX::Mouse_Cursor_To_ARGB8888()
{
    const unsigned char* src;
    unsigned char lutpixel;
    unsigned int* dst;
    unsigned int pixelcount;
    size_t i;

    pixelcount = mouse_cursor_width * mouse_cursor_height;

    dst = (unsigned int*)AllocVec(pixelcount * 4, MEMF_ANY);
    if (dst) {
        src = (const unsigned char*)mouse_cursor_pixels;
        for (i = 0; i < pixelcount; i++) {
            lutpixel = src[i];
            if (lutpixel) {
                dst[i] = 0xff000000 | (palette_cgx[lutpixel * 4 + 1] << 16) | (palette_cgx[lutpixel * 4 + 2] << 8)
                         | (palette_cgx[lutpixel * 4 + 3] << 0);
            } else {
                dst[i] = 0;
            }

            dst[i] |= 0xff00;

            dst[i] = palette_cursor[src[i]];
        }

        WritePixelArray(dst,
                        0,
                        0,
                        mouse_cursor_width * 4,
                        &mouse_cursor_rastport,
                        0,
                        0,
                        mouse_cursor_width,
                        mouse_cursor_height,
                        RECTFMT_ARGB);

        FreeVec(dst);
    }
}

void VideoMorphOSCGX::RefreshMouseCursor()
{
    struct BitMap* old_mouse_cursor_bitmap;
    APTR old_mouse_cursor_object;

    if (mouse_cursor_width && mouse_cursor_height) {
        old_mouse_cursor_bitmap = mouse_cursor_bitmap;
        old_mouse_cursor_object = mouse_cursor_object;

        mouse_cursor_bitmap = AllocBitMap(
            mouse_cursor_width, mouse_cursor_height, 32, BMF_CLEAR | BMF_SPECIALFMT | SHIFT_PIXFMT(PIXFMT_ARGB32), 0);
        if (mouse_cursor_bitmap) {
            mouse_cursor_rastport.BitMap = mouse_cursor_bitmap;

            Mouse_Cursor_To_ARGB8888();

            mouse_cursor_object = NewObject(NULL,
                                            "pointerclass",
                                            POINTERA_BitMap,
                                            mouse_cursor_bitmap,
                                            POINTERA_XOffset,
                                            -mouse_cursor_hotspot_x,
                                            POINTERA_YOffset,
                                            -mouse_cursor_hotspot_y,
                                            TAG_DONE);
            if (!mouse_cursor_object) {
                DEBUG("Failed to create pointer class object\n");
                FreeBitMap(mouse_cursor_bitmap);
                mouse_cursor_bitmap = 0;
            } else if (mouse_cursor_visible) {
                SetWindowPointer(morphos_window, WA_Pointer, mouse_cursor_object, TAG_DONE);
            }
        }

        if (mouse_cursor_bitmap && mouse_cursor_object) {
            if (old_mouse_cursor_bitmap) {
                FreeBitMap(old_mouse_cursor_bitmap);
            }

            if (old_mouse_cursor_object) {
                DisposeObject(old_mouse_cursor_object);
            }
        } else {
            mouse_cursor_bitmap = old_mouse_cursor_bitmap;
            mouse_cursor_object = old_mouse_cursor_object;
        }
    }
}

void VideoMorphOSCGX::Render_Frame(const void* displaybuffer)
{
    bool show_mouse_cursor;
    int mouse_x;
    int mouse_y;

    Get_Video_Mouse_Raw(mouse_x, mouse_y);

    if (!screen) {
        WriteLUTPixelArray((void*)displaybuffer,
                           0,
                           0,
                           game_width,
                           morphos_window->RPort,
                           palette_cgx,
                           morphos_window_xoffset + morphos_window->BorderLeft,
                           morphos_window_yoffset + morphos_window->BorderTop,
                           game_width,
                           game_height,
                           CTABFMT_XRGB8);
    } else {
        while (!screen_buffers_writable) {
            WaitPort(port_screen_buffer_write);
            while (GetMsg(port_screen_buffer_write)) {
                screen_buffers_writable++;
            }
        }

        screen_buffers_writable--;

        rastport.BitMap = screen_buffers[current_screen_buffer]->sb_BitMap;

        WritePixelArray((void*)displaybuffer,
                        0,
                        0,
                        game_width,
                        &rastport,
                        morphos_window_xoffset + morphos_window->BorderLeft,
                        morphos_window_yoffset + morphos_window->BorderTop,
                        game_width,
                        game_height,
                        RECTFMT_LUT8);

        while (!screen_buffers_swappable) {
            WaitPort(port_screen_buffer_display);
            while (GetMsg(port_screen_buffer_display)) {
                screen_buffers_swappable++;
            }
        }

        screen_buffers_swappable--;

        ChangeScreenBuffer(screen, screen_buffers[current_screen_buffer]);
        current_screen_buffer = (current_screen_buffer + 1) % 3;
    }

    show_mouse_cursor = !Get_Mouse_State();
    if (show_mouse_cursor != mouse_cursor_visible) {
        if (show_mouse_cursor) {
            SetWindowPointer(morphos_window, WA_Pointer, mouse_cursor_object, TAG_DONE);
        } else {
            Hide_Mouse_Cursor();
        }

        mouse_cursor_visible = show_mouse_cursor;
    }
}

void VideoMorphOSCGX::Set_Palette(void* newpaletteptr)
{
    unsigned char* newpalette;
    int i;

    newpalette = (unsigned char*)newpaletteptr;

    palette_cursor[0] = 0;
    for (i = 1; i < 256; i++) {
        palette_cursor[i] =
            0xff000000 | (newpalette[i * 3 + 0] << 18) | (newpalette[i * 3 + 1] << 10) | (newpalette[i * 3 + 2] << 2);
    }

    for (i = 0; i < 256; i++) {
        palette_cgx[i * 4] = 0;
        palette_cgx[i * 4 + 1] = newpalette[i * 3 + 0] << 2;
        palette_cgx[i * 4 + 2] = newpalette[i * 3 + 1] << 2;
        palette_cgx[i * 4 + 3] = newpalette[i * 3 + 2] << 2;
    }
    for (i = 0; i < 256; i++) {
        screen_palette[1 + (i * 3)] = ((unsigned int)newpalette[i * 3 + 0]) << 26;
        screen_palette[2 + (i * 3)] = ((unsigned int)newpalette[i * 3 + 1]) << 26;
        screen_palette[3 + (i * 3)] = ((unsigned int)newpalette[i * 3 + 2]) << 26;
    }

    if (screen && screen_depth == 8) {
        LoadRGB32(&screen->ViewPort, screen_palette);
    }
}

void VideoMorphOSCGX::Shutdown()
{
    unsigned int i;

    if (morphos_window) {
        SetWindowPointer(morphos_window, WA_Pointer, 0, TAG_DONE);
    }

    if (mouse_cursor_object) {
        DisposeObject(mouse_cursor_object);
        mouse_cursor_object = 0;
    }

    if (mouse_cursor_bitmap) {
        FreeBitMap(mouse_cursor_bitmap);
        mouse_cursor_bitmap = 0;
    }

    if (screen) {
        for (i = 0; i < 3; i++) {
            if (screen_buffers[i]) {
                screen_buffers[i]->sb_DBufInfo->dbi_DispMessage.mn_ReplyPort = 0;
                screen_buffers[i]->sb_DBufInfo->dbi_SafeMessage.mn_ReplyPort = 0;
                FreeScreenBuffer(screen, screen_buffers[i]);
            }
        }
    }

    if (port_screen_buffer_display) {
        DeleteMsgPort(port_screen_buffer_display);
        port_screen_buffer_display = 0;
    }

    if (port_screen_buffer_write) {
        DeleteMsgPort(port_screen_buffer_write);
        port_screen_buffer_write = 0;
    }

    if (morphos_window) {
        CloseWindow(morphos_window);
        morphos_window = 0;
    }

    if (screen) {
        CloseScreen(screen);
        screen = 0;
    }
}
