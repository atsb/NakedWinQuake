// vid_sdl.h -- sdl video driver 

#include "SDL.h"
#include "quakedef.h"
#include "d_local.h"

viddef_t    vid;                // global video state
unsigned short  d_8to16table[256];

#define BASEWIDTH           320
#define BASEHEIGHT          200

unsigned screenWidth = 320;
unsigned screenHeight = 200;

int    VGA_width, VGA_height, VGA_rowbytes, VGA_bufferrowbytes = 0;
byte    *VGA_pagebase;

static int	lockcount;
static qboolean	vid_initialized = false;
static SDL_Window *window;
static SDL_Renderer *renderer;
static SDL_Texture *texture;
static Uint32 SdlPalette[256]; // For storing the palette in texture format
static byte *frame_buffer;    // 8-bit indexed pixel data buffer
static qboolean	palette_changed;
static unsigned char	vid_curpal[256*3];	/* save for mode changes */
static qboolean mouse_avail;
static float   mouse_x, mouse_y;
static int mouse_oldbuttonstate = 0;

// No support for option menus
void (*vid_menudrawfn)(void) = NULL;
void (*vid_menukeyfn)(int key) = NULL;

void    VID_SetPalette (unsigned char *palette)
{
	int		i;
	SDL_Color colors[256];
	palette_changed = true;

	if (palette != vid_curpal)
		memcpy(vid_curpal, palette, sizeof(vid_curpal));

	for (i = 0; i < 256; ++i)
	{
		colors[i].r = *palette++;
		colors[i].g = *palette++;
		colors[i].b = *palette++;
	}

	for (i = 0; i < 256; ++i) {
        // Simpler direct construction for ARGB8888 (0xAARRGGBB)
        SdlPalette[i] = ((Uint32)0xFF << 24) | // Alpha
                        ((Uint32)colors[i].r << 16) |
                        ((Uint32)colors[i].g << 8) |
                        ((Uint32)colors[i].b);
	}
}

void    VID_ShiftPalette (unsigned char *palette)
{
    VID_SetPalette(palette);
}

void VID_LockBuffer (void)
{
	lockcount++;

	if (lockcount > 1)
		return;

	vid.buffer = vid.conbuffer = vid.direct = frame_buffer;
	vid.rowbytes = vid.conrowbytes = vid.width;

	if (r_dowarp)
		d_viewbuffer = r_warpbuffer;
	else
		d_viewbuffer = frame_buffer;
	if (r_dowarp)
		screenwidth = WARP_WIDTH;
	else
		screenwidth = vid.width;
}

void VID_UnlockBuffer (void)
{
	lockcount--;

	if (lockcount > 0)
		return;

	if (lockcount < 0) {
		lockcount = 0; // Reset to prevent further issues
		return;
	}
}

void    VID_Init (unsigned char *palette)
{
    int pnum, chunk;
    byte *cache;
    int cachesize;
    Uint8 video_bpp;
    Uint16 video_w, video_h;
    Uint32 flags;
    char caption[50];
    Uint32 window_flags = 0; // For SDL_CreateWindow

    // Load the SDL library
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO) < 0)
        Sys_Error("VID: Couldn't load SDL: %s", SDL_GetError());

    // Set up display mode (width and height)
    vid.width = screenWidth;
    vid.height = screenHeight;
    vid.maxwarpwidth = screenWidth;
    vid.maxwarpheight = screenHeight;

    if ( !COM_CheckParm ("-window") ) {
        window_flags |= SDL_WINDOW_FULLSCREEN_DESKTOP;
    }
    
    SDL_DisplayMode DispMode;
    SDL_GetCurrentDisplayMode(0, &DispMode);
    
#ifdef _WIN32
	screenWidth = DispMode.h * 3.0 / 4.0;
	screenHeight = DispMode.w * 3.0 / 4.0;
#else
	screenWidth = DispMode.w * 3.0 / 4.0;
	screenHeight = DispMode.h * 3.0 / 4.0;
#endif

    window = SDL_CreateWindow("NakedWinQuake", // Title will be set later
                              SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
                              screenWidth, screenHeight, window_flags);
    if (!window) {
        Sys_Error("VID: Couldn't create window: %s\n", SDL_GetError());
    }

    renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_SOFTWARE);
    if (!renderer) {
        SDL_DestroyWindow(window);
        Sys_Error("VID: Couldn't create renderer: %s\n", SDL_GetError());
    }

    texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888,
                                SDL_TEXTUREACCESS_STREAMING,
                                vid.width, vid.height);
    if (!texture) {
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        Sys_Error("VID: Couldn't create texture: %s\n", SDL_GetError());
    }

    // Allocate the 8-bit frame buffer
    frame_buffer = (byte *)malloc(vid.width * vid.height);
    if (!frame_buffer) {
        SDL_DestroyTexture(texture);
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        Sys_Error("VID: Couldn't allocate frame_buffer\n");
    }
        
    VID_SetPalette(palette); // This will now populate SdlPalette
    
    sprintf(caption, "NakedWinQuake - Version %4.2f", VERSION);
    SDL_SetWindowTitle(window, caption);
    
    // Update vid structure members
    VGA_width = vid.conwidth = vid.width;
    VGA_height = vid.conheight = vid.height;
    vid.aspect = ((float)vid.height / (float)vid.width) * (320.0 / 200.0);
    vid.numpages = 1;
    vid.colormap = host_colormap;
    vid.fullbright = 256 - LittleLong (*((int *)vid.colormap + 2048));
    
    VGA_pagebase = vid.buffer = vid.conbuffer = vid.direct = frame_buffer;
    vid.rowbytes = vid.conrowbytes = vid.width; 
    
    // allocate z buffer and surface cache
    chunk = vid.width * vid.height * sizeof (*d_pzbuffer);
    cachesize = D_SurfaceCacheForRes (vid.width, vid.height);
    chunk += cachesize;
    d_pzbuffer = Hunk_HighAllocName(chunk, "video");
    if (d_pzbuffer == NULL)
        Sys_Error ("Not enough memory for video mode\n");

    // initialize the cache memory 
        cache = (byte *) d_pzbuffer
                + vid.width * vid.height * sizeof (*d_pzbuffer);
    D_InitCaches (cache, cachesize);

    // initialize the mouse
    SDL_ShowCursor(SDL_DISABLE);
    
    vid_initialized = true;
}

void    VID_Shutdown (void)
{
	if (vid_initialized)
	{
		if (texture) {
            SDL_DestroyTexture(texture);
            texture = NULL;
        }
        if (renderer) {
            SDL_DestroyRenderer(renderer);
            renderer = NULL;
        }
        if (window) {
            SDL_DestroyWindow(window);
            window = NULL;
        }
        if (frame_buffer) {
            free(frame_buffer);
            frame_buffer = NULL;
        }

		vid_initialized = 0; // Set to false after cleanup
		SDL_QuitSubSystem(SDL_INIT_VIDEO); // This should be called after destroying window/renderer/texture associated with it
	}
}

void    VID_Update (vrect_t *rects)
{
    (void)rects; // Mark as unused to prevent compiler warnings

    void *texture_pixels;
    int texture_pitch;
    byte *source_buffer; // Use d_viewbuffer to account for r_dowarp

    if (!vid_initialized || !renderer || !texture) return;

    // Determine the source buffer for pixel data
    if (r_dowarp && d_viewbuffer == r_warpbuffer) { // Check if warp is active AND d_viewbuffer is set to warp
        source_buffer = r_warpbuffer; // Use the warp buffer
        // Ensure r_warpbuffer is treated as 8-bit indexed
    } else {
        source_buffer = frame_buffer; // Default to the main 8-bit frame_buffer
    }
    
    // Ensure source_buffer is valid
    if (!source_buffer) {
        Sys_Error("VID_Update: source_buffer is NULL\n");
        return;
    }

    if (SDL_LockTexture(texture, NULL, &texture_pixels, &texture_pitch) == 0) {
        Uint8 *src_row = source_buffer;
        Uint32 *dst_row = (Uint32 *)texture_pixels;
        int current_width;

        if (r_dowarp && d_viewbuffer == r_warpbuffer) {
             current_width = WARP_WIDTH; // Use WARP_WIDTH for pitch if warp buffer is active source
        } else {
             current_width = vid.width; // Use vid.width for pitch if frame_buffer is active source
        }

        for (int y = 0; y < vid.height; ++y) {
            for (int x = 0; x < current_width; ++x) { // Use current_width for iteration up to screen width
                if (x < vid.width) { // Only write pixels within the actual texture width
                    dst_row[x] = SdlPalette[src_row[x]];
                }
            }
            src_row += current_width; // Pitch of the source 8-bit buffer (vid.width or WARP_WIDTH)
            dst_row = (Uint32*)((Uint8*)dst_row + texture_pitch); // Pitch of 32-bit texture
        }
        SDL_UnlockTexture(texture);
    } else {
        Sys_Printf("VID_Update: Couldn't lock texture: %s\n", SDL_GetError());
        return; // Don't proceed if texture can't be locked
    }

    SDL_RenderClear(renderer);
    SDL_RenderCopy(renderer, texture, NULL, NULL);
    SDL_RenderPresent(renderer);
}

/*
================
D_BeginDirectRect
================
*/
void D_BeginDirectRect (int x, int y, byte *pbitmap, int width, int height)
{
    Uint8 *offset;

    // if (!screen) return; // screen is no longer the direct drawing target
    if (!frame_buffer) return; // Check if frame_buffer is allocated

    // Assuming x coordinates can be negative for right-alignment, adjust for vid.width
    if ( x < 0 ) x = vid.width + x; // vid.width instead of screen->w

    // Ensure x and y are within bounds to prevent buffer overflow
    if (x < 0 || x >= vid.width || y < 0 || y >= vid.height) return;
    if (x + width > vid.width) width = vid.width - x; // Clamp width
    if (y + height > vid.height) height = vid.height - y; // Clamp height
    if (width <= 0 || height <= 0) return;

    offset = frame_buffer + y * vid.width + x; // vid.width is the pitch of frame_buffer
    
    while ( height-- )
    {
        memcpy(offset, pbitmap, width);
        offset += vid.width; // Use vid.width for pitch
        pbitmap += width;
    }
}


/*
================
D_EndDirectRect
================
*/
void D_EndDirectRect (int x, int y, int width, int height)
{
	(void)x; (void)y; (void)width; (void)height; // Mark as unused
}


/*
================
Sys_SendKeyEvents
================
*/

void Sys_SendKeyEvents(void)
{
    SDL_Event event;
    int sym, state;
     int modstate;

    while (SDL_PollEvent(&event))
    {
        switch (event.type) {

            case SDL_KEYDOWN:
            case SDL_KEYUP:
                sym = event.key.keysym.sym;
                state = event.key.state;
                modstate = SDL_GetModState();
                switch(sym)
                {
                   case SDLK_DELETE: sym = K_DEL; break;
                   case SDLK_BACKSPACE: sym = K_BACKSPACE; break;
                   case SDLK_F1: sym = K_F1; break;
                   case SDLK_F2: sym = K_F2; break;
                   case SDLK_F3: sym = K_F3; break;
                   case SDLK_F4: sym = K_F4; break;
                   case SDLK_F5: sym = K_F5; break;
                   case SDLK_F6: sym = K_F6; break;
                   case SDLK_F7: sym = K_F7; break;
                   case SDLK_F8: sym = K_F8; break;
                   case SDLK_F9: sym = K_F9; break;
                   case SDLK_F10: sym = K_F10; break;
                   case SDLK_F11: sym = K_F11; break;
                   case SDLK_F12: sym = K_F12; break;
                   case SDLK_PAUSE: sym = K_PAUSE; break;
                   case SDLK_UP: sym = K_UPARROW; break;
                   case SDLK_DOWN: sym = K_DOWNARROW; break;
                   case SDLK_RIGHT: sym = K_RIGHTARROW; break;
                   case SDLK_LEFT: sym = K_LEFTARROW; break;
                   case SDLK_INSERT: sym = K_INS; break;
                   case SDLK_HOME: sym = K_HOME; break;
                   case SDLK_END: sym = K_END; break;
                   case SDLK_PAGEUP: sym = K_PGUP; break;
                   case SDLK_PAGEDOWN: sym = K_PGDN; break;
                   case SDLK_RSHIFT:
                   case SDLK_LSHIFT: sym = K_SHIFT; break;
                   case SDLK_RCTRL:
                   case SDLK_LCTRL: sym = K_CTRL; break;
                   case SDLK_RALT:
                   case SDLK_LALT: sym = K_ALT; break;
                   case SDLK_KP_0: 
                       if(modstate & KMOD_NUM) sym = K_INS; 
                       else sym = SDLK_0;
                       break;
                   case SDLK_KP_1:
                       if(modstate & KMOD_NUM) sym = K_END;
                       else sym = SDLK_1;
                       break;
                   case SDLK_KP_2:
                       if(modstate & KMOD_NUM) sym = K_DOWNARROW;
                       else sym = SDLK_2;
                       break;
                   case SDLK_KP_3:
                       if(modstate & KMOD_NUM) sym = K_PGDN;
                       else sym = SDLK_3;
                       break;
                   case SDLK_KP_4:
                       if(modstate & KMOD_NUM) sym = K_LEFTARROW;
                       else sym = SDLK_4;
                       break;
                   case SDLK_KP_5: sym = SDLK_5; break;
                   case SDLK_KP_6:
                       if(modstate & KMOD_NUM) sym = K_RIGHTARROW;
                       else sym = SDLK_6;
                       break;
                   case SDLK_KP_7:
                       if(modstate & KMOD_NUM) sym = K_HOME;
                       else sym = SDLK_7;
                       break;
                   case SDLK_KP_8:
                       if(modstate & KMOD_NUM) sym = K_UPARROW;
                       else sym = SDLK_8;
                       break;
                   case SDLK_KP_9:
                       if(modstate & KMOD_NUM) sym = K_PGUP;
                       else sym = SDLK_9;
                       break;
                   case SDLK_KP_PERIOD:
                       if(modstate & KMOD_NUM) sym = K_DEL;
                       else sym = SDLK_PERIOD;
                       break;
                   case SDLK_KP_DIVIDE: sym = SDLK_SLASH; break;
                   case SDLK_KP_MULTIPLY: sym = SDLK_ASTERISK; break;
                   case SDLK_KP_MINUS: sym = SDLK_MINUS; break;
                   case SDLK_KP_PLUS: sym = SDLK_PLUS; break;
                   case SDLK_KP_ENTER: sym = SDLK_RETURN; break;
                   case SDLK_KP_EQUALS: sym = SDLK_EQUALS; break;
                }
                // If we're not directly handled and still above 255
                // just force it to 0
                if(sym > 255) sym = 0;
                Key_Event(sym, state);
                break;

            case SDL_MOUSEMOTION:
                if ( (event.motion.x != (vid.width/2)) ||
                     (event.motion.y != (vid.height/2)) ) {
                    mouse_x = event.motion.xrel*10;
                    mouse_y = event.motion.yrel*10;
                    if ( (event.motion.x < ((vid.width/2)-(vid.width/4))) ||
                         (event.motion.x > ((vid.width/2)+(vid.width/4))) ||
                         (event.motion.y < ((vid.height/2)-(vid.height/4))) ||
                         (event.motion.y > ((vid.height/2)+(vid.height/4))) ) {
                        if (window) { // Ensure window is valid before warping
                            SDL_WarpMouseInWindow(window, vid.width/2, vid.height/2);
                        }
                    }
                }
                break;

            case SDL_WINDOWEVENT: // Handle window events, especially close
                if (event.window.event == SDL_WINDOWEVENT_CLOSE) {
                    CL_Disconnect ();
                    Host_ShutdownServer(false);        
                    Sys_Quit ();
                }
                break;

            case SDL_QUIT: // This can still be triggered by other means (e.g. Alt+F4 on some systems)
                CL_Disconnect ();
                Host_ShutdownServer(false);        
                Sys_Quit ();
                break;
            default:
                break;
        }
    }
}

void IN_Init (void)
{
    if ( COM_CheckParm ("-nomouse") )
        return;
    mouse_x = mouse_y = 0.0;
    mouse_avail = 1;
}

void IN_Shutdown (void)
{
    mouse_avail = 0;
}

void IN_Commands (void)
{
    int i;
    int mouse_buttonstate;
   
    if (!mouse_avail) return;
   
    i = SDL_GetMouseState(NULL, NULL);
    /* Quake swaps the second and third buttons */
    mouse_buttonstate = (i & ~0x06) | ((i & 0x02)<<1) | ((i & 0x04)>>1);
    for (i=0 ; i<3 ; i++) {
        if ( (mouse_buttonstate & (1<<i)) && !(mouse_oldbuttonstate & (1<<i)) )
            Key_Event (K_MOUSE1 + i, true);

        if ( !(mouse_buttonstate & (1<<i)) && (mouse_oldbuttonstate & (1<<i)) )
            Key_Event (K_MOUSE1 + i, false);
    }
    mouse_oldbuttonstate = mouse_buttonstate;
}

void IN_Move (usercmd_t *cmd)
{
    if (!mouse_avail)
        return;
   
    mouse_x *= sensitivity.value;
    mouse_y *= sensitivity.value;
   
    if ( (in_strafe.state & 1) || (lookstrafe.value && (in_mlook.state & 1) ))
        cmd->sidemove += m_side.value * mouse_x;
    else
        cl.viewangles[YAW] -= m_yaw.value * mouse_x;
    if (in_mlook.state & 1)
        V_StopPitchDrift ();
   
    if ( (in_mlook.state & 1) && !(in_strafe.state & 1)) {
        cl.viewangles[PITCH] += m_pitch.value * mouse_y;
        if (cl.viewangles[PITCH] > 80)
            cl.viewangles[PITCH] = 80;
        if (cl.viewangles[PITCH] < -70)
            cl.viewangles[PITCH] = -70;
    } else {
        if ((in_strafe.state & 1) && noclip_anglehack)
            cmd->upmove -= m_forward.value * mouse_y;
        else
            cmd->forwardmove -= m_forward.value * mouse_y;
    }
    mouse_x = mouse_y = 0.0;
}

/*
================
Sys_ConsoleInput
================
*/
char *Sys_ConsoleInput (void)
{
    return 0;
}
