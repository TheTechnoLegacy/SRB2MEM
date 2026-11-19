// SRB2 2.0 - SDL2 / Android video backend (software renderer + touch controls)
// Replaces src/sdl/i_video.c, which targets SDL 1.2.
// GPL v2, same as SRB2.

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>

#include "SDL.h"

#include "../doomdef.h"
#include "../doomstat.h"
#include "../i_system.h"
#include "../v_video.h"
#include "../m_argv.h"
#include "../m_menu.h"
#include "../d_main.h"
#include "../s_sound.h"
#include "../i_sound.h"
#include "../i_joy.h"
#include "../st_stuff.h"
#include "../g_game.h"
#include "../i_video.h"
#include "../console.h"
#include "../command.h"
#include "../keys.h"
#include "../d_netcmd.h"
#include "../g_input.h"
#include "sdlmain.h"

#define MAXWINMODES (27)

rendermode_t rendermode = render_soft;
boolean highcolor = false;
consvar_t cv_vidwait = {"vid_wait", "On", CV_SAVE, CV_OnOff, NULL, 0, NULL, NULL, 0, 0, NULL};
UINT8 graphics_started = 0;
boolean allow_fullscreen = false;

static SDL_Window   *window   = NULL;
static SDL_Renderer *renderer = NULL;
static SDL_Texture  *texture  = NULL;
static Uint32       *pix      = NULL;      // ARGB8888 copy of the 8-bit screen
static Uint32        palette32[256];
static INT32         texW = 0, texH = 0;

static INT32 windowedModes[MAXWINMODES][2] =
{
	{1920,1200},{1680,1050},{1600,1200},{1600,1000},{1536,1152},{1536, 960},
	{1440, 900},{1400,1050},{1400, 875},{1360, 850},{1280, 960},{1280, 800},
	{1152, 864},{1120, 700},{1024, 768},{ 960, 720},{ 960, 600},{ 800, 600},
	{ 800, 500},{ 640, 480},{ 640, 400},{ 576, 432},{ 512, 384},{ 416, 312},
	{ 400, 300},{ 320, 240},{ 320, 200},
};
static char vidModeName[MAXWINMODES+1][32];

// ---------------------------------------------------------------------------
// Touch controls
// ---------------------------------------------------------------------------

enum { TC_UP, TC_DOWN, TC_LEFT, TC_RIGHT, TC_JUMP, TC_SPIN, TC_FIRE, TC_ENTER, TC_ESC, TC_CONSOLE, TC_SCREENSHOT, TC_COUNT };

static const INT32 tcKey[TC_COUNT] =
{
	KEY_UPARROW, KEY_DOWNARROW, KEY_LEFTARROW, KEY_RIGHTARROW,
	'/', '.', KEY_RCTRL, KEY_ENTER, KEY_ESCAPE, KEY_CONSOLE, KEY_F8
};

#define MAXFINGERS 10
// role: 0 = pressing on-screen controls, 1 = "look" finger (started outside every control; swipes turn the camera)
static struct { SDL_FingerID id; int active; int role; float x, y; } fingers[MAXFINGERS];
static INT32 tcPosted[TC_COUNT];       // key actually sent for each control while it is held
static boolean tcHeld[TC_COUNT];       // key state we have told the game about
static boolean touchMode = false;     // overlay shown once a finger touched the screen

// 5x7 glyphs for the button labels (7 rows of 5 columns)
static const char *const *glyph(char c)
{
	static const char *const gA[7] = {".###.", "#...#", "#...#", "#####", "#...#", "#...#", "#...#"};
	static const char *const gB[7] = {"####.", "#...#", "#...#", "####.", "#...#", "#...#", "####."};
	static const char *const gC[7] = {".###.", "#...#", "#....", "#....", "#....", "#...#", ".###."};
	static const char *const gE[7] = {"#####", "#....", "#....", "####.", "#....", "#....", "#####"};
	static const char *const gK[7] = {"#...#", "#..#.", "#.#..", "##...", "#.#..", "#..#.", "#...#"};
	static const char *const gN[7] = {"#...#", "##..#", "##..#", "#.#.#", "#..##", "#..##", "#...#"};
	static const char *const gO[7] = {".###.", "#...#", "#...#", "#...#", "#...#", "#...#", ".###."};
	static const char *const gS[7] = {".####", "#....", "#....", ".###.", "....#", "....#", "####."};
	static const char *const gX[7] = {"#...#", "#...#", ".#.#.", "..#..", ".#.#.", "#...#", "#...#"};
	switch (c)
	{
		case 'A': return gA;
		case 'B': return gB;
		case 'C': return gC;
		case 'E': return gE;
		case 'K': return gK;
		case 'N': return gN;
		case 'O': return gO;
		case 'S': return gS;
		case 'X': return gX;
		default:  return NULL;
	}
}

static void DrawLabel(const char *s, float cx, float cy, float px)
{
	const int n = (int)strlen(s);
	float x0 = cx - (n*6 - 1)*px/2.0f;
	float y0 = cy - 3.5f*px;
	int i, r, c;
	for (i = 0; i < n; i++)
	{
		const char *const *g = glyph(s[i]);
		if (!g) continue;
		for (r = 0; r < 7; r++)
			for (c = 0; c < 5; c++)
				if (g[r][c] == '#')
				{
					SDL_Rect rc;
					rc.x = (int)(x0 + (i*6+c)*px);
					rc.y = (int)(y0 + r*px);
					rc.w = rc.h = (int)px + 1;
					SDL_RenderFillRect(renderer, &rc);
				}
	}
}

typedef struct { float cx, cy, r; int id; const char *label; } tcbutton_t;

static void TouchLayout(int w, int h, float *u, float *dpx, float *dpy, tcbutton_t b[], int *nb)
{
	const float U = (float)(h < w ? h : w) / 6.0f;
	*u = U;
	*dpx = U*1.9f;
	*dpy = (float)h - U*1.9f;
	b[0].cx = w - U*1.1f; b[0].cy = h - U*1.7f; b[0].r = U*0.85f; b[0].id = TC_JUMP;  b[0].label = "A";
	b[1].cx = w - U*3.0f; b[1].cy = h - U*0.95f;b[1].r = U*0.8f;  b[1].id = TC_SPIN;  b[1].label = "B";
	b[2].cx = w - U*3.0f; b[2].cy = h - U*2.7f; b[2].r = U*0.65f; b[2].id = TC_FIRE;  b[2].label = "X";
	b[3].cx = w - U*1.0f; b[3].cy = U*0.55f;    b[3].r = U*0.5f;  b[3].id = TC_ENTER; b[3].label = "OK";
	b[4].cx = U*0.9f;     b[4].cy = U*0.55f;    b[4].r = U*0.5f;  b[4].id = TC_ESC;   b[4].label = "ESC";
	b[5].cx = w - U*2.5f; b[5].cy = U*0.55f;    b[5].r = U*0.5f;  b[5].id = TC_CONSOLE; b[5].label = "CON";
	b[6].cx = w - U*4.0f; b[6].cy = U*0.55f;    b[6].r = U*0.5f;  b[6].id = TC_SCREENSHOT; b[6].label = "SS";
	*nb = 7;
}

// d-pad left/right press A/D (strafe) while playing, but the arrow keys in menus and the console
static INT32 TouchKeyFor(int id)
{
	const boolean uiActive = menuactive || con_destlines > 0;
	if (id == TC_LEFT)  return uiActive ? KEY_LEFTARROW  : 'a';
	if (id == TC_RIGHT) return uiActive ? KEY_RIGHTARROW : 'd';
	return tcKey[id];
}

// is the point (in pixels) on the d-pad or on one of the buttons?
static boolean TouchPointInControl(float fx, float fy, int w, int h)
{
	float u, dpx, dpy;
	tcbutton_t b[8];
	int k, nb;
	float dx, dy;

	TouchLayout(w, h, &u, &dpx, &dpy, b, &nb);
	dx = fx - dpx;
	dy = fy - dpy;
	if (dx*dx + dy*dy < (u*2.2f)*(u*2.2f))
		return true;
	for (k = 0; k < nb; k++)
	{
		const float bx = fx - b[k].cx, by = fy - b[k].cy, r = b[k].r * 1.25f;
		if (bx*bx + by*by < r*r)
			return true;
	}
	return false;
}

static void TouchUpdate(void)
{
	boolean want[TC_COUNT];
	int w = 640, h = 400, i, k, nb;
	float u, dpx, dpy;
	tcbutton_t b[8];
	event_t ev;

	if (!renderer) return;
	SDL_GetRendererOutputSize(renderer, &w, &h);
	TouchLayout(w, h, &u, &dpx, &dpy, b, &nb);
	memset(want, 0, sizeof(want));

	for (i = 0; i < MAXFINGERS; i++)
	{
		float fx, fy;
		if (!fingers[i].active || fingers[i].role != 0) continue; // look fingers never press buttons
		fx = fingers[i].x * w;
		fy = fingers[i].y * h;

		// d-pad
		{
			const float dx = fx - dpx, dy = fy - dpy;
			if (dx*dx + dy*dy < (u*2.2f)*(u*2.2f))
			{
				const float dead = u*0.45f;
				if (dx < -dead) want[TC_LEFT]  = true;
				if (dx >  dead) want[TC_RIGHT] = true;
				if (dy < -dead) want[TC_UP]    = true;
				if (dy >  dead) want[TC_DOWN]  = true;
			}
		}
		for (k = 0; k < nb; k++)
		{
			const float dx = fx - b[k].cx, dy = fy - b[k].cy, r = b[k].r * 1.25f;
			if (dx*dx + dy*dy < r*r) want[b[k].id] = true;
		}
	}

	for (i = 0; i < TC_COUNT; i++)
	{
		if (want[i] != tcHeld[i])
		{
			tcHeld[i] = want[i];
			if (want[i]) tcPosted[i] = TouchKeyFor(i); // release the same key that was pressed
			memset(&ev, 0, sizeof(ev));
			ev.type = want[i] ? ev_keydown : ev_keyup;
			ev.data1 = tcPosted[i];
			D_PostEvent(&ev);
		}
	}
}

// ---------------------------------------------------------------------------
// Swipe to look: a finger that starts outside every control turns the camera.
//  - horizontal: added straight to the turn amount of the next tic command (touch_turn), so it
//    works the same in normal and Analog mode and does not depend on the mouse sensitivity
//  - vertical: mouse-look aiming (only when mouse-look is on)
// Speed: console variable "touchsens" (percent, default 100).
// ---------------------------------------------------------------------------
#define LOOK_TURN_FULLWIDTH 56000.0f  // angleturn units (65536 = 360 degrees) for a swipe across the whole width at 100% (~308 degrees)
// Vertical uses the SAME angle per swiped pixel as horizontal: one mouse-look unit is 8 turn units
// (360/8192 degrees vs 360/65536), and a full-height swipe is H/W of a full-width swipe.
#define LOOK_AIM_MAXUNITS 2000        // per tic; keeps (units << 19) from wrapping past 180 degrees

extern consvar_t cv_mlooksens;

static CV_PossibleValue_t touchsens_cons_t[] = {{10, "MIN"}, {1000, "MAX"}, {0, NULL}};
static consvar_t cv_touchsens = {"touchsens", "100", CV_SAVE, touchsens_cons_t, NULL, 0, NULL, NULL, 0, 0, NULL};

static float lookDX = 0.0f, lookDY = 0.0f;   // pending swipe, as a fraction of the screen
static tic_t lookLastTic = 0;

static void TouchLookReset(void)
{
	lookDX = lookDY = 0.0f;
}

static void TouchLookUpdate(void)
{
	const float sens = (float)cv_touchsens.value / 100.0f;
	const tic_t now = I_GetTime();
	event_t ev;

	if (con_destlines > 0) // typing in the console: ignore swipes
	{
		TouchLookReset();
		return;
	}

	// horizontal: swipe right = turn right (the engine's positive turn is to the left)
	if (lookDX != 0.0f)
	{
		const float k = LOOK_TURN_FULLWIDTH * sens;
		const INT32 turn = (INT32)(-lookDX * k);
		if (turn)
		{
			touch_turn += turn;
			lookDX += (float)turn / k;    // keep the fractional remainder
		}
	}

	// vertical: one mouse event per game tic (the engine keeps only the last one)
	if (!cv_alwaysfreelook.value)
		lookDY = 0.0f;
	else if (lookDY != 0.0f && now != lookLastTic)
	{
		int ow = 0, oh = 0;
		float aspect = 0.625f;
		float k;
		INT32 my;

		if (renderer) SDL_GetRendererOutputSize(renderer, &ow, &oh);
		if (ow > 0 && oh > 0) aspect = (float)oh / (float)ow;
		k = LOOK_TURN_FULLWIDTH * sens * aspect / 8.0f;
		// undo the engine's own mouse-look scaling so a changed "mlooksens" does not alter the swipe
		k /= ((float)cv_mlooksens.value * (float)cv_mlooksens.value) / 110.0f + 0.1f;
		my = (INT32)(-lookDY * k);
		if (my > LOOK_AIM_MAXUNITS) my = LOOK_AIM_MAXUNITS;
		else if (my < -LOOK_AIM_MAXUNITS) my = -LOOK_AIM_MAXUNITS;
		if (my)
		{
			memset(&ev, 0, sizeof(ev));
			ev.type = ev_mouse;
			ev.data2 = 0;
			ev.data3 = my;
			D_PostEvent(&ev);
			lookLastTic = now;
			lookDY += (float)my / k;
		}
	}
}

static void TouchFinger(const SDL_TouchFingerEvent *tf, int type)
{
	int i, slot = -1;
	touchMode = true;
	for (i = 0; i < MAXFINGERS; i++)
		if (fingers[i].active && fingers[i].id == tf->fingerId) { slot = i; break; }
	if (slot < 0 && type == SDL_FINGERDOWN)
		for (i = 0; i < MAXFINGERS; i++)
			if (!fingers[i].active) { slot = i; break; }
	if (slot < 0) return;

	if (type == SDL_FINGERDOWN)
	{
		int w = 640, h = 400;
		if (renderer) SDL_GetRendererOutputSize(renderer, &w, &h);
		fingers[slot].role = TouchPointInControl(tf->x * w, tf->y * h, w, h) ? 0 : 1;
	}
	else if (type == SDL_FINGERMOTION && fingers[slot].role == 1)
	{
		lookDX += tf->dx;
		lookDY += tf->dy;
	}

	fingers[slot].id = tf->fingerId;
	fingers[slot].x = tf->x;
	fingers[slot].y = tf->y;
	fingers[slot].active = (type != SDL_FINGERUP);
	TouchUpdate();
}

static void TouchDraw(int w, int h)
{
	float u, dpx, dpy;
	tcbutton_t b[8];
	int nb, k, i;
	SDL_Rect rc;

	if (!touchMode) return;
	TouchLayout(w, h, &u, &dpx, &dpy, b, &nb);
	SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);

	// d-pad: a plus shape
	for (i = 0; i < 4; i++)
	{
		const boolean on = tcHeld[i];
		const int dxs[4] = {0, 0, -1, 1}, dys[4] = {-1, 1, 0, 0};
		SDL_SetRenderDrawColor(renderer, 255, 255, 255, on ? 150 : 60);
		rc.w = rc.h = (int)(u*0.9f);
		rc.x = (int)(dpx + dxs[i]*u*0.95f - rc.w/2);
		rc.y = (int)(dpy + dys[i]*u*0.95f - rc.h/2);
		SDL_RenderFillRect(renderer, &rc);
	}
	for (k = 0; k < nb; k++)
	{
		const boolean on = tcHeld[b[k].id];
		SDL_SetRenderDrawColor(renderer, 255, 255, 255, on ? 150 : 60);
		rc.w = rc.h = (int)(b[k].r*2);
		rc.x = (int)(b[k].cx - b[k].r);
		rc.y = (int)(b[k].cy - b[k].r);
		SDL_RenderFillRect(renderer, &rc);
		SDL_SetRenderDrawColor(renderer, 0, 0, 0, 200);
		DrawLabel(b[k].label, b[k].cx, b[k].cy, u*0.05f);
	}
	SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);
}

// ---------------------------------------------------------------------------
// Keyboard
// ---------------------------------------------------------------------------


// ---------------------------------------------------------------------------
// On-screen keyboard: shown while the console is open. Printable characters come from SDL's
// text input events (exact characters, with Shift/symbols already applied) and are turned
// into the key (+Shift) events the console expects.
// ---------------------------------------------------------------------------

static boolean kbShown = false;

static void PostKeyEvent(INT32 key, boolean down)
{
	event_t ev;
	memset(&ev, 0, sizeof(ev));
	ev.type = down ? ev_keydown : ev_keyup;
	ev.data1 = key;
	D_PostEvent(&ev);
}

static void PostTypedChar(unsigned char c)
{
	static const char shifted[] = "~!@#$%^&*()_+{}|:\"<>?";
	static const char unshifted[] = "`1234567890-=[]\\;',./";
	INT32 key = c;
	boolean shift = false;

	if (c >= 'A' && c <= 'Z')
	{
		key = c - 'A' + 'a';
		shift = true;
	}
	else if (c < 32 || c > 126)
		return;
	else
	{
		const char *p = strchr(shifted, c);
		if (p && *p)
		{
			key = unshifted[p - shifted];
			shift = true;
		}
	}

	if (shift) PostKeyEvent(KEY_LSHIFT, true);
	PostKeyEvent(key, true);
	PostKeyEvent(key, false);
	if (shift) PostKeyEvent(KEY_LSHIFT, false);
}

static void UpdateSoftKeyboard(void)
{
	const boolean want = (con_destlines > 0);
	if (want == kbShown) return;
	kbShown = want;
	if (want) SDL_StartTextInput();
	else      SDL_StopTextInput();
}

static INT32 SDLatekey(SDL_Keycode sym)
{
	switch (sym)
	{
		case SDLK_UP:        return KEY_UPARROW;
		case SDLK_DOWN:      return KEY_DOWNARROW;
		case SDLK_LEFT:      return KEY_LEFTARROW;
		case SDLK_RIGHT:     return KEY_RIGHTARROW;
		case SDLK_RETURN:
		case SDLK_KP_ENTER:  return KEY_ENTER;
		case SDLK_AC_BACK:   // Android back button
		case SDLK_ESCAPE:    return KEY_ESCAPE;
		case SDLK_BACKSPACE: return KEY_BACKSPACE;
		case SDLK_TAB:       return KEY_TAB;
		case SDLK_SPACE:     return KEY_SPACE;
		case SDLK_LSHIFT:    return KEY_LSHIFT;
		case SDLK_RSHIFT:    return KEY_RSHIFT;
		case SDLK_LCTRL:     return KEY_LCTRL;
		case SDLK_RCTRL:     return KEY_RCTRL;
		case SDLK_LALT:      return KEY_LALT;
		case SDLK_RALT:      return KEY_RALT;
		case SDLK_PAUSE:     return KEY_PAUSE;
		case SDLK_HOME:      return KEY_HOME;
		case SDLK_END:       return KEY_END;
		case SDLK_PAGEUP:    return KEY_PGUP;
		case SDLK_PAGEDOWN:  return KEY_PGDN;
		case SDLK_INSERT:    return KEY_INS;
		case SDLK_DELETE:    return KEY_DEL;
		case SDLK_F1:  return KEY_F1;
		case SDLK_F2:  return KEY_F2;
		case SDLK_F3:  return KEY_F3;
		case SDLK_F4:  return KEY_F4;
		case SDLK_F5:  return KEY_F5;
		case SDLK_F6:  return KEY_F6;
		case SDLK_F7:  return KEY_F7;
		case SDLK_F8:  return KEY_F8;
		case SDLK_F9:  return KEY_F9;
		case SDLK_F10: return KEY_F10;
		case SDLK_F11: return KEY_F11;
		case SDLK_F12: return KEY_F12;
		default:
			if (sym >= 'A' && sym <= 'Z') return (INT32)(sym - 'A' + 'a');
			if (sym >= 32 && sym < 127)   return (INT32)sym;
			return 0;
	}
}

// ---------------------------------------------------------------------------
// Texture / screen buffer management
// ---------------------------------------------------------------------------

static void FreeTexture(void)
{
	if (texture) SDL_DestroyTexture(texture);
	texture = NULL;
	if (pix) free(pix);
	pix = NULL;
}

static boolean MakeTexture(INT32 w, INT32 h)
{
	FreeTexture();
	if (!renderer) return false;
	texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, w, h);
	if (!texture) return false;
	pix = (Uint32 *)malloc((size_t)w * (size_t)h * sizeof(Uint32));
	if (!pix) return false;
	memset(pix, 0, (size_t)w * (size_t)h * sizeof(Uint32));
	texW = w;
	texH = h;
	return true;
}

// ---------------------------------------------------------------------------
// Events
// ---------------------------------------------------------------------------

void I_GetEvent(void)
{
	SDL_Event e;
	static boolean sdlquit = false;
	event_t event;

	if (!graphics_started)
		return;

	UpdateSoftKeyboard();

	while (SDL_PollEvent(&e))
	{
		memset(&event, 0, sizeof(event));
		switch (e.type)
		{
			case SDL_KEYDOWN:
			case SDL_KEYUP:
				if (e.key.repeat) break;
				// while typing into the console, printable characters arrive as text input instead
				if (kbShown && e.type == SDL_KEYDOWN && e.key.keysym.sym >= 32 && e.key.keysym.sym < 127) break;
				event.type = (e.type == SDL_KEYUP) ? ev_keyup : ev_keydown;
				event.data1 = SDLatekey(e.key.keysym.sym);
				if (event.data1) D_PostEvent(&event);
				break;

			case SDL_TEXTINPUT:
				if (kbShown)
				{
					const char *t;
					for (t = e.text.text; *t; t++)
						PostTypedChar((unsigned char)*t);
				}
				break;

			case SDL_FINGERDOWN:
			case SDL_FINGERMOTION:
			case SDL_FINGERUP:
				TouchFinger(&e.tfinger, e.type);
				break;

			case SDL_WINDOWEVENT:
				if (e.window.event == SDL_WINDOWEVENT_FOCUS_LOST ||
				    e.window.event == SDL_WINDOWEVENT_MINIMIZED)
				{
					int i;
					memset(fingers, 0, sizeof(fingers));
					TouchUpdate();
					for (i = 0; i < TC_COUNT; i++) tcHeld[i] = false;
					TouchLookReset();
				}
				break;

			case SDL_RENDER_TARGETS_RESET:
			case SDL_RENDER_DEVICE_RESET:
				if (texW > 0 && texH > 0) { INT32 w = texW, h = texH; MakeTexture(w, h); }
				break;

			case SDL_QUIT:
				if (!sdlquit)
				{
					sdlquit = true;
					M_QuitResponse('y');
				}
				break;

			default:
				break;
		}
	}

	TouchLookUpdate();
}

void I_StartupMouse(void) {}

void I_OsPolling(void)
{
	if (consolevent)
		I_GetConsoleEvents();
	I_GetMouseEvents();
	I_GetEvent();
}

void I_UpdateNoBlit(void) {}

// ---------------------------------------------------------------------------
// Drawing
// ---------------------------------------------------------------------------

#ifdef __ANDROID__
extern void Android_Heartbeat(void);
extern void Android_SetPaused(int paused);

// called by SDL on the Android UI thread the moment the app is paused/resumed
static int SDLCALL AppStateWatch(void *userdata, SDL_Event *e)
{
	(void)userdata;
	if (e->type == SDL_APP_WILLENTERBACKGROUND)
		Android_SetPaused(1);
	else if (e->type == SDL_APP_DIDENTERFOREGROUND)
		Android_SetPaused(0);
	return 1;
}
#endif

void I_FinishUpdate(void)
{
#ifdef __ANDROID__
	Android_Heartbeat();
#endif
	int ow = 0, oh = 0;
	SDL_Rect dst;
	INT32 x, y;
	const UINT8 *src;

	if (rendermode == render_none || !renderer || !texture || !pix || !screens[0])
		return;

	src = screens[0];
	for (y = 0; y < texH; y++)
	{
		const UINT8 *row = src + (size_t)y * vid.rowbytes;
		Uint32 *out = pix + (size_t)y * texW;
		for (x = 0; x < texW; x++)
			out[x] = palette32[row[x]];
	}
	SDL_UpdateTexture(texture, NULL, pix, texW * (int)sizeof(Uint32));

	SDL_GetRendererOutputSize(renderer, &ow, &oh);
	{
		// letterbox, keep aspect
		const float sx = (float)ow / (float)texW, sy = (float)oh / (float)texH;
		const float s = sx < sy ? sx : sy;
		dst.w = (int)(texW * s);
		dst.h = (int)(texH * s);
		dst.x = (ow - dst.w) / 2;
		dst.y = (oh - dst.h) / 2;
	}
	SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
	SDL_RenderClear(renderer);
	SDL_RenderCopy(renderer, texture, NULL, &dst);
	TouchDraw(ow, oh);
	SDL_RenderPresent(renderer);
}

void I_UpdateNoVsync(void)
{
	I_FinishUpdate();
}

void I_ReadScreen(UINT8 *scr)
{
	if (rendermode != render_soft)
		I_Error("I_ReadScreen: called while in non-software mode");
	else
		VID_BlitLinearScreen(screens[0], scr, vid.width*vid.bpp, vid.height, vid.rowbytes, vid.rowbytes);
}

void I_SetPalette(RGBA_t *palette)
{
	size_t i;
	for (i = 0; i < 256; i++)
		palette32[i] = 0xFF000000u
			| ((Uint32)palette[i].s.red   << 16)
			| ((Uint32)palette[i].s.green << 8)
			|  (Uint32)palette[i].s.blue;
}

// ---------------------------------------------------------------------------
// Video modes (only used to size the internal software buffer)
// ---------------------------------------------------------------------------

INT32 VID_NumModes(void)
{
	return MAXWINMODES;
}

const char *VID_GetModeName(INT32 modeNum)
{
	if (modeNum < 0 || modeNum >= MAXWINMODES)
		return NULL;
	sprintf(&vidModeName[modeNum][0], "%dx%d", windowedModes[modeNum][0], windowedModes[modeNum][1]);
	return &vidModeName[modeNum][0];
}

INT32 VID_GetModeForSize(INT32 w, INT32 h)
{
	INT32 i, match = -1;
	for (i = 0; i < MAXWINMODES; i++)
		if (windowedModes[i][0] == w && windowedModes[i][1] == h) { match = i; break; }
	if (match == -1)
	{
		w -= w % BASEVIDWIDTH;
		h -= h % BASEVIDHEIGHT;
		for (i = 0; i < MAXWINMODES; i++)
			if (windowedModes[i][0] == w && windowedModes[i][1] == h) { match = i; break; }
		if (match == -1) match = MAXWINMODES - 1;
	}
	return match;
}

void VID_PrepareModeList(void) {}

INT32 VID_SetMode(INT32 modeNum)
{
	if (modeNum < 0) modeNum = 0;
	if (modeNum >= MAXWINMODES) modeNum = MAXWINMODES - 1;

	vid.recalc = true;
	vid.width  = windowedModes[modeNum][0];
	vid.height = windowedModes[modeNum][1];

	if (vid.buffer) free(vid.buffer);
	vid.buffer = NULL;

	if (!MakeTexture(vid.width, vid.height))
		I_Error("Could not create video texture: %s\n", SDL_GetError());

	vid.modenum  = modeNum;
	vid.rowbytes = vid.width * vid.bpp;
	vid.direct   = NULL;
	vid.buffer   = (UINT8 *)malloc(vid.rowbytes * vid.height * NUMSCREENS);
	if (!vid.buffer)
		I_Error("Not enough memory for video buffer\n");
	memset(vid.buffer, 0x00, vid.rowbytes * vid.height * NUMSCREENS);

	I_StartupMouse();
	return true;
}

// ---------------------------------------------------------------------------
// Startup / shutdown
// ---------------------------------------------------------------------------

void I_StartupGraphics(void)
{
	Uint32 wflags = SDL_WINDOW_SHOWN;
	Uint32 rflags = SDL_RENDERER_ACCELERATED;

	if (graphics_started || dedicated)
		return;

	CV_RegisterVar(&cv_vidwait);
	CV_RegisterVar(&cv_touchsens);

	keyboard_started = true;

	SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "0");
	SDL_SetHint(SDL_HINT_TOUCH_MOUSE_EVENTS, "0");
	SDL_SetHint(SDL_HINT_ORIENTATIONS, "LandscapeLeft LandscapeRight");

	if (SDL_InitSubSystem(SDL_INIT_VIDEO) < 0)
	{
		CONS_Printf("Couldn't initialize SDL's Video System: %s\n", SDL_GetError());
		return;
	}
	CONS_Printf("Starting up with video driver : %s\n", SDL_GetCurrentVideoDriver());

	rendermode = render_soft; // no OpenGL in this port

#ifdef __ANDROID__
	wflags |= SDL_WINDOW_FULLSCREEN;
#else
	wflags |= SDL_WINDOW_RESIZABLE;
#endif
	window = SDL_CreateWindow("SRB2", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, 640, 400, wflags);
	if (!window)
	{
		CONS_Printf("Could not create window: %s\n", SDL_GetError());
		return;
	}
	if (cv_vidwait.value) rflags |= SDL_RENDERER_PRESENTVSYNC;
	renderer = SDL_CreateRenderer(window, -1, rflags);
	if (!renderer)
		renderer = SDL_CreateRenderer(window, -1, 0); // software fallback
	if (!renderer)
	{
		CONS_Printf("Could not create renderer: %s\n", SDL_GetError());
		return;
	}
	SDL_ShowCursor(SDL_DISABLE);
#ifdef __ANDROID__
	SDL_AddEventWatch(AppStateWatch, NULL);
#endif

	vid.buffer = NULL;
	vid.width  = BASEVIDWIDTH;
	vid.height = BASEVIDHEIGHT;
	vid.recalc = true;
	vid.direct = NULL;
	vid.bpp    = 1;
	vid.WndParent = NULL;

	memset(palette32, 0, sizeof(palette32));
	memset(fingers, 0, sizeof(fingers));
	memset(tcHeld, 0, sizeof(tcHeld));

	if (!MakeTexture(vid.width, vid.height))
	{
		CONS_Printf("Could not create video texture: %s\n", SDL_GetError());
		return;
	}
	vid.rowbytes = vid.width * vid.bpp;
	vid.buffer = (UINT8 *)malloc(vid.rowbytes * vid.height * NUMSCREENS);
	if (vid.buffer) memset(vid.buffer, 0x00, vid.rowbytes * vid.height * NUMSCREENS);
	else CONS_Printf("Not enough memory for video buffer\n");

	graphics_started = true;
}

void I_ShutdownGraphics(void)
{
	rendermode = render_none;
	if (vid.buffer) free(vid.buffer);
	vid.buffer = NULL;
	FreeTexture();
	if (renderer) SDL_DestroyRenderer(renderer);
	renderer = NULL;
	if (window) SDL_DestroyWindow(window);
	window = NULL;

	if (!graphics_started)
		return;
	CONS_Printf("I_ShutdownGraphics: ");
	graphics_started = false;
	CONS_Printf("shut down\n");
	SDL_QuitSubSystem(SDL_INIT_VIDEO);
	framebuffer = SDL_FALSE;
}

void SDLforceUngrabMouse(void) {}
