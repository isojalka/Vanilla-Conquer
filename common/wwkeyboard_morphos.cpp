#include <proto/exec.h>
#include <proto/intuition.h>
#include <proto/keymap.h>

#include <string.h>

#include "video_morphos.h"
#include "wwkeyboard_morphos.h"

#if 0
static const unsigned char MorphOSToVQKeyMap[256] =
{
    [RAWKEY_TILDE] = ,
    [RAWKEY_1] = VK_1,
    [RAWKEY_2] = VK_2,
    [RAWKEY_3] = VK_3,
    [RAWKEY_4] = VK_4,
    [RAWKEY_5] = VK_5,
    [RAWKEY_6] = VK_6,
    [RAWKEY_7] = VK_7,
    [RAWKEY_8] = VK_8,
    [RAWKEY_9] = VK_9,
    [RAWKEY_0] = VK_0,
    [RAWKEY_MINUS] = ,
    [RAWKEY_EQUAL] = ,
    [RAWKEY_BACKSLASH] = ,
    [RAWKEY_FN] = ,
    [RAWKEY_KP_0] = ,
    [RAWKEY_Q] = ,
    [RAWKEY_W] = ,
    [RAWKEY_E] = ,
    [RAWKEY_R] = ,
    [RAWKEY_T] = ,
    [RAWKEY_Y] = ,
    [RAWKEY_U] = ,
    [RAWKEY_I] = ,
    [RAWKEY_O] = ,
    [RAWKEY_P] = ,
    [RAWKEY_LBRACKET] = ,
    [RAWKEY_RBRACKET] = ,
    [RAWKEY_KP_1] = ,
    [RAWKEY_KP_2] = ,
    [RAWKEY_KP_3] = ,
    [RAWKEY_A] = ,
    [RAWKEY_S] = ,
    [RAWKEY_D] = ,
    [RAWKEY_F] = ,
    [RAWKEY_G] = ,
    [RAWKEY_H] = ,
    [RAWKEY_J] = ,
    [RAWKEY_K] = ,
    [RAWKEY_L] = ,
    [RAWKEY_SEMICOLON] = ,
    [RAWKEY_QUOTE] = ,
    [RAWKEY_2B] = ,
    [RAWKEY_KP_4] = ,
    [RAWKEY_KP_5] = ,
    [RAWKEY_KP_6] = ,
    [RAWKEY_LESSGREATER] = ,
    [RAWKEY_Z] = ,
    [RAWKEY_X] = ,
    [RAWKEY_C] = ,
    [RAWKEY_V] = ,
    [RAWKEY_B] = ,
    [RAWKEY_N] = ,
    [RAWKEY_M] = ,
    [RAWKEY_COMMA] = ,
    [RAWKEY_PERIOD] = ,
    [RAWKEY_SLASH] = ,
    [RAWKEY_KP_DECIMAL] = ,
    [RAWKEY_KP_7] = ,
    [RAWKEY_KP_8] = ,
    [RAWKEY_KP_9] = ,
    [RAWKEY_SPACE] = ,
    [RAWKEY_BACKSPACE] = ,
    [RAWKEY_TAB] = ,
    [RAWKEY_KP_ENTER] = ,
    [RAWKEY_RETURN] = ,
    [RAWKEY_ESCAPE] = ,
    [RAWKEY_DELETE] = ,
    [RAWKEY_INSERT] = ,
    [RAWKEY_PAGEUP] = ,
    [RAWKEY_PAGEDOWN] = ,
    [RAWKEY_KP_MINUS] = ,
    [RAWKEY_F11] = ,
    [RAWKEY_UP] = ,
    [RAWKEY_DOWN] = ,
    [RAWKEY_RIGHT] = ,
    [RAWKEY_LEFT] = ,
    [RAWKEY_F1] = ,
    [RAWKEY_F2] = ,
    [RAWKEY_F3] = ,
    [RAWKEY_F4] = ,
    [RAWKEY_F5] = ,
    [RAWKEY_F6] = ,
    [RAWKEY_F7] = ,
    [RAWKEY_F8] = ,
    [RAWKEY_F9] = ,
    [RAWKEY_F10] = ,
    [RAWKEY_KP_DIVIDE] = ,
    [RAWKEY_KP_MULTIPLY] = ,
    [RAWKEY_KP_PLUS] = ,
    [RAWKEY_HELP] = ,
    [RAWKEY_LSHIFT] = ,
    [RAWKEY_RSHIFT] = ,
    [RAWKEY_CAPSLOCK] = ,
    [RAWKEY_CONTROL] = ,
    [RAWKEY_LCONTROL] = ,
    [RAWKEY_LALT] = ,
    [RAWKEY_RALT] = ,
    [RAWKEY_LAMIGA] = ,
    [RAWKEY_RAMIGA] = ,
    [RAWKEY_SCRLOCK] = ,
    [RAWKEY_PRTSCREEN] = ,
    [RAWKEY_NUMLOCK] = ,
    [RAWKEY_PAUSE] = ,
    [RAWKEY_F12] = ,
    [RAWKEY_HOME] = ,
    [RAWKEY_END] = ,
    [RAWKEY_MEDIA1] = ,
    [RAWKEY_MEDIA2] = ,
    [RAWKEY_MEDIA3] = ,
    [RAWKEY_MEDIA4] = ,
    [RAWKEY_MEDIA5] = ,
    [RAWKEY_MEDIA6] = ,
    [RAWKEY_CDTV_STOP] = ,
    [RAWKEY_CDTV_PLAY] = ,
    [RAWKEY_CDTV_PREV] = ,
    [RAWKEY_CDTV_NEXT] = ,
    [RAWKEY_CDTV_REW] = ,
    [RAWKEY_CDTV_FF] = ,
    [RAWKEY_NM_WHEEL_UP] = ,
    [RAWKEY_NM_WHEEL_DOWN] = ,
    [RAWKEY_NM_WHEEL_LEFT] = ,
    [RAWKEY_NM_WHEEL_RIGHT] = ,
    [RAWKEY_NM_BUTTON_FOURTH] = ,
};
#endif

extern "C" {
void dprintf(const char*, ...);
}

WWKeyboardClassMorphOS::~WWKeyboardClassMorphOS()
{
}

KeyASCIIType WWKeyboardClassMorphOS::To_ASCII(unsigned short key)
{
    struct InputEvent ie;
    unsigned char uc;
    int r;

    if (key & WWKEY_RLS_BIT) {
        return KA_NONE;
    }

    memset(&ie, 0, sizeof(ie));
    ie.ie_Class = IECLASS_RAWKEY;
    ie.ie_Code = (key - 1) & 0x7f;

    if ((key & WWKEY_SHIFT_BIT)) {
        ie.ie_Qualifier |= IEQUALIFIER_LSHIFT;
    }

    if ((key & WWKEY_CTRL_BIT)) {
        ie.ie_Qualifier |= IEQUALIFIER_CONTROL;
    }

    if ((key & WWKEY_ALT_BIT)) {
        ie.ie_Qualifier |= IEQUALIFIER_LALT;
    }

    r = MapRawKey(&ie, (STRPTR)&uc, 1, 0);
    if (r != 1) {
        uc = KA_NONE;
    }

    return (KeyASCIIType)uc;
}

void WWKeyboardClassMorphOS::Fill_Buffer_From_System(void)
{
    struct Window* window;
    struct IntuiMessage* msg;
    unsigned short key;
    bool mouse_valid;
    int x;
    int y;

    window = videomorphos ? videomorphos->Get_Window() : 0;

    if (window) {
        while (!Is_Buffer_Full() && (msg = (struct IntuiMessage*)GetMsg(window->UserPort))) {
            switch (msg->Class) {
            case IDCMP_MOUSEBUTTONS:
                x = msg->MouseX;
                y = msg->MouseY;

                mouse_valid = videomorphos->Translate_Mouse_Coordinates(&x, &y);

                if ((msg->Code & IECODE_UP_PREFIX) || mouse_valid) {
                    switch ((msg->Code & 0x7f)) {
                    case IECODE_LBUTTON:
                        key = KN_LMOUSE;
                        break;

                    case IECODE_MBUTTON:
                        key = KN_MMOUSE;
                        break;

                    case IECODE_RBUTTON:
                        key = KN_RMOUSE;
                        break;

                    default:
                        key = 0;
                        break;
                    }

                    if (key) {
                        Put_Mouse_Message(key, x, y, (msg->Code & IECODE_UP_PREFIX) ? true : false);
                    }
                }

                break;

            case IDCMP_RAWKEY:
                Put_Key_Message(((msg->Code & 0x7f) + 1), (msg->Code & IECODE_UP_PREFIX) ? true : false);
                break;

            default:
                dprintf("%s:%d/%s(): Unknown message\n", __FILE__, __LINE__, __func__);
                break;
            }

            ReplyMsg((struct Message*)msg);
        }
    }
}

WWKeyboardClass* CreateWWKeyboardClass(void)
{
    return new WWKeyboardClassMorphOS;
}
