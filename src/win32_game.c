#include <string.h>

#include "win32_internal.h"
#include <xinput.h>

uint8_t w32_xinput_get_state(uint32_t user_index, w32_gamepad_state *out) {
    if (!out || user_index >= XUSER_MAX_COUNT) return 0;
    memset(out, 0, sizeof(*out));

    XINPUT_STATE st;
    memset(&st, 0, sizeof(st));
    DWORD res = XInputGetState((DWORD)user_index, &st);
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
    if (user_index >= XUSER_MAX_COUNT) return 0;
    XINPUT_VIBRATION vib;
    vib.wLeftMotorSpeed = (WORD)left_motor;
    vib.wRightMotorSpeed = (WORD)right_motor;
    return XInputSetState((DWORD)user_index, &vib) == ERROR_SUCCESS ? 1 : 0;
}
