cmake_minimum_required(VERSION 3.16)
project(digilogic LANGUAGES C CXX)

set(DIGILOGIC_SRCS
    src/main.c
    src/core/save.c
    src/core/load.c
    src/core/bvh.c
    src/core/circuit.c
    src/core/changelog.c
    src/ux/ux.c
    src/ux/input.c
    src/ux/snap.c
    src/ux/undo.c
    src/ui/ui.c
    src/view/view.c
    src/import/digital.c
    src/autoroute/autoroute.c
    src/render/fons_sgp.c
    src/render/sokol_nuklear.c
    src/render/fons_nuklear.c
    src/render/polyline.c
    src/render/draw.c
    src/main.c
    src/assets.c
    thirdparty/yyjson.c
    thirdparty/nfd/nfd_common.c
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
        "thirdparty/nfd/nfd_cocoa.m"
    )

    target_compile_definitions(digilogic PUBLIC "SOKOL_METAL")
    target_link_libraries(digilogic PUBLIC
        "-framework Metal"
        "-framework MetalKit"
        "-framework Quartz"
        "-framework Cocoa"
        "-framework UniformTypeIdentifiers"
    )

    set_target_properties(digilogic PROPERTIES
        MACOSX_BUNDLE TRUE
        MACOSX_BUNDLE_INFO_PLIST "${CMAKE_CURRENT_SOURCE_DIR}/res/Info.plist"
    )
endif()

#####################################
# Windows
#####################################

if(CMAKE_SYSTEM_NAME STREQUAL "Windows")
    add_executable(digilogic ${DIGILOGIC_SRCS}
        "res/app.o"
        "src/nonapple.c"
        "thirdparty/nfd/nfd_win.cpp"
    )

    target_compile_definitions(digilogic PUBLIC "UNICODE" "_UNICODE")

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
        "thirdparty/nfd/nfd_gtk.c"
    )

    find_package(PkgConfig REQUIRED)
    pkg_check_modules(GTK REQUIRED IMPORTED_TARGET "gtk+-3.0")
    target_link_libraries(digilogic PRIVATE PkgConfig::GTK)

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

set_property(TARGET digilogic PROPERTY C_STANDARD 11gnu)

find_package(Freetype REQUIRED)
target_link_libraries(digilogic PRIVATE Freetype::Freetype)

# some virtual machines and old hardware don't support MSAA
if (MSAA_SAMPLE_COUNT)
    target_compile_definitions(digilogic PRIVATE "MSAA_SAMPLE_COUNT=${MSAA_SAMPLE_COUNT}")
endif()

target_include_directories(digilogic PRIVATE "thirdparty/nfd/include")
target_include_directories(digilogic PRIVATE "thirdparty")
target_include_directories(digilogic PRIVATE "src")

# link rust crates into digilogic
target_link_libraries(digilogic PRIVATE digilogic_routing)
