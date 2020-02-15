/*
 * Copyright (c) 2019 Pierre Evenou
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/select.h>

#include <xcb/xcb.h>
#include <xcb/xcb_keysyms.h>
#include <xcb/xkb.h>
#include <xcb/xtest.h>
#include <xcb/xcb_aux.h>
#include <xkbcommon/xkbcommon.h>
#include <xkbcommon/xkbcommon-compose.h>
#include <xkbcommon/xkbcommon-x11.h>

#ifndef NDEBUG
#define MODKEY XCB_MOD_MASK_1
#else
#define MODKEY XCB_MOD_MASK_4
#endif

#define K_M         MODKEY
#define K_MC        MODKEY | XCB_MOD_MASK_CONTROL
#define K_MS        MODKEY | XCB_MOD_MASK_SHIFT
#define K_MCS       MODKEY | XCB_MOD_MASK_CONTROL | XCB_MOD_MASK_SHIFT

/*
 * function declarations
 */
static void spawn(char **argv);
static void trap(int sig);

/*
 * variables
 */
static const struct {
    struct {unsigned int modifier; xkb_keysym_t keysym;} sequence;
    char *args[16];
} bindings[] = {
    /* modifier key                                 arguments      */
    { {K_M,     XKB_KEY_Return },                   {"uxterm"} },
    { {K_MS,    XKB_KEY_Return },                   {"uxterm", "-e", "ranger" } },
    { {K_M,     XKB_KEY_d },                        {"dmenu_run", "-b", "-fn", "Noto Sans:size=12", "-nb", "#1d2021", "-nf", "#888888", "-sb", "#1d2021", "-sf", "#eeeeec"} },
    { {0,       XKB_KEY_XF86AudioRaiseVolume },     {"pactl", "set-sink-volume", "0", "+5%"} },
    { {0,       XKB_KEY_XF86AudioLowerVolume },     {"pactl", "set-sink-volume", "0", "-5%"} },
    { {0,       XKB_KEY_XF86AudioMute },            {"pactl", "set-sink-mute", "0", "toggle"} },
    { {0,       XKB_KEY_XF86AudioMicMute },         {"pactl", "set-source-mute", "1", "toggle"} },
    { {K_M,     XKB_KEY_XF86AudioMute },            {"pavucontrol"} },
    { {0,       XKB_KEY_XF86MonBrightnessUp },      {"xbacklight", "+", "5" } },
    { {0,       XKB_KEY_XF86MonBrightnessDown },    {"xbacklight", "-", "5" } },
    { { 0, 0 }, {NULL} }
};

static xcb_connection_t     *xcb = NULL;
static struct xkb_state     *xkb_state = NULL;
static int                  running;

/*
 * function definitions
 */
void spawn(char **argv)
{
    if (fork() == 0) {
        if (xcb)
            close(xcb_get_file_descriptor(xcb));
        setsid();
        execvp(argv[0], argv);
        exit(EXIT_SUCCESS);
    }
}

void trap(int sig)
{
    running = 0;
    /* interrupt the xcb event loop
     * does not seems efficient */
    xcb_test_fake_input(
            xcb,
            XCB_KEY_PRESS,
            XCB_NONE,
            XCB_CURRENT_TIME,
            XCB_NONE, 0, 0, 0);
    xcb_flush(xcb);
}

int main(int argc, char **argv)
{
    /* setup xcb */
    xcb = xcb_connect(0, NULL);
    if (xcb_connection_has_error (xcb)) {
        fprintf(stderr, "can't get xcb connection.");
        exit(EXIT_FAILURE);
    }

    /* setup xkb */
    int device;
    unsigned char xkb_base_event;
    xkb_x11_setup_xkb_extension(
            xcb,
            XKB_X11_MIN_MAJOR_XKB_VERSION,
            XKB_X11_MIN_MINOR_XKB_VERSION,
            0,
            NULL,
            NULL,
            &xkb_base_event,
            NULL);
    device = xkb_x11_get_core_keyboard_device_id(xcb);
    xkb_state = xkb_x11_state_new_from_device(
            xkb_x11_keymap_new_from_device(
                    xkb_context_new(XKB_CONTEXT_NO_FLAGS),
                    xcb,
                    device,
                    XKB_KEYMAP_COMPILE_NO_FLAGS),
            xcb,
            device);

    /* iterate over screens to catch input, ungrab all keys and and grab
     * configured ones */
    const struct xcb_setup_t *setup = xcb_get_setup(xcb);
    xcb_screen_iterator_t it = xcb_setup_roots_iterator(setup);
    int sc = xcb_setup_roots_iterator(setup).rem;
    xcb_key_symbols_t *ks = xcb_key_symbols_alloc(xcb);
    for (int i = 0; i < sc; ++i) {
        xcb_screen_t *screen = it.data;

        /* catch input */
        xcb_change_window_attributes(
                xcb,
                screen->root,
                XCB_CW_EVENT_MASK,
                (const int []) { XCB_EVENT_MASK_KEY_PRESS });

        /* ungrab all keys */
        xcb_ungrab_key(xcb, XCB_GRAB_ANY, screen->root, XCB_MOD_MASK_ANY);

        /* grab configured ones */
        int bindex = 0;
        while (bindings[bindex].args[0] != NULL) {
            xcb_keycode_t *kcs;
            xcb_keycode_t *kc;
            kcs = xcb_key_symbols_get_keycode(ks, bindings[bindex].sequence.keysym);
            for (kc = kcs; *kc != XCB_NO_SYMBOL; kc++)
                xcb_grab_key(
                        xcb,
                        1,
                        screen->root,
                        bindings[bindex].sequence.modifier,
                        *kc,
                        XCB_GRAB_MODE_ASYNC,
                        XCB_GRAB_MODE_ASYNC);
            bindex++;
        }
        xcb_screen_next(&it);
    }
    xcb_key_symbols_free(ks);
    xcb_flush(xcb);

    /* trap signals */
    signal(SIGCHLD, SIG_IGN);
    signal(SIGTSTP, SIG_IGN);
    signal(SIGTTOU, SIG_IGN);
    signal(SIGTTIN, SIG_IGN);
    signal(SIGHUP,  SIG_IGN);
    signal(SIGINT,  trap);
    signal(SIGKILL, trap);
    signal(SIGTERM, trap);

    /* enter the main loop */
    running = 1;
    while (running) {
        xcb_generic_event_t *event;
        while ((event = xcb_wait_for_event(xcb))) {
            if (! running)
                break;

            if (event->response_type == 0) {
                xcb_generic_error_t *e = (xcb_generic_error_t *)event;
                fprintf(stderr, "X11 Error, sequence 0x%x, resource %d, code = %d\n",
                        e->sequence,
                        e->resource_id,
                        e->error_code);
                free(event);
                running = 0;
                break;
            }

            switch (event->response_type & ~0x80) {
                case XCB_KEY_PRESS:
                {
                    xcb_key_press_event_t *e = (xcb_key_press_event_t *)event;
                    xkb_keycode_t keycode = e->detail;
                    xkb_keysym_t keysym = xkb_state_key_get_one_sym(xkb_state, keycode);
                    int bindex = 0;
                    while (bindings[bindex].args[0] != NULL) {
                        if (bindings[bindex].sequence.modifier == e->state &&
                                bindings[bindex].sequence.keysym == keysym)
                            spawn((char**)bindings[bindex].args);
                        bindex++;
                    }
                    break;
                }
                default:
                    break;
            }

           free(event);
        }
    }

    xcb_disconnect(xcb);
}
