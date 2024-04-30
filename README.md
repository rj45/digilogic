# digilogic

- Initially (MVP) just a circuit diagram editor for iOS and Mac
- With the following features:
    - Wire auto-routing
    - Alignment based snapping (rather than grid based)
    - First class dark mode support
- Inspirations:
    - Issie (github.com/tomcl/issie)
    - Digital Logic Sim (github.com/SebLague/Digital-Logic-Sim)
    - Digital (github.com/hneemann/Digital)
    - LogiSim Evolution (github.com/logisim-evolution/logisim-evolution)

## Building

You'll need rust via rustup, and cmake. Windows you'll need MSVC.

### Mac

```sh
git clone --recurse-submodules https://github.com/rj45/digilogic.git
cd digilogic
mkdir build
cd build
cmake -G Xcode ..
cmake --build . --config Release
```

### Windows

```sh
git clone --recurse-submodules https://github.com/rj45/digilogic.git
cd digilogic
cmake -B build
cmake --build build --config Release
build\Release\digilogic.exe
```

### Linux

```sh
git clone --recurse-submodules https://github.com/rj45/digilogic.git
cd digilogic
cmake -B build -DCMAKE_BUILD_TYPE=Release
make
./digilogic
```

## License

Apache 2.0, see [NOTICE](./NOTICE) and [LICENSE](./LICENSE).