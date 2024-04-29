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

pub fn build(b: *std.build.Builder) void {
    const opt_use_gl = b.option(bool, "gl", "Force OpenGL (default: false)") orelse false;
    const opt_use_wgpu = b.option(bool, "wgpu", "Force WebGPU (default: false, web only)") orelse false;
    const opt_use_x11 = b.option(bool, "x11", "Force X11 (default: true, Linux only)") orelse true;
    const opt_use_wayland = b.option(bool, "wayland", "Force Wayland (default: false, Linux only, not supported in main-line headers)") orelse false;
    const opt_use_egl = b.option(bool, "egl", "Force EGL (default: false, Linux only)") orelse false;
    const sokol_backend: SokolBackend = if (opt_use_gl) .gl else if (opt_use_wgpu) .wgpu else .auto;

    const target = b.standardTargetOptions(.{});
    const optimize = b.standardOptimizeOption(.{});

    const backend = resolveSokolBackend(sokol_backend, target);
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

    const default_cflags: []const []const u8 = &.{ backend_cflags, "-Ithirdparty", "-Isrc", "-Wall", "-Werror" };

    // platform specific compile and link options
    var cflags: []const []const u8 = default_cflags;
    if (target.isDarwin()) {
        cflags = &.{ "-O0", "-g", "-fsanitize=address,undefined", "-fno-omit-frame-pointer", backend_cflags, "-Ithirdparty", "-Isrc", "-Wall", "-Werror" };
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

        var mflags: []const []const u8 = &.{ "-ObjC", "-O0", "-g", "-fsanitize=address,undefined", "-fno-omit-frame-pointer", "-fobjc-arc", backend_cflags, "-Ithirdparty", "-Isrc", "-Wall", "-Werror" };
        digilogic.addCSourceFile(.{
            .file = .{ .path = "src/apple.m" },
            .flags = mflags,
        });

        digilogic.addLibraryPath(.{ .path = "/opt/homebrew/opt/llvm/lib/clang/17/lib/darwin/" });
        digilogic.linkSystemLibrary("clang_rt.asan_osx_dynamic");
        digilogic.linkSystemLibrary("clang_rt.ubsan_osx_dynamic");

        digilogic.addLibraryPath(.{ .path = "thirdparty/routing/target/release" });
        digilogic.linkSystemLibrary("digilogic_routing");
    } else if (target.abi == .android) {
        if (.gles3 != backend) {
            @panic("For android targets, you must have backend set to GLES3");
        }
        digilogic.linkSystemLibrary("GLESv3");
        digilogic.linkSystemLibrary("EGL");
        digilogic.linkSystemLibrary("android");
        digilogic.linkSystemLibrary("log");
        digilogic.addCSourceFile(.{
            .file = .{ .path = "src/nonapple.c" },
            .flags = cflags,
        });
    } else if (target.os_tag == .linux) {
        const egl_cflags = if (opt_use_egl) "-DSOKOL_FORCE_EGL " else "";
        const x11_cflags = if (!opt_use_x11) "-DSOKOL_DISABLE_X11 " else "";
        const wayland_cflags = if (!opt_use_wayland) "-DSOKOL_DISABLE_WAYLAND" else "";
        const link_egl = opt_use_egl or opt_use_wayland;
        cflags = &.{ backend_cflags, "-Ithirdparty", "-Isrc", "-Wall", "-Werror", egl_cflags, x11_cflags, wayland_cflags };
        digilogic.linkSystemLibrary("asound");
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
            .file = .{ .path = "src/nonapple.c" },
            .flags = cflags,
        });
    } else if (target.os_tag == .windows) {
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
            .file = .{ .path = "src/nonapple.c" },
            .flags = cflags,
        });
        digilogic.addLibraryPath(.{ .path = "thirdparty/routing/target/x86_64-pc-windows-gnu/release" });
        digilogic.linkSystemLibrary("digilogic_routing");
    }

    // finally add the C source files
    const csrc_root = "src/";
    const csources = [_][]const u8{
        "core/circuit.c",
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
            .file = .{ .path = csrc_root ++ csrc },
            .flags = cflags,
        });
    }

    b.installArtifact(digilogic);
    var run: ?*Build.Step.Run = b.addRunArtifact(digilogic);
    b.step("run-digilogic", "Run digilogic").dependOn(&run.?.step);
}

// helper function to resolve .auto backend based on target platform
pub fn resolveSokolBackend(backend: SokolBackend, target: std.zig.CrossTarget) SokolBackend {
    if (backend != .auto) {
        return backend;
    } else if (target.isDarwin()) {
        return .metal;
    } else if (target.isWindows()) {
        return .d3d11;
    } else if (target.os_tag == .wasi) {
        return .gles3;
    } else if (target.abi == .android) {
        return .gles3;
    } else {
        return .gl;
    }
}
