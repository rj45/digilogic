const std = @import("std");
const builtin = @import("builtin");
const Build = std.Build;
const OptimizeMode = std.builtin.OptimizeMode;

pub const SokolBackend = enum {
    auto, // Windows: D3D11, macOS/iOS: Metal, otherwise: GL
    d3d11,
    metal,
    gl,
    gles3,
    wgpu,
};

pub fn build(b: *std.Build) void {
    const opt_use_gl = b.option(bool, "gl", "Force OpenGL (default: false)") orelse false;
    const opt_use_wgpu = b.option(bool, "wgpu", "Force WebGPU (default: false, web only)") orelse false;
    const opt_use_x11 = b.option(bool, "x11", "Force X11 (default: true, Linux only)") orelse true;
    const opt_use_wayland = b.option(bool, "wayland", "Force Wayland (default: false, Linux only, not supported in main-line headers)") orelse false;
    const opt_use_egl = b.option(bool, "egl", "Force EGL (default: false, Linux only)") orelse false;
    const sokol_backend: SokolBackend = if (opt_use_gl) .gl else if (opt_use_wgpu) .wgpu else .auto;

    const target = b.standardTargetOptions(.{});
    const optimize = b.standardOptimizeOption(.{});

    const backend = resolveSokolBackend(sokol_backend, target.result);
    const backend_cflags = switch (backend) {
        .d3d11 => "-DSOKOL_D3D11",
        .metal => "-DSOKOL_METAL",
        .gl => "-DSOKOL_GLCORE33",
        .gles3 => "-DSOKOL_GLES3",
        .wgpu => "-DSOKOL_WGPU",
        else => @panic("unknown sokol backend"),
    };

    const digilogic = b.addExecutable(.{
        .name = "digilogic",
        .target = target,
        .optimize = optimize,
    });
    digilogic.linkLibC();

    const cargoCmd = b.addSystemCommand(&.{ "cargo", "build", "--release" });
    cargoCmd.cwd = b.path("thirdparty/routing");
    // cargoCmd.addDirectoryArg(b.path("thirdparty/routing"));
    digilogic.step.dependOn(&cargoCmd.step);

    const default_cflags: []const []const u8 = &.{ backend_cflags, "-Ithirdparty", "-Isrc", "-Wall", "-Werror" };

    // platform specific compile and link options
    var cflags: []const []const u8 = default_cflags;
    if (target.result.isDarwin()) {
        cflags = &.{ backend_cflags, "-Ithirdparty", "-Isrc", "-Wall", "-Werror" };
        digilogic.linkFramework("Foundation");
        // digilogic.linkFramework("CoreGraphics");
        // digilogic.linkFramework("AppKit");
        // digilogic.linkFramework("AudioToolbox");
        if (.metal == backend) {
            digilogic.linkFramework("MetalKit");
            digilogic.linkFramework("Metal");
        }
        // if (target.os_tag == .ios) {
        //     digilogic.linkFramework("UIKit");
        //     digilogic.linkFramework("AVFoundation");
        //     if (.gl == backend) {
        //         digilogic.linkFramework("OpenGLES");
        //         digilogic.linkFramework("GLKit");
        //     }
        // } else if (target.os_tag == .macos) {
        digilogic.linkFramework("Cocoa");
        digilogic.linkFramework("Quartz");
        if (.gl == backend) {
            digilogic.linkFramework("OpenGL");
        }
        // }

        const mflags: []const []const u8 = &.{ "-ObjC", "-fobjc-arc", backend_cflags, "-Ithirdparty", "-Isrc", "-Wall", "-Werror" };
        digilogic.addCSourceFile(.{
            .file = b.path("src/apple.m"),
            .flags = mflags,
        });
    } else if (target.result.isAndroid()) {
        if (.gles3 != backend) {
            @panic("For android targets, you must have backend set to GLES3");
        }
        digilogic.linkSystemLibrary("GLESv3");
        digilogic.linkSystemLibrary("EGL");
        digilogic.linkSystemLibrary("android");
        digilogic.linkSystemLibrary("log");
        digilogic.addCSourceFile(.{
            .file = b.path("src/nonapple.c"),
            .flags = cflags,
        });
    } else if (target.result.os.tag == .linux) {
        const egl_cflags = if (opt_use_egl) "-DSOKOL_FORCE_EGL " else "";
        const x11_cflags = if (!opt_use_x11) "-DSOKOL_DISABLE_X11 " else "";
        const wayland_cflags = if (!opt_use_wayland) "-DSOKOL_DISABLE_WAYLAND" else "";
        const link_egl = opt_use_egl or opt_use_wayland;
        cflags = &.{ backend_cflags, "-Ithirdparty", "-Isrc", "-Wall", "-Werror", egl_cflags, x11_cflags, wayland_cflags };
        // digilogic.linkSystemLibrary("asound");
        digilogic.linkSystemLibrary("GL");
        if (opt_use_x11) {
            digilogic.linkSystemLibrary("X11");
            digilogic.linkSystemLibrary("Xi");
            digilogic.linkSystemLibrary("Xcursor");
        }
        if (opt_use_wayland) {
            digilogic.linkSystemLibrary("wayland-client");
            digilogic.linkSystemLibrary("wayland-cursor");
            digilogic.linkSystemLibrary("wayland-egl");
            digilogic.linkSystemLibrary("xkbcommon");
        }
        if (link_egl) {
            digilogic.linkSystemLibrary("EGL");
        }
        digilogic.addCSourceFile(.{
            .file = b.path("src/nonapple.c"),
            .flags = cflags,
        });
    } else if (target.result.os.tag == .windows) {
        cflags = &.{ backend_cflags, "-Ithirdparty", "-Isrc", "-Wall", "-Werror", "-Wno-unknown-pragmas" };
        digilogic.linkSystemLibrary("kernel32");
        digilogic.linkSystemLibrary("user32");
        digilogic.linkSystemLibrary("gdi32");
        digilogic.linkSystemLibrary("ole32");
        if (.d3d11 == backend) {
            digilogic.linkSystemLibrary("d3d11");
            digilogic.linkSystemLibrary("dxgi");
        }
        digilogic.addCSourceFile(.{
            .file = b.path("src/nonapple.c"),
            .flags = cflags,
        });
    }

    digilogic.addLibraryPath(b.path("thirdparty/routing/target/release"));
    digilogic.linkSystemLibrary("digilogic_routing");

    // finally add the C source files
    const csrc_root = "src/";
    const csources = [_][]const u8{
        "core/circuit.c",
        "core/timer.c",
        "ux/ux.c",
        "ux/input.c",
        "ux/undo.c",
        "ux/autoroute.c",
        "view/view.c",
        "import/digital.c",
        "avoid/avoid.c",
        "main.c",
        "noto_sans_regular.c",
    };
    inline for (csources) |csrc| {
        digilogic.addCSourceFile(.{
            .file = b.path(csrc_root ++ csrc),
            .flags = cflags,
        });
    }

    b.installArtifact(digilogic);
    var run: ?*Build.Step.Run = b.addRunArtifact(digilogic);
    b.step("run-digilogic", "Run digilogic").dependOn(&run.?.step);
}

// helper function to resolve .auto backend based on target platform
pub fn resolveSokolBackend(backend: SokolBackend, target: std.Target) SokolBackend {
    if (backend != .auto) {
        return backend;
    } else if (target.isDarwin()) {
        return .metal;
    } else if (target.os.tag == .windows) {
        return .d3d11;
    } else if (target.isWasm()) {
        return .gles3;
    } else if (target.isAndroid()) {
        return .gles3;
    } else {
        return .gl;
    }
}
