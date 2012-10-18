/*
pxl - ppm image viewer
Written in 2012 by <Olga Miller> <olga.rgb@googlemail.com>
To the extent possible under law, the author(s) have dedicated all copyright and related and neighboring rights to this software to the public domain worldwide. This software is distributed without any warranty.
You should have received a copy of the CC0 Public Domain Dedication along with this software. If not, see <http://creativecommons.org/publicdomain/zero/1.0/>.
*/

#include <SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <stdarg.h>
#include "image.h"
#include "reader.h"

char* icon = "pixelka"; //TODO: create icon

SDL_Surface* screen;
int scale;
int grid;

int args_num;
char** args;

struct image img;
char* filename;

int fb_dirty;

int x_grid_cell;
int y_grid_cell;

int offset_x;
int offset_y;

int start_x;
int start_y;

void exiterr(const char* fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	fprintf(stderr, fmt, args);
	fprintf(stderr, "%s\n", SDL_GetError());
	exit(1);
}

void resize_video(int w, int h)
{
	if(!screen || (screen->w != w || screen->h != h))
	{
		if(screen)
			SDL_FreeSurface(screen);

		screen = SDL_SetVideoMode(w, h, 32, SDL_HWSURFACE | SDL_RESIZABLE | SDL_DOUBLEBUF);

		if(!screen)
			exiterr("Unable to set video.\n");
		if(screen->format->BitsPerPixel != 32)
			exiterr("Could not init 32 bit format.\n");
		if(screen->w != w || screen->h != h)
			exiterr("Could not get %dx%d window.\n", w, h);
	}

	SDL_WM_SetCaption(filename, icon);
	SDL_FillRect(screen, 0, 0);
}

void set_pixel(int x, int y, uint32_t* fb, int length, uint32_t rgb)
{
	int i = y * screen->w + x;

	if((0 <= i && i < length) && (x < screen->w && y < screen->h))
		fb[i] = rgb;
}

void get_color(int x, int y, uint32_t* color)
{
	if(x < img.w && y < img.h)
	{
		struct pixel p = img.pixels[y * img.w + x];
		*color = (p.red << 16) | (p.green << 8) | (p.blue);
	}
}

void draw()
{
	int step = scale + grid;
	//uint32_t rgb_grid = 0;
	uint32_t* fb = (uint32_t*) screen->pixels;

	int w = screen->w;
	int h = screen->h;

//TODO: fix offset
	offset_x = 0;
	offset_y = 0;
	//int length = screen->w * screen->h;

	for(int y = 0; y < h; y++)
	{
		int r_j = (y + offset_y - grid) % step;
		int j = (y + offset_y - grid) / step;

		for(int x = 0; x < w; x++)
		{
			int r_i = (x + offset_x - grid) % step;
			int i = (x + offset_x - grid) / step;
			uint32_t rgb = 0x00000000;

			if(0 <= r_i && 0 <= r_j)
			{
				if(!grid || (r_i != scale && r_j != scale))
					get_color(i, j, &rgb);
			}

			fb[y * w + x] = rgb;
/*
			for(int k = 0; k < scale; k++)
			{
				int w_pos_k = w_pos + k;

				for(int l = 0; l < scale; l++)
				{
					set_pixel(w_pos_k, h_pos + l, fb, length, rgb);
				}

				if(grid)
					set_pixel(w_pos_k, h_pos + scale, fb, length, rgb_grid);
			}

			if(grid)
			{
				for(int l = 0; l < scale; l++)
				{
					set_pixel(w_pos + scale, h_pos + l, fb, length, rgb_grid);
				}
			}*/
		}
	}

	fb_dirty = 1;
}

void set_filename(int direction)
{
	static int curr_arg = -1;

	if(direction != 1 && direction != -1)
		direction = 1;

	curr_arg = (curr_arg + args_num + direction) % args_num;
	filename = args[curr_arg + 1];
}

void read_image(int direction)
{
	int i = 0;

	set_filename(direction);

	while(!read_ppm_P6(filename, &img))
	{
		set_filename(direction);

		if(i == args_num)
			exiterr("No file is readable.\n");
		i++;
	}
}

void draw_grid_cell(uint32_t rgb)
{
	uint32_t* fb = (uint32_t*) screen->pixels;
	int length = screen->w * screen->h;

	int jump = scale + 1;
	int line = scale + 2;

	for(int i = 0; i < line; i++)
	{
		int x = x_grid_cell + i;
		int y = y_grid_cell;
		set_pixel(x, y, fb, length, rgb);
		set_pixel(x, y + jump, fb, length, rgb);

		x = x_grid_cell;
		y = y_grid_cell + i;
		set_pixel(x, y, fb, length, rgb);
		set_pixel(x + jump, y, fb, length, rgb);
	}

	fb_dirty = 1;
}

void change(int x_mouse, int y_mouse)
{
	char caption[100];

	int step = scale + grid;

	int x = (x_mouse - grid) / step;
	int y = (y_mouse - grid) / step;

	if((offset_x <= x && x < (img.w + offset_x)) 
	&& (offset_y <= y && y < (img.h + offset_y)))
	{
		if(grid)
		{
			draw_grid_cell(0);

			x_grid_cell = x * step;
			y_grid_cell = y * step;

			draw_grid_cell(0x00dddddd);
		}

		x -= offset_x;
		y -= offset_y;
		struct pixel p = img.pixels[y * img.w + x];

		snprintf(caption, 100, "%s [%d x %d] (%d; %d; %d)", filename, x, y, p.red, p.green, p.blue);
	}
	else
		snprintf(caption, 50, "%s", filename);

	SDL_WM_SetCaption(caption, icon);
}

void set_offset(int new_x, int new_y)
{
	int max_x = (img.w * (scale + grid) + grid) - screen->w;	
	int max_y = (img.h * (scale + grid) + grid) - screen->h;

	if(max_x >= 0)
	{
		offset_x = (int)(max_x * 0.5f);
		start_x = 0;
	}
	else
	{
		offset_x = fminf(fmaxf(new_x, max_x), 0);
		start_x = -offset_x;
	}

	if(max_y >= 0)
	{
		offset_y = (int)(max_y * 0.5);
		start_y = 0;
	}
	else
	{
		offset_y = fminf(fmaxf(new_y, max_y), 0);
		start_y = -offset_y;
	}
}

void redraw()
{
	x_grid_cell = 0;
	y_grid_cell = 0;

	set_offset(offset_x, offset_y);

	SDL_WM_SetCaption(filename, icon);
	SDL_FillRect(screen, 0, 0);

	draw();
}

void handle_keydown(SDLKey key)
{
	switch(key)
	{
		case SDLK_LEFT:
			set_offset(offset_x - 1, offset_y);
			draw();
			break;
		case SDLK_UP:
			set_offset(offset_x, offset_y - 1);
			draw();
			break;
		case SDLK_RIGHT:
			set_offset(offset_x + 1, offset_y);
			draw();
			break;
		case SDLK_DOWN:
			set_offset(offset_x, offset_y + 1);
			draw();
			break;
		case SDLK_g:
			grid ^= 1;
			redraw();
			break;
		case SDLK_SPACE:
			read_image(1);
			redraw();
			break;
		case SDLK_BACKSPACE:
			read_image(-1);
			redraw();
			break;
		case SDLK_q:
		case SDLK_ESCAPE:
			exit(0);
			break;
		default:
			if(SDLK_0 < key && key <= SDLK_9)
			{
				scale = key - SDLK_0;
				redraw();
			}
			break;
	}
}

void handle_event()
{
	SDL_Event event;

	static int mousebuttonleft_down = 0;

	int mouse_x = -1;
	int mouse_y = -1;

	int mouse_xrel = 0;
	int mouse_yrel = 0;

	int w = 0;
	int h = 0;

	while(SDL_PollEvent(&event))
	{
		switch(event.type)
		{
			case SDL_MOUSEMOTION:
				mouse_x = event.motion.x;
				mouse_y = event.motion.y;
				mouse_xrel += event.motion.xrel;
				mouse_yrel += event.motion.yrel;
				break;
			case SDL_KEYDOWN:
				handle_keydown(event.key.keysym.sym);
				break;
			case SDL_MOUSEBUTTONDOWN:
				if(event.button.button == SDL_BUTTON_LEFT)
					mousebuttonleft_down = 1;
				break;
			case SDL_MOUSEBUTTONUP:
				if(event.button.button == SDL_BUTTON_LEFT)
					mousebuttonleft_down = 0;
				break;
			case SDL_VIDEORESIZE:
				w = event.resize.w;
				h = event.resize.h;
				break;
			case SDL_QUIT:
				exit(0);
				break;
			default:
				break;
		}
	}

	if(mousebuttonleft_down)
	{
		set_offset(offset_x + mouse_xrel, offset_y + mouse_yrel);
		draw();
	}

	if(mouse_x != -1 && mouse_y != -1)
		change(mouse_x, mouse_y);

	if(w && h)
	{
		resize_video(w, h);
		redraw();
	}
}

int main(int argc, char** argv)
{
	if(argc <= 1)
		exiterr("No args.\n");

	img.pixels = 0;

	scale = 4;
	grid = 0;

	x_grid_cell = 0;
	y_grid_cell = 0;

	offset_x = 0;
	offset_y = 0;

	start_x = 0;
	start_y = 0;

	fb_dirty = 0;

	args_num = argc - 1;
	args = argv;

	if(SDL_Init(SDL_INIT_VIDEO) < 0)
		exiterr("SDL can not be initialized.\n");
	SDL_EnableKeyRepeat(SDL_DEFAULT_REPEAT_DELAY, SDL_DEFAULT_REPEAT_INTERVAL);

	read_image(1);
	resize_video(640, 480);
	draw();

	for(;;)
	{
		handle_event();

		if(fb_dirty)
		{
			SDL_Flip(screen);
			fb_dirty = 0;
		}

		SDL_Delay(10);
	}

	return 0;
}
