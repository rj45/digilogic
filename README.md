# digilogic

A digital logic simulation and schematic capture program.

- With the following features:
    - Wire auto-routing
    - Alignment based snapping (rather than grid based)
    - First class dark mode support
    - Cross platform (Windows, Mac, Linux)
    - Import from verilog via Yosys with auto layout and routing
    - Import from H. Neeman's [Digital](github.com/hneemann/Digital)
    - Written in Rust
- Inspirations:
    - Issie (github.com/tomcl/issie)
    - Digital Logic Sim (github.com/SebLague/Digital-Logic-Sim)
    - Digital (github.com/hneemann/Digital)
    - LogiSim Evolution (github.com/logisim-evolution/logisim-evolution)

## Status

This is under heavy development.

The only editing feature implemented so far is moving components around.

Currently, importing a Digital circuit works as long as it:
- Contains no rotated components
- Only uses Inputs, Outputs, And, Or, Xor and Not
- All wires/components are 1 bit wide
- Contains no embedded circuits

Yosys import works with similar constraints:
- Only produces And, Or, Xor and Not gates
- Only a single module with input and output ports (but no inout ports)

## Building / Running

Install rust via rustup, then:

```sh
cargo run
```

To run the simulation server (required to simulate circuits) run this in a separate terminal:

```sh
cargo run -- server
```

## Yosys Import

Use the following command to generate an *unoptimized* yosys file for import:

```sh
yosys -p "read_verilog <INPUT_VERILOG>.v; hierarchy -auto-top; proc; opt_clean; fsm -expand; memory -nomap; wreduce -memx; opt_clean; write_json <OUTPUT_FILE>.yosys"
```

To have yosys do some basic optimizations on the verilog, use this command:

```sh
yosys -p "read_verilog <INPUT_VERILOG>.v; hierarchy -auto-top; proc; opt; fsm -expand; memory -nomap; wreduce -memx; opt; write_json <OUTPUT_FILE>.yosys"
```

If it crashes/errors on loading, it likely contains components that have not been implemented yet. Simplify your verilog until it works.

## Credits

The vast majority of the code in this repo is written by [Mathis Rech](https://github.com/Artentus), especially the simulation engine and auto-router.

I (rj45) mainly wrote the import code, UX code, and did lots of project management / design type stuff.

## License

Apache 2.0, see [LICENSE](./LICENSE).
