# NUCLEO-F429ZI: Cargo로 빌드하는 Rust + FreeRTOS Hello World

Rust 태스크가 1초마다 `Hello World from Rust + FreeRTOS!`를 출력합니다.
CMake 없이 Cargo의 `build.rs`가 ARM GCC로 FreeRTOS와 CMSIS 시작 코드를 컴파일하고,
Rust의 기본 `rust-lld`가 최종 ELF를 연결합니다. 추가 Cargo 의존성은 없습니다.
기존 `01_hello_world`의 사용자 코드는 수정하지 않습니다.

## 빌드

프로젝트 루트에서:

```bash
cd register/Rust/03_hello_world_cargo
cargo build --release --target thumbv7em-none-eabihf
```

결과 `target/thumbv7em-none-eabihf/release/hello-world-f429zi`는 확장자가 없는 **ELF 펌웨어**입니다.
보드용 코드이므로 `cargo run` 대신 OpenOCD로 기록합니다.

FreeRTOS와 CMSIS는 저장소의 `external/`에 있어야 합니다([의존성 복원](../../../docs/dependencies.md)).
기본 C 컴파일러는 기존 프로젝트와 같은
`~/zephyr-sdk-1.0.1/gnu/arm-zephyr-eabi/bin/arm-zephyr-eabi-gcc`입니다.
다른 ARM GCC를 쓰려면 libc와 libgcc를 포함하는 툴체인을 준비하고 다음처럼 지정합니다.

```bash
ARM_GCC=/absolute/path/to/arm-none-eabi-gcc \
  cargo build --release --target thumbv7em-none-eabihf
```

## 보드 기록

ST-LINK USB 포트에 데이터 케이블을 연결하고, 위 예제 폴더에서:

```bash
openocd -f board/st_nucleo_f4.cfg \
  -c "program target/thumbv7em-none-eabihf/release/hello-world-f429zi verify reset exit"
```

## 출력 확인

[ST Nucleo-144 사용자 매뉴얼](https://www.st.com/resource/en/user_manual/dm00244518-stm32-nucleo144-boardsstmicroelectronics.pdf)에
따라 기본 ST-LINK VCP 연결인 USART3(TX=PD8, RX=PD9, AF7)를 사용합니다.
통신 설정은 **115200 baud, 8N1, 흐름제어 없음**입니다.

macOS에서 연결된 포트를 찾고, 실제 장치 이름으로 바꿔 실행합니다.

```bash
ls /dev/cu.usbmodem*
screen /dev/cu.usbmodem실제장치이름 115200
```

화면에 다음 문장이 매초 나옵니다.

```text
Hello World from Rust + FreeRTOS!
```

screen 종료: Ctrl-A → K → Y.

## 구성

- `src/main.rs`: GPIO/UART 초기화, FreeRTOS 태스크 생성, 문자열 출력
- `build.rs`: FreeRTOS·CMSIS·공통 런타임 C/ASM 빌드, ELF 링크 설정
- `FreeRTOSConfig.h`: 기존 예제의 커널 설정 공유(1 tick = 1 ms)
- `../../../cmake/stm32f429zi.ld`: F429ZI 메모리 배치 공유

release 빌드와 ARM ELF 및 인터럽트 벡터 연결을 확인했습니다. 보드 기록과 실제 UART 출력은 미검증입니다.
