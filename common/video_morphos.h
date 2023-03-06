#ifndef VIDEO_MORPHOS_H
#define VIDEO_MORPHOS_H

struct Window;

class VideoMorphOS
{
public:
    VideoMorphOS(int width, int height);
    virtual ~VideoMorphOS();

    virtual bool Initialise() = 0;
    virtual void Recalculate_Window();
    virtual void RefreshMouseCursor() = 0;
    virtual void Render_Frame(const void* displaybuffer) = 0;
    virtual void Set_Palette(void* newpaletteptr) = 0;
    virtual void Shutdown() = 0;

    void Clear_Display();
    void Get_Video_Mouse_Raw(int& x, int& y);
    struct Window* Get_Window();
    void Refresh_Settings();
    void Set_Cursor(const void* cursor, int width, int height, int hotx, int hoty);
    bool Translate_Mouse_Coordinates(int* x, int* y);

protected:
    void Hide_Mouse_Cursor();

    unsigned int display_width;
    unsigned int display_height;
    int game_width;
    int game_height;
    int mouse_cursor_width;
    int mouse_cursor_height;
    int mouse_cursor_hotspot_x;
    int mouse_cursor_hotspot_y;
    void* mouse_cursor_pixels;
    unsigned int scaled_width;
    unsigned int scaled_height;
    bool scaling_enabled;
    struct Screen* screen;
    bool use_fullscreen;
    int morphos_window_xoffset;
    int morphos_window_yoffset;
    float morphos_window_xscale;
    float morphos_window_yscale;
    struct Window* morphos_window;
};

extern VideoMorphOS* videomorphos;

#endif /* VIDEO_MORPHOS_H */
