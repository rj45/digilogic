# digilogic

- Initially (MVP) just a circuit diagram editor
- With the following features:
    - Wire auto-routing
    - Alignment based snapping (rather than grid based)
    - First class dark mode support
    - Cross platform (Windows, Mac, Linux, iOS, Android)
    - Native C (with some Rust), and light weight
- Inspirations:
    - Issie (github.com/tomcl/issie)
    - Digital Logic Sim (github.com/SebLague/Digital-Logic-Sim)
    - Digital (github.com/hneemann/Digital)
    - LogiSim Evolution (github.com/logisim-evolution/logisim-evolution)

## Building

### Using Zig

You will need rust installed via rustup, zig, and on windows you'll need MSVC.

#### Mac

```sh
git clone --recurse-submodules https://github.com/rj45/digilogic.git
cd digilogic
zig build
open zig-out/digilogic.app
```

You can move the app in `zig-out` to your `Applications` folder if you want to.

#### Windows

```sh
git clone --recurse-submodules https://github.com/rj45/digilogic.git
cd digilogic
zig build "-Dtarget=x86_64-windows-msvc"
zig-out\digilogic.exe
```

If you get a crash immediately, try disabling MSAA:

```sh
zig build "-Dtarget=x86_64-windows-msvc" "-Dmsaa_sample_count=1"
```

#### Linux

The zig build script on linux needs more testing. Pester me about this if it doesn't work. In the meantime the cmake build should work.

### Using cmake

You'll need rust via rustup, and cmake. Windows you'll need MSVC. You need FreeType too.

#### Mac

(You can also follow the linux instructions to avoid XCode and use make.)

```sh
git clone --recurse-submodules https://github.com/rj45/digilogic.git
cd digilogic
mkdir build
cd build
cmake .. -G Xcode -DCMAKE_BUILD_TYPE=Release
cmake --build . --config Release
```

#### Windows

You will need cmake and MSVC installed.

You you can also either use vcpkg or install freetype and pass the info to cmake.

vcpkg is shown below, assuming vcpkg is installed at `c:\Users\User\Documents\vcpkg`.


```sh
git clone --recurse-submodules https://github.com/rj45/digilogic.git
cd digilogic
cmake -B build -DCMAKE_BUILD_TYPE=Release "-DCMAKE_TOOLCHAIN_FILE=c:/Users/User/Documents/vcpkg/scripts/buildsystems/vcpkg.cmake" "-DVCPKG_TARGET_TRIPLET=x64-windows-static-md"
cmake --build build --config Release
build\Release\digilogic.exe
```

#### Linux X11 (Xorg)

You will need FreeType, cmake and build essentials installed. You may also need dev libraries for X11.

Note: Older versions of FreeType may cause font rendering issues, need to investigate if there is a fix.

```sh
git clone --recurse-submodules https://github.com/rj45/digilogic.git
cd digilogic
mkdir build
cmake .. -DCMAKE_BUILD_TYPE=Release
make
./digilogic
```

#### Linux Wayland

Note: For Gnome, I recommend using Xwayland and compiling for X11 rather than native Wayland. This will give you window decorations. If you don't use Gnome, then you won't have this problem and Wayland will work fine for you. I welcome contributions to get libdecor working for Gnome+Wayland.

You will need FreeType, cmake and build essentials installed. You may also need dev libraries for Wayland.

```sh
git clone --recurse-submodules https://github.com/rj45/digilogic.git
cd digilogic
mkdir build
cmake .. -DCMAKE_BUILD_TYPE=Release -DUSE_WAYLAND=1
make
./digilogic
```

## License

Apache 2.0, see [NOTICE](./NOTICE) and [LICENSE](./LICENSE).