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

# Building

You'll need rust via rustup, and zig 0.12 (or newer?). The following instructions should work on Mac and Windows. Linux as yet untested. Instructions may need adaptation for windows, and you'll need MSVC or mingw.

```sh
git clone --recurse-submodules https://github.com/rj45/digilogic.git
cd thirdparty/routing
cargo build --release
cd ../..
zig build run-digilogic
```

# License

Apache 2.0, see [NOTICE](./NOTICE) and [LICENSE](./LICENSE).