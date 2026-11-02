# 문제 해결

| 증상 | 해결 |
|---|---|
| dependency missing | [의존성 복원](dependencies.md) 후 재구성 |
| libc.a 또는 string.h 없음 | platform/arm-gcc.cmake의 SDK 경로 확인 |
| CMake 경로·컴파일러 오류 | 해당 앱 build를 지우고 toolchain 옵션과 함께 재구성 |
| missing core | rustup target list --installed에서 thumbv7em-none-eabihf 확인 |
| uses VFP register arguments | C hard-float / Rust eabihf 일치 여부 확인 후 clean |
| Cargo clean의 CACHEDIR.TAG 오류 | cmake --build를 한 번 실행한 뒤 다시 clean |
| Cargo.lock 갱신 필요 | 의존성 변경을 확인한 뒤 해당 앱 cargo generate-lockfile --offline |
| 수정 내용이 보드에 반영 안 됨 | 저장 → 같은 앱 빌드 → 같은 ELF 기록 |
| CMake Release인데 Rust는 dev | Cargo profile은 별도 설정. [빌드 문서](build.md) 참고 |
| clangd 헤더 오류 | Configure 후 compile_commands.json 경로 확인, clangd 재시작 |
| UART 무출력 | 기록한 앱·장치명·115200 baud·VCP 연결·케이블 확인 |
| Blink 시작 로그를 놓침 | 시리얼 터미널 연결 후 NRST |
| Flash reset/SWD WAIT | NRST 연결, 다른 서버 종료, reset 중 연결 옵션 확인 |
| RAM overlaps | heap·태스크 stack 크기와 firmware.map 확인 |
| LED 정지 | GDB에서 g_diagnostic, PC, backtrace 확인 |

Cargo 출력 폴더의 `CACHEDIR.TAG`은 공통 CMake가 빌드 전에 생성합니다.
`size`의 bss에는 4KiB MSP 예약이 포함되며, heap_4의 32KiB도 정적 RAM에 포함됩니다.
