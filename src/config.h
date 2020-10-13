// Copyright 2020 The Taote Authors.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//    https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#define FONT "Go Mono Regular 10"

#define SCROLLBACK_LINES 4096

// WORD_CHAR_EXCEPTIONS is VTE's WORD_CHAR_EXCEPTIONS_DEFAULT without the
// "\302\267" octal escapes (non-ASCII bytes, presumably U+00B7 MIDDLE DOT) but
// with an extra ":".
#define WORD_CHAR_EXCEPTIONS "-#%&+,./=?@\\_~:"

#define MAKE_GDKRGBA(r, g, b) \
  { (r) / 255.0, (g) / 255.0, (b) / 255.0, 1.0 }

#define NUM_G_PALETTE_COLORS 16
static GdkRGBA g_palette_colors[NUM_G_PALETTE_COLORS] = {
    // Default to the same palette as GNOME Terminal.

    MAKE_GDKRGBA(0x17, 0x14, 0x21),  //
    MAKE_GDKRGBA(0xC0, 0x1C, 0x28),  //
    MAKE_GDKRGBA(0x26, 0xA2, 0x69),  //
    MAKE_GDKRGBA(0xA2, 0x73, 0x4C),  //
    MAKE_GDKRGBA(0x12, 0x48, 0x8B),  //
    MAKE_GDKRGBA(0xA3, 0x47, 0xBA),  //
    MAKE_GDKRGBA(0x2A, 0xA1, 0xB3),  //
    MAKE_GDKRGBA(0xD0, 0xCF, 0xCC),  //

    MAKE_GDKRGBA(0x5E, 0x5C, 0x64),  //
    MAKE_GDKRGBA(0xF6, 0x61, 0x51),  //
    MAKE_GDKRGBA(0x33, 0xD1, 0x7A),  //
    MAKE_GDKRGBA(0xE9, 0xAD, 0x0C),  //
    MAKE_GDKRGBA(0x2A, 0x7B, 0xDE),  //
    MAKE_GDKRGBA(0xC0, 0x61, 0xCB),  //
    MAKE_GDKRGBA(0x33, 0xC7, 0xDE),  //
    MAKE_GDKRGBA(0xFF, 0xFF, 0xFF),  //
};

#define NUM_G_TITLE_COLORS 8
static GdkRGBA g_title_colors[NUM_G_TITLE_COLORS] = {
    // These colors from rebrickable.com/colors/ are as good as any.

    MAKE_GDKRGBA(0x6D, 0x6E, 0x5C),  // Dark Gray
    MAKE_GDKRGBA(0x00, 0x55, 0xBF),  // Blue
    MAKE_GDKRGBA(0x00, 0x8F, 0x9B),  // Dark Turquoise
    MAKE_GDKRGBA(0x4B, 0x9F, 0x4A),  // Bright Green
    MAKE_GDKRGBA(0xC8, 0x70, 0xA0),  // Dark Pink
    MAKE_GDKRGBA(0xAE, 0x7A, 0x59),  // Copper
    MAKE_GDKRGBA(0xC9, 0x1A, 0x09),  // Red
    MAKE_GDKRGBA(0x92, 0x39, 0x78),  // Magenta
};
