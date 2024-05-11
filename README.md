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

You'll need rust via rustup, and cmake. Windows you'll need MSVC. You need FreeType too.

### Mac

(You can also follow the linux instructions to avoid XCode and use make.)

```sh
git clone --recurse-submodules https://github.com/rj45/digilogic.git
cd digilogic
mkdir build
cd build
cmake .. -G Xcode -DCMAKE_BUILD_TYPE=Release
cmake --build . --config Release
```

### Windows

You need to install FreeType somehow and then pass the path to cmake.

This might be potential a source: https://github.com/ubawurinna/freetype-windows-binaries

MSVC is also required, as well as cmake.

```sh
git clone --recurse-submodules https://github.com/rj45/digilogic.git
cd digilogic
cmake -B build -DCMAKE_BUILD_TYPE=Release -DFREETYPE_DIR=<path_to_freetype>
cmake --build build --config Release
build\Release\digilogic.exe
```

### Linux

*Note: linux is currently untested. I do have a linux box, so this will be fixed eventually.*

You will need FreeType, cmake and build essentials installed. You may also need dev libraries for X11 or Wayland. Wayland is the default, but X11 is also possible with `-DUSE_X11=1` on the cmake line below.

```sh
git clone --recurse-submodules https://github.com/rj45/digilogic.git
cd digilogic
mkdir build
cmake .. -DCMAKE_BUILD_TYPE=Release
make
./digilogic
```

## License

Apache 2.0, see [NOTICE](./NOTICE) and [LICENSE](./LICENSE).