cmake_minimum_required(VERSION 3.16)
project(digilogic LANGUAGES C)

set(DIGILOGIC_SRCS
    src/main.c
    src/core/circuit.c
    src/core/smap.c
    src/ux/ux.c
    src/ux/input.c
    src/ux/snap.c
    src/ux/undo.c
    src/ui/ui.c
    src/view/view.c
    src/import/digital.c
    src/autoroute/autoroute.c
    src/render/fons_sgp.c
    src/render/polyline.c
    src/render/draw.c
    src/main.c
    src/assets.c
)


#####################################
# MacOS, iOS
#####################################

if(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
    # file must have .m extension to be compiled as Objective-C
    # and this must be compiled as Objective-C because Apple
    configure_file(
        "src/nonapple.c"
        "${CMAKE_CURRENT_SOURCE_DIR}/src/apple.m"
        COPYONLY
    )

    add_executable(digilogic MACOSX_BUNDLE
        ${DIGILOGIC_SRCS}
        "res/logo.icns"
        "src/apple.m"
    )

    target_compile_definitions(digilogic PUBLIC "SOKOL_METAL")
    target_link_libraries(digilogic PUBLIC
        "-framework Metal"
        "-framework MetalKit"
        "-framework Quartz"
        "-framework Cocoa"
    )

    set_target_properties(digilogic PROPERTIES
        MACOSX_BUNDLE TRUE
        MACOSX_BUNDLE_GUI_IDENTIFIER ca.rj45.digilogic
        MACOSX_BUNDLE_BUNDLE_NAME digilogic
        MACOSX_BUNDLE_ICON_FILE "logo.icns"
        MACOSX_BUNDLE_COPYRIGHT "© 2024 Ryan \"rj45\" Sanche"
        MACOSX_BUNDLE_BUNDLE_VERSION "0.1"
        MACOSX_BUNDLE_SHORT_VERSION_STRING "0.1"
        RESOURCE "res/logo.icns"
    )
endif()

#####################################
# Windows
#####################################

if(CMAKE_SYSTEM_NAME STREQUAL "Windows")
    add_executable(digilogic ${DIGILOGIC_SRCS}
        "res/app.o"
        "src/nonapple.c"
    )

    if(USE_GL)
        target_compile_definitions(digilogic PUBLIC "SOKOL_GLCORE33")
        find_package(OpenGL REQUIRED)
        target_link_libraries(digilogic PUBLIC OpenGL::GL)
    else()
        target_compile_definitions(digilogic PUBLIC "SOKOL_D3D11")
        target_link_libraries(digilogic PUBLIC
            "d3d11"
            "dxgi"
        )
    endif()


    target_link_libraries(digilogic PUBLIC
        "kernel32"
        "user32"
        "gdi32"
        "ole32"
        "bcrypt" # required by rust
    )
endif()

#####################################
# Linux
#####################################

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    add_executable(digilogic
        ${DIGILOGIC_SRCS}
        "src/nonapple.c"
    )

    if (USE_GLES)
        target_compile_definitions(digilogic PUBLIC "SOKOL_GLES3")
    else()
        target_compile_definitions(digilogic PUBLIC "SOKOL_GLCORE33")
    endif()

    if (USE_EGL)
        target_compile_definitions(digilogic PUBLIC "SOKOL_FORCE_EGL")
    endif()

    if(USE_WAYLAND)
        if(NOT DEFINED WAYLAND_SCANNER_EXECUTABLE)
            find_program(WAYLAND_SCANNER_EXECUTABLE NAMES wayland-scanner)
        endif()

        # todo: make this less hard-coded
        add_custom_command(
            OUTPUT  xdg-shell-protocol.h
            COMMAND ${WAYLAND_SCANNER_EXECUTABLE} client-header
                    < /usr/share/wayland-protocols/stable/xdg-shell/xdg-shell.xml
                    > ${CMAKE_CURRENT_BINARY_DIR}/xdg-shell-protocol.h
        )
        add_custom_command(
            OUTPUT  xdg-shell-protocol.c
            COMMAND ${WAYLAND_SCANNER_EXECUTABLE} private-code
                    < /usr/share/wayland-protocols/stable/xdg-shell/xdg-shell.xml
                    > ${CMAKE_CURRENT_BINARY_DIR}/xdg-shell-protocol.c
        )

        add_custom_command(
            OUTPUT  pointer-constraints-unstable-v1-protocol.h
            COMMAND ${WAYLAND_SCANNER_EXECUTABLE} client-header
                    < /usr/share/wayland-protocols/unstable/pointer-constraints/pointer-constraints-unstable-v1.xml
                    > ${CMAKE_CURRENT_BINARY_DIR}/pointer-constraints-unstable-v1-protocol.h
        )
        add_custom_command(
            OUTPUT  pointer-constraints-unstable-v1-protocol.c
            COMMAND ${WAYLAND_SCANNER_EXECUTABLE} private-code
                    < /usr/share/wayland-protocols/unstable/pointer-constraints/pointer-constraints-unstable-v1.xml
                    > ${CMAKE_CURRENT_BINARY_DIR}/pointer-constraints-unstable-v1-protocol.c
        )

        add_custom_command(
            OUTPUT  relative-pointer-unstable-v1-protocol.h
            COMMAND ${WAYLAND_SCANNER_EXECUTABLE} client-header
                    < /usr/share/wayland-protocols/unstable/relative-pointer/relative-pointer-unstable-v1.xml
                    > ${CMAKE_CURRENT_BINARY_DIR}/relative-pointer-unstable-v1-protocol.h
        )
        add_custom_command(
            OUTPUT  relative-pointer-unstable-v1-protocol.c
            COMMAND ${WAYLAND_SCANNER_EXECUTABLE} private-code
                    < /usr/share/wayland-protocols/unstable/relative-pointer/relative-pointer-unstable-v1.xml
                    > ${CMAKE_CURRENT_BINARY_DIR}/relative-pointer-unstable-v1-protocol.c
        )

        include_directories(
            ${CMAKE_CURRENT_BINARY_DIR}
        )

        target_include_directories(digilogic PUBLIC ${CMAKE_CURRENT_BINARY_DIR})

        target_compile_definitions(digilogic PUBLIC "SOKOL_DISABLE_X11")
        target_compile_definitions(digilogic PUBLIC "SOKOL_LINUX_CUSTOM")
        target_link_libraries(digilogic PUBLIC
            "wayland-client"
            "wayland-cursor"
            "wayland-egl"
            "xkbcommon"
            "GL"
            "EGL"
        )
    else() # use X11 (default)
        target_compile_definitions(digilogic PUBLIC "SOKOL_DISABLE_WAYLAND")
        target_link_libraries(digilogic PUBLIC
            "X11"
            "Xi"
            "Xcursor"
            "GL"
        )
        if (USE_EGL)
            target_link_libraries(digilogic PUBLIC
                "EGL"
            )
        endif()
    endif()
endif()

#####################################
# Android
#####################################

if(CMAKE_SYSTEM_NAME STREQUAL "Android")
    add_executable(digilogic
        ${DIGILOGIC_SRCS}
        "src/nonapple.c"
    )

    target_compile_definitions(digilogic PUBLIC "SOKOL_GLES3")
    target_link_libraries(digilogic PUBLIC
        "log"
        "android"
        "GLESv3"
        "EGL"
    )
endif()

#####################################
# Rust Cargo
#####################################

include(FetchContent)

FetchContent_Declare(
    Corrosion
    GIT_REPOSITORY https://github.com/corrosion-rs/corrosion.git
    # GIT_TAG v0.4.8
    GIT_TAG master
)
FetchContent_MakeAvailable(Corrosion)

if(CMAKE_BUILD_TYPE STREQUAL "Release")
    set(RUST_PROFILE "release")
else()
    set(RUST_PROFILE "dev")
endif()

# will make targets for each crate in the Cargo.toml
corrosion_import_crate(
    MANIFEST_PATH thirdparty/routing/Cargo.toml
    PROFILE ${RUST_PROFILE}
)

#####################################
# C
#####################################

set_property(TARGET digilogic PROPERTY C_STANDARD 11)

find_package(Freetype REQUIRED)
target_link_libraries(digilogic PUBLIC Freetype::Freetype)

target_include_directories(digilogic PUBLIC "thirdparty")
target_include_directories(digilogic PUBLIC "src")

# link rust crates into digilogic
target_link_libraries(digilogic PUBLIC digilogic_routing)
