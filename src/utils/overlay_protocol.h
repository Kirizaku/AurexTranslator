/******************************************************************************
    at-overlay - in-game overlay for AurexTranslator

    Copyright (C) 2026 by Daniil Nabiulin
    Copyright (C) 2021 by David Rosca <nowrep@gmail.com> (obs-vkcapture)

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
******************************************************************************/

// Wire protocol between the injected hook and the app
//
// NOTE: hand-duplicated from
// aurextranslator-plugins/include/host/overlay_protocol.h (itself derived
// from obs-vkcapture/src/capture.h), keep the shared definitions identical
// when editing either one.
//
// This is a SUBSET of the plugin repo's header: only the wire-format structs
// and constants the app needs to act as the capture server. The plugin-side
// file also declares capture_init()/capture_update_socket()/etc. - that is
// the injected hook's own internal API, not part of the wire protocol, and
// has no business here

#pragma once

#include <stdint.h>
#include <assert.h>

#if defined(_MSC_VER)
#  define AT_PACK_PUSH __pragma(pack(push, 1))
#  define AT_PACK_POP  __pragma(pack(pop))
#  define AT_PACKED
#else
#  define AT_PACK_PUSH
#  define AT_PACK_POP
#  define AT_PACKED __attribute__((packed))
#endif

#ifdef __cplusplus
extern "C" {
#endif

AT_PACK_PUSH

struct capture_client_data {
    uint8_t type;
    char exe[48];
    uint32_t pid;
    uint8_t padding[75];
} AT_PACKED;

#define CAPTURE_CLIENT_DATA_TYPE 10
#define CAPTURE_CLIENT_DATA_SIZE 128
static_assert(sizeof(struct capture_client_data) == CAPTURE_CLIENT_DATA_SIZE, "size mismatch");

struct capture_control_data {
    uint8_t capturing;            // text overlay session active
    uint8_t allow_frame_capture;  // frame delivery permitted (independent of the above)
    uint8_t padding[30];
} AT_PACKED;

#define CAPTURE_CONTROL_DATA_SIZE 32
static_assert(sizeof(struct capture_control_data) == CAPTURE_CONTROL_DATA_SIZE, "size mismatch");

struct capture_text_data {
    uint8_t  type;       // CAPTURE_TEXT_DATA_TYPE
    uint8_t  visible;    // 0 hides the panel without discarding the text
    uint16_t reserved0;
    uint32_t src_len;    // bytes of UTF-8 source label, no NUL
    uint32_t text_len;   // bytes of UTF-8 text, no NUL
    uint32_t reserved1;
} AT_PACKED;

#define CAPTURE_TEXT_DATA_TYPE        13
#define CAPTURE_TEXT_DATA_HEADER_SIZE 16
static_assert(sizeof(struct capture_text_data) == CAPTURE_TEXT_DATA_HEADER_SIZE, "size mismatch");

#define CAPTURE_TEXT_SRC_MAX   64
#define CAPTURE_TEXT_TEXT_MAX  8192

struct capture_style_data {
    uint8_t  type;         // CAPTURE_STYLE_DATA_TYPE
    uint8_t  align;        // 0 = left, 1 = center, 2 = right
    uint16_t reserved0;
    uint32_t text_color;   // 0xRRGGBBAA
    uint32_t bg_color;     // 0xRRGGBBAA; alpha is the panel's opacity
    float    font_size;    // pt; hook clamps to [8, 96]; ignored when font_size_auto is set
    float    zone_x;
    float    zone_y;
    float    zone_w;
    float    zone_h;
    uint8_t  font_size_auto; // 0 = use font_size; 1 = size the text to fill the panel's own drawn box (zone_w/zone_h) instead
    uint8_t  corner_radius;  // px; hook clamps to [0, 64]; 0 = square corners
    uint8_t  margin_top;     // px; hook clamps to [0, 64]
    uint8_t  margin_bottom;  // px; hook clamps to [0, 64]
    uint8_t  margin_left;    // px; hook clamps to [0, 64]
    uint8_t  margin_right;   // px; hook clamps to [0, 64]
    uint8_t  padding[26];
} AT_PACKED;

#define CAPTURE_STYLE_DATA_TYPE 14
#define CAPTURE_STYLE_DATA_SIZE 64
static_assert(sizeof(struct capture_style_data) == CAPTURE_STYLE_DATA_SIZE, "size mismatch");

struct capture_font_data {
    uint8_t  type;
    uint8_t  reserved0;
    uint16_t reserved1;
    uint32_t path_len;
} AT_PACKED;

#define CAPTURE_FONT_DATA_TYPE        15
#define CAPTURE_FONT_DATA_HEADER_SIZE 8
static_assert(sizeof(struct capture_font_data) == CAPTURE_FONT_DATA_HEADER_SIZE, "size mismatch");

#define CAPTURE_FONT_PATH_MAX 512

#define AT_OVERLAY_FRAME_MAPPING "Local\\at_overlay_frame"

// Linux: POSIX shared memory (shm_open) + named semaphores (sem_open)
#define AT_OVERLAY_FRAME_MAPPING_POSIX  "/at_overlay_frame"
#define AT_OVERLAY_CAPTURE_REQUEST_SEM  "/at_overlay_capture_request"
#define AT_OVERLAY_CAPTURE_READY_SEM    "/at_overlay_capture_ready"

#define AT_OVERLAY_FRAME_MAX_WIDTH   3840
#define AT_OVERLAY_FRAME_MAX_HEIGHT  2160
#define AT_OVERLAY_FRAME_FORMAT_BGRA8 1

struct at_overlay_frame_header {
    uint32_t width;
    uint32_t height;
    uint32_t stride;
    uint32_t format;
    uint32_t frame_counter;
    uint32_t reserved[3];
} AT_PACKED;

#define AT_OVERLAY_FRAME_HEADER_SIZE 32
static_assert(sizeof(struct at_overlay_frame_header) == AT_OVERLAY_FRAME_HEADER_SIZE, "size mismatch");

#define AT_OVERLAY_FRAME_MAPPING_SIZE \
    (AT_OVERLAY_FRAME_HEADER_SIZE + (uint64_t)AT_OVERLAY_FRAME_MAX_WIDTH * AT_OVERLAY_FRAME_MAX_HEIGHT * 4)

AT_PACK_POP

// Windows
#define AT_OVERLAY_CAPTURE_PIPE "\\\\.\\pipe\\at_overlay_capture"

// Linux
#define AT_OVERLAY_CAPTURE_SOCKET "/com/at_overlay/capture"

// On-demand frame handshake (Windows)
#define AT_OVERLAY_CAPTURE_REQUEST_EVENT "Local\\at_overlay_capture_request"
#define AT_OVERLAY_CAPTURE_READY_EVENT   "Local\\at_overlay_capture_ready"

// Windows: the Vulkan layer loads into every Vulkan process
#define AT_OVERLAY_VK_TARGETS_FILE "at-overlay-vktargets.txt"

#ifdef __cplusplus
}
#endif
