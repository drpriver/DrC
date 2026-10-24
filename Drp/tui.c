#ifndef DRP_TUI_C
#define DRP_TUI_C
#include "tui.h"
#include "measure_time.h"

#include <string.h>
#ifndef _WIN32
#include <unistd.h>
#include <sys/ioctl.h>
#include <poll.h>
#include <errno.h>
#include <fcntl.h>
#ifdef __linux__
#include <sys/signalfd.h>
#else
#include <sys/event.h>
#endif
#endif
#include "file_util.h"

#ifdef __clang__
#pragma clang assume_nonnull begin
#endif

static int tui_check_resize(Tui*, TuiEvent*);

static
void
tui_character(TuiEvent* ev, uint32_t cp){
    if(cp >= 1 && cp <= 26){
        ev->control.kind = TUI_CONTROL;
        ev->control.key = (TuiControlKind)(TUI_CONTROL_A + cp - 1);
        ev->control.ctrl = cp != 9 && cp != 10 && cp != 13;
        if(cp == 10) ev->control.key = TUI_CONTROL_ENTER;
    }
    else if(cp == 27 || cp == 127){
        ev->control.kind = TUI_CONTROL;
        ev->control.key = cp == 27 ? TUI_CONTROL_ESC : TUI_CONTROL_BACKSPACE;
    }
    else {
        ev->key.kind = TUI_KEY;
        ev->key.key = cp;
        ev->key.ctrl = cp < 32;
    }
}

static
unsigned
tui_first_button(uint32_t buttons){
    for(unsigned i = 0; i < 3; i++)
        if(buttons & (1u << i)) return i;
    return 3;
}

static
int
tui_dimensions(Tui* tui, uint32_t* w, uint32_t* h){
    #ifdef _WIN32
        CONSOLE_SCREEN_BUFFER_INFO info;
        if(!GetConsoleScreenBufferInfo(tui->hstdout, &info)) return 1;
        *w = (uint32_t)(info.srWindow.Right - info.srWindow.Left + 1);
        *h = (uint32_t)(info.srWindow.Bottom - info.srWindow.Top + 1);
    #else
        struct winsize size;
        if(ioctl(tui->hstdout, TIOCGWINSZ, &size) < 0
        && ioctl(tui->hstderr, TIOCGWINSZ, &size) < 0)
            return 1;
        *w = size.ws_col;
        *h = size.ws_row;
    #endif
    return 0;
}

static
int
tui_write(Tui* tui, const void* data, size_t count){
    FileError fe = write_file_handle(tui->hstdout, data, count);
    return fe.errored;
}

#ifndef _WIN32

static
void
tui_consume(Tui* tui, uint32_t count){
    tui->count -= count;
    memmove(tui->bytes, tui->bytes + count, tui->count);
}

// Return 1 for an event, 0 for an incomplete sequence, -1 for discarded input.
static
int
tui_parse_character(Tui* tui, TuiEvent* ev, uint32_t start){
    const unsigned char* s = tui->bytes + start;
    uint32_t available = tui->count - start;
    uint32_t length, cp, minimum;
    if(s[0] < 128){
        tui_character(ev, s[0]);
        tui_consume(tui, start + 1);
        return 1;
    }
    if(s[0] >= 0xc2 && s[0] <= 0xdf){
        length = 2;
        cp = s[0] & 31;
        minimum = 0x80;
    }
    else if(s[0] >= 0xe0 && s[0] <= 0xef){
        length = 3;
        cp = s[0] & 15;
        minimum = 0x800;
    }
    else if(s[0] >= 0xf0 && s[0] <= 0xf4){
        length = 4;
        cp = s[0] & 7;
        minimum = 0x10000;
    }
    else {
        tui_character(ev, 0xfffd);
        tui_consume(tui, start + 1);
        return 1;
    }
    for(uint32_t i = 1; i < length; i++){
        if(i >= available){
            if(!tui->eof) return 0;
            tui_character(ev, 0xfffd);
            tui_consume(tui, tui->count);
            return 1;
        }
        if((s[i] & 0xc0) != 0x80){
            // Keep the unrelated byte for the next event.
            tui_character(ev, 0xfffd);
            tui_consume(tui, start + i);
            return 1;
        }
        cp = (cp << 6) | (s[i] & 63);
    }
    if(cp < minimum || cp > 0x10ffff || (cp >= 0xd800 && cp <= 0xdfff))
        cp = 0xfffd;
    tui_character(ev, cp);
    tui_consume(tui, start + length);
    return 1;
}

static
int
tui_mouse_sequence(Tui* tui, TuiEvent* ev, uint32_t code, uint32_t x, uint32_t y, _Bool release, _Bool legacy){
    if(!x || !y || code > 127)
        return -1;
    unsigned button = code & 3;
    if(code & 64){
        ev->scroll.kind = TUI_SCROLL;
        ev->scroll.cx = x - 1;
        ev->scroll.cy = y - 1;
        if(button < 2) ev->scroll.dy = button == 0 ? -1 : 1;
        else ev->scroll.dx = button == 2 ? -1 : 1;
    }
    else {
        ev->mouse.kind = TUI_MOUSE;
        ev->mouse.cx = x - 1;
        ev->mouse.cy = y - 1;
        if(code & 32){
            // Older Kitty versions encode hover using the left-drag code.
            // Only drag buttons whose press we actually received.
            if(button < 3 && (tui->mouse_buttons & (1u << button)))
                ev->mouse.mouse = TUI_MOUSE_DRAG;
            else {
                ev->mouse.mouse = TUI_MOUSE_MOVE;
                button = 3;
            }
        }
        else if(release || (legacy && button == 3)){
            if(legacy) button = tui_first_button(tui->mouse_buttons);
            ev->mouse.mouse = TUI_MOUSE_UP;
            if(button < 3) tui->mouse_buttons &= ~(1u << button);
        }
        else {
            if(button == 3) return -1;
            ev->mouse.mouse = TUI_MOUSE_DOWN;
            tui->mouse_buttons |= 1u << button;
        }
        ev->mouse.which = button & 3u;
    }
    ev->shift = !!(code & 4);
    ev->alt = !!(code & 8);
    ev->ctrl = !!(code & 16);
    return 1;
}

static
int
tui_tilde_key(TuiEvent* ev, uint32_t number){
    switch(number){
        case 1:
        case 7:
            ev->nav.kind = TUI_NAV;
            ev->nav.nav = TUI_NAV_HOME;
            return 1;
        case 4:
        case 8:
            ev->nav.kind = TUI_NAV;
            ev->nav.nav = TUI_NAV_END;
            return 1;
        case 5:
            ev->nav.kind = TUI_NAV;
            ev->nav.nav = TUI_NAV_PAGE_UP;
            return 1;
        case 6:
            ev->nav.kind = TUI_NAV;
            ev->nav.nav = TUI_NAV_PAGE_DOWN;
            return 1;
        case 2:
            ev->control.kind = TUI_CONTROL;
            ev->control.key = TUI_CONTROL_INSERT;
            return 1;
        case 3:
            ev->control.kind = TUI_CONTROL;
            ev->control.key = TUI_CONTROL_DELETE;
            return 1;
        default:
            break;
    }
    static const unsigned char fkeys[] = {
        11, 12, 13, 14, 15, 17, 18, 19, 20, 21,
        23, 24, 25, 26, 28, 29, 31, 32, 33, 34,
    };
    for(unsigned i = 0; i < sizeof fkeys; i++){
        if(number != fkeys[i]) continue;
        ev->control.kind = TUI_CONTROL;
        ev->control.key = (TuiControlKind)(TUI_CONTROL_F1 + i);
        return 1;
    }
    return -1;
}

static
int
tui_final_key(TuiEvent* ev, unsigned char final, int ss3){
    switch(final){
        case 'A':
            ev->nav.kind = TUI_NAV;
            ev->nav.nav = TUI_NAV_UP;
            return 1;
        case 'B':
            ev->nav.kind = TUI_NAV;
            ev->nav.nav = TUI_NAV_DOWN;
            return 1;
        case 'C':
            ev->nav.kind = TUI_NAV;
            ev->nav.nav = TUI_NAV_RIGHT;
            return 1;
        case 'D':
            ev->nav.kind = TUI_NAV;
            ev->nav.nav = TUI_NAV_LEFT;
            return 1;
        case 'H':
            ev->nav.kind = TUI_NAV;
            ev->nav.nav = TUI_NAV_HOME;
            return 1;
        case 'F':
            ev->nav.kind = TUI_NAV;
            ev->nav.nav = TUI_NAV_END;
            return 1;
        case 'Z':
            ev->control.kind = TUI_CONTROL;
            ev->control.key = TUI_CONTROL_TAB;
            ev->control.shift = 1;
            return 1;
        case 'P':
        case 'Q':
        case 'R':
        case 'S':
            ev->control.kind = TUI_CONTROL;
            ev->control.key = (TuiControlKind)(TUI_CONTROL_F1 + final - 'P');
            return 1;
        default:
            break;
    }
    if(!ss3) return -1;
    if(final == 'a' || final == 'b'){
        ev->nav.kind = TUI_NAV;
        ev->nav.nav = final == 'a' ? TUI_NAV_UP : TUI_NAV_DOWN;
        ev->nav.ctrl = 1;
        return 1;
    }
    if(final == 'M'){
        ev->control.kind = TUI_CONTROL;
        ev->control.key = TUI_CONTROL_ENTER;
        return 1;
    }
    uint32_t cp;
    if(final >= 'p' && final <= 'y')
        cp = '0' + final - 'p';
    else
        switch(final){
            case 'j': cp = '*'; break;
            case 'k': cp = '+'; break;
            case 'l': cp = ','; break;
            case 'm': cp = '-'; break;
            case 'n': cp = '.'; break;
            case 'o': cp = '/'; break;
            case 'X': cp = '='; break;
            default: return -1;
        }
    ev->key.kind = TUI_KEY;
    ev->key.key = cp;
    ev->key.keypad = 1;
    return 1;
}

static
int
tui_keyboard_modifiers(TuiEvent* ev, uint32_t modifiers){
    if(!modifiers || modifiers > 256) return -1;
    uint32_t bits = modifiers - 1;
    // The event API supports Shift/Alt/Ctrl. Ignore lock state, but don't
    // accidentally execute Super/Hyper/Meta shortcuts as unmodified input.
    if(bits & (8 | 16 | 32)) return -1;
    ev->shift = !!(bits & 1);
    ev->alt = !!(bits & 2);
    ev->ctrl = !!(bits & 4);
    return 1;
}

static
int
tui_kitty_key(TuiEvent* ev, uint32_t cp, uint32_t shifted){
    if(ev->ctrl && cp >= 'a' && cp <= 'z'){
        ev->control.kind = TUI_CONTROL;
        ev->control.key = (TuiControlKind)(TUI_CONTROL_A + cp - 'a');
        return 1;
    }
    if(cp >= 57364 && cp <= 57383){
        ev->control.kind = TUI_CONTROL;
        ev->control.key = (TuiControlKind)(TUI_CONTROL_F1 + cp - 57364);
        return 1;
    }
    if(cp >= 57399 && cp <= 57408){
        ev->key.kind = TUI_KEY;
        ev->key.key = '0' + cp - 57399;
        ev->key.keypad = 1;
        return 1;
    }
    switch(cp){
        case 27: case 57344: ev->control.kind=TUI_CONTROL; ev->control.key=TUI_CONTROL_ESC; return 1;
        case 13: case 57345: ev->control.kind=TUI_CONTROL; ev->control.key=TUI_CONTROL_ENTER; return 1;
        case 9: case 57346: ev->control.kind=TUI_CONTROL; ev->control.key=TUI_CONTROL_TAB; return 1;
        case 127: case 57347: ev->control.kind=TUI_CONTROL; ev->control.key=TUI_CONTROL_BACKSPACE; return 1;
        case 57348: ev->control.kind=TUI_CONTROL; ev->control.key=TUI_CONTROL_INSERT; return 1;
        case 57349: ev->control.kind=TUI_CONTROL; ev->control.key=TUI_CONTROL_DELETE; return 1;
        case 57350: case 57417: ev->nav.kind=TUI_NAV; ev->nav.nav=TUI_NAV_LEFT; return 1;
        case 57351: case 57418: ev->nav.kind=TUI_NAV; ev->nav.nav=TUI_NAV_RIGHT; return 1;
        case 57352: case 57419: ev->nav.kind=TUI_NAV; ev->nav.nav=TUI_NAV_UP; return 1;
        case 57353: case 57420: ev->nav.kind=TUI_NAV; ev->nav.nav=TUI_NAV_DOWN; return 1;
        case 57354: case 57421: ev->nav.kind=TUI_NAV; ev->nav.nav=TUI_NAV_PAGE_UP; return 1;
        case 57355: case 57422: ev->nav.kind=TUI_NAV; ev->nav.nav=TUI_NAV_PAGE_DOWN; return 1;
        case 57356: case 57423: ev->nav.kind=TUI_NAV; ev->nav.nav=TUI_NAV_HOME; return 1;
        case 57357: case 57424: ev->nav.kind=TUI_NAV; ev->nav.nav=TUI_NAV_END; return 1;
        case 57425: ev->control.kind=TUI_CONTROL; ev->control.key=TUI_CONTROL_INSERT; return 1;
        case 57426: ev->control.kind=TUI_CONTROL; ev->control.key=TUI_CONTROL_DELETE; return 1;
        case 57414: ev->control.kind=TUI_CONTROL; ev->control.key=TUI_CONTROL_ENTER; return 1;
        default: break;
    }
    if(cp >= 57409 && cp <= 57416){
        static const char symbols[] = "./*-+\0=,";
        ev->key.kind=TUI_KEY;
        ev->key.key=(unsigned char)symbols[cp - 57409];
        ev->key.keypad=1;
        return 1;
    }
    if(cp < 32 || cp > 0x10ffff || (cp >= 0xd800 && cp <= 0xdfff) ||
       (cp >= 57344 && cp <= 63743)) return -1;
    if(ev->shift && shifted){
        if(shifted < 32 || shifted > 0x10ffff || (shifted >= 0xd800 && shifted <= 0xdfff)) return -1;
        cp = shifted;
    }
    ev->key.kind=TUI_KEY;
    ev->key.key=cp;
    return 1;
}

static
int
tui_kitty_sequence(TuiEvent* ev, const unsigned char* s, uint32_t end, unsigned char final){
    uint32_t values[2][3]={{0}}, field=0, sub=0;
    for(uint32_t i=2; i<end; i++){
        unsigned char byte=s[i];
        if(byte==';'){
            if(field==1) return -1;
            field++; sub=0;
        }
        else if(byte==':'){
            if(sub==(field ? 1u : 2u)) return -1;
            sub++;
        }
        else if(byte>='0' && byte<='9'){
            uint32_t* value = &values[field][sub];
            unsigned digit = byte-'0';
            if(*value>(UINT32_MAX-digit)/10)
                return -1;
            *value = *value*10+digit;
        }
        else return -1;
    }
    uint32_t type=values[1][1];
    if(type>2) return -1; // Release events are not editing actions.
    if(tui_keyboard_modifiers(ev, values[1][0] ? values[1][0] : 1)<0)
        return -1;
    if(final=='u') return tui_kitty_key(ev, values[0][0], values[0][1]);
    if(final=='~') return tui_tilde_key(ev, values[0][0]);
    if(values[0][0]>1) return -1;
    return tui_final_key(ev, final, 0);
}

static
int
tui_parse_input(Tui* tui, TuiEvent* ev){
    if(!tui->count) return 0;
    const unsigned char* s = tui->bytes;
    if(tui->discard_sequence){
        uint32_t n = 0;
        while(n < tui->count){
            unsigned char byte = s[n++];
            if(byte >= 0x40 && byte <= 0x7e){
                tui->discard_sequence = 0;
                break;
            }
        }
        tui_consume(tui, n);
        return -1;
    }
    if(s[0] != 27)
        return tui_parse_character(tui, ev, 0);
    if(tui->count == 1){
        tui_character(ev, 27);
        tui_consume(tui, 1);
        return 1;
    }
    if(s[1] != '[' && s[1] != 'O'){
        int result = tui_parse_character(tui, ev, 1);
        if(result == 1) ev->alt = 1;
        return result;
    }
    if(s[1] == '[' && tui->count >= 3 && s[2] == 'M'){
        if(tui->count < 6){
            if(!tui->eof)
                return 0;
            tui_consume(tui, tui->count);
            return -1;
        }
        int result = -1;
        if(s[3] >= 32 && s[4] > 32 && s[5] > 32)
            result = tui_mouse_sequence(tui, ev, s[3] - 32, s[4] - 32, s[5] - 32, 0, 1);
        tui_consume(tui, 6);
        return result;
    }
    uint32_t end = 2;
    while(end < tui->count && !(s[end] >= 0x40 && s[end] <= 0x7e))
        end++;
    if(end == tui->count){
        if(tui->eof || tui->count == sizeof tui->bytes){
            tui->discard_sequence = !tui->eof;
            tui_consume(tui, tui->count);
            return -1;
        }
        return 0;
    }
    unsigned char final = s[end];
    int ss3 = s[1] == 'O';
    int sgr_mouse = !ss3 && end > 2 && s[2] == '<';
    if(!ss3 && !sgr_mouse && (final=='u' || memchr(s+2, ':', end-2))){
        int result = tui_kitty_sequence(ev, s, end, final);
        tui_consume(tui, end+1);
        return result;
    }
    uint32_t values[8] = {0},
             count = 0;
    int valid = 1;
    uint32_t start = sgr_mouse ? 3 : 2;
    if(start < end){
        count = 1;
        for(uint32_t i = start; i < end; i++){
            unsigned char byte = s[i];
            if(byte == ';'){
                if(count == 8){ valid = 0; break; }
                count++;
            }
            else if(byte >= '0' && byte <= '9'){
                uint32_t* value = &values[count - 1];
                uint32_t digit = byte - '0';
                if(*value > (UINT32_MAX - digit) / 10){ valid = 0; break; }
                *value = *value * 10 + digit;
            }
            else { valid = 0; break; }
        }
    }
    int result = -1;
    if(valid){
        if(sgr_mouse){
            if(count == 3 && (final == 'M' || final == 'm'))
                result = tui_mouse_sequence(tui, ev, values[0], values[1], values[2], final == 'm', 0);
        }
        else {
            if(count <= 2){
                if(final == '~' && count) result = tui_tilde_key(ev, values[0]);
                else if(!count || values[0] <= 1) result = tui_final_key(ev, final, ss3);
                if(result == 1 && count == 2 && tui_keyboard_modifiers(ev, values[1])<0) result=-1;
            }
        }
    }
    tui_consume(tui, end + 1);
    return result;
}

// Drain immediately readable bytes before interpreting a standalone Escape.
static
int
tui_fill_input(Tui* tui){
    while(!tui->eof && tui->count < sizeof tui->bytes){
        struct pollfd pfd = {tui->hstdin, POLLIN, 0};
        int ready = poll(&pfd, 1, 0);
        if(ready < 0){ if(errno == EINTR) continue; return errno; }
        if(!ready) return 0;
        if(pfd.revents & POLLNVAL) return EBADF;
        if(!(pfd.revents & (POLLIN | POLLHUP)))
            return pfd.revents & POLLERR ? EIO : 0;
        ssize_t n = read(tui->hstdin, tui->bytes + tui->count, sizeof tui->bytes - tui->count);
        if(n < 0){
            if(errno == EINTR) continue;
            if(errno == EAGAIN || errno == EWOULDBLOCK) return 0;
            // PTYs can report their closed peer as EIO instead of a zero read.
            if(errno == EIO && (pfd.revents & POLLHUP)){ tui->eof = 1; return 0; }
            return errno;
        }
        if(!n){
            tui->eof = 1;
            return 0;
        }
        tui->count += (uint32_t)n;
    }
    return 0;
}

#else // _WIN32

static
void
tui_windows_modifiers(TuiEvent* ev, DWORD flags){
    ev->shift = !!(flags & SHIFT_PRESSED);
    ev->ctrl = !!(flags & (LEFT_CTRL_PRESSED | RIGHT_CTRL_PRESSED));
    ev->alt = !!(flags & (LEFT_ALT_PRESSED | RIGHT_ALT_PRESSED));
}

static
int
tui_windows_key(Tui* tui, const KEY_EVENT_RECORD* key, TuiEvent* ev){
    tui->repeat_is_character = 0;
    WORD vk = key->wVirtualKeyCode;
    switch(vk){
        case VK_UP:     ev->nav.kind = TUI_NAV;         ev->nav.nav = TUI_NAV_UP;                break;
        case VK_DOWN:   ev->nav.kind = TUI_NAV;         ev->nav.nav = TUI_NAV_DOWN;              break;
        case VK_LEFT:   ev->nav.kind = TUI_NAV;         ev->nav.nav = TUI_NAV_LEFT;              break;
        case VK_RIGHT:  ev->nav.kind = TUI_NAV;         ev->nav.nav = TUI_NAV_RIGHT;             break;
        case VK_HOME:   ev->nav.kind = TUI_NAV;         ev->nav.nav = TUI_NAV_HOME;              break;
        case VK_END:    ev->nav.kind = TUI_NAV;         ev->nav.nav = TUI_NAV_END;               break;
        case VK_PRIOR:  ev->nav.kind = TUI_NAV;         ev->nav.nav = TUI_NAV_PAGE_UP;           break;
        case VK_NEXT:   ev->nav.kind = TUI_NAV;         ev->nav.nav = TUI_NAV_PAGE_DOWN;         break;
        case VK_INSERT: ev->control.kind = TUI_CONTROL; ev->control.key = TUI_CONTROL_INSERT;    break;
        case VK_DELETE: ev->control.kind = TUI_CONTROL; ev->control.key = TUI_CONTROL_DELETE;    break;
        case VK_BACK:   ev->control.kind = TUI_CONTROL; ev->control.key = TUI_CONTROL_BACKSPACE; break;
        case VK_TAB:    ev->control.kind = TUI_CONTROL; ev->control.key = TUI_CONTROL_TAB;       break;
        case VK_RETURN: ev->control.kind = TUI_CONTROL; ev->control.key = TUI_CONTROL_ENTER;     break;
        case VK_ESCAPE: ev->control.kind = TUI_CONTROL; ev->control.key = TUI_CONTROL_ESC;       break;
        default: {
            if(vk >= VK_F1 && vk <= VK_F20){
                ev->control.kind = TUI_CONTROL;
                ev->control.key = (TuiControlKind)(TUI_CONTROL_F1 + vk - VK_F1);
                break;
            }
            uint32_t cp = key->uChar.UnicodeChar;
            if(cp >= 0xd800 && cp <= 0xdbff){ tui->high_surrogate = (WORD)cp; return 0; }
            if(cp >= 0xdc00 && cp <= 0xdfff){
                if(tui->high_surrogate)
                    cp = 0x10000 + (((uint32_t)tui->high_surrogate - 0xd800) << 10) + cp - 0xdc00;
                else cp = 0xfffd;
            }
            else if(!cp && !(vk == VK_SPACE && (key->dwControlKeyState & (LEFT_CTRL_PRESSED | RIGHT_CTRL_PRESSED))))
                return 0;
            tui->high_surrogate = 0;
            tui_character(ev, cp);
            tui->repeat_codepoint = cp;
            tui->repeat_is_character = 1;
            if(ev->kind == TUI_KEY)
                ev->key.keypad = (vk >= VK_NUMPAD0 && vk <= VK_DIVIDE);
            break;
        }
    }
    tui->high_surrogate = 0;
    tui_windows_modifiers(ev, key->dwControlKeyState);
    return 1;
}

static
uint32_t
tui_windows_buttons(DWORD buttons){
    return (!!(buttons & FROM_LEFT_1ST_BUTTON_PRESSED)) |
           ((uint32_t)!!(buttons & FROM_LEFT_2ND_BUTTON_PRESSED) << 1) |
           ((uint32_t)!!(buttons & RIGHTMOST_BUTTON_PRESSED) << 2);
}

static
void
tui_windows_position(Tui* tui, COORD position, uint32_t* x, uint32_t* y){
    CONSOLE_SCREEN_BUFFER_INFO info;
    int cx = position.X, cy = position.Y;
    if(GetConsoleScreenBufferInfo(tui->hstdout, &info)){
        cx -= info.srWindow.Left;
        cy -= info.srWindow.Top;
    }
    *x = cx < 0 ? 0 : (uint32_t)cx;
    *y = cy < 0 ? 0 : (uint32_t)cy;
}

static
int
tui_windows_mouse(Tui* tui, TuiEvent* ev){
    MOUSE_EVENT_RECORD* mouse = &tui->pending_mouse;
    uint32_t buttons = tui_windows_buttons(mouse->dwButtonState);
    if(mouse->dwEventFlags == MOUSE_WHEELED || mouse->dwEventFlags == MOUSE_HWHEELED){
        int32_t delta = (SHORT)HIWORD(mouse->dwButtonState);
        int32_t* remainder = mouse->dwEventFlags == MOUSE_WHEELED ? &tui->wheel_y : &tui->wheel_x;
        *remainder += delta;
        int32_t steps = *remainder / WHEEL_DELTA;
        *remainder %= WHEEL_DELTA;
        tui->mouse_pending = 0;
        if(!steps) return 0;
        ev->scroll.kind = TUI_SCROLL;
        if(mouse->dwEventFlags == MOUSE_WHEELED) ev->scroll.dy = -steps;
        else ev->scroll.dx = steps;
        tui_windows_position(tui, mouse->dwMousePosition, &ev->scroll.cx, &ev->scroll.cy);
    }
    else {
        ev->mouse.kind = TUI_MOUSE;
        tui_windows_position(tui, mouse->dwMousePosition, &ev->mouse.cx, &ev->mouse.cy);
        uint32_t changed = buttons ^ tui->mouse_buttons;
        if(changed){
            unsigned button = tui_first_button(changed);
            ev->mouse.which = button & 3u;
            ev->mouse.mouse = buttons & (1u << button) ? TUI_MOUSE_DOWN : TUI_MOUSE_UP;
            tui->mouse_buttons ^= 1u << button;
            tui->mouse_pending = tui->mouse_buttons != buttons;
        }
        else {
            tui->mouse_pending = 0;
            if(mouse->dwEventFlags != MOUSE_MOVED){ *ev = (TuiEvent){0}; return 0; }
            ev->mouse.which = tui_first_button(buttons) & 3u;
            ev->mouse.mouse = buttons ? TUI_MOUSE_DRAG : TUI_MOUSE_MOVE;
        }
    }
    tui_windows_modifiers(ev, mouse->dwControlKeyState);
    return 1;
}

static
int
tui_windows_poll(Tui* tui, TuiEvent* ev){
    for(;;){
        if(tui->mouse_pending){
            if(tui_windows_mouse(tui, ev)) return 0;
        }
        if(tui->repeats){
            tui->repeats--;
            if(tui->repeat_is_character){
                tui_character(ev, tui->repeat_codepoint);
                if(ev->kind == TUI_KEY)
                    ev->key.keypad = tui->repeat.wVirtualKeyCode >= VK_NUMPAD0 && tui->repeat.wVirtualKeyCode <= VK_DIVIDE;
                tui_windows_modifiers(ev, tui->repeat.dwControlKeyState);
                return 0;
            }
            if(tui_windows_key(tui, &tui->repeat, ev)) return 0;
        }
        DWORD ready = WaitForSingleObject(tui->hstdin, 0);
        if(ready == WAIT_TIMEOUT){ ev->kind = TUI_NO_INPUT; return 0; }
        if(ready == WAIT_FAILED) return (int)GetLastError();
        if(ready != WAIT_OBJECT_0) return ERROR_INVALID_HANDLE;
        INPUT_RECORD record;
        DWORD count;
        if(!ReadConsoleInputW(tui->hstdin, &record, 1, &count)) return (int)GetLastError();
        if(!count){ ev->kind = TUI_EOF; return 0; }
        switch(record.EventType){
            case KEY_EVENT:
                if(!record.Event.KeyEvent.bKeyDown) break;
                tui->repeat = record.Event.KeyEvent;
                if(tui_windows_key(tui, &tui->repeat, ev)){
                    tui->repeats = tui->repeat.wRepeatCount > 1 ? tui->repeat.wRepeatCount - 1 : 0;
                    return 0;
                }
                break;
            case MOUSE_EVENT:
                tui->pending_mouse = record.Event.MouseEvent;
                tui->mouse_pending = 1;
                break;
            case WINDOW_BUFFER_SIZE_EVENT:
                if(tui_check_resize(tui, ev)) return 0;
                break;
            default: break;
        }
    }
}
#endif

#ifdef __clang__
#pragma clang assume_nonnull end
#endif
#include <stdarg.h>
#include <string.h>
#include <stdint.h>
#include <limits.h>
#ifdef _WIN32
#include "../Drp/windowsheader.h"
#else
#include <unistd.h>
#include <errno.h>
#endif
#ifdef STB_SPRINTF_H_INCLUDE
#define _tui_vsnprintf stbsp_vsnprintf
#else
#include <stdio.h>
#define _tui_vsnprintf vsnprintf
#endif

#ifdef __clang__
#pragma clang assume_nonnull begin
#endif


static inline
int
tui_allocation_sizes(int w, int h, size_t* cells, size_t* bytes, size_t* output){
    if(w <= 0 || h <= 0 || (size_t)w > SIZE_MAX / (size_t)h) return -1;
    *cells = (size_t)w * (size_t)h;
    if(*cells > SIZE_MAX / sizeof(TuiCell) || *cells > SIZE_MAX / 32) return -1;
    *bytes = *cells * sizeof(TuiCell);
    *output = *cells * 32;
    return 0;
}

static inline
_Bool
tui_visible(Tui* tui, int x, int y, int width){
    return width > 0 && x >= 0 && y >= 0 && (uint32_t)y < tui->canvas.h &&
           (uint32_t)x < tui->canvas.w && (uint32_t)width <= tui->canvas.w - (uint32_t)x;
}

static inline
_Bool
tui_clip_test(Tui* tui, int x, int y, int width){
    if(!tui_visible(tui, x, y, width)) return 0;
    TuiPaintState* state = &tui->canvas.state_stack[tui->canvas.state_cursor];
    if(!state->scissor_enabled) return 1;
    return x >= state->scissor.x && y >= state->scissor.y &&
           (int64_t)x + width <= (int64_t)state->scissor.x + state->scissor.w &&
           (int64_t)y < (int64_t)state->scissor.y + state->scissor.h;
}

static inline
TuiCell* _Nullable
tui_current_cell(Tui* tui){
    if(!tui->canvas.cells[tui->canvas.active_cells]) return NULL;
    if(!tui_visible(tui, tui->canvas.x, tui->canvas.y, 1)) return NULL;
    return &tui->canvas.cells[tui->canvas.active_cells][(size_t)tui->canvas.x+(size_t)tui->canvas.y*(size_t)tui->canvas.alloc_w];
}
static inline
TuiCell* _Nullable
tui_old_cell(Tui* tui){
    if(!tui->canvas.cells[!tui->canvas.active_cells]) return NULL;
    if(!tui_visible(tui, tui->canvas.x, tui->canvas.y, 1)) return NULL;
    return &tui->canvas.cells[!tui->canvas.active_cells][(size_t)tui->canvas.x+(size_t)tui->canvas.y*(size_t)tui->canvas.alloc_w];
}

static inline
TuiPaintState*
tui_current_state(Tui* tui){
    return &tui->canvas.state_stack[tui->canvas.state_cursor];
}

static inline
void
tui_sprintf(Tui* tui, const char* fmt, ...){
    if(!tui->canvas.buff.data || tui->canvas.buff.errored) return;
    va_list va, retry;
    va_start(va, fmt);
    va_copy(retry, va);
    char* buff = tui->canvas.buff.data + tui->canvas.buff.cursor;
    size_t remainder = tui->canvas.buff.capacity - tui->canvas.buff.cursor;
    size_t capacity = remainder;
    ptrdiff_t n = _tui_vsnprintf(buff, capacity, fmt, va);
    va_end(va);
    if(n < 0 || (size_t)n > SIZE_MAX - tui->canvas.buff.cursor - 1){
        tui->canvas.buff.errored = 1;
    }
    else if((size_t)n >= capacity){
        size_t needed = tui->canvas.buff.cursor + (size_t)n + 1;
        size_t cap = tui->canvas.buff.capacity <= SIZE_MAX / 2 ? tui->canvas.buff.capacity * 2 : SIZE_MAX;
        if(cap < needed) cap = needed;
        char* new_buff = Allocator_realloc(tui->canvas.buff.allocator, tui->canvas.buff.data, tui->canvas.buff.capacity, cap, 1, 1);
        if(!new_buff){
            tui->canvas.buff.errored = 1;
        }
        else {
            tui->canvas.buff.data = new_buff;
            tui->canvas.buff.capacity = cap;
            remainder = cap - tui->canvas.buff.cursor;
            capacity = remainder;
            n = _tui_vsnprintf(new_buff + tui->canvas.buff.cursor, capacity, fmt, retry);
            if(n < 0 || (size_t)n >= capacity) tui->canvas.buff.errored = 1;
        }
    }
    va_end(retry);
    if(!tui->canvas.buff.errored) tui->canvas.buff.cursor += (size_t)n;
}

static inline
void
tui_flush(Tui* tui){
    if(tui_visible(tui, tui->canvas.cursor_x, tui->canvas.cursor_y, 1))
        tui_sprintf(tui, "\x1b[%d;%dH", tui->canvas.cursor_y+1, tui->canvas.cursor_x+1);
    if(tui->canvas.buff.errored) return;
    if(tui_write(tui, tui->canvas.buff.data, tui->canvas.buff.cursor)){
        tui->canvas.buff.errored = 1;
        return;
    }
    tui->canvas.buff.cursor = 0;
    tui->canvas.force_paint = 0;
    tui->canvas.cursor_dirty = 0;
}

typedef struct TuiPaint TuiPaint;
struct TuiPaint {
    TuiPaintState state;
    int x, y;
};

void
tui_set_256_color(Tui* tui, _Bool enabled){
    if(tui->canvas.color_256 == enabled) return;
    tui->canvas.color_256 = enabled;
    tui_invalidate(tui);
}

static inline
unsigned
tui_color_cube_component(unsigned char c){
    return c < 48 ? 0 : c < 115 ? 1 : c < 155 ? 2 :
           c < 195 ? 3 : c < 235 ? 4 : 5;
}

static inline
unsigned
tui_color_256(TuiColor color){
    static const int levels[] = {0, 95, 135, 175, 215, 255};
    unsigned r = tui_color_cube_component(color.rgb.r);
    unsigned g = tui_color_cube_component(color.rgb.g);
    unsigned b = tui_color_cube_component(color.rgb.b);
    int dr = color.rgb.r - levels[r],
        dg = color.rgb.g - levels[g],
        db = color.rgb.b - levels[b];
    int cube_distance = dr*dr + dg*dg + db*db;
    int gray = ((int)color.rgb.r + color.rgb.g + color.rgb.b - 24 + 15) / 30;
    if(gray < 0) gray = 0;
    if(gray > 23) gray = 23;
    int level = 8 + 10*gray;
    dr = color.rgb.r - level;
    dg = color.rgb.g - level;
    db = color.rgb.b - level;
    if(dr*dr + dg*dg + db*db < cube_distance) return 232 + gray;
    return 16 + 36*r + 6*g + b;
}

static inline
void
tui_paint_update(Tui* tui, TuiPaint* p, int x, int y, TuiCell* new){
    if(x != p->x || y != p->y){
        // Goto coord
        int term_x = x+1;
        int term_y = y+1;
        tui_sprintf(tui, "\x1b[%d;%dH", term_y, term_x);
    }
    _Bool started = 0;
    if(p->state.style.bits != new->style){
        // set style
        tui_sprintf(tui, "\x1b[0;");
        started = 1;
        p->state.color = (TuiColor){0};
        p->state.bg_color = (TuiColor){0};
        if(new->style & 1){
            tui_sprintf(tui, "1;");
        }
        if(new->style & 2){
            tui_sprintf(tui, "3;");
        }
        if(new->style & 4){
            tui_sprintf(tui, "4;");
        }
        if(new->style & 8){
            tui_sprintf(tui, "9;");
        }
    }
    if(p->state.color.bits != new->color.bits){
        if(!started){
            started = 1;
            tui_sprintf(tui, "\x1b[");
        }
        // set foreground color
        if(!new->color.is_set){
            tui_sprintf(tui, "39;");
        }
        else if(new->color.is_8bit){
            tui_sprintf(tui, "38;5;%u;", new->color._8bit.color_index);
        }
        else if(tui->canvas.color_256){
            tui_sprintf(tui, "38;5;%u;", tui_color_256(new->color));
        }
        else {
            tui_sprintf(tui, "38;2;%d;%d;%d;", new->color.rgb.r, new->color.rgb.g, new->color.rgb.b);
        }
    }
    if(p->state.bg_color.bits != new->bg_color.bits){
        if(!started){
            started = 1;
            tui_sprintf(tui, "\x1b[");
        }
        // set background color
        if(!new->bg_color.is_set){
            tui_sprintf(tui, "49;");
        }
        else if(new->bg_color.is_8bit){
            tui_sprintf(tui, "48;5;%u;", new->bg_color._8bit.color_index);
        }
        else if(tui->canvas.color_256){
            tui_sprintf(tui, "48;5;%u;", tui_color_256(new->bg_color));
        }
        else {
            tui_sprintf(tui, "48;2;%d;%d;%d;", new->bg_color.rgb.r, new->bg_color.rgb.g, new->bg_color.rgb.b);
        }
    }
    if(started && !tui->canvas.buff.errored){
        tui->canvas.buff.data[tui->canvas.buff.cursor-1] = 'm';
    }
    // write char
    if((unsigned)(unsigned char)new->txt[0] <= 0x20u)
        tui_sprintf(tui, " ");
    else if((unsigned)(unsigned char)new->txt[0] == 0x7fu)
        tui_sprintf(tui, " ");
    else
        tui_sprintf(tui, "%.*s", (int)sizeof new->txt, new->txt);
    p->x = x+(new->rend_width?new->rend_width:1);
    p->y = y;
    p->state.style.bits = new->style;
    p->state.color = new->color;
    p->state.bg_color = new->bg_color;
}

int
tui_paint(Tui* tui){
    if(!tui->canvas.cells[0] || !tui->canvas.cells[1] || tui->canvas.buff.errored) return -1;
    if(!tui->canvas.dirty && !tui->canvas.force_paint){
        if(!tui->canvas.cursor_dirty) return 0;
        tui_sprintf(tui, tui->canvas.cursor_visible && tui_visible(tui, tui->canvas.cursor_x, tui->canvas.cursor_y, 1) ? "\x1b[?25h" : "\x1b[?25l");
        tui_flush(tui);
        return tui->canvas.buff.errored ? -1 : 0;
    }
    tui_sprintf(tui, "\x1b[?25l");
    tui_sprintf(tui, "\x1b[?2026h");
    tui_sprintf(tui, "\x1b[0m");
    TuiPaint paint = {.x = -1, .y=-1};
    for(int y = 0; y < (int)tui->canvas.h; y++)
        for(int x = 0; x < (int)tui->canvas.w; x++){
            if(!tui_visible(tui, x, y, 1)) continue;
            size_t index = (size_t)x + (size_t)y * (size_t)tui->canvas.alloc_w;
            TuiCell* old = &tui->canvas.cells[!tui->canvas.active_cells][index];
            TuiCell cell = tui->canvas.cells[tui->canvas.active_cells][index];
            if(!cell.rend_width) cell.rend_width = 1;
            if(!tui_visible(tui, x, y, cell.rend_width) ||
               (unsigned char)cell.txt[0] <= 0x20 || (unsigned char)cell.txt[0] == 0x7f){
                memset(cell.txt, 0, sizeof cell.txt);
                cell.txt[0] = ' ';
                cell.rend_width = 1;
            }
            TuiCell continuation = cell;
            memset(continuation.txt, 0, sizeof continuation.txt);
            continuation.rend_width = 0;
            continuation.continuation = 1;
            int changed = tui->canvas.force_paint || memcmp(old, &cell, sizeof cell);
            for(unsigned i = 1; i < cell.rend_width; i++)
                if(memcmp(old + i, &continuation, sizeof continuation)) changed = 1;
            if(changed) tui_paint_update(tui, &paint, x, y, &cell);
            *old = cell;
            for(unsigned i = 1; i < cell.rend_width; i++) old[i] = continuation;
            x += cell.rend_width - 1;
        }
    if(tui->canvas.cursor_visible && tui_visible(tui, tui->canvas.cursor_x, tui->canvas.cursor_y, 1)){
        tui_sprintf(tui, "\x1b[?25h");
    }
    tui_sprintf(tui, "\x1b[0m");
    tui_sprintf(tui, "\x1b[?2026l");
    tui_flush(tui);
    if(tui->canvas.buff.errored) return -1;
    tui->canvas.dirty = 0;
    return 0;
}

static void tui_clear_at(Tui*, int, int);

void
tui_clear_screen(Tui* tui){
    if(!tui->canvas.cells[tui->canvas.active_cells]) return;
    for(int y = 0; y < (int)tui->canvas.h; y++)
        for(int x = 0; x < (int)tui->canvas.w; x++) tui_clear_at(tui, x, y);
}

void
tui_invalidate(Tui* tui){
    tui->canvas.force_paint = 1;
}

void
tui_move(Tui* tui, int x, int y){
    tui->canvas.x = x;
    tui->canvas.y = y;
}

void
tui_cursor(Tui* tui, int* x, int* y){
    *x = tui->canvas.x;
    *y = tui->canvas.y;
}

void
tui_push_state(Tui* tui){
    if(tui->canvas.state_cursor +1 >= sizeof tui->canvas.state_stack / sizeof tui->canvas.state_stack[0]){
        // __builtin_debugtrap();
        return;
    }
    tui->canvas.state_stack[tui->canvas.state_cursor+1] = tui->canvas.state_stack[tui->canvas.state_cursor];
    tui->canvas.state_cursor++;
}

void
tui_pop_state(Tui* tui){
    if(tui->canvas.state_cursor) --tui->canvas.state_cursor;
    // else __builtin_debugtrap();
}

void
tui_pop_all_states(Tui* tui){
    tui->canvas.state_cursor = 0;
    *tui_current_state(tui) = (TuiPaintState){.bg_alpha=255};
}

void
tui_clear_state(Tui* tui){
    *tui_current_state(tui) = (TuiPaintState){.bg_alpha=255};
}


void
tui_scissor(Tui* tui, int x, int y, uint32_t w, uint32_t h){
    TuiPaintState* s = tui_current_state(tui);
    s->scissor.x = x;
    s->scissor.y = y;
    s->scissor.w = w > 0 ? w : 0;
    s->scissor.h = h > 0 ? h : 0;
    s->scissor_enabled = 1;
}

void
tui_clear_scissor(Tui* tui){
    tui_current_state(tui)->scissor_enabled = 0;
}

void
tui_set_style(Tui* tui, TuiStyle style){
    // if(tui->canvas.state_cursor == 0) __builtin_debugtrap();
    tui_current_state(tui)->style.bits = style.bits & 15;
}

void
tui_clear_color(Tui* tui){
    tui_current_state(tui)->color = (TuiColor){0};
}

void
tui_set_24bit_color(Tui* tui, unsigned char r, unsigned char g, unsigned char b){
    tui_current_state(tui)->color = (TuiColor){.rgb={.r=r, .g=g, .b=b, .is_set=1}};
}

void
tui_bg_clear_color(Tui* tui){
    tui_current_state(tui)->bg_color = (TuiColor){0};
}

void
tui_bg_set_24bit_color(Tui* tui, unsigned char r, unsigned char g, unsigned char b){
    TuiPaintState* s = tui_current_state(tui);
    s->bg_color = (TuiColor){.rgb={.r=r, .g=g, .b=b, .is_set=1}};
    s->bg_alpha = 255;
}

void
tui_bg_set_color_rgba(Tui* tui, unsigned char r, unsigned char g, unsigned char b, unsigned char a){
    TuiPaintState* s = tui_current_state(tui);
    s->bg_color = (TuiColor){.rgb={.r=r, .g=g, .b=b, .is_set=1}};
    s->bg_alpha = a;
}

void
tui_bg_set_color_uint32(Tui* tui, uint32_t c){ // 0xRRGGBBAA
    TuiPaintState* s = tui_current_state(tui);
    s->bg_alpha = (unsigned char)(c & 0xff);
    s->bg_color.is_set = 1;
    c >>= 8;
    s->bg_color.rgb.b = (unsigned char)(c & 0xff);
    c >>= 8;
    s->bg_color.rgb.g = (unsigned char)(c & 0xff);
    c >>= 8;
    s->bg_color.rgb.r = (unsigned char)(c & 0xff);
}

static inline
TuiColor
tui_blend_bg(TuiColor new_bg, unsigned char alpha, TuiColor old_bg){
    if(alpha == 255) return new_bg;
    if(alpha == 0 || !new_bg.is_set) return old_bg;
    if(!old_bg.is_set) return new_bg;
    // Blend: result = new * alpha + old * (255 - alpha)
    unsigned a = alpha;
    unsigned inv_a = 255 - alpha;
    unsigned char r = (unsigned char)((new_bg.rgb.r * a + old_bg.rgb.r * inv_a) / 255);
    unsigned char g = (unsigned char)((new_bg.rgb.g * a + old_bg.rgb.g * inv_a) / 255);
    unsigned char b = (unsigned char)((new_bg.rgb.b * a + old_bg.rgb.b * inv_a) / 255);
    return (TuiColor){.rgb={.r=r, .g=g, .b=b, .is_set=1}};
}

// Validate the whole glyph and any overlapping wide glyphs before changing
// anything. Scissoring through an existing wide glyph must not erase its other
// half outside the clipping rectangle.
static
int
tui_prepare_write(Tui* tui, int x, int y, int width){
    if(!tui->canvas.cells[tui->canvas.active_cells] || !tui_clip_test(tui, x, y, width)) return 0;
    TuiCell* row = tui->canvas.cells[tui->canvas.active_cells] + (size_t)y * (size_t)tui->canvas.alloc_w;
    TuiPaintState* state = tui_current_state(tui);
    int first = x > 6 ? x - 6 : 0;
    for(int i = first; i < x + width; i++){
        int span = row[i].rend_width;
        if(span <= 1 || (int64_t)i + span <= x) continue;
        if(state->scissor_enabled &&
           (i < state->scissor.x || (int64_t)i + span > (int64_t)state->scissor.x + state->scissor.w))
            return 0;
    }
    for(int i = first; i < x + width; i++){
        if(i == x || row[i].rend_width <= 1 || (int64_t)i + row[i].rend_width <= x) continue;
        memset(row[i].txt, 0, sizeof row[i].txt);
        row[i].txt[0] = ' ';
        row[i].rend_width = 1;
        tui->canvas.dirty = 1;
    }
    return 1;
}

static
void
tui_write_glyph(Tui* tui, int x, int y, const char* text, size_t length, size_t width){
    if(!length || length > 7 || width < 1 || width > 7) return;
    if(!tui_prepare_write(tui, x, y, (int)width)) return;
    size_t index = (size_t)x + (size_t)y * (size_t)tui->canvas.alloc_w;
    TuiCell* cell = &tui->canvas.cells[tui->canvas.active_cells][index];
    TuiCell* old = &tui->canvas.cells[!tui->canvas.active_cells][index];
    TuiPaintState* state = tui_current_state(tui);
    memset(cell->txt, 0, sizeof cell->txt);
    memcpy(cell->txt, text, length);
    cell->color = state->color;
    cell->bg_color = tui_blend_bg(state->bg_color, state->bg_alpha, cell->bg_color);
    cell->style = (unsigned char)(state->style.bits & 15);
    cell->rend_width = (unsigned char)(width & 7);
    cell->continuation = 0;
    if(memcmp(cell, old, sizeof *cell)) tui->canvas.dirty = 1;
}

static
void
tui_clear_at(Tui* tui, int x, int y){
    if(!tui_prepare_write(tui, x, y, 1)) return;
    size_t index = (size_t)x + (size_t)y * (size_t)tui->canvas.alloc_w;
    TuiCell blank = {0};
    blank.txt[0] = ' ';
    blank.rend_width = 1;
    tui->canvas.cells[tui->canvas.active_cells][index] = blank;
    if(memcmp(&blank, &tui->canvas.cells[!tui->canvas.active_cells][index], sizeof blank)) tui->canvas.dirty = 1;
}

static
void
tui_advance(Tui* tui, size_t width){
    if(width > INT_MAX){ tui->canvas.x = INT_MAX; return; }
    int64_t next = (int64_t)tui->canvas.x + (int64_t)width;
    tui->canvas.x = next > INT_MAX ? INT_MAX : (int)next;
}

void
tui_setc(Tui* tui, char c){
    tui_write_glyph(tui, tui->canvas.x, tui->canvas.y, &c, 1, 1);
}

void
tui_setc_at(Tui* tui, int x, int y, char c){
    tui_write_glyph(tui, x, y, &c, 1, 1);
}

void
tui_put_ascii(Tui* tui, char c){
    tui_setc(tui, c);
    tui_advance(tui, 1);
}

void
tui_fill_ascii_rect(Tui* d, int x, int y, int w, int h, char c){
    if(w <= 0 || h <= 0 || !d->canvas.cells[d->canvas.active_cells]) return;
    TuiPaintState* state = tui_current_state(d);
    int64_t cl = 0, ct = 0, cr = d->canvas.w, cb = d->canvas.h;
    if(state->scissor_enabled){
        if(cl < state->scissor.x) cl = state->scissor.x;
        if(ct < state->scissor.y) ct = state->scissor.y;
        if(cr > (int64_t)state->scissor.x+state->scissor.w) cr = (int64_t)state->scissor.x+state->scissor.w;
        if(cb > (int64_t)state->scissor.y+state->scissor.h) cb = (int64_t)state->scissor.y+state->scissor.h;
    }
    int64_t left = x > cl ? x : cl, top = y > ct ? y : ct;
    int64_t right = (int64_t)x+w, bottom = (int64_t)y+h;
    if(right > cr) right = cr;
    if(bottom > cb) bottom = cb;
    if(right <= left || bottom <= top) return;
    TuiCell fill = {0};
    fill.txt[0] = c; fill.rend_width = 1;
    fill.style = (unsigned char)(state->style.bits & 15);
    fill.color = state->color; fill.bg_color = state->bg_color;
    for(int64_t row_y = top; row_y < bottom; row_y++){
        size_t offset = (size_t)row_y*(size_t)d->canvas.alloc_w;
        TuiCell* row = d->canvas.cells[d->canvas.active_cells]+offset;
        int first = (int)left, end = (int)right;
        // Only boundary glyphs can cross the clip; interior cells need no
        // repeated clipping/overlap checks. Widths are at most seven cells.
        int scan = first > 6 ? first-6 : 0;
        for(int i = scan; i < first; i++){
            int span = row[i].rend_width;
            if(span > 1 && (int64_t)i+span > first &&
               state->scissor_enabled && (i < state->scissor.x ||
                   (int64_t)i+span > (int64_t)state->scissor.x+state->scissor.w)) first = i+span;
        }
        scan = end > 6 ? end-6 : 0;
        for(int i = scan; i < end; i++)
            if(state->scissor_enabled && row[i].rend_width > 1 &&
               (int64_t)i+row[i].rend_width > (int64_t)state->scissor.x+state->scissor.w){ end = i; break; }
        if(first >= end || !tui_prepare_write(d,first,(int)row_y,1)) continue;
        size_t count = (size_t)(end-first);
        if(state->bg_alpha == 255){
            row[first] = fill;
            for(size_t filled = 1; filled < count;){
                size_t chunk = filled < count-filled ? filled : count-filled;
                memcpy(row+first+filled,row+first,chunk*sizeof *row);
                filled += chunk;
            }
        }
        else for(int i = first; i < end; i++){
            TuiColor bg = tui_blend_bg(state->bg_color,state->bg_alpha,row[i].bg_color);
            row[i] = fill; row[i].bg_color = bg;
        }
        if(memcmp(row+first,d->canvas.cells[!d->canvas.active_cells]+offset+first,count*sizeof *row)) d->canvas.dirty = 1;
    }
}

void
tui_setc_mb(Tui* tui, const char* c, size_t length, size_t rendwidth){
    tui_write_glyph(tui, tui->canvas.x, tui->canvas.y, c, length, rendwidth);
}
void
tui_putc_mb(Tui* tui, const char* c, size_t length, size_t rend_width){
    tui_setc_mb(tui, c, length, rend_width);
    tui_advance(tui, rend_width);
}

void
tui_puts_utf8(Tui* tui, const char* str, size_t length){
    const unsigned char* s = (const unsigned char*)str;
    const unsigned char* end = s + length;
    while(s != end){
        if(*s < 128){
            // ASCII character
            tui_put_ascii(tui, (char)*s);
            s++;
        }
        else {
            // Multi-byte UTF-8 sequence
            size_t len = 0;
            if((*s & 0xe0) == 0xc0) len = 2;
            else if((*s & 0xf0) == 0xe0) len = 3;
            else if((*s & 0xf8) == 0xf0) len = 4;
            else {
                s++; // Invalid, skip
                continue;
            }

            if(len > (size_t)(end - s)) break;
            // Use tui_putc_mb with render width of 1 for most chars
            tui_putc_mb(tui, (const char*)s, len, 1);
            s += len;
        }
    }
}

void
tui_set_cursor_visibility(Tui* tui, _Bool show){
    if(tui->canvas.cursor_visible != show) tui->canvas.cursor_dirty = 1;
    tui->canvas.cursor_visible = show;
}

void
tui_move_cursor(Tui* tui, int x, int y){
    if(tui->canvas.cursor_x != x || tui->canvas.cursor_y != y) tui->canvas.cursor_dirty = 1;
    tui->canvas.cursor_x = x;
    tui->canvas.cursor_y = y;
}

void
tui_printf(Tui* tui, const char* fmt, ...){
    char buff[1024];
    va_list va;
    va_start(va, fmt);
    ptrdiff_t n = _tui_vsnprintf(buff, sizeof buff, fmt, va);
    va_end(va);
    if(n < 0) return;
    tui_puts_utf8(tui, buff, (size_t)n < sizeof buff ? (size_t)n : sizeof buff - 1);
}
void
tui_clear_to_end_of_row(Tui*tui){
    if(!tui->canvas.cells[tui->canvas.active_cells] || tui->canvas.y < 0 || (uint32_t)tui->canvas.y >= tui->canvas.h) return;
    int first = tui->canvas.x < 0 ? 0 : tui->canvas.x;
    TuiPaintState* state = tui_current_state(tui);
    TuiPaintState saved = *state;
    state->color = (TuiColor){0}; state->bg_color = (TuiColor){0};
    state->style.bits = 0; state->bg_alpha = 255;
    tui_fill_ascii_rect(tui,first,tui->canvas.y,tui->canvas.w-first,1,' ');
    *state = saved;
}

static
int
tui_resize_canvas(Tui* tui, uint32_t w, uint32_t h){
    if(w > INT_MAX || h > INT_MAX) return -1;
    TuiCanvas* c = &tui->canvas;
    if(c->buff.allocator.type == ALLOCATOR_UNSET)
        c->buff.allocator = (Allocator){.type=ALLOCATOR_MALLOC};
    if(w > c->alloc_w || h > c->alloc_h){
        uint32_t nw = w > c->alloc_w ? w : c->alloc_w;
        uint32_t nh = h > c->alloc_h ? h : c->alloc_h;
        size_t count, bytes, output;
        if(tui_allocation_sizes(nw, nh, &count, &bytes, &output)) return -1;
        TuiCell* cells[2] = {
            Allocator_zalloc(c->buff.allocator, count, sizeof *c->cells[0], _Alignof(TuiCell)),
            Allocator_zalloc(c->buff.allocator, count, sizeof *c->cells[0], _Alignof(TuiCell)),
        };
        if(!cells[0] || !cells[1]){
            for(int i = 0; i < 2; i++)
                if(cells[i]) Allocator_free(c->buff.allocator, cells[i], count, sizeof *cells[i], _Alignof(TuiCell));
            return -1;
        }
        for(int i = 0; i < 2; i++){
            for(uint32_t y = 0; y < c->alloc_h; y++)
                memcpy(cells[i] + (size_t)y*nw, c->cells[i] + (size_t)y*c->alloc_w,
                    (size_t)c->alloc_w*sizeof(TuiCell));
            if(c->cells[i])
                Allocator_free(c->buff.allocator, c->cells[i], (size_t)c->alloc_w*c->alloc_h, sizeof *c->cells[i], _Alignof(TuiCell));
            c->cells[i] = cells[i];
        }
        c->alloc_w = nw;
        c->alloc_h = nh;
    }
    if(msb_ensure_additional(&c->buff, 128)) return -1;
    c->w = w;
    c->h = h;
    c->force_paint = 1;
    return 0;
}

static
int
tui_check_resize(Tui* tui, TuiEvent* ev){
    uint32_t w, h;
    if(tui_dimensions(tui, &w, &h)) return 0;
    if(w == tui->columns && h == tui->rows) return 0;
    if(tui_resize_canvas(tui, w, h)) return -1;
    tui->columns = w;
    tui->rows = h;
    ev->resize.kind = TUI_RESIZE;
    ev->resize.w = w;
    ev->resize.h = h;
    return 1;
}

#ifndef _WIN32
static
int
tui_stop_events(Tui* tui){
    if(!tui->events_active) return 0;
    close(tui->event_fd);
    tui->event_fd = -1;
    tui->events_active = 0;
#ifdef __linux__
    if(!tui->winch_was_blocked){
        sigset_t mask;
        sigemptyset(&mask);
        sigaddset(&mask, SIGWINCH);
        if(sigprocmask(SIG_UNBLOCK, &mask, NULL) < 0) return errno;
    }
#endif
    return 0;
}

static
int
tui_start_events(Tui* tui){
#ifdef __linux__
    sigset_t mask, previous;
    sigemptyset(&mask);
    sigaddset(&mask, SIGWINCH);
    if(sigprocmask(SIG_BLOCK, &mask, &previous) < 0) return errno;
    tui->winch_was_blocked = sigismember(&previous, SIGWINCH) == 1;
    tui->event_fd = signalfd(-1, &mask, SFD_NONBLOCK | SFD_CLOEXEC);
    if(tui->event_fd < 0){
        int err = errno;
        if(!tui->winch_was_blocked) sigprocmask(SIG_UNBLOCK, &mask, NULL);
        return err;
    }
#else
    tui->event_fd = kqueue();
    if(tui->event_fd < 0) return errno;
    struct kevent events[2];
    EV_SET(&events[0], (uintptr_t)tui->hstdin, EVFILT_READ, EV_ADD, 0, 0, NULL);
    EV_SET(&events[1], SIGWINCH, EVFILT_SIGNAL, EV_ADD, 0, 0, NULL);
    if(fcntl(tui->event_fd, F_SETFD, FD_CLOEXEC) < 0 ||
       kevent(tui->event_fd, events, 2, NULL, 0, NULL) < 0){
        int err = errno;
        close(tui->event_fd);
        tui->event_fd = -1;
        return err;
    }
#endif
    tui->events_active = 1;
    return 0;
}

static
int
tui_read_events(Tui* tui, int timeout_ms){
#ifdef __linux__
    struct pollfd fds[] = {
        {tui->hstdin, POLLIN, 0},
        {tui->event_fd, POLLIN, 0},
    };
    if(poll(fds, 2, timeout_ms) < 0) return errno == EINTR ? 0 : errno;
    if((fds[0].revents | fds[1].revents) & POLLNVAL) return EBADF;
    if(fds[1].revents & (POLLERR | POLLHUP)) return EIO;
    if(fds[1].revents & POLLIN){
        struct signalfd_siginfo info;
        for(;;){
            ssize_t n = read(tui->event_fd, &info, sizeof info);
            if(n == sizeof info) continue;
            if(n < 0 && errno == EINTR) continue;
            if(n < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) break;
            return n < 0 ? errno : EIO;
        }
    }
#else
    struct kevent events[2];
    struct timespec timeout = {timeout_ms > 0 ? timeout_ms / 1000 : 0,
                              timeout_ms > 0 ? (timeout_ms % 1000) * 1000000L : 0};
    int n = kevent(tui->event_fd, NULL, 0, events, 2, timeout_ms < 0 ? NULL : &timeout);
    if(n < 0) return errno == EINTR ? 0 : errno;
    for(int i = 0; i < n; i++)
        if(events[i].flags & EV_ERROR) return (int)events[i].data;
#endif
    return 0;
}
#endif

static
int
tui_pause(Tui* tui){
    if(!tui->enabled) return 0;
    const char restore[] = "\033[0m\033[?2026l\033[?25h\033[?1006l\033[?1003l\033[<u\033[?1049l";
    int err = tui_write(tui, restore, sizeof restore - 1);
#ifdef _WIN32
    if(!SetConsoleMode(tui->hstdin, tui->input_mode)) err = 1;
    if(!SetConsoleMode(tui->hstdout, tui->output_mode)) err = 1;
#else
    int result;
    do { result = tcsetattr(tui->hstdin, TCSANOW, &tui->orig); }
    while(result < 0 && errno == EINTR);
    if(result < 0) err = errno;
    int event_err = tui_stop_events(tui);
    if(event_err) err = event_err;
#endif
    tui->enabled = 0;
    return err;
}

static
int
tui_resume(Tui* tui){
    if(tui->enabled) return 0;
#ifdef _WIN32
    DWORD mode = tui->input_mode;
    mode &= ~(DWORD)(ENABLE_LINE_INPUT | ENABLE_ECHO_INPUT | ENABLE_PROCESSED_INPUT |
        ENABLE_QUICK_EDIT_MODE | ENABLE_VIRTUAL_TERMINAL_INPUT);
    mode |= ENABLE_EXTENDED_FLAGS | ENABLE_WINDOW_INPUT | ENABLE_MOUSE_INPUT;
    if(!SetConsoleMode(tui->hstdin, mode)) return (int)GetLastError();
    if(!SetConsoleMode(tui->hstdout, tui->output_mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING)){
        int err = (int)GetLastError();
        SetConsoleMode(tui->hstdin, tui->input_mode);
        return err;
    }
#else
    int event_err = tui_start_events(tui);
    if(event_err) return event_err;
    if(tcsetattr(tui->hstdin, TCSANOW, &tui->raw) < 0){
        int err = errno;
        tui_stop_events(tui);
        return err;
    }
#endif
    tui->enabled = 1;
#ifdef _WIN32
    const char modes[] = "\033[?1049h\033[?25l";
#else
    const char modes[] = "\033[?1049h\033[?25l\033[>5u\033[?1003h\033[?1006h";
#endif
    int err = tui_write(tui, modes, sizeof modes - 1);
    if(err){ tui_pause(tui); return err; }
    tui->canvas.buff.cursor = 0;
    tui->canvas.buff.errored = 0;
    tui_invalidate(tui);
    return 0;
}

static
void
tui_close(Tui* tui){
    tui_pause(tui);
    TuiCanvas* c = &tui->canvas;
    for(int i = 0; i < 2; i++){
        if(c->cells[i])
            Allocator_free(c->buff.allocator, c->cells[i], (size_t)c->alloc_w*c->alloc_h, sizeof *c->cells[i], _Alignof(TuiCell));
    }
    if(c->buff.data) msb_destroy(&c->buff);
    *c = (TuiCanvas){0};
}

static
int
tui_open(Tui* tui, TuiOptions* options){
    *tui = (Tui){0};
    if(!options || (!options->input && !options->output && !options->error)){
        #ifdef _WIN32
            tui->hstdin  = GetStdHandle(STD_INPUT_HANDLE),
            tui->hstdout = GetStdHandle(STD_OUTPUT_HANDLE),
            tui->hstderr = GetStdHandle(STD_ERROR_HANDLE)
        #else
            tui->hstdin  = STDIN_FILENO;
            tui->hstdout = STDOUT_FILENO;
            tui->hstderr = STDERR_FILENO;
        #endif
    }
    else {
        tui->hstdin  = options->input;
        tui->hstdout = options->output;
        tui->hstderr = options->error;
    }
#ifdef _WIN32
    if(!GetConsoleMode(tui->hstdin, &tui->input_mode) || !GetConsoleMode(tui->hstdout, &tui->output_mode))
        return (int)GetLastError();
#else
    if(tcgetattr(tui->hstdin, &tui->orig) < 0)
        return errno;
    tui->raw = tui->orig;
    tui->raw.c_iflag &= ~(tcflag_t)(BRKINT | ICRNL | INPCK | ISTRIP | IXON | INLCR | IGNCR | PARMRK);
    tui->raw.c_oflag &= ~(tcflag_t)OPOST;
    tui->raw.c_cflag &= ~(tcflag_t)(CSIZE | PARENB);
    tui->raw.c_cflag |= CS8;
    tui->raw.c_lflag &= ~(tcflag_t)(ECHO | ICANON | IEXTEN | ISIG);
    tui->raw.c_cc[VMIN] = 1;
    tui->raw.c_cc[VTIME] = 0;
#endif
    if(tui_dimensions(tui, &tui->columns, &tui->rows))
        return 1;
    int err = tui_resize_canvas(tui, tui->columns, tui->rows);
    if(err){
        tui_close(tui);
        return err;
    }
    tui_clear_state(tui);
    err = tui_resume(tui);
    if(err) tui_close(tui);
    return err;
}

static
int
tui_poll_event(Tui* tui, TuiEvent* ev){
    *ev = (TuiEvent){.kind=TUI_NO_INPUT};
    if(!tui->enabled) return 1;
    #ifndef _WIN32
        int event_err = tui_read_events(tui, 0);
        if(event_err) return event_err;
    #endif
        int resize = tui_check_resize(tui, ev);
        if(resize) return resize < 0 ? 1 : 0;
    #ifdef _WIN32
        return tui_windows_poll(tui, ev);
    #else
        for(;;){
            int err = tui_fill_input(tui);
            if(err) return err;
            int result = tui_parse_input(tui, ev);
            if(result == 1) return 0;
            *ev = (TuiEvent){.kind=TUI_NO_INPUT};
            if(result == -1) continue;
            if(tui->eof && !tui->count) ev->kind = TUI_EOF;
            return 0;
        }
    #endif
}

static
int
tui_wait_event(Tui* tui, TuiEvent* ev){
    return tui_wait_event_timeout(tui, ev, -1);
}

static
int
tui_wait_event_timeout(Tui* tui, TuiEvent* ev, int timeout_ms){
    uint64_t start = timeout_ms > 0 ? monotonic_time_us() : 0;
    for(;;){
        int err = tui_poll_event(tui, ev);
        if(err || ev->kind != TUI_NO_INPUT) return err;
        int remaining = timeout_ms;
        if(timeout_ms >= 0){
            uint64_t elapsed = timeout_ms ? monotonic_time_us() - start : 0;
            uint64_t duration = (uint64_t)timeout_ms * 1000;
            if(elapsed >= duration) return 0;
            remaining = (int)((duration - elapsed + 999) / 1000);
        }
        #ifdef _WIN32
        DWORD ready = WaitForSingleObject(tui->hstdin, remaining < 0 ? INFINITE : (DWORD)remaining);
        if(ready == WAIT_FAILED) return (int)GetLastError();
        #else
        err = tui_read_events(tui, remaining);
        if(err) return err;
        #endif
    }
}

static
void
tui_set_color(Tui* tui, TuiColor fg, TuiColor bg){
    TuiPaintState* s = tui_current_state(tui);
    if(!fg.is_none) s->color = fg;
    if(!bg.is_none){ s->bg_color = bg; s->bg_alpha = 255; }
}

static
void
tui_putc(Tui* tui, uint32_t cp, unsigned width){
    char text[4];
    size_t n;
    if(cp > 0x10ffff || (cp >= 0xd800 && cp <= 0xdfff)) cp = 0xfffd;
    if(cp < 0x80){ text[0] = (char)cp; n = 1; }
    else if(cp < 0x800){
        text[0] = (char)(0xc0 | (cp >> 6));
        text[1] = (char)(0x80 | (cp & 63)); n = 2;
    }
    else if(cp < 0x10000){
        text[0] = (char)(0xe0 | (cp >> 12));
        text[1] = (char)(0x80 | ((cp >> 6) & 63));
        text[2] = (char)(0x80 | (cp & 63)); n = 3;
    }
    else{
        text[0] = (char)(0xf0 | (cp >> 18));
        text[1] = (char)(0x80 | ((cp >> 12) & 63));
        text[2] = (char)(0x80 | ((cp >> 6) & 63));
        text[3] = (char)(0x80 | (cp & 63)); n = 4;
    }
    if(!width || width > 7) return;
    tui_write_glyph(tui, tui->canvas.x, tui->canvas.y, text, n, width);
    tui_advance(tui, width);
}

static
void
tui_puts(Tui* tui, StringView text){
    tui_puts_utf8(tui, text.text, text.length);
}

static
void
tui_fill_rect(Tui* tui, int x, int y, unsigned w, unsigned h, uint32_t cp, unsigned width){
    if(!width || width > 7) return;
    int64_t right = (int64_t)x + w, bottom = (int64_t)y + h;
    if(right > tui->canvas.w) right = tui->canvas.w;
    if(bottom > tui->canvas.h) bottom = tui->canvas.h;
    int64_t left = x;
    if(left < 0) left += ((-left + width - 1) / width) * width;
    int64_t top = y < 0 ? 0 : y;
    int old_x = tui->canvas.x, old_y = tui->canvas.y;
    for(int64_t row = top; row < bottom; row++)
        for(int64_t col = left; col + width <= right; col += width){
            tui->canvas.x = (int)col;
            tui->canvas.y = (int)row;
            tui_putc(tui, cp, width);
        }
    tui->canvas.x = old_x;
    tui->canvas.y = old_y;
}

static
void
tui_clear_rect(Tui* tui, int x, int y, unsigned w, unsigned h){
    int64_t right = (int64_t)x + w, bottom = (int64_t)y + h;
    if(right > tui->canvas.w) right = tui->canvas.w;
    if(bottom > tui->canvas.h) bottom = tui->canvas.h;
    int64_t left = x < 0 ? 0 : x, top = y < 0 ? 0 : y;
    for(int64_t row = top; row < bottom; row++)
        for(int64_t col = left; col < right; col++) tui_clear_at(tui, (int)col, (int)row);
}

#ifdef __clang__
#pragma clang assume_nonnull end
#endif

#endif
