//#ifdef CONFIG_ZEPHYR_HDL
#include "hdl/hdl.h"
#include <device.h>
#include <init.h>
#include <logging/log.h>
#include "../drivers/epd/il0323n.h"
#include <zmk/battery.h>

static const struct device *display;

struct HDL_Interface interface;

unsigned char HDL_PAGE_OUTPUT[1413] = {
0x00, 0x01, 0x01, 0x00, 0x11, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
0x80, 0x04, 0xC0, 0x00, 0x30, 0x00, 0x18, 0x18, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
0x00, 0x3E, 0x00, 0x00, 0x3E, 0x00, 0x00, 0x3E, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x41, 0x00, 0x00, 0x41, 0x00, 0x00, 0x41, 
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x18, 0x00, 0x00, 0x00, 
0x00, 0x9C, 0x80, 0x00, 0x9C, 0x80, 0x00, 0x9C, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
0x00, 0x00, 0x00, 0x00, 0x3C, 0x00, 0x00, 0x01, 0xC0, 0x22, 0x00, 0x00, 0x22, 0x00, 0x00, 0x22, 
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x3C, 0x00, 0x00, 0x03, 
0xE0, 0x08, 0x00, 0x00, 0x08, 0x00, 0x00, 0x08, 0x00, 0x00, 0x00, 0xFF, 0x00, 0x00, 0x00, 0x00, 
0x00, 0x00, 0x00, 0x00, 0x7E, 0x00, 0x00, 0x07, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
0x00, 0x00, 0x07, 0xFF, 0xE0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x7E, 0x00, 0x00, 0x0F, 
0x00, 0x00, 0xFF, 0x00, 0x00, 0x18, 0x00, 0x00, 0x00, 0x00, 0x1F, 0xFF, 0xF8, 0x00, 0x00, 0x00, 
0x00, 0x00, 0x00, 0x00, 0xFF, 0x00, 0x00, 0x0E, 0x00, 0x00, 0xFF, 0x00, 0x00, 0x7E, 0x00, 0x00, 
0x00, 0x00, 0x3F, 0xFF, 0xFC, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xFF, 0x00, 0x00, 0x0E, 
0x08, 0x00, 0x81, 0x00, 0x00, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x7F, 0x00, 0xFE, 0x00, 0x00, 0x00, 
0x00, 0x00, 0x00, 0x01, 0xE7, 0x80, 0x00, 0x0F, 0x18, 0x00, 0x81, 0x00, 0x03, 0xFF, 0xC0, 0x03, 
0x81, 0xC0, 0xF8, 0x00, 0x1F, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0xE7, 0x80, 0x00, 0x1F, 
0xF8, 0x3F, 0xFF, 0xFC, 0x0F, 0xFF, 0xF0, 0x0F, 0xFF, 0xF0, 0xF0, 0x00, 0x0F, 0x00, 0x00, 0x00, 
0x00, 0x00, 0x00, 0x03, 0xE7, 0xC0, 0x00, 0x3F, 0xF0, 0x3F, 0xFF, 0xFC, 0x3F, 0xFF, 0xFC, 0x1F, 
0xFF, 0xF8, 0x60, 0xFF, 0x06, 0x00, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x03, 0xE7, 0xC0, 0x00, 0x7F, 
0xC0, 0x3F, 0xFF, 0xFC, 0x7F, 0xFF, 0xFE, 0x3F, 0xFF, 0xFC, 0x03, 0xFF, 0xC0, 0x03, 0xFF, 0xC0, 
0x00, 0x00, 0x00, 0x07, 0xE7, 0xE0, 0x00, 0xFC, 0x00, 0x3F, 0xFF, 0xFC, 0x1F, 0xFF, 0xF8, 0x7D, 
0xFF, 0xCE, 0x07, 0xFF, 0xE0, 0x07, 0xFF, 0xE0, 0x00, 0x00, 0x00, 0x07, 0xE7, 0xE0, 0x01, 0xF8, 
0x00, 0x3F, 0xFF, 0xFC, 0x1F, 0xFF, 0xF8, 0x7D, 0xFF, 0xCE, 0x07, 0x81, 0xE0, 0x07, 0x81, 0xE0, 
0x00, 0x00, 0x00, 0x0F, 0xFF, 0xF0, 0x03, 0xF0, 0x00, 0x3F, 0xFF, 0xFC, 0x1C, 0x7E, 0x38, 0x70, 
0x7F, 0xFE, 0x03, 0x00, 0xC0, 0x03, 0x00, 0xC0, 0x00, 0x00, 0x00, 0x0F, 0xFF, 0xF0, 0x07, 0xE0, 
0x00, 0x3F, 0xFF, 0xFC, 0x1C, 0x7E, 0x38, 0x7D, 0xFE, 0x7E, 0x00, 0x18, 0x00, 0x00, 0x18, 0x00, 
0x00, 0x18, 0x00, 0x1F, 0xE7, 0xF8, 0x0E, 0xC0, 0x00, 0x3F, 0xFF, 0xFC, 0x1C, 0x7E, 0x38, 0x7D, 
0xFE, 0x7E, 0x00, 0x3C, 0x00, 0x00, 0x3C, 0x00, 0x00, 0x3C, 0x00, 0x1F, 0xE7, 0xF8, 0x0F, 0x80, 
0x00, 0x3F, 0xFF, 0xFC, 0x1F, 0xFF, 0xF8, 0x3F, 0xFF, 0xFC, 0x00, 0x3C, 0x00, 0x00, 0x3C, 0x00, 
0x00, 0x3C, 0x00, 0x3F, 0xFF, 0xFC, 0x07, 0x00, 0x00, 0x3F, 0xFF, 0xFC, 0x1F, 0xFF, 0xF8, 0x1F, 
0xFF, 0xF8, 0x00, 0x18, 0x00, 0x00, 0x18, 0x00, 0x00, 0x18, 0x00, 0x3F, 0xFF, 0xFC, 0x02, 0x00, 
0x00, 0x3F, 0xFF, 0xFC, 0x1F, 0xFF, 0xF8, 0x0F, 0xC3, 0xF0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
0x00, 0x00, 0x00, 0x1F, 0xFF, 0xF8, 0x00, 0x00, 0x00, 0x1F, 0xFF, 0xF8, 0x1F, 0xFF, 0xF8, 0x07, 
0x81, 0xE0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x03, 0x80, 0x00, 0x00, 0x7E, 0x00, 0x00, 
0x7E, 0x00, 0x00, 0x7E, 0x00, 0x00, 0x7E, 0x00, 0x00, 0x7E, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
0x00, 0x0F, 0xC0, 0x00, 0x00, 0x7E, 0x00, 0x00, 0x7E, 0x00, 0x00, 0x7E, 0x00, 0x00, 0x7E, 0x00, 
0x00, 0x7E, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x1E, 0xE0, 0x00, 0x00, 0x66, 0x00, 0x00, 
0x66, 0x00, 0x00, 0x66, 0x00, 0x00, 0x66, 0x00, 0x00, 0x66, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
0x00, 0x1C, 0x70, 0x00, 0x03, 0xFF, 0xC0, 0x03, 0xFF, 0xC0, 0x03, 0xFF, 0xC0, 0x03, 0xFF, 0xC0, 
0x03, 0xFF, 0xC0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x38, 0x38, 0x00, 0x07, 0xFF, 0xE0, 0x07, 
0xFF, 0xE0, 0x07, 0xFF, 0xE0, 0x07, 0xFF, 0xE0, 0x07, 0xFF, 0xE0, 0x00, 0x00, 0x00, 0x00, 0x00, 
0x00, 0x30, 0x18, 0x00, 0x06, 0x00, 0x60, 0x06, 0x00, 0x60, 0x06, 0x00, 0x60, 0x06, 0x00, 0x60, 
0x06, 0x00, 0x60, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x38, 0x58, 0x00, 0x06, 0xFF, 0x60, 0x06, 
0x00, 0x60, 0x06, 0x00, 0x60, 0x06, 0x00, 0x60, 0x06, 0xC3, 0x60, 0x00, 0x00, 0x00, 0x00, 0x00, 
0x00, 0x1C, 0xF8, 0x00, 0x06, 0xFF, 0x60, 0x06, 0x00, 0x60, 0x06, 0x00, 0x60, 0x06, 0x00, 0x60, 
0x06, 0xC3, 0x60, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0E, 0x70, 0x00, 0x06, 0xFF, 0x60, 0x06, 
0xFF, 0x60, 0x06, 0x00, 0x60, 0x06, 0x00, 0x60, 0x06, 0xE7, 0x60, 0x00, 0x00, 0x00, 0x00, 0x00, 
0x00, 0x07, 0xFB, 0xC0, 0x06, 0xFF, 0x60, 0x06, 0xFF, 0x60, 0x06, 0x00, 0x60, 0x06, 0x00, 0x60, 
0x06, 0x7E, 0x60, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x03, 0xDF, 0xE0, 0x06, 0xFF, 0x60, 0x06, 
0xFF, 0x60, 0x06, 0xFF, 0x60, 0x06, 0x00, 0x60, 0x06, 0x3C, 0x60, 0x00, 0x00, 0x00, 0x00, 0x00, 
0x00, 0x00, 0x0E, 0x70, 0x06, 0xFF, 0x60, 0x06, 0xFF, 0x60, 0x06, 0xFF, 0x60, 0x06, 0x00, 0x60, 
0x06, 0x18, 0x60, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x1F, 0x38, 0x06, 0xFF, 0x60, 0x06, 
0xFF, 0x60, 0x06, 0xFF, 0x60, 0x06, 0x00, 0x60, 0x06, 0x3C, 0x60, 0x00, 0x00, 0x00, 0x00, 0x00, 
0x00, 0x00, 0x1A, 0x1C, 0x06, 0xFF, 0x60, 0x06, 0xFF, 0x60, 0x06, 0xFF, 0x60, 0x06, 0x00, 0x60, 
0x06, 0x7E, 0x60, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x18, 0x0C, 0x06, 0xFF, 0x60, 0x06, 
0xFF, 0x60, 0x06, 0xFF, 0x60, 0x06, 0xFF, 0x60, 0x06, 0xE7, 0x60, 0x00, 0x00, 0x00, 0x00, 0x00, 
0x00, 0x00, 0x1C, 0x1C, 0x06, 0xFF, 0x60, 0x06, 0xFF, 0x60, 0x06, 0xFF, 0x60, 0x06, 0xFF, 0x60, 
0x06, 0xC3, 0x60, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0E, 0x38, 0x06, 0xFF, 0x60, 0x06, 
0xFF, 0x60, 0x06, 0xFF, 0x60, 0x06, 0xFF, 0x60, 0x06, 0xC3, 0x60, 0x00, 0x00, 0x00, 0x00, 0x00, 
0x00, 0x00, 0x07, 0xF8, 0x06, 0x00, 0x60, 0x06, 0x00, 0x60, 0x06, 0x00, 0x60, 0x06, 0x00, 0x60, 
0x06, 0x00, 0x60, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x03, 0xF0, 0x07, 0xFF, 0xE0, 0x07, 
0xFF, 0xE0, 0x07, 0xFF, 0xE0, 0x07, 0xFF, 0xE0, 0x07, 0xFF, 0xE0, 0x00, 0x00, 0x00, 0x00, 0x00, 
0x00, 0x00, 0x01, 0xC0, 0x03, 0xFF, 0xC0, 0x03, 0xFF, 0xC0, 0x03, 0xFF, 0xC0, 0x03, 0xFF, 0xC0, 
0x03, 0xFF, 0xC0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x01, 0x0C, 0x08, 0x01, 0x02, 
0x02, 0x01, 0x00, 0x01, 0x0C, 0x08, 0x01, 0x01, 0x02, 0x00, 0x00, 0x02, 0x05, 0x04, 0x01, 0x02, 
0x0A, 0x04, 0x01, 0x05, 0x02, 0x00, 0x68, 0x69, 0x64, 0x3A, 0x00, 0x02, 0x04, 0x04, 0x01, 0x01, 
0x0A, 0x04, 0x01, 0x02, 0x00, 0x00, 0x53, 0x50, 0x00, 0x02, 0x04, 0x04, 0x01, 0x03, 0x0A, 0x04, 
0x01, 0x04, 0x00, 0x00, 0x00, 0x02, 0x05, 0x04, 0x01, 0x02, 0x0A, 0x04, 0x01, 0x05, 0x02, 0x00, 
0x65, 0x72, 0x67, 0x6F, 0x00, 0x02, 0x04, 0x04, 0x01, 0x01, 0x0A, 0x04, 0x01, 0x02, 0x00, 0x00, 
0x4C, 0x49, 0x54, 0x00, 0x02, 0x04, 0x04, 0x01, 0x03, 0x0A, 0x04, 0x01, 0x04, 0x00, 0x00, 0x00, 
0x01, 0x05, 0x04, 0x01, 0x02, 0x02, 0x00, 0x00, 0x02, 0x05, 0x04, 0x01, 0x01, 0x04, 0x04, 0x01, 
0x05, 0x02, 0x00, 0x00, 0x01, 0x05, 0x04, 0x01, 0x02, 0x02, 0x00, 0x00, 0x04, 0x0A, 0x04, 0x01, 
0x01, 0x07, 0x07, 0x01, 0x00, 0x0D, 0x04, 0x01, 0x03, 0x04, 0x04, 0x01, 0x05, 0x00, 0x00, 0x52, 
0x46, 0x53, 0x48, 0x0A, 0x25, 0x69, 0x00, 0x03, 0x06, 0x04, 0x01, 0x04, 0x0A, 0x04, 0x01, 0x01, 
0x04, 0x04, 0x01, 0x01, 0x00, 0x00, 0x00, 0x01, 0x05, 0x04, 0x01, 0x02, 0x02, 0x00, 0x00, 0x04, 
0x0A, 0x04, 0x01, 0x01, 0x07, 0x07, 0x01, 0x00, 0x0D, 0x08, 0x01, 0x05, 0x04, 0x04, 0x01, 0x05, 
0x00, 0x00, 0x25, 0x69, 0x00, 0x03, 0x06, 0x04, 0x01, 0x03, 0x0A, 0x04, 0x01, 0x02, 0x04, 0x04, 
0x01, 0x01, 0x00, 0x00, 0x53, 0x50, 0x0A, 0x4C, 0x49, 0x54, 0x00, 0x02, 0x0A, 0x04, 0x01, 0x04, 
0x04, 0x04, 0x01, 0x0A, 0x00
};

LOG_MODULE_REGISTER(hdldisp, CONFIG_DISPLAY_LOG_LEVEL);

int rfsh = 0;

void dsp_clear (int16_t x, int16_t y, uint16_t w, uint16_t h) {
    il0323_clear_area(display, x, y, w, h);
}

void dsp_render (int16_t x, int16_t y, uint16_t w, uint16_t h) {
    rfsh++;
    il0323_refresh(display, x, y, w, h);
}

void dsp_pixel (int16_t x, int16_t y) {
    il0323_set_pixel(display, x, y);
}

void dsp_hline (int16_t x, int16_t y, int16_t len) {
    il0323_h_line(display, x, y, len);
}

void dsp_vline (int16_t x, int16_t y, int16_t len) {
    il0323_v_line(display, x, y, len);
}

extern const char HDL_FONT[2048];

void dsp_text (int16_t x, int16_t y, const char *text, uint8_t fontSize) {

    int len = strlen(text);
    int line = 0;
    int acol = 0;

    for (int g = 0; g < len; g++) {
		// Starting character in single quotes

        if (text[g] == '\n') {
			line++;
			acol = 0;
			continue;
		}
		else if (text[g] == ' ') {
			acol++;
		}
		
		for (int py = 0; py < 8; py++) {
			for (int px = 0; px < 5; px++) {
				if ((HDL_FONT[text[g] * 8 + py] >> (7 - px)) & 1) {
                    int rx = x + (px + acol * 5) * fontSize;
                    int ry = y + (py + line * 6) * fontSize;

                    for(int sy = 0; sy < fontSize; sy++) {
                        for(int sx = 0; sx < fontSize; sx++) {
                            dsp_pixel(rx + sx, ry + sy);
                        }
                    }
				}
			}
		}
		acol++;

    }
}

int err = 0;

#if IS_ENABLED(CONFIG_ZMK_SPLIT_ROLE_CENTRAL)
const int role = 1;
#else
const int role = 0;
#endif

enum DSP_View {
    VIEW_SPLASH,
    VIEW_MAIN
} view;

int battery_percent = 0;

// Battery sprite, calculated from battery percent
int battery_sprite = 0;

// Sprite offset for battery icons
#define SPRITES_OFFSET_BATTERY  9
// Battery sprite count
#define SPRITES_BATTERY_COUNT   5

// Updates battery sprite index 
void update_battery_sprite () {
    battery_sprite = SPRITES_BATTERY_COUNT - (((battery_percent >= 100 ? 99 : battery_percent) * SPRITES_BATTERY_COUNT) / 100) - 1 + SPRITES_OFFSET_BATTERY;
}

static void display_thread(void *arg, void *unused2, void *unused3) {
    view = VIEW_SPLASH;
    k_msleep(2000);
    // Initialize HDL
    interface = HDL_CreateInterface(80, 128, HDL_COLORS_MONO, HDL_FEAT_TEXT | HDL_FEAT_LINE_HV);

    // Set interface functions
    interface.f_clear = dsp_clear;
    interface.f_renderPart = dsp_render;
    interface.f_pixel = dsp_pixel;
    interface.f_hline = dsp_hline;
    interface.f_vline = dsp_vline;
    interface.f_text = dsp_text;

    interface.textHeight = 6;
    interface.textWidth = 4;

    HDL_SetBinding(&interface, "role", 1, &role);
    HDL_SetBinding(&interface, "view", 2, &view);
    HDL_SetBinding(&interface, "batt", 3, &battery_percent);
    HDL_SetBinding(&interface, "rfsh", 4, &rfsh);
    HDL_SetBinding(&interface, "battsprite", 5, &battery_sprite);


    err |= HDL_Build(&interface, HDL_PAGE_OUTPUT, sizeof(HDL_PAGE_OUTPUT));

    err |= HDL_Update(&interface);

    k_msleep(2000);

    view = VIEW_MAIN;
    battery_percent = zmk_battery_state_of_charge();
    update_battery_sprite();
    err |= HDL_Update(&interface);

    il0323_hibernate(display);

    while(1) {
        k_msleep(30000);
        battery_percent = zmk_battery_state_of_charge();
        update_battery_sprite();

        err |= HDL_Update(&interface);

        il0323_hibernate(display);
    }
}


static int display_init () {

    display = DEVICE_DT_GET_ANY(gooddisplay_il0323n);

    if (display == NULL) {
        LOG_ERR("Failed to get il0323n device");
        err |= -EINVAL;
        return -EINVAL;
    }

    return 0;
}

SYS_INIT(display_init, APPLICATION, CONFIG_APPLICATION_INIT_PRIORITY);
K_THREAD_DEFINE(display_thr, 4096, display_thread, NULL, NULL, NULL, K_PRIO_COOP(10), 0, 0);
//#endif