//   Copyright 2024 Ryan "rj45" Sanche
//   Copyright 2024 Ben Crist
//
//   Licensed under the Apache License, Version 2.0 (the "License");
//   you may not use this file except in compliance with the License.
//   You may obtain a copy of the License at
//
//       http://www.apache.org/licenses/LICENSE-2.0
//
//   Unless required by applicable law or agreed to in writing, software
//   distributed under the License is distributed on an "AS IS" BASIS,
//   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
//   See the License for the specific language governing permissions and
//   limitations under the License.

/////////////////////////////////////////////////////////////////////////
// NOTE: This zib build script is experimental! If you want to use it,
// please contribute any fixes it needs! It's very appreciated!
// - rj45
/////////////////////////////////////////////////////////////////////////

const std = @import("std");
const zcc = @import("compile_commands");
const globlin = @import("globlin");
const crab = @import("build.crab");

pub fn build(b: *std.Build) void {
    const target = b.standardTargetOptions(.{});
    const optimize = b.standardOptimizeOption(.{ .preferred_optimize_mode = .ReleaseFast });

    const digilogic = b.addExecutable(.{
        .name = "digilogic",
        .target = target,
        .optimize = optimize,
    });

    const digilogic_test = b.addExecutable(.{
        .name = "test",
        .target = target,
        .optimize = optimize,
    });

    const Sanitizer = enum {
        none,
        address,
        // note: memory sanitizer requires the whole world be built with it
        // including Xorg, OpenGL, etc. This isn't very practical for `digilogic`
        // but `digilogic_test` can use it.
        memory,
        thread,
    };
    const sanitizer = b.option(Sanitizer, "sanitize", "Specify which sanitizer to use on linux in debug mode") orelse .address;

    var cflags = std.ArrayList([]const u8).init(b.allocator);
    cflags.append("-std=gnu11") catch @panic("OOM");

    if (optimize == .Debug) {
        digilogic.root_module.addCMacro("DEBUG", "1");

        if (target.result.abi == .msvc) {
            cflags.appendSlice(&.{
                "-g",
                "-O1",
            }) catch @panic("OOM");
        } else {
            cflags.appendSlice(&.{
                "-g",
                "-O1",
                "-fno-omit-frame-pointer",
                "-fno-optimize-sibling-calls",
                "-Wall",
                "-Werror",
                "-Wno-unused-function",
            }) catch @panic("OOM");

            if (sanitizer != .none) {
                if (find_llvm_lib_path(b)) |llvm_lib_path| {
                    // turn on address and undefined behaviour sanitizers
                    const sanstr = switch (sanitizer) {
                        .address => "address",
                        .memory => "memory",
                        .thread => "thread",
                        else => "",
                    };
                    cflags.append(b.fmt("-fsanitize={s},undefined", .{sanstr})) catch @panic("OOM");

                    // by default many UB sanitizers will just trap, but
                    // that is super confusing to debug. So instead we tell it to
                    // print a stacktrace instead of trapping.
                    cflags.append("-fno-sanitize-trap=all") catch @panic("OOM");

                    if (sanitizer == .memory) {
                        cflags.append("-fsanitize-memory-track-origins=2") catch @panic("OOM");
                    }

                    inline for ([_]*std.Build.Step.Compile{ digilogic, digilogic_test }) |exe| {
                        exe.addLibraryPath(.{ .cwd_relative = llvm_lib_path });

                        if (target.result.os.tag.isDarwin()) {
                            // todo: figure out libraries for other sanitizers
                            exe.linkSystemLibrary("clang_rt.asan_osx_dynamic");
                            exe.linkSystemLibrary("clang_rt.ubsan_osx_dynamic");
                        } else if (target.result.os.tag == .linux) {
                            if (sanitizer == .address) {
                                exe.linkSystemLibrary("clang_rt.asan_static-x86_64");
                                exe.linkSystemLibrary("clang_rt.asan-x86_64");
                            } else if (sanitizer == .memory) {
                                exe.linkSystemLibrary("clang_rt.msan-x86_64");
                            } else if (sanitizer == .thread) {
                                exe.linkSystemLibrary("clang_rt.tsan-x86_64");
                            }
                            exe.linkSystemLibrary("clang_rt.ubsan-x86_64");
                            exe.linkSystemLibrary("pthread");
                            exe.linkSystemLibrary("rt");
                            exe.linkSystemLibrary("m");
                            exe.linkSystemLibrary("dl");
                            exe.linkSystemLibrary("resolv");
                        }
                    }
                }
            }
        }
    }

    // add files common to both the main and test executables
    inline for ([_]*std.Build.Step.Compile{ digilogic, digilogic_test }) |exe| {
        exe.addCSourceFiles(.{
            .root = b.path("src"),
            .files = &.{
                "core/circuit.c",
                "core/smap.c",
                "core/save.c",
                "core/load.c",
                "core/bvh.c",
                "core/structdescs.c",
                "ux/ux.c",
                "ux/input.c",
                "ux/snap.c",
                "ux/undo.c",
                "view/view.c",
                "import/digital.c",
                "autoroute/autoroute.c",
            },
            .flags = cflags.items,
        });
        exe.addCSourceFiles(.{
            .root = b.path("thirdparty"),
            .files = &.{"yyjson.c"},
            .flags = cflags.items,
        });
    }

    digilogic.addCSourceFiles(.{
        .root = b.path("src"),
        .files = &.{
            "main.c",
            "ui/ui.c",
            "render/fons_sgp.c",
            "render/sokol_nuklear.c",
            "render/fons_nuklear.c",
            "render/polyline.c",
            "render/draw.c",
        },
        .flags = cflags.items,
    });

    // complile src/gen.c to generate C code
    const asset_gen = b.addExecutable(.{
        .name = "gen",
        .target = b.host,
    });
    asset_gen.addCSourceFile(.{
        .file = b.path("src/gen.c"),
        .flags = &.{"-std=gnu11"},
    });
    asset_gen.addIncludePath(b.path("thirdparty"));
    asset_gen.addIncludePath(b.path("src"));
    asset_gen.linkLibC();

    // generate assets.c from assets.zip
    const asset_gen_step = b.addRunArtifact(asset_gen);
    asset_gen_step.addFileArg(b.path("res/assets/NotoSans-Regular.ttf"));
    asset_gen_step.addFileArg(b.path("res/assets/symbols.ttf"));
    asset_gen_step.addFileArg(b.path("res/assets/testdata/simple_test.dig"));
    asset_gen_step.addFileArg(b.path("res/assets/testdata/alu_1bit_2inpgate.dig"));
    asset_gen_step.addFileArg(b.path("res/assets/testdata/alu_1bit_2gatemux.dig"));
    const assets_c = asset_gen_step.addOutputFileArg("assets.c");
    digilogic.addCSourceFile(.{
        .file = assets_c,
        .flags = cflags.items,
    });

    digilogic.addIncludePath(b.path("src"));
    digilogic.addIncludePath(b.path("thirdparty"));

    digilogic.linkLibC();

    const freetype = b.dependency("freetype", .{
        .target = target,
        .optimize = optimize,
    }).artifact("freetype");
    digilogic.linkLibrary(freetype);

    digilogic.root_module.addCMacro("NVD_STATIC_LINKAGE", "");
    digilogic.linkLibrary(build_nvdialog(b, target, optimize));

    const Renderer = enum {
        metal,
        opengl,
        opengles,
        d3d11,
    };
    const renderer = b.option(Renderer, "renderer", "Specify which rendering API to use (not all renderers work on all platforms");

    const msaa_sample_count = b.option(u32, "msaa_sample_count", "Number of MSAA samples to use (1 for no MSAA, default 4)") orelse 4;
    digilogic.root_module.addCMacro("MSAA_SAMPLE_COUNT", b.fmt("{d}", .{msaa_sample_count}));

    if (target.result.os.tag.isDarwin()) {
        // add apple.m (a copy of nonapple.c) to the build
        // this is required in order for the file to be compiled as Objective-C
        var mflags2 = std.ArrayList([]const u8).init(b.allocator);
        mflags2.append("-ObjC") catch @panic("OOM");
        mflags2.append("-fobjc-arc") catch @panic("OOM");
        mflags2.appendSlice(cflags.items) catch @panic("OOM");
        digilogic.addCSourceFile(.{
            .file = b.addWriteFiles().addCopyFile(b.path("src/nonapple.c"), "apple.m"),
            .flags = mflags2.items,
        });

        if (.metal != (renderer orelse .metal)) {
            @panic("This target supports only -Drenderer=metal");
        }

        digilogic.root_module.addCMacro("SOKOL_METAL", "");

        digilogic.linkFramework("Metal");
        digilogic.linkFramework("MetalKit");
        digilogic.linkFramework("Quartz");
        digilogic.linkFramework("Cocoa");
        digilogic.linkFramework("UniformTypeIdentifiers");
    } else if (target.result.os.tag == .windows) {
        digilogic.addWin32ResourceFile(.{
            .file = b.path("res/app.rc"),
        });

        digilogic.addCSourceFiles(.{
            .root = b.path("src"),
            .files = &.{
                "nonapple.c",
            },
            .flags = cflags.items,
        });

        switch (renderer orelse .d3d11) {
            .opengl => {
                digilogic.root_module.addCMacro("SOKOL_GLCORE33", "");
                digilogic.linkSystemLibrary("opengl32");
            },
            .d3d11 => {
                digilogic.root_module.addCMacro("SOKOL_D3D11", "");
                digilogic.linkSystemLibrary("d3d11");
                digilogic.linkSystemLibrary("dxgi");
            },
            else => @panic("This target supports only -Drenderer=d3d11 or -Drenderer=opengl"),
        }

        digilogic.linkSystemLibrary("kernel32");
        digilogic.linkSystemLibrary("user32");
        digilogic.linkSystemLibrary("gdi32");
        digilogic.linkSystemLibrary("ole32");
        digilogic.linkSystemLibrary("bcrypt"); // required by rust
        digilogic.linkSystemLibrary("ws2_32"); // required by rust
        digilogic.linkSystemLibrary("userenv"); // required by rust
        digilogic.linkSystemLibrary("advapi32"); // required by rust

        switch (target.result.abi) {
            .msvc => {
                digilogic.linkSystemLibrary("synchronization");
            },
            .gnu => {
                digilogic.linkSystemLibrary("API-MS-Win-Core-Synch-l1-2-0"); // required by rust
                digilogic.linkSystemLibrary("winmm"); // required by rust
                digilogic.linkSystemLibrary("unwind"); // required by rust
            },
            else => @panic("Unsupported target"),
        }

        digilogic.subsystem = .Windows;
    } else {
        // assuming linux

        digilogic.addCSourceFiles(.{
            .root = b.path("src"),
            .files = &.{
                "nonapple.c",
            },
            .flags = cflags.items,
        });

        const use_wayland = b.option(bool, "wayland", "Compile for Wayland instead of X11") orelse false;

        switch (renderer orelse .opengl) {
            .opengl => digilogic.root_module.addCMacro("SOKOL_GLCORE33", ""),
            .opengles => digilogic.root_module.addCMacro("SOKOL_GLES3", ""),
            else => @panic("This target supports only -Drenderer=opengl or -Drenderer=opengles"),
        }

        digilogic.linkSystemLibrary("GL");

        digilogic.linkSystemLibrary("unwind"); // required by rust
        digilogic_test.linkSystemLibrary("unwind");

        const use_egl = b.option(bool, "egl", "Force Sokol to use EGL instead of GLX for OpenGL context creation") orelse use_wayland;
        if (use_egl) {
            digilogic.root_module.addCMacro("SOKOL_FORCE_EGL", "");
            digilogic.linkSystemLibrary("EGL");
        }

        if (use_wayland) {
            digilogic.root_module.addCMacro("SOKOL_DISABLE_X11", "");
            digilogic.root_module.addCMacro("SOKOL_LINUX_CUSTOM", "");

            // TODO not sure if this is normally on the path; may need a better autodetection?
            const wayland_scanner_path = b.option([]const u8, "wayland-scanner-path", "Path to the system's wayland-scanner binary, if not on the path") orelse "wayland-scanner";

            const WaylandSource = struct {
                xml_path: []const u8,
                basename: []const u8,
            };

            inline for ([_]WaylandSource{
                .{ .xml_path = "/usr/share/wayland-protocols/stable/xdg-shell/xdg-shell.xml", .basename = "xdg-shell-protocol" },
                .{ .xml_path = "/usr/share/wayland-protocols/unstable/pointer-constraints/pointer-constraints-unstable-v1.xml", .basename = "pointer-constraints-unstable-v1-protocol" },
                .{ .xml_path = "/usr/share/wayland-protocols/unstable/relative-pointer/relative-pointer-unstable-v1.xml", .basename = "relative-pointer-unstable-v1-protocol" },
            }) |source| {
                const generate_header = b.addSystemCommand(&.{ wayland_scanner_path, "client-header" });
                generate_header.setStdIn(.{ .lazy_path = b.path(source.xml_path) });
                const header_file = generate_header.captureStdOut();
                const header_wf = b.addWriteFiles();
                _ = header_wf.addCopyFile(header_file, source.basename ++ ".h");

                const generate_source = b.addSystemCommand(&.{ wayland_scanner_path, "private-code" });
                generate_source.setStdIn(.{ .lazy_path = b.path(source.xml_path) });
                const source_file = generate_source.captureStdOut();

                digilogic.addCSourceFile(.{
                    .file = source_file,
                    .flags = cflags.items,
                });
                digilogic.addIncludePath(header_wf.getDirectory());
            }

            digilogic.linkSystemLibrary("wayland-client");
            digilogic.linkSystemLibrary("wayland-cursor");
            digilogic.linkSystemLibrary("wayland-egl");
            digilogic.linkSystemLibrary("xkbcommon");
        } else {
            // X11
            digilogic.root_module.addCMacro("SOKOL_DISABLE_WAYLAND", "1");

            digilogic.linkSystemLibrary("X11");
            digilogic.linkSystemLibrary("Xi");
            digilogic.linkSystemLibrary("Xcursor");
        }
    }

    const rust_lib_path = crab.addRustStaticlib(b, .{
        .name = if (target.result.os.tag == .windows) "digilogic_routing.lib" else "libdigilogic_routing.a",
        .manifest_path = b.path("thirdparty/routing/Cargo.toml"),
        .target = .{ .zig = target },
        .profile = crab.Profile.fromOptimizeMode(optimize),
        .cargo_args = &.{},
    });

    digilogic.addLibraryPath(rust_lib_path.dirname());
    digilogic.linkSystemLibrary("digilogic_routing");
    digilogic_test.addLibraryPath(rust_lib_path.dirname());
    digilogic_test.linkSystemLibrary("digilogic_routing");

    if (target.result.os.tag.isDarwin()) {
        // apple has their own way of doing things
        // we need to create an app bundle folder with the right structure
        const install_bin = b.addInstallArtifact(digilogic, .{ .dest_dir = .{ .override = .{ .custom = "digilogic.app/Contents/MacOS" } } });
        const install_plist = b.addInstallFile(b.path("res/Info.plist"), "digilogic.app/Contents/Info.plist");
        const install_icns = b.addInstallFile(b.path("res/logo.icns"), "digilogic.app/Contents/Resources/logo.icns");
        install_bin.step.dependOn(&install_plist.step);
        install_bin.step.dependOn(&install_icns.step);
        b.getInstallStep().dependOn(&install_bin.step);
    } else {
        b.installArtifact(digilogic);
    }

    digilogic_test.linkLibC();

    digilogic_test.addCSourceFiles(.{
        .root = b.path("src"),
        .files = &.{
            "test.c",
            "ux/ux_test.c",
            "view/view_test.c",
            "core/core_test.c",
            "render/draw_test.c",
        },
        .flags = cflags.items,
    });

    digilogic_test.addIncludePath(b.path("src"));
    digilogic_test.addIncludePath(b.path("thirdparty"));

    const test_run = b.addRunArtifact(digilogic_test);

    const test_step = b.step("test", "Build and run tests");
    test_step.dependOn(&test_run.step);

    zcc.createStep(b, "cdb", .{ .targets = &.{ digilogic, digilogic_test, asset_gen } });
}

fn build_nvdialog(b: *std.Build, target: std.Build.ResolvedTarget, optimize: std.builtin.OptimizeMode) *std.Build.Step.Compile {
    const nvdialog = b.addStaticLibrary(.{
        .name = "nvdialog",
        .target = target,
        .optimize = optimize,
    });

    const cflags = &.{
        "-std=gnu11",
        "-Wno-unused-parameter",
        "-Wconversion",
        "-Werror=format",
        "-Werror=format-security",
        "-Winline",
        "-Wall",
        "-Wextra",
        "-fstack-protector-all",
        "--param",
        "ssp-buffer-size=4",
    };

    nvdialog.addIncludePath(b.path("thirdparty/nvdialog/include"));
    nvdialog.addIncludePath(b.path("thirdparty/nvdialog/src"));
    nvdialog.addIncludePath(b.path("thirdparty/nvdialog/src/impl"));

    nvdialog.linkLibC();

    nvdialog.root_module.addCMacro("NVDIALOG_MAXBUF", "4096");
    nvdialog.root_module.addCMacro("NVD_EXPORT_SYMBOLS", "");
    nvdialog.root_module.addCMacro("NVD_STATIC_LINKAGE", "");

    const platform_ext = if (target.result.os.tag.isDarwin()) ".m" else ".c";

    const platform_files = &.{
        "nvdialog_dialog_box" ++ platform_ext,
        "nvdialog_file_dialog" ++ platform_ext,
        "nvdialog_question_dialog" ++ platform_ext,
        "nvdialog_notification" ++ platform_ext,
        "nvdialog_about_dialog" ++ platform_ext,
    };

    if (target.result.os.tag.isDarwin()) {
        nvdialog.root_module.addCMacro("NVD_USE_COCOA", "1");

        nvdialog.addCSourceFiles(.{
            .root = b.path("thirdparty/nvdialog/src/backend/cocoa"),
            .files = platform_files,
            .flags = cflags,
        });

        nvdialog.linkFramework("AppKit");
        nvdialog.linkFramework("Cocoa");
        nvdialog.linkFramework("Foundation");
        nvdialog.linkFramework("UserNotifications");
    } else if (target.result.os.tag == .windows) {
        nvdialog.addCSourceFiles(.{
            .root = b.path("thirdparty/nvdialog/src/backend/win32"),
            .files = platform_files,
            .flags = cflags,
        });

        nvdialog.linkSystemLibrary("comdlg32");
        nvdialog.linkSystemLibrary("shell32");
        nvdialog.linkSystemLibrary("user32");
    } else {
        nvdialog.root_module.addCMacro("NVD_SANDBOX_SUPPORT", "0");
        nvdialog.addCSourceFiles(.{
            .root = b.path("thirdparty/nvdialog/src/backend/gtk"),
            .files = platform_files,
            .flags = cflags,
        });

        nvdialog.addCSourceFiles(.{
            .root = b.path("thirdparty/nvdialog/src/backend/sandbox"),
            .files = platform_files,
            .flags = cflags,
        });

        nvdialog.linkSystemLibrary("gtk+-3.0");
    }

    nvdialog.addCSourceFiles(.{
        .root = b.path("thirdparty/nvdialog/src"),
        .files = &.{
            "nvdialog_error.c",
            "nvdialog_capab.c",
            "nvdialog_version.c",
            "nvdialog_main.c",
            "nvdialog_util.c",
        },
        .flags = cflags,
    });

    nvdialog.installHeadersDirectory(b.path("thirdparty/nvdialog/include"), "", .{});

    return nvdialog;
}

fn find_llvm_lib_path(b: *std.Build) ?[]const u8 {
    const zig_version = @import("builtin").zig_version;
    const llvm_version = if (zig_version.major == 0 and zig_version.minor < 13) 17 else 18;

    var base_path: []const u8 = undefined;
    var glob_pattern: []const u8 = undefined;

    if (b.host.result.os.tag.isDarwin()) {
        glob_pattern = b.fmt("llvm@{}/*/lib/clang/*/lib/darwin", .{llvm_version});
        const result = std.process.Child.run(.{
            .allocator = b.allocator,
            .argv = &.{
                "brew",
                "--cellar",
            },
            .cwd = b.pathFromRoot("."),
            .expand_arg0 = .expand,
        }) catch return null;
        switch (result.term) {
            .Exited => |status| if (status != 0) return null,
            else => return null,
        }
        base_path = std.mem.trim(u8, result.stdout, &std.ascii.whitespace);
    } else if (b.host.result.os.tag == .windows) {
        return null;
    } else {
        glob_pattern = b.fmt("llvm-{}/lib/clang/*/lib/linux", .{llvm_version});
        base_path = "/usr/lib";
    }

    var base_dir = std.fs.cwd().openDir(base_path, .{ .iterate = true }) catch return null;
    defer base_dir.close();

    var iter = base_dir.walk(b.allocator) catch return null;
    while (iter.next() catch return null) |entry| {
        if (entry.kind == .directory and globlin.match(glob_pattern, entry.path)) {
            return std.fs.path.join(b.allocator, &.{ base_path, entry.path }) catch @panic("OOM");
        }
    }
    return null;
}
