#include <string.h>

#include "win32_internal.h"
#include <xinput.h>

typedef DWORD (WINAPI *pfn_XInputGetState)(DWORD, XINPUT_STATE *);
typedef DWORD (WINAPI *pfn_XInputSetState)(DWORD, XINPUT_VIBRATION *);

static struct {
    int tried;
    HMODULE mod;
    pfn_XInputGetState get_state;
    pfn_XInputSetState set_state;
} g_xinput;

static int ensure_xinput_api(void) {
    if (g_xinput.get_state && g_xinput.set_state) return 1;
    if (g_xinput.tried) return 0;
    g_xinput.tried = 1;
    static const wchar_t *const kDlls[3] = {
        L"xinput1_4.dll",
        L"xinput9_1_0.dll",
        L"xinput1_3.dll"
    };
    for (int i = 0; i < 3; i++) {
        g_xinput.mod = LoadLibraryW(kDlls[i]);
        if (g_xinput.mod) {
            g_xinput.get_state = (pfn_XInputGetState)(uintptr_t)GetProcAddress(g_xinput.mod, "XInputGetState");
            g_xinput.set_state = (pfn_XInputSetState)(uintptr_t)GetProcAddress(g_xinput.mod, "XInputSetState");
            if (g_xinput.get_state) return 1;
        }
    }
    return 0;
}

uint8_t w32_xinput_get_state(uint32_t user_index, w32_gamepad_state *out) {
    if (!out || user_index >= XUSER_MAX_COUNT) return 0;
    memset(out, 0, sizeof(*out));
    if (!ensure_xinput_api() || !g_xinput.get_state) return 0;

    XINPUT_STATE st;
    memset(&st, 0, sizeof(st));
    DWORD res = g_xinput.get_state((DWORD)user_index, &st);
    if (res != ERROR_SUCCESS) {
        return 0;
    }

    out->connected = 1;
    out->buttons = (uint16_t)st.Gamepad.wButtons;
    out->left_trigger = (uint8_t)st.Gamepad.bLeftTrigger;
    out->right_trigger = (uint8_t)st.Gamepad.bRightTrigger;
    out->thumb_lx = (int16_t)st.Gamepad.sThumbLX;
    out->thumb_ly = (int16_t)st.Gamepad.sThumbLY;
    out->thumb_rx = (int16_t)st.Gamepad.sThumbRX;
    out->thumb_ry = (int16_t)st.Gamepad.sThumbRY;
    out->packet_number = (uint32_t)st.dwPacketNumber;
    return 1;
}

uint8_t w32_xinput_set_vibration(uint32_t user_index, uint16_t left_motor, uint16_t right_motor) {
    if (user_index >= XUSER_MAX_COUNT || !ensure_xinput_api() || !g_xinput.set_state) return 0;
    XINPUT_VIBRATION vib;
    vib.wLeftMotorSpeed = (WORD)left_motor;
    vib.wRightMotorSpeed = (WORD)right_motor;
    return g_xinput.set_state((DWORD)user_index, &vib) == ERROR_SUCCESS ? 1 : 0;
}
