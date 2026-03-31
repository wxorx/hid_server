
/* Copyright (c) 2020, Peter Barrett
**
** Permission to use, copy, modify, and/or distribute this software for
** any purpose with or without fee is hereby granted, provided that the
** above copyright notice and this permission notice appear in all copies.
**
** THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL
** WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED
** WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR
** BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES
** OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS,
** WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION,
** ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS
** SOFTWARE.
*/

#ifndef hid_server_hpp
#define hid_server_hpp

#include "freertos/FreeRTOS.h"
#include "freertos/timers.h"
#include "freertos/task.h"
#include "freertos/queue.h"

#include <stdio.h>
#include "hci_server.h"

#define HID_SERVER_STACK_SIZE   (4096)
#define HID_SERVER_PRIO         (5)
#define HID_SERVER_BUFFER_SIZE  (64)
#define HID_SERVER_MAX_KEYS     (6)
#define HID_SERVER_KEY_TIMEOUT  (20) /* ms */

#define DEFAULT_REPEAT_DELAY_MS (250)
#define DEFAULT_REPEAT_RATE_MS  (30)

enum {
    KEY_RESERVED,
    KEY_A,
    KEY_B,
    KEY_C,
    KEY_D,
    KEY_E,
    KEY_F,
    KEY_G,
    KEY_H,
    KEY_I,
    KEY_J,
    KEY_K,
    KEY_L,
    KEY_M,
    KEY_N,
    KEY_O,
    KEY_P,
    KEY_Q,
    KEY_R,
    KEY_S,
    KEY_T,
    KEY_U,
    KEY_V,
    KEY_W,
    KEY_X,
    KEY_Y,
    KEY_Z,
    KEY_1,
    KEY_2,
    KEY_3,
    KEY_4,
    KEY_5,
    KEY_6,
    KEY_7,
    KEY_8,
    KEY_9,
    KEY_0,
    KEY_ENTER,
    KEY_ESC,
    KEY_BACKSPACE,
    KEY_TAB,
    KEY_SPACE,
    KEY_MINUS,
    KEY_EQUAL,
    KEY_LEFTBRACE,
    KEY_RIGHTBRACE,
    KEY_BACKSLASH,
    KEY_SEMICOLON,
    KEY_APOSTROPHE,
    KEY_GRAVE,
    KEY_COMMA,
    KEY_DOT,
    KEY_SLASH,
    KEY_CAPSLOCK,
    KEY_F1,
    KEY_F2,
    KEY_F3,
    KEY_F4,
    KEY_F5,
    KEY_F6,
    KEY_F7,
    KEY_F8,
    KEY_F9,
    KEY_F10,
    KEY_F11,
    KEY_F12,
    KEY_SYSRQ,
    KEY_SCROLLLOCK,
    KEY_PAUSE,
    KEY_INSERT,
    KEY_HOME,
    KEY_PAGEUP,
    KEY_DELETE,
    KEY_END,
    KEY_PAGEDOWN,
    KEY_RIGHT,
    KEY_LEFT,
    KEY_DOWN,
    KEY_UP,
    KEY_NUMLOCK,
    KEY_KPSLASH,
    KEY_KPASTERISK,
    KEY_KPMINUS,
    KEY_KPPLUS,
    KEY_KPENTER,
    KEY_KP1,
    KEY_KP2,
    KEY_KP3,
    KEY_KP4,
    KEY_KP5,
    KEY_KP6,
    KEY_KP7,
    KEY_KP8,
    KEY_KP9,
    KEY_KP0,
    KEY_KPDOT,
    KEY_102ND,
    KEY_COMPOSE,
    KEY_POWER,
    KEY_KPEQUAL,
    KEY_F13,
    KEY_F14,
    KEY_F15,
    KEY_F16,
    KEY_F17,
    KEY_F18,
    KEY_F19,
    KEY_F20,
    KEY_F21,
    KEY_F22,
    KEY_F23,
    KEY_F24,
    KEY_OPEN,
    KEY_HELP,
    KEY_PROPS,
    KEY_FRONT,
    KEY_AGAIN,
    KEY_UNDO,
    KEY_CUT,
    KEY_COPY,
    KEY_PASTE,
    KEY_KPCOMMA,
    KEY_RO,
    KEY_KATAKANAHIRAGANA,
    KEY_YEN,
    KEY_HENKAN,
    KEY_MUHENKAN,
    KEY_KPJPCOMMA,
    KEY_HANGEUL,
    KEY_HANJA,
    KEY_KATAKANA,
    KEY_HIRAGANA,
    KEY_ZENKAKUHANKAKU,
    KEY_LEFTCTRL,
    KEY_LEFTSHIFT,
    KEY_LEFTALT,
    KEY_LEFTMETA,
    KEY_RIGHTCTRL,
    KEY_RIGHTSHIFT,
    KEY_RIGHTALT,
    KEY_RIGHTMETA,
    KEY_PLAYPAUSE,
    KEY_STOPCD,
    KEY_PREVIOUSSONG,
    KEY_NEXTSONG,
    KEY_EJECTCD,
    KEY_VOLUMEUP,
    KEY_VOLUMEDOWN,
    KEY_MUTE,
    KEY_WWW,
    KEY_BACK,
    KEY_FORWARD,
    KEY_STOP,
    KEY_FIND,
    KEY_SCROLLUP,
    KEY_SCROLLDOWN,
    KEY_EDIT,
    KEY_SLEEP,
    KEY_COFFEE,
    KEY_REFRESH,
    KEY_CALC,
    /* keep last */
    KEY_LAST
} E_Key;

typedef enum {
    MOD_RIGHTMETA   = (1 << 0),
    MOD_RIGHTALT    = (1 << 1),
    MOD_RIGHTSHIFT  = (1 << 2),
    MOD_RIGHTCTRL   = (1 << 3),
    MOD_LEFTMETA    = (1 << 4),
    MOD_LEFTALT     = (1 << 5),
    MOD_LEFTSHIFT   = (1 << 6),
    MOD_LEFTCTRL    = (1 << 7)
} E_ModKey;

typedef enum
{
    GP_A            = (1 << 0),
    GP_B            = (1 << 1),
    GP_X            = (1 << 3),
    GP_Y            = (1 << 4),
    GP_L            = (1 << 6),
    GP_R            = (1 << 7),
    GP_SELECT       = (1 << 10),
    GP_START        = (1 << 11)
} E_GamePadKey;

struct HIDKey
{
    uint8_t mod;
    uint8_t len;

    union {
        uint8_t keys[HID_SERVER_MAX_KEYS];
        struct {
            uint16_t key;
            int16_t  x;
            int16_t  y;
        } dpad;
    };
};

class HIDSource;
class HIDServer {
    public:
        HIDServer(const char* local_name,
                  uint32_t repeat_delay_ms = DEFAULT_REPEAT_DELAY_MS,
                  uint32_t repeat_rate_ms = DEFAULT_REPEAT_RATE_MS);
        ~HIDServer();

        void begin();
        void end();

    bool keyAvailable();
    bool getNextKey(HIDKey & key);
    private:
        HIDSource       *_hid_source;
        QueueHandle_t   _queue;
        TaskHandle_t    _task;
        const uint32_t  _repeat_delay_ms;
        const uint32_t  _repeat_rate_ms;
        std::string     _local_name;
        uint8_t         _buf[HID_SERVER_BUFFER_SIZE];
        int checkKeys(HIDKey & key);
        static void updateLoop(void *arg);
};

#endif /* hid_server_hpp */
