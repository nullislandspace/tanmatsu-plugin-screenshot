// SPDX-License-Identifier: MIT
// Screenshot Plugin - Capture screenshots with LOGO+P
//
// Saves screenshots to /sd/screenshot-YYYYMMDDHHMMSS.ppm

#include "tanmatsu_plugin.h"
#include "pax_gfx.h"
#include <asp/display.h>
#include <stdio.h>
#include <time.h>
#include <stdint.h>
#include <stdbool.h>

// Plugin state
static plugin_context_t* plugin_ctx = NULL;
static int hook_id = -1;
static volatile bool logo_key_held = false;

// Scancodes (from bsp/input.h)
#define BSP_INPUT_SCANCODE_P              0x19
#define BSP_INPUT_SCANCODE_LEFTMETA       0xe05b
#define BSP_INPUT_SCANCODE_RIGHTMETA      0xe05c
#define BSP_INPUT_SCANCODE_LEFTMETA_REL   0xe0db  // 0xe05b | 0x80
#define BSP_INPUT_SCANCODE_RIGHTMETA_REL  0xe0dc  // 0xe05c | 0x80

static void save_screenshot(void) {
    // Get PAX framebuffer
    pax_buf_t* buf = NULL;
    asp_disp_get_pax_buf(&buf);
    if (buf == NULL) {
        asp_log_error("screenshot", "Failed to get PAX buffer");
        return;
    }

    // Get dimensions - use physical buffer dimensions
    int width = buf->width;
    int height = buf->height;

    // Get raw pixel data
    const uint8_t* pixels = (const uint8_t*)pax_buf_get_pixels(buf);
    if (pixels == NULL) {
        asp_log_error("screenshot", "Failed to get pixel data");
        return;
    }

    // Generate filename with timestamp
    time_t now = time(NULL);
    struct tm tm_info;
    localtime_r(&now, &tm_info);

    char filename[64];
    snprintf(filename, sizeof(filename), "/sd/screenshot-%04d%02d%02d%02d%02d%02d.ppm",
             tm_info.tm_year + 1900, tm_info.tm_mon + 1, tm_info.tm_mday,
             tm_info.tm_hour, tm_info.tm_min, tm_info.tm_sec);

    asp_log_info("screenshot", "Saving screenshot to %s (%dx%d)", filename, width, height);

    // Open file for writing
    FILE* file = fopen(filename, "wb");
    if (file == NULL) {
        asp_log_error("screenshot", "Failed to open file %s", filename);
        return;
    }

    // Write PPM P6 header
    fprintf(file, "P6\n%d %d\n255\n", width, height);

    // Write pixel data (convert BGR to RGB)
    // Framebuffer is PAX_BUF_24_888RGB format - 3 bytes per pixel, BGR order
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            int idx = (y * width + x) * 3;  // 3 bytes per pixel
            uint8_t b = pixels[idx + 0];
            uint8_t g = pixels[idx + 1];
            uint8_t r = pixels[idx + 2];
            // Write as RGB for PPM
            fputc(r, file);
            fputc(g, file);
            fputc(b, file);
        }
    }

    fclose(file);
    asp_log_info("screenshot", "Screenshot saved: %s", filename);
}

// Input hook callback - called for every input event
static bool input_hook_callback(plugin_input_event_t* event, void* user_data) {
    (void)user_data;

    // Only handle scancode events
    if (event->type != PLUGIN_INPUT_EVENT_TYPE_SCANCODE) {
        return false;
    }

    uint32_t scancode = event->key;

    // Track LOGO key press (left or right meta key)
    if (scancode == BSP_INPUT_SCANCODE_LEFTMETA || scancode == BSP_INPUT_SCANCODE_RIGHTMETA) {
        logo_key_held = true;
        return false;  // Don't consume LOGO key events
    }

    // Track LOGO key release
    if (scancode == BSP_INPUT_SCANCODE_LEFTMETA_REL || scancode == BSP_INPUT_SCANCODE_RIGHTMETA_REL) {
        logo_key_held = false;
        return false;  // Don't consume LOGO key events
    }

    // Check for LOGO+P (P press while LOGO is held)
    if (scancode == BSP_INPUT_SCANCODE_P && logo_key_held) {
        asp_log_info("screenshot", "LOGO+P detected, taking screenshot...");
        save_screenshot();
        // Assume keys are released during save
        logo_key_held = false;
        return true;  // Consume the P key event
    }

    return false;
}

// Plugin metadata
static const plugin_info_t plugin_info = {
    .name = "Screenshot",
    .slug = "at.cavac.screenshot",
    .version = "1.0.0",
    .author = "Rene Schickbauer",
    .description = "Capture screenshots with LOGO+P",
    .api_version = TANMATSU_PLUGIN_API_VERSION,
    .type = PLUGIN_TYPE_HOOK,
    .flags = 0,
};

// Return plugin info
static const plugin_info_t* get_info(void) {
    return &plugin_info;
}

// Plugin initialization
static int plugin_init(plugin_context_t* ctx) {
    asp_log_info("screenshot", "Screenshot plugin initializing...");

    // Store context
    plugin_ctx = ctx;

    // Register input hook
    hook_id = asp_plugin_input_hook_register(ctx, input_hook_callback, NULL);
    if (hook_id < 0) {
        asp_log_error("screenshot", "Failed to register input hook");
        return -1;
    }

    asp_log_info("screenshot", "Screenshot plugin initialized, hook_id=%d", hook_id);
    return 0;
}

// Plugin cleanup
static void plugin_cleanup(plugin_context_t* ctx) {
    (void)ctx;

    // Unregister input hook
    if (hook_id >= 0) {
        asp_plugin_input_hook_unregister(hook_id);
        hook_id = -1;
    }

    plugin_ctx = NULL;
    asp_log_info("screenshot", "Screenshot plugin cleaned up");
}

// Plugin entry point structure
static const plugin_entry_t entry = {
    .get_info = get_info,
    .init = plugin_init,
    .cleanup = plugin_cleanup,
    .menu_render = NULL,
    .menu_select = NULL,
    .service_run = NULL,
    .hook_event = NULL,
};

// Register this plugin with the host
TANMATSU_PLUGIN_REGISTER(entry);
