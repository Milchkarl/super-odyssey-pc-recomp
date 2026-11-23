// The game's button bits (libnx HidNpadButton: nn::hid NpadButton) by name, for switch2_proto.h (dkb-pc-recomp's
// input_map.h B_* values; the same bits as hle/input.c's BTN_*).
#pragma once
enum {
    B_A = 0, B_B, B_X, B_Y, B_STICK_L, B_STICK_R, B_L, B_R, B_ZL, B_ZR, B_PLUS, B_MINUS, B_LEFT, B_UP, B_RIGHT, B_DOWN,
    B_SL_LEFT = 16, B_SL_UP, B_SL_RIGHT, B_SL_DOWN, B_SR_LEFT, B_SR_UP, B_SR_RIGHT, B_SR_DOWN,
};
