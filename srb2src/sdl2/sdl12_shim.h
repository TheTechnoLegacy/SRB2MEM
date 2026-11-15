// Force-included (-include) when building SRB2 2.0 against SDL2.
// Provides the few SDL 1.2 calls the old sdl/ interface code still makes.
#ifndef SDL12_SHIM_H
#define SDL12_SHIM_H

#include <string.h>
#include "SDL.h"
#ifdef HAVE_MIXER
#include "SDL_mixer.h"
#endif

// SDL2's SDL_config.h defines HAVE_FMOD (the libm function), which the
// 2.0 sound code mistakes for the FMOD audio library.
#ifdef HAVE_FMOD
#undef HAVE_FMOD
#endif

static inline const SDL_version *SDL12_Linked_Version(void)
{
	static SDL_version v;
	SDL_GetVersion(&v);
	return &v;
}
#define SDL_Linked_Version SDL12_Linked_Version

static inline char *SDL12_AudioDriverName(char *namebuf, int maxlen)
{
	const char *n = SDL_GetCurrentAudioDriver();
	if (!n || maxlen <= 0) return NULL;
	strncpy(namebuf, n, (size_t)maxlen - 1);
	namebuf[maxlen - 1] = '\0';
	return namebuf;
}
#define SDL_AudioDriverName SDL12_AudioDriverName

// SDL 1.2 SDL_JoystickName(int index) became SDL_JoystickNameForIndex
#define SDL_JoystickName SDL_JoystickNameForIndex

#define SDL_HasMMXExt()   SDL_FALSE
#define SDL_Has3DNowExt() SDL_FALSE

#ifdef HAVE_MIXER
// SDL2_mixer added a "freesrc" argument
#define Mix_LoadMUS_RW(rw) Mix_LoadMUS_RW((rw), 0)
#endif

#endif
