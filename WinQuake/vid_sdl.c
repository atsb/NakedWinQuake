// vid_sdl.c -- SDL3 video driver with GPU-accelerated rendering

#include <SDL3/SDL.h>
#include "quakedef.h"
#include "d_local.h"

extern viddef_t vid;
unsigned short d_8to16table[256];

#define BASEWIDTH           320
#define BASEHEIGHT          200

unsigned screenWidth = 320;
unsigned screenHeight = 200;

int VGA_width, VGA_height, VGA_rowbytes, VGA_bufferrowbytes = 0;
byte* VGA_pagebase;

static int lockcount;
static qboolean vid_initialized = false;
static SDL_Window* window = NULL;
SDL_Renderer* renderer = NULL;
static SDL_Texture* texture = NULL;
static Uint32 SdlPalette[256];
static byte* frame_buffer;
static qboolean palette_changed;
static unsigned char vid_curpal[256 * 3];
static qboolean mouse_avail;
static float mouse_x, mouse_y;
static int mouse_oldbuttonstate = 0;

static qboolean relative_mode_available = false;
float accumulated_mouse_dx = 0.0f;
float accumulated_mouse_dy = 0.0f;

void (*vid_menudrawfn)(void) = NULL;
void (*vid_menukeyfn)(int key) = NULL;

void VID_SetPalette(unsigned char* palette)
{
    int i;
    palette_changed = true;

    if (palette != vid_curpal)
        memcpy(vid_curpal, palette, sizeof(vid_curpal));

    // Build 32-bit ARGB palette for GPU
    for (i = 0; i < 256; ++i) {
        SdlPalette[i] = ((Uint32)0xFF << 24) |
                        ((Uint32)palette[i*3] << 16) |
                        ((Uint32)palette[i*3+1] << 8) |
                        ((Uint32)palette[i*3+2]);
    }
}

void VID_ShiftPalette(unsigned char* palette)
{
    VID_SetPalette(palette);
}

void VID_Init(unsigned char* palette)
{
    int chunk;
    byte* cache;
    int cachesize;
    char caption[50];
    SDL_WindowFlags window_flags = 0;

    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO))
        Sys_Error("VID: Couldn't load SDL: %s", SDL_GetError());

    vid.width = screenWidth;
    vid.height = screenHeight;
    vid.maxwarpwidth = WARP_WIDTH;
    vid.maxwarpheight = WARP_HEIGHT;

    if (!COM_CheckParm("-window")) {
        window_flags |= SDL_WINDOW_FULLSCREEN;
    }

    const SDL_DisplayMode* mode = SDL_GetCurrentDisplayMode(SDL_GetPrimaryDisplay());
    if (mode) {
#ifdef _WIN32
        if (COM_CheckParm("-window")) {
            screenWidth = mode->w * 3.0 / 4.0;
            screenHeight = mode->h * 3.0 / 4.0;
        }
        else {
            screenWidth = mode->h * 3.0 / 4.0;
            screenHeight = mode->w * 3.0 / 4.0;
        }
#else
        screenWidth = mode->w * 3.0 / 4.0;
        screenHeight = mode->h * 3.0 / 4.0;
#endif
    }

    window = SDL_CreateWindow("NakedWinQuake",
                               screenWidth, screenHeight,
                               window_flags);
    if (!window) {
        Sys_Error("VID: Couldn't create window: %s\n", SDL_GetError());
    }

    // Create GPU-accelerated renderer
    renderer = SDL_CreateRenderer(window, NULL);
    if (!renderer) {
        SDL_DestroyWindow(window);
        Sys_Error("VID: Couldn't create renderer: %s\n", SDL_GetError());
    }

    // Use VSync for smooth presentation on ARM
    SDL_SetRenderVSync(renderer, 1);
    
    // Use GPU scaling with nearest neighbor
    SDL_SetRenderLogicalPresentation(renderer, vid.width, vid.height,
                                      SDL_LOGICAL_PRESENTATION_LETTERBOX);

    // Create streaming texture - GPU will handle the blitting
    texture = SDL_CreateTexture(renderer,
                                 SDL_PIXELFORMAT_ARGB8888,
                                 SDL_TEXTUREACCESS_STREAMING,
                                 vid.width, vid.height);
    if (!texture) {
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        Sys_Error("VID: Couldn't create texture: %s\n", SDL_GetError());
    }

    // Nearest neighbor for pixel-perfect scaling
    SDL_SetTextureScaleMode(texture, SDL_SCALEMODE_NEAREST);

    frame_buffer = (byte*)malloc(vid.width * vid.height);
    if (!frame_buffer) {
        SDL_DestroyTexture(texture);
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        Sys_Error("VID: Couldn't allocate frame_buffer\n");
    }

    VID_SetPalette(palette);

    sprintf(caption, "NakedWinQuake - Version %4.2f", (float)VERSION);
    SDL_SetWindowTitle(window, caption);

    VGA_width = vid.conwidth = vid.width;
    VGA_height = vid.conheight = vid.height;
    vid.aspect = ((float)vid.height / (float)vid.width) * (320.0 / 200.0);
    vid.numpages = 1;
    vid.colormap = host_colormap;
    vid.fullbright = 256 - LittleLong(*((int*)vid.colormap + 2048));

    VGA_pagebase = vid.buffer = vid.conbuffer = vid.direct = frame_buffer;
    vid.rowbytes = vid.conrowbytes = vid.width;

    chunk = vid.width * vid.height * sizeof(*d_pzbuffer);
    cachesize = D_SurfaceCacheForRes(vid.width, vid.height);
    chunk += cachesize;
    d_pzbuffer = Hunk_HighAllocName(chunk, "video");
    if (d_pzbuffer == NULL)
        Sys_Error("Not enough memory for video mode\n");

    cache = (byte*)d_pzbuffer + vid.width * vid.height * sizeof(*d_pzbuffer);
    D_InitCaches(cache, cachesize);

    SDL_HideCursor();

    vid_initialized = true;
    
    Con_Printf("Video: GPU-accelerated renderer initialized\n");
}

void VID_Shutdown(void)
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

        vid_initialized = false;
        SDL_Quit();
    }
}

void VID_Update(vrect_t* rects)
{
    void* pixels;
    int pitch;
    static Uint64 last_frame_time = 0;
    Uint64 current_time;
    Uint64 frame_time;
    const Uint64 target_frame_time = 16;

    if (!vid_initialized || !renderer || !texture || !frame_buffer)
        return;

    current_time = SDL_GetTicks();
    frame_time = current_time - last_frame_time;
    
    if (frame_time < target_frame_time) {
        SDL_Delay(target_frame_time - frame_time);
        current_time = SDL_GetTicks();
    }
    last_frame_time = current_time;

    if (!SDL_LockTexture(texture, NULL, &pixels, &pitch))
        return;

    byte* src = frame_buffer;
    Uint32* dst = (Uint32*)pixels;
    int total = vid.width * vid.height;

#if defined(__aarch64__) || defined(__ARM_ARCH_8__) || defined(__arm64__)
    // ARM64 optimized version with prefetching
    __builtin_prefetch(SdlPalette, 0, 3);
    
    int i = 0;
    
    // Process 16 pixels at a time
    for (; i <= total - 16; i += 16) {
        __builtin_prefetch(src + i + 64, 0, 0);
        
        dst[i+0]  = SdlPalette[src[i+0]];
        dst[i+1]  = SdlPalette[src[i+1]];
        dst[i+2]  = SdlPalette[src[i+2]];
        dst[i+3]  = SdlPalette[src[i+3]];
        dst[i+4]  = SdlPalette[src[i+4]];
        dst[i+5]  = SdlPalette[src[i+5]];
        dst[i+6]  = SdlPalette[src[i+6]];
        dst[i+7]  = SdlPalette[src[i+7]];
        dst[i+8]  = SdlPalette[src[i+8]];
        dst[i+9]  = SdlPalette[src[i+9]];
        dst[i+10] = SdlPalette[src[i+10]];
        dst[i+11] = SdlPalette[src[i+11]];
        dst[i+12] = SdlPalette[src[i+12]];
        dst[i+13] = SdlPalette[src[i+13]];
        dst[i+14] = SdlPalette[src[i+14]];
        dst[i+15] = SdlPalette[src[i+15]];
    }
    
    for (; i < total; i++) {
        dst[i] = SdlPalette[src[i]];
    }
    
#else
    int i = 0;
    
    for (; i <= total - 8; i += 8) {
        dst[i+0] = SdlPalette[src[i+0]];
        dst[i+1] = SdlPalette[src[i+1]];
        dst[i+2] = SdlPalette[src[i+2]];
        dst[i+3] = SdlPalette[src[i+3]];
        dst[i+4] = SdlPalette[src[i+4]];
        dst[i+5] = SdlPalette[src[i+5]];
        dst[i+6] = SdlPalette[src[i+6]];
        dst[i+7] = SdlPalette[src[i+7]];
    }
    
    for (; i < total; i++) {
        dst[i] = SdlPalette[src[i]];
    }
    
#endif

    SDL_UnlockTexture(texture);
    SDL_RenderTexture(renderer, texture, NULL, NULL);
    SDL_RenderPresent(renderer);
}

void D_BeginDirectRect(int x, int y, byte* pbitmap, int width, int height)
{
    Uint8* offset;

    if (!frame_buffer) return;

    if (x < 0) x = vid.width + x;

    if (x < 0 || x >= vid.width || y < 0 || y >= vid.height) return;
    if (x + width > vid.width) width = vid.width - x;
    if (y + height > vid.height) height = vid.height - y;
    if (width <= 0 || height <= 0) return;

    offset = frame_buffer + y * vid.width + x;

    while (height--)
    {
        memcpy(offset, pbitmap, width);
        offset += vid.width;
        pbitmap += width;
    }
}

void D_EndDirectRect(int x, int y, int width, int height)
{
    (void)x; (void)y; (void)width; (void)height;
}

void Sys_SendKeyEvents(void)
{
    SDL_Event event;
    int sym, state;
    SDL_Keymod modstate;
    static int center_x = 0, center_y = 0;
    int current_x, current_y;

    if (center_x == 0 || center_y == 0) {
        center_x = screenWidth / 2;
        center_y = screenHeight / 2;
    }

    while (SDL_PollEvent(&event))
    {
        switch (event.type) {

        case SDL_EVENT_KEY_DOWN:
        case SDL_EVENT_KEY_UP:
            sym = event.key.key;
            state = (event.type == SDL_EVENT_KEY_DOWN);
            modstate = SDL_GetModState();
            
            switch (sym)
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
                if (modstate & SDL_KMOD_NUM) sym = K_INS;
                else sym = SDLK_0;
                break;
            case SDLK_KP_1:
                if (modstate & SDL_KMOD_NUM) sym = K_END;
                else sym = SDLK_1;
                break;
            case SDLK_KP_2:
                if (modstate & SDL_KMOD_NUM) sym = K_DOWNARROW;
                else sym = SDLK_2;
                break;
            case SDLK_KP_3:
                if (modstate & SDL_KMOD_NUM) sym = K_PGDN;
                else sym = SDLK_3;
                break;
            case SDLK_KP_4:
                if (modstate & SDL_KMOD_NUM) sym = K_LEFTARROW;
                else sym = SDLK_4;
                break;
            case SDLK_KP_5: sym = SDLK_5; break;
            case SDLK_KP_6:
                if (modstate & SDL_KMOD_NUM) sym = K_RIGHTARROW;
                else sym = SDLK_6;
                break;
            case SDLK_KP_7:
                if (modstate & SDL_KMOD_NUM) sym = K_HOME;
                else sym = SDLK_7;
                break;
            case SDLK_KP_8:
                if (modstate & SDL_KMOD_NUM) sym = K_UPARROW;
                else sym = SDLK_8;
                break;
            case SDLK_KP_9:
                if (modstate & SDL_KMOD_NUM) sym = K_PGUP;
                else sym = SDLK_9;
                break;
            case SDLK_KP_PERIOD:
                if (modstate & SDL_KMOD_NUM) sym = K_DEL;
                else sym = SDLK_PERIOD;
                break;
            case SDLK_KP_DIVIDE: sym = SDLK_SLASH; break;
            case SDLK_KP_MULTIPLY: sym = SDLK_ASTERISK; break;
            case SDLK_KP_MINUS: sym = SDLK_MINUS; break;
            case SDLK_KP_PLUS: sym = SDLK_PLUS; break;
            case SDLK_KP_ENTER: sym = SDLK_RETURN; break;
            case SDLK_KP_EQUALS: sym = SDLK_EQUALS; break;
            }
            if (sym > 255) sym = 0;
            Key_Event(sym, state);
            break;

        case SDL_EVENT_MOUSE_MOTION:
            if (relative_mode_available) {
                accumulated_mouse_dx += event.motion.xrel;
                accumulated_mouse_dy += event.motion.yrel;
            } else {
                current_x = (int)event.motion.x;
                current_y = (int)event.motion.y;
                
                accumulated_mouse_dx += (current_x - center_x);
                accumulated_mouse_dy += (current_y - center_y);
            }
            break;

        case SDL_EVENT_WINDOW_CLOSE_REQUESTED:
            CL_Disconnect();
            Host_ShutdownServer(false);
            Sys_Quit();
            break;

        case SDL_EVENT_QUIT:
            CL_Disconnect();
            Host_ShutdownServer(false);
            Sys_Quit();
            break;
            
        default:
            break;
        }
    }
}

void IN_Init(void)
{
    if (COM_CheckParm("-nomouse")) {
        mouse_avail = 0;
        return;
    }

    if (SDL_SetWindowRelativeMouseMode(window, true)) {
        relative_mode_available = true;
        Sys_Printf("Relative mouse mode enabled\n");
    } else {
        relative_mode_available = false;
        Sys_Printf("Warning: Could not enable relative mouse mode: %s\n", SDL_GetError());
        Sys_Printf("Falling back to mouse warp mode\n");
    }

    mouse_x = 0.0;
    mouse_y = 0.0;
    accumulated_mouse_dx = 0.0f;
    accumulated_mouse_dy = 0.0f;
    mouse_oldbuttonstate = 0;
    mouse_avail = 1;
}

void IN_Shutdown(void)
{
    if (mouse_avail)
    {
        SDL_SetWindowRelativeMouseMode(window, false);
    }
    mouse_avail = 0;
}

void IN_Commands(void)
{
    int i;
    Uint32 current_mouse_buttonstate_sdl;
    int ingame_mouse_buttonstate;

    if (!mouse_avail) return;

    current_mouse_buttonstate_sdl = SDL_GetMouseState(NULL, NULL);

    ingame_mouse_buttonstate = 0;
    if (current_mouse_buttonstate_sdl & SDL_BUTTON_LMASK)
        ingame_mouse_buttonstate |= (1 << 0);
    if (current_mouse_buttonstate_sdl & SDL_BUTTON_RMASK)
        ingame_mouse_buttonstate |= (1 << 1);
    if (current_mouse_buttonstate_sdl & SDL_BUTTON_MMASK)
        ingame_mouse_buttonstate |= (1 << 2);

    for (i = 0; i < 3; i++) {
        if ((ingame_mouse_buttonstate & (1 << i)) && !(mouse_oldbuttonstate & (1 << i)))
            Key_Event(K_MOUSE1 + i, true);

        if (!(ingame_mouse_buttonstate & (1 << i)) && (mouse_oldbuttonstate & (1 << i)))
            Key_Event(K_MOUSE1 + i, false);
    }
    mouse_oldbuttonstate = ingame_mouse_buttonstate;
}

void IN_Move(usercmd_t* cmd)
{
    static Uint64 last_warp_time = 0;
    
    if (!mouse_avail)
        return;

    mouse_x = accumulated_mouse_dx;
    mouse_y = accumulated_mouse_dy;

    accumulated_mouse_dx = 0.0f;
    accumulated_mouse_dy = 0.0f;

    // If not in relative mode, warp mouse periodically (not every frame!)
    if (!relative_mode_available) {
        Uint64 now = SDL_GetTicks();
        if (now - last_warp_time > 100) { // Only warp every 100ms
            int center_x = screenWidth / 2;
            int center_y = screenHeight / 2;
            SDL_WarpMouseInWindow(window, center_x, center_y);
            last_warp_time = now;
        }
    }

    mouse_x *= sensitivity.value;
    mouse_y *= sensitivity.value;

    if ((in_strafe.state & 1) || (lookstrafe.value && (in_mlook.state & 1))) {
        cmd->sidemove += m_side.value * mouse_x;
    }
    else {
        cl.viewangles[YAW] -= m_yaw.value * mouse_x;
        while (cl.viewangles[YAW] < 0)
            cl.viewangles[YAW] += 360;
        while (cl.viewangles[YAW] >= 360)
            cl.viewangles[YAW] -= 360;
    }

    if (in_mlook.state & 1) {
        V_StopPitchDrift();
    }

    if ((in_mlook.state & 1) && !(in_strafe.state & 1)) {
        cl.viewangles[PITCH] += m_pitch.value * mouse_y;
        if (cl.viewangles[PITCH] > 80)
            cl.viewangles[PITCH] = 80;
        if (cl.viewangles[PITCH] < -70)
            cl.viewangles[PITCH] = -70;
    }
    else {
        if ((in_strafe.state & 1) && noclip_anglehack) {
            cmd->upmove -= m_forward.value * mouse_y;
        }
        else {
            cmd->forwardmove -= m_forward.value * mouse_y;
        }
    }
}

char* Sys_ConsoleInput(void)
{
    return 0;
}

