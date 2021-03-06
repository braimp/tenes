#include <stdio.h>
#include <stdlib.h>
#include <SDL.h>
#include <SDL_timer.h>
#include <sys/types.h>
#include <unistd.h>
#include <math.h>
#include <string.h>
#include <sys/time.h>
#include <time.h>
#include <sched.h>

#ifndef _WIN32
#include <sys/stat.h>
#include <sys/wait.h>
#endif

#include "nespal.h"
#include "nes.h"
#include "config.h"

static long long tv_to_micros (struct timeval *tv)
{
    return (((long long)tv->tv_sec) * 1000000ll) + ((long long)tv->tv_usec);
}

long long usectime (void)
{
    struct timeval tv;
    if (gettimeofday(&tv, 0)) {
        perror("gettimeofday");
        exit(1);
    }
    return tv_to_micros(&tv);
}

void sys_framesync (void)
{
    long long target = time_frame_target;
    long long now;

    do {
        now = usectime();
        assert((target - now) < 1000000ll);
        if (target-now > 10000) usleep(6000);
    } while (now < target);

    //if ((now-target)>1) printf("framesync: missed by %lli microseconds\n", now - target);
}

SDL_Color palette[129];
struct joystick joystick[4];
int numsticks = 0;

void print_video_info (void)
{
    const SDL_VideoInfo *vi = SDL_GetVideoInfo();
    char namebuf[64];
    printf("Video driver: %s\n", SDL_VideoDriverName(namebuf, 64));
    printf("Video memory: %i KB   BPP: %i R:%X(%i) G:%X(%i) B:%X(%i)   HW? %s   Accel: %s %s\n",
           vi->video_mem,
           vi->vfmt->BitsPerPixel,
           vi->vfmt->Rmask, vi->vfmt->Rshift,
           vi->vfmt->Gmask, vi->vfmt->Gshift,
           vi->vfmt->Bmask, vi->vfmt->Bshift,
           vi->hw_available? "yes" : "no",
           vi->blit_hw? "HW" : "-",
           vi->blit_sw? "SW" : "-");
}

void sys_init (void)
{
    struct sched_param sparam;
    memset(&sparam, 0, sizeof(sparam));
    sparam.sched_priority = 1;
    // Probably a bad idea:
    //if (sched_setscheduler(getpid(), SCHED_FIFO, &sparam)) perror("sched_setscheduler");

    int i, tmp;
    int surfaceflags = SDL_DOUBLEBUF | SDL_HWACCEL | SDL_HWSURFACE | (vid_fullscreen ? SDL_FULLSCREEN : 0);

    if (SDL_Init (SDL_INIT_AUDIO | SDL_INIT_VIDEO | SDL_INIT_JOYSTICK)) {
        printf ("Could not initialize SDL!\n");
        exit (1);
    }

    /* Initializes video fitering */
    build_color_maps();
    if (vid_fullscreen && (vid_filter == no_filter)) vid_filter = rescale_2x;
    vid_filter();

    window_width = max(window_width, vid_width);
    window_height = max(window_height, vid_height);

      window_surface = SDL_SetVideoMode(window_width, window_height, vid_bpp, surfaceflags);
      if (window_surface == NULL) {
          printf("Could not set desired display mode!\n");
          exit(1);
      }

      const SDL_VideoInfo *vi = SDL_GetVideoInfo();
      rgb_shifts.r_shift = vi->vfmt->Rshift;
      rgb_shifts.g_shift = vi->vfmt->Gshift;
      rgb_shifts.b_shift = vi->vfmt->Bshift;

      print_video_info();

      SDL_WM_SetCaption ("tenes","tenes");
      SDL_FillRect(window_surface, NULL, SDL_MapRGB(window_surface->format, 0, 0, 0));
      SDL_Flip (window_surface);
      SDL_ShowCursor(SDL_DISABLE);

      tmp = SDL_NumJoysticks();
      if (!cfg_disable_joysticks) {
          printf("Found %i joystick%s.\n", tmp, tmp==1?"":"s");
          //  for (i=0; i<tmp; i++) printf ("  %i: %s\n",i,SDL_JoystickName(i));
          if (tmp) {
              numsticks = (tmp>4)?4:tmp;
              for (i=0; i<numsticks; i++) {
                  joystick[i].sdl = SDL_JoystickOpen(i);
                  if (!joystick[i].sdl) printf ("Could not open joystick %i \n", i);
                  else {
                      int j, js_mapping=-1;
                      joystick[i].connected = 1;
                      for (j=0; j<4; j++)
                      {
                          if (cfg_jsmap[j]==i)
                          {
                              js_mapping=j;
                              break;
                          }
                      }
                      printf ("  %i: %s (%i buttons)   ", i, SDL_JoystickName(i), SDL_JoystickNumButtons(joystick[i].sdl));
                      if (js_mapping==-1) printf("[unmapped]\n");
                      else printf("[joypad %i]\n", js_mapping);
                  }
              }
              SDL_JoystickEventState (SDL_ENABLE);
          }
      } else printf("Joysticks are disabled.\n");
}



void sys_shutdown (void)
{
    int i;

    for (i=0; i<numsticks; i++) {
        if (joystick[i].sdl) SDL_JoystickClose(joystick[i].sdl);
    }

    SDL_FreeSurface (window_surface);
    SDL_Quit ();
}

void image_free (image_t image)
{
    SDL_FreeSurface(image->_sdl);
    if (image->freeptr) free(image->freeptr);
    free(image);
}

void make_dir (const char *path)
{
#ifdef _WIN32
    mkdir(path);
#else
    mkdir(path, 0755);
#endif
}

void swizzle_pixels (uint32_t *pixels, size_t len)
{
    struct rgb_shifts sw = rgb_shifts;

    for (size_t x = 0; x < len; x++)
    {
        unsigned px = pixels[x];
        byte r = (px >> 16) & 0xFF, g = (px >> 8) & 0xFF, b = px & 0xFF;
        pixels[x] = (r << sw.r_shift) | (g << sw.g_shift) | (b << sw.b_shift);
    }
}

