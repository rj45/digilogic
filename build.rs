use std::path::{Path, PathBuf};

fn main() {
    println!("cargo::rustc-link-lib=static=digilogic_routing");
    println!("cargo::rustc-link-search=native=thirdparty/routing/target/release");
    cc::Build::new()
        .files(&[
            "src/c/core/bvh.c",
            "src/c/core/changelog.c",
            "src/c/core/circuit.c",
            "src/c/core/errors.c",
            "src/c/core/load.c",
            "src/c/core/save.c",
            "src/c/view/view.c",
            "src/c/autoroute/autoroute.c",
            "src/c/ux/actions.c",
            "src/c/ux/input.c",
            "src/c/ux/snap.c",
            "src/c/ux/ux.c",
            "src/c/import/digital.c",
            "src/c/lib.c",
        ])
        .include(Path::new("src/c"))
        .include(Path::new("thirdparty"))
        .compile("core");

    let bindings = bindgen::Builder::default()
        // The input header we would like to generate
        // bindings for.
        .header("src/c/lib.h")
        .clang_args(&["-Isrc/c", "-Ithirdparty"])
        // Tell cargo to invalidate the built crate whenever any of the
        // included header files changed.
        .parse_callbacks(Box::new(bindgen::CargoCallbacks::new()))
        // Finish the builder and generate the bindings.
        .generate()
        // Unwrap the Result and panic on failure.
        .expect("Unable to generate bindings");

    // Write the bindings to the $OUT_DIR/bindings.rs file.
    let out_path = PathBuf::from(std::env::var("OUT_DIR").unwrap());
    bindings
        .write_to_file(out_path.join("bindings.rs"))
        .expect("Couldn't write bindings!");
}
