cmake_minimum_required(VERSION 3.10)
project(NakedWinQuake VERSION 1.0.9 LANGUAGES C)

# Options
option(USE_SDL3 "Use SDL3" ON)
option(BUILD_NULL "Build NULL version (no video/sound)" OFF)

# Check for required headers
include(CheckIncludeFile)
check_include_file("arpa/inet.h" HAVE_ARPA_INET_H)
check_include_file("fcntl.h" HAVE_FCNTL_H)
check_include_file("limits.h" HAVE_LIMITS_H)
check_include_file("netdb.h" HAVE_NETDB_H)
check_include_file("netinet/in.h" HAVE_NETINET_IN_H)
check_include_file("stddef.h" HAVE_STDDEF_H)
check_include_file("stdlib.h" HAVE_STDLIB_H)
check_include_file("string.h" HAVE_STRING_H)
check_include_file("sys/file.h" HAVE_SYS_FILE_H)
check_include_file("sys/ioctl.h" HAVE_SYS_IOCTL_H)
check_include_file("sys/param.h" HAVE_SYS_PARAM_H)
check_include_file("sys/socket.h" HAVE_SYS_SOCKET_H)
check_include_file("sys/time.h" HAVE_SYS_TIME_H)
check_include_file("termios.h" HAVE_TERMIOS_H)
check_include_file("unistd.h" HAVE_UNISTD_H)
check_include_file("stdbool.h" HAVE_STDBOOL_H)

# Check for required functions
include(CheckFunctionExists)
check_function_exists(atexit HAVE_ATEXIT)
check_function_exists(floor HAVE_FLOOR)
check_function_exists(gethostbyaddr HAVE_GETHOSTBYADDR)
check_function_exists(gethostbyname HAVE_GETHOSTBYNAME)
check_function_exists(gethostname HAVE_GETHOSTNAME)
check_function_exists(getpagesize HAVE_GETPAGESIZE)
check_function_exists(gettimeofday HAVE_GETTIMEOFDAY)
check_function_exists(inet_ntoa HAVE_INET_NTOA)
check_function_exists(memset HAVE_MEMSET)
check_function_exists(mkdir HAVE_MKDIR)
check_function_exists(munmap HAVE_MUNMAP)
check_function_exists(pow HAVE_POW)
check_function_exists(select HAVE_SELECT)
check_function_exists(socket HAVE_SOCKET)
check_function_exists(sqrt HAVE_SQRT)
check_function_exists(strcasecmp HAVE_STRCASECMP)
check_function_exists(strchr HAVE_STRCHR)
check_function_exists(strerror HAVE_STRERROR)
check_function_exists(strstr HAVE_STRSTR)

# Generate config.h
configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/config.h.in
    ${CMAKE_CURRENT_BINARY_DIR}/config.h
)

# Platform-specific compiler flags
if(CMAKE_SYSTEM_PROCESSOR MATCHES "alpha")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -mcpu=ev4 -Wa,-mall")
elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64" OR CMAKE_SYSTEM_PROCESSOR MATCHES "AMD64")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -ffast-math -funroll-loops -fomit-frame-pointer -fexpensive-optimizations")
elseif(APPLE)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -ffast-math -funroll-loops -fomit-frame-pointer")
elseif(UNIX AND NOT APPLE)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -ffast-math -funroll-loops -fomit-frame-pointer -fexpensive-optimizations")
endif()

# Platform-specific libraries
if(WIN32)
    set(MATHLIB "")
    set(INETLIB wsock32)
elseif(BEOS)
    set(MATHLIB "")
    set(INETLIB "")
else()
    set(MATHLIB m)
    set(INETLIB "")
endif()

# Find SDL (SDL3) if not building NULL version
if(NOT BUILD_NULL)
    add_definitions(-DSDL)
    if(USE_SDL3)
        find_package(SDL3 REQUIRED CONFIG)
        set(SDL_LIBRARY SDL3::SDL3)
        message(STATUS "Using SDL3")
    endif()
endif()

# Common source files (always included)
set(COMMON_SOURCES
    adivtab.h
    anorm_dots.h
    anorms.h
    block16.h
    block8.h
    bspfile.h
    cdaudio.h
    chase.c
    cl_demo.c
    cl_input.c
    cl_main.c
    cl_parse.c
    cl_tent.c
    client.h
    cmd.c
    cmd.h
    common.c
    common.h
    conproc.h
    console.c
    console.h
    crc.c
    crc.h
    cvar.c
    cvar.h
    d_edge.c
    d_fill.c
    d_iface.h
    d_ifacea.h
    d_init.c
    d_local.h
    d_modech.c
    d_part.c
    d_polyse.c
    d_scan.c
    d_sky.c
    d_sprite.c
    d_surf.c
    d_zpoint.c
    draw.c
    draw.h
    host.c
    host_cmd.c
    input.h
    keys.c
    keys.h
    mathlib.c
    mathlib.h
    menu.c
    menu.h
    model.c
    model.h
    modelgen.h
    mpdosock.h
    net.h
    net_bsd.c
    net_bw.h
    net_dgrm.c
    net_dgrm.h
    net_loop.c
    net_loop.h
    net_main.c
    net_udp.c
    net_udp.h
    net_vcr.c
    net_vcr.h
    net_wso.c
    pr_cmds.c
    pr_comp.h
    pr_edict.c
    pr_exec.c
    progdefs.h
    progs.h
    protocol.h
    quakeasm.h
    quakedef.h
    r_aclip.c
    r_alias.c
    r_bsp.c
    r_draw.c
    r_edge.c
    r_efrag.c
    r_light.c
    r_local.h
    r_main.c
    r_misc.c
    r_part.c
    r_shared.h
    r_sky.c
    r_sprite.c
    r_surf.c
    r_vars.c
    render.h
    resource.h
    sbar.c
    sbar.h
    screen.c
    screen.h
    server.h
    snd_dma.c
    snd_mem.c
    snd_mix.c
    sound.h
    spritegn.h
    sv_main.c
    sv_move.c
    sv_phys.c
    sv_user.c
    sys.h
    vgamodes.h
    vid.h
    view.c
    view.h
    wad.c
    wad.h
    winquake.h
    world.c
    world.h
    zone.c
    zone.h
)

# Non-x86 sources (always included for portability)
set(NONX86_SOURCES
    d_vars.c
    nonintel.c
)

# NULL platform sources (no video/sound)
set(NULL_SOURCES
    cd_null.c
    in_null.c
    net_none.c
    snd_null.c
    sys_null.c
    vid_null.c
)

# DOS platform sources
set(DOS_SOURCES
    cd_audio.c
    dosasm.S
    dosisms.h
    dos_v2.c
    in_dos.c
    mplib.c
    mplpc.c
    net_bw.c
    net_comx.c
    net_dos.c
    net_ipx.c
    net_ipx.h
    net_ser.c
    net_ser.h
    snd_dos.c
    snd_gus.c
    sys_dos.c
    vid_dos.c
    vid_dos.h
    vid_ext.c
    vid_vga.c
    vregset.c
    vregset.h
)

# Windows platform sources
set(WIN_SOURCES
    cd_win.c
    conproc.c
    in_win.c
    net_mp.c
    net_mp.h
    net_win.c
    net_wins.c
    net_wins.h
    net_wipx.c
    net_wipx.h
    snd_win.c
    sys_win.c
    sys_wina.S
    sys_wind.c
    vid_win.c
)

# Linux platform sources
set(LNX_SOURCES
    cd_linux.c
    snd_linux.c
    sys_linux.c
    vid_svgalib.c
    vid_x.c
)

# SDL platform sources (cross-platform)
set(SDL_SOURCES
    cd_sdl.c
    snd_sdl.c
    sys_sdl.c
    vid_sdl.c
)

# Select platform-specific sources
set(PLATFORM_SOURCES "")

if(BUILD_NULL)
    # NULL build - minimal implementation
    set(PLATFORM_SOURCES ${NULL_SOURCES})
    message(STATUS "Building NULL version (no video/sound)")
elseif(WIN32)
    if(USE_SDL3)
        set(PLATFORM_SOURCES ${SDL_SOURCES})
        message(STATUS "Building Windows version with SDL")
    else()
        set(PLATFORM_SOURCES ${WIN_SOURCES})
        message(STATUS "Building native Windows version")
    endif()
elseif(UNIX AND NOT APPLE)
    # Linux/Unix build - use SDL
    if(USE_SDL3 OR NOT BUILD_NULL)
        set(PLATFORM_SOURCES ${SDL_SOURCES})
        message(STATUS "Building Linux version with SDL")
    endif()
elseif(APPLE)
    # macOS - SDL only
    set(PLATFORM_SOURCES ${SDL_SOURCES})
    message(STATUS "Building macOS version with SDL")
else()
    # Fallback to SDL for unknown platforms
    set(PLATFORM_SOURCES ${SDL_SOURCES})
    message(STATUS "Building with SDL for unknown platform")
endif()

# Create the executable
add_executable(winquake
    ${COMMON_SOURCES}
    ${NONX86_SOURCES}
    ${PLATFORM_SOURCES}
)

# Include directories
target_include_directories(winquake
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_BINARY_DIR}
)

# Link libraries
target_link_libraries(winquake
    PRIVATE
        ${MATHLIB}
        ${INETLIB}
)

# Add SDL library if using SDL
if(NOT BUILD_NULL AND (USE_SDL3 OR (UNIX AND NOT APPLE) OR APPLE))
    target_link_libraries(winquake PRIVATE ${SDL_LIBRARY})
endif()

# Platform-specific: X11 libraries for native Linux X11 build
if(UNIX AND NOT APPLE AND NOT USE_SDL3 AND NOT BUILD_NULL)
    find_package(X11)
    if(X11_FOUND)
        target_link_libraries(winquake PRIVATE ${X11_LIBRARIES})
        target_include_directories(winquake PRIVATE ${X11_INCLUDE_DIR})
    endif()
endif()

# Windows-specific: Add resources and GUI subsystem
if(WIN32)
    # Add icon resource if available
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/winquake.rc")
        target_sources(winquake PRIVATE winquake.rc)
    endif()
    
    # Set Windows subsystem
    if(MSVC)
        set_target_properties(winquake PROPERTIES
            WIN32_EXECUTABLE TRUE
        )
    endif()
endif()

# Installation
install(TARGETS winquake
    RUNTIME DESTINATION bin
)

# Install documentation
install(FILES
    README.md
    README.SDL
    DESTINATION share/doc/winquake
    OPTIONAL
)

# Install data files if they exist
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/data")
    install(DIRECTORY data/
        DESTINATION share/winquake/data
        OPTIONAL
    )
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/docs")
    install(DIRECTORY docs/
        DESTINATION share/doc/winquake
        OPTIONAL
    )
endif()

