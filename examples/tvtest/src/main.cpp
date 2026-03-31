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
#include "Arduino.h"
#include "esp_system.h"
#include "esp_int_wdt.h"
#include "esp_spiffs.h"
#include <hid_server.h>

#include <Preferences.h>


#include "fabgl.h"
#include "devdrivers/cvbsgenerator.h"

HIDServer hid("esp_hid");

#define W 50
#define H 25

#define VIDEOOUT_GPIO GPIO_NUM_25

//#define RAW_CVBS

#ifdef RAW_CVBS
fabgl::CVBSGenerator    CVBSGenerator;
static int cntr;
static void CVBSDrawScanlineCallback(void * arg, uint16_t * dest, int destSample, int scanLine)
{
	if (dest) {
		for (int i = 0; i < 10; i++)	
			dest[i + destSample] = 0x7fff;
	}
}

#else


Preferences preferences;
fabgl::CVBS16Controller DisplayController;
fabgl::Canvas        	canvas(&DisplayController);
#endif

fabgl::Rect 			rc(20,20,W+20,H+20);


int maxW = 320;
int maxH = 240;

bool _update;
bool _color;
bool _reverse;
int _view;

typedef enum {
	VIEW_COLORBARS,
	VIEW_GRAYBARS,
	VIEW_GRID,
	VIEW_DETAIL,
	VIEW_TESTCARD,

	VIEW_LAST
} E_View;

void setup()
{ 
	//hid_init("esp_hid");
	hid.begin();

#ifdef RAW_CVBS
	CVBSGenerator.setVideoGPIO(GPIO_NUM_25);
	CVBSGenerator.setDrawScanlineCallback(CVBSDrawScanlineCallback, NULL);
	CVBSGenerator.setup("I-PAL-B");
	maxW = CVBSGenerator.visibleSamples();
	maxH = CVBSGenerator.visibleLines();
	CVBSGenerator.run();
#else
  	DisplayController.begin(GPIO_NUM_25);  
  	DisplayController.setHorizontalRate(2);
  	DisplayController.setMonochrome(false);  
  	DisplayController.setResolution("P-PAL-B-WIDE", -1, -1, true);
	


	maxW = DisplayController.getViewPortWidth();
	maxH = DisplayController.getViewPortHeight();
#endif

	printf("w:%d, h:%d\n", maxW, maxH);

	_view = VIEW_COLORBARS;
	_update = true;
	_color = false;
	_reverse = false;
}

#ifndef RAW_CVBS
void draw_colorbars(bool reverse)
{
	const Color bar[8] = {
		Color::White,
		Color::Yellow,
		Color::Cyan,
		Color::Green,
		Color::Magenta,
		Color::Red,
		Color::Blue,
		Color::Black,
	};

	int w = maxW / 8;


	if (DisplayController.monochrome()) {	
		canvas.setBrushColor(Color(0));
		canvas.clear();

		canvas.waitCompletion();
		DisplayController.setMonochrome(false);
		DisplayController.setupDefaultPalette();
	}
	
	canvas.beginUpdate();
	for (int i = 0; i < 8; i++) {
		canvas.setBrushColor( bar[(reverse ? (7-i) :  i)]);
		canvas.fillRectangle(i*w, 0, i*w+w, maxH-1);
	}
	canvas.endUpdate();
}

void draw_graybars(bool reverse)
{
	int w = maxW / 16;

	if (!DisplayController.monochrome()) {
		canvas.setBrushColor(Color(0));
		canvas.clear();	

		canvas.waitCompletion();	
		DisplayController.setMonochrome(true);
		for (int i = 0; i < 16; i++) {
			uint8_t c = i*16;
			DisplayController.setPaletteItem(i, RGB888(c, c, c));
		}
	}
	canvas.beginUpdate();
	for (int i = 0; i < 16; i++) {
		canvas.setBrushColor( Color(reverse ? (15-i) : i));
		canvas.fillRectangle(i*w, 0, i*w+w, maxH-1);
	}
	canvas.endUpdate();
}

void draw_grid(bool reverse)
{
	canvas.setBrushColor(Color::Black);
	canvas.clear();		
	canvas.waitCompletion();	

	if (DisplayController.monochrome()) {

		DisplayController.setMonochrome(false);
		DisplayController.setupDefaultPalette();
	}

	canvas.beginUpdate();
	canvas.setPenColor(Color::BrightWhite);
	canvas.setBrushColor(Color::BrightWhite);

	canvas.drawRectangle(0, 0, maxW-1, maxH-1);

	int div = 8;//maxW == 768 ? 8 : 10;
	for (int y = 0; y < maxH; y += (maxH/div)) {
		canvas.drawLine(0, y, maxW-1, y);

		for (int x = 0; x < div; x++) {
			int xofft = (maxW / div) / 2;
			int yofft = (maxH / div) / 2;
			//canvas.setPixel(x * xofft + xofft, y + yofft);
			canvas.fillRectangle(x * (2*xofft) + xofft - 2, y + yofft - 2, 
			                     x * (2*xofft) + xofft + 2, y + yofft + 2);

		}
	}

	for (int x = 0; x < maxW; x += (maxW/div)) {
		canvas.drawLine(x, 0, x, maxH-1);
	}


	//canvas.setPenColor(Color::BrightRed);
	canvas.setBrushColor(Color::BrightRed);	
	canvas.fillRectangle(maxW/2-1, 0, maxW/2+1, maxH-1);
	canvas.fillRectangle(0, maxH/2-1, maxW-1, maxH/2+1);
	if (reverse) {
		canvas.invertRectangle(0, 0, maxW-1, maxH-1);
	}

	canvas.endUpdate();
}
	
void draw_detail(bool reverse)
{
	canvas.setBrushColor(Color::Black);
	canvas.clear();		
	canvas.waitCompletion();	

	if (DisplayController.monochrome()) {

		DisplayController.setMonochrome(false);
		DisplayController.setupDefaultPalette();
	}

    // Split screen into zones for different tests
    int zoneHeight = maxH / 1;
    
    // Zone 1: Fine to coarse vertical lines (focus test)
    canvas.setPenColor(Color::BrightWhite);  // White
    int spacing = 2;
    int x = 0;
    int lineNum = 0;
    
    while (x < maxW) {
		int X = reverse ? (maxW - x - 1) : x;
        canvas.drawLine(X, 0, X, zoneHeight - 1);
        
        // Exponential spacing increase for more dramatic effect
        if (lineNum < 20) {
            spacing = 2;
        } else if (lineNum < 40) {
            spacing = 3;
        } else if (lineNum < 60) {
            spacing = 5;			
        } else if (lineNum < 80) {
            spacing = 7;
        } else if (lineNum < 100) {
            spacing = 9;
        } else if (lineNum < 120) {
            spacing = 11;
        } else if (lineNum < 140) {
            spacing = 13;			
        } else if (lineNum < 160) {
            spacing = 15;												
        } else {
            spacing = 16;
        }
        x += spacing;
		lineNum++;
    }
    #if 0
    // Draw reference markers at key intervals to help identify regions
    canvas.setPenColor(Color::BrightGreen); // Green for markers
    
    // Add markers every 100 pixels or at key transition points
    for (int x = 0; x < maxW; x += (maxW/10)) {
        // Draw small horizontal markers at the top and bottom
        canvas.drawLine(x, 1, x + 10, 1);           // Top marker
        canvas.drawLine(x, maxH - 2, x + 10, maxH - 2);  // Bottom marker
    }
    
    // Add center reference line (optional)
	canvas.setPenColor(Color::Blue);  // Light blue
	
    canvas.drawLine(maxW / 2, 0, maxW / 2, maxH - 1);
	#endif
	canvas.endUpdate();

/*
	canvas.beginUpdate();
	canvas.setPenColor(Color::White);
	canvas.setBrushColor(Color::BrightWhite);

	double dx = 1.0;
	for (int x = 0; x < maxW; x += int(dx)) {
		canvas.drawLine(x, 0, x, maxH-1);
		dx += 0.15;
	}
	canvas.endUpdate();
	*/
}

void draw_testcard(bool reverse)
{
	canvas.setBrushColor(Color::Black);
	canvas.clear();		
	canvas.waitCompletion();	

	if (DisplayController.monochrome()) {

		DisplayController.setMonochrome(false);
		DisplayController.setupDefaultPalette();
	}

	canvas.beginUpdate();
	canvas.setPenColor(Color::White);
	canvas.setBrushColor(Color::White);
	
	canvas.fillEllipse(maxW/2, maxH/2, maxH, maxH);
	

	canvas.endUpdate();
}

#endif



void redraw()
{
	switch (_view) {
		case VIEW_COLORBARS: {
			draw_colorbars(_reverse);
		} break;

		case VIEW_GRAYBARS: { 
			draw_graybars(_reverse);
		} break;

		case VIEW_GRID: {
			draw_grid(_reverse);
		} break;

		case VIEW_DETAIL: {
			draw_detail(_reverse);
		} break;

		case VIEW_TESTCARD: {
			draw_testcard(_reverse);
		} break;

		default: {

		} break;
	}
}

void loop()
{  
	if (_update) {
		_update = false;
		redraw();
	}

	if (hid.keyAvailable()) {
		HIDKey key;
		if (hid.getNextKey(key)) {

			int dx = 0;
			int dy = 0;

			if (key.mod == 0xff) {
				uint32_t ts = millis();
				//printf("dpad: %04X %d %d %d/%d [%u]\n", key.dpad.key, key.dpad.x, key.dpad.y, key.mod, key.len, ts - last_ts);
				dx = key.dpad.x;
				dy = key.dpad.y;
				
				if (key.dpad.key & GP_A) {
					_view += 1;
					_view %= VIEW_LAST;
				} else if (key.dpad.key & GP_B) {
					if (_view >= 0) {
						_view -= 1;
					} else {
						_view = (VIEW_LAST - 1);
					}
				} else if (key.dpad.key & GP_X) {
					_reverse = !_reverse;
				}
				if ((key.dpad.key & (GP_A|GP_B|GP_X))) {
					_update = true;
					//printf("rev:%d clr:%d\n", _reverse, _color);
				}
			
			} /*else if ((key.mod == 0) && (key.len >= 1)) {

				switch (key.keys[0]) {
					case KEY_UP: 	printf("up\n");    dy = -1; break;
					case KEY_DOWN: 	printf("down\n");  dy =  1; break;
					case KEY_LEFT: 	printf("left\n");  dx = -1; break;
					case KEY_RIGHT: printf("right\n"); dx =  1; break;
					default: {
						printf("key(%02x):", key.mod);
						for (int i = 0; i < key.len; i++) {
							printf("%02x ", key.keys[i]);
						}
						printf("\n");
					}break;
				}
			}*/

			if (dx != 0 || dy != 0) {

				rc.X1 += dx * W/2;
				rc.X2 += dx * W/2;

				rc.Y1 += dy * H/2;
				rc.Y2 += dy * H/2;

				if (rc.X1 < 0) {
					rc.X1 = 0;
					rc.X2 = W;
				} else if (rc.X2 > maxW) {
					rc.X2 = maxW-1;
					rc.X1 = rc.X2 - W;
				}

				if (rc.Y1 < 0) {
					rc.Y1 = 0;
					rc.Y2 = H;
				} else if (rc.Y2 > maxH) {
					rc.Y2 = maxH-1;
					rc.Y1 = rc.Y2 - H;
				}
				/*
				if (dx < 0) {
					_reverse = true;
				} else if (dx > 1) {
					_reverse = false;
				}

				if (dy < 0) {
					_color = true;
				} else if (dy > 1) {
					_color = false;
				}
				_update = true;
				*/				
			}
		}
	}
}
