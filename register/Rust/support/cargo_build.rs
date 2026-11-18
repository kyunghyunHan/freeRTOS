use std::{
    env,
    path::{Path, PathBuf},
    process::Command,
};

const CPU_FLAGS: &[&str] = &[
    "-mcpu=cortex-m4",
    "-mthumb",
    "-mfpu=fpv4-sp-d16",
    "-mfloat-abi=hard",
];

fn gcc_library(gcc: &Path, name: &str) -> PathBuf {
    let output = Command::new(gcc)
        .args(CPU_FLAGS)
        .arg(format!("-print-file-name={name}"))
        .output()
        .expect("Cannot run ARM GCC; set ARM_GCC to its full path");
    assert!(output.status.success(), "ARM GCC library lookup failed");
    let path = PathBuf::from(String::from_utf8(output.stdout).unwrap().trim());
    assert!(path.is_file(), "Missing ARM library: {}", path.display());
    path
}

fn main() {
    assert_eq!(
        env::var("TARGET").unwrap(),
        "thumbv7em-none-eabihf",
        "Build with --target thumbv7em-none-eabihf"
    );
    println!("cargo:rerun-if-env-changed=ARM_GCC");
    let app = PathBuf::from(env::var_os("CARGO_MANIFEST_DIR").unwrap());
    let root = app.join("../../..").canonicalize().unwrap();
    let out = PathBuf::from(env::var_os("OUT_DIR").unwrap());
    let gcc = env::var_os("ARM_GCC")
        .map(PathBuf::from)
        .unwrap_or_else(|| {
            PathBuf::from(env::var_os("HOME").unwrap())
                .join("zephyr-sdk-1.0.1/gnu/arm-zephyr-eabi/bin/arm-zephyr-eabi-gcc")
        });
    let kernel = root.join("external/FreeRTOS-Kernel");
    let device = root.join("external/cmsis-device-f4");
    let core = root.join("external/CMSIS_5/CMSIS/Core/Include");
    let linker = root.join("cmake/stm32f429zi.ld");
    let includes = [
        app.clone(),
        kernel.join("include"),
        kernel.join("portable/GCC/ARM_CM4F"),
        device.join("Include"),
        core,
    ];
    let sources = [
        device.join("Source/Templates/system_stm32f4xx.c"),
        kernel.join("tasks.c"),
        kernel.join("queue.c"),
        kernel.join("list.c"),
        kernel.join("portable/GCC/ARM_CM4F/port.c"),
        kernel.join("portable/MemMang/heap_4.c"),
        root.join("cmake/freertos_hooks.c"),
        root.join("cmake/freertos_runtime.c"),
        // Keep weak fault hooks ahead of the startup's weak default aliases.
        device.join("Source/Templates/gcc/startup_stm32f429xx.s"),
    ];
    // Track headers as well as C/ASM sources when Cargo checks freshness.
    // Do not track the whole app directory: it also contains Cargo's target output.
    for path in includes.iter().skip(1).chain(sources.iter()).chain([
        &linker,
        &app.join("FreeRTOSConfig.h"),
        &app.join("../01_hello_world/FreeRTOSConfig.h"),
        &app.join("build.rs"),
        &root.join("register/Rust/support/cargo_build.rs"),
    ]) {
        println!("cargo:rerun-if-changed={}", path.display());
    }
    for (index, source) in sources.iter().enumerate() {
        let object = out.join(format!("firmware_{index}.o"));
        let mut command = Command::new(&gcc);
        command.args(CPU_FLAGS).args([
            "-DSTM32F429xx",
            "-DHSI_VALUE=16000000U",
            "-Os",
            "-g3",
            "-ffunction-sections",
            "-fdata-sections",
            "-fno-common",
            "-Wall",
            "-Wextra",
            "-Werror",
        ]);
        for include in &includes {
            command.arg("-I").arg(include);
        }
        let status = command
            .arg("-c")
            .arg(source)
            .arg("-o")
            .arg(&object)
            .status()
            .expect("Cannot run ARM GCC; set ARM_GCC to its full path");
        assert!(
            status.success(),
            "C/ASM compilation failed: {}",
            source.display()
        );
        println!("cargo:rustc-link-arg={}", object.display());
    }
    // Rust's built-in rust-lld links the ARM objects and GCC runtime libraries.
    println!("cargo:rustc-link-arg=--start-group");
    for library in ["libc.a", "libgcc.a"] {
        println!(
            "cargo:rustc-link-arg={}",
            gcc_library(&gcc, library).display()
        );
    }
    println!("cargo:rustc-link-arg=--end-group");
    println!("cargo:rustc-link-arg=-T{}", linker.display());
    println!("cargo:rustc-link-arg=--wrap=vTaskStartScheduler");
    println!("cargo:rustc-link-arg=--gc-sections");
    println!(
        "cargo:rustc-link-arg=-Map={}",
        out.join("firmware.map").display()
    );
}
