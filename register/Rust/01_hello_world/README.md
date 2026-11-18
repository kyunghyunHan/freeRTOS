# 01_hello_world — NUCLEO-F429ZI

현재 `src/lib.rs`에는 사용자가 작성한 F429ZI LED·버튼 코드가 있습니다.
문자열 Hello World는 `../03_hello_world_cargo` 예제를 사용하세요.

이 예제 폴더에서 빌드하고 기록합니다.

```bash
cargo build --release --target thumbv7em-none-eabihf
openocd -f board/st_nucleo_f4.cfg \
  -c "program target/thumbv7em-none-eabihf/release/firmware verify reset exit"
```

출력 `firmware`는 확장자가 없는 ARM ELF입니다.
기존 `.a` 또는 `hello-world-f429zi` 경로를 사용하지 마세요.
앱의 C 소스 없이 Rust가 main과 태스크를 정의합니다.
공유 `../support/cargo_build.rs`가 C/ASM FreeRTOS·CMSIS를 빌드하고 링크합니다.
CMake 호환 빌드도 제공하지만 Cargo만으로 빌드 가능합니다.

빌드는 확인했으며 실제 보드 기록과 동작은 미검증입니다.
