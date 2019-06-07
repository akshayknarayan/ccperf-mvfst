extern crate bindgen;

use std::env;
use std::path::PathBuf;

fn main() {
    let out_path = PathBuf::from(env::var("OUT_DIR").unwrap());

    std::process::Command::new("make")
        .arg("libccperf.so")
        .current_dir("./cpp")
        .spawn()
        .expect("mvfst spawn failed")
        .wait()
        .expect("mvfst build failed");

    let bindings = bindgen::Builder::default()
        .header("./cpp/ccperf.h")
        .clang_arg("-xc++")
        .clang_arg("-isysroot/Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX.sdk/")
        .blacklist_type(r#"u\d+"#)
        .whitelist_function(".*QuicClient.*")
        .whitelist_function(".*QuicServer.*")
        .whitelist_function("get")
        .generate()
        .expect("bindgen on ./cpp/mvfst.h");

    bindings
        .write_to_file(out_path.join("ccperf.rs"))
        .expect("Unable to write mvfst.rs");

    println!("cargo:rustc-link-search=./cpp");
    println!("cargo:rustc-link-lib=ccperf");
}
