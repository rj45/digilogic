cmake_minimum_required(VERSION 3.16)
project(digilogic LANGUAGES C)

set(DIGILOGIC_SRCS
    src/main.c
    src/core/circuit.c
    src/core/timer.c
    src/ux/ux.c
    src/ux/input.c
    src/ux/undo.c
    src/ux/autoroute.c
    src/view/view.c
    src/import/digital.c
    src/avoid/avoid.c
    src/main.c
    src/noto_sans_regular.c
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

    target_link_libraries(digilogic PUBLIC
        "GL"
    )

    if (USE_GLES)
        target_compile_definitions(digilogic PUBLIC "SOKOL_GLES3")
    else()
        target_compile_definitions(digilogic PUBLIC "SOKOL_GLCORE33")
    endif()

    if (USE_EGL)
        target_compile_definitions(digilogic PUBLIC "SOKOL_FORCE_EGL")
    endif()

    if(USE_X11)
        target_compile_definitions(digilogic PUBLIC "SOKOL_DISABLE_WAYLAND")
        target_link_libraries(digilogic PUBLIC
            "X11"
            "Xcursor"
            "Xi"
            "GL"
        )
        if (USE_EGL)
            target_link_libraries(digilogic PUBLIC
                "EGL"
            )
        endif()
    else() # use wayland (default)
        target_compile_definitions(digilogic PUBLIC "SOKOL_DISABLE_X11")
        target_link_libraries(digilogic PUBLIC
            "wayland-client"
            "wayland-cursor"
            "wayland-egl"
            "xkbcommon"
            "EGL"
        )
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

# will make targets for each crate in the Cargo.toml
corrosion_import_crate(
    MANIFEST_PATH thirdparty/routing/Cargo.toml
    PROFILE release
)

#####################################
# C
#####################################

set_property(TARGET digilogic PROPERTY C_STANDARD 11)

file(COPY "testdata" DESTINATION "${CMAKE_CURRENT_BINARY_DIR}")

target_include_directories(digilogic PUBLIC "thirdparty")
target_include_directories(digilogic PUBLIC "src")

# link rust crates into digilogic
target_link_libraries(digilogic PUBLIC digilogic_routing)
