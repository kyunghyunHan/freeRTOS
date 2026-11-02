# Flash·전체 삭제·UART·GDB

프로젝트 루트에서 실행합니다. 기록하면 기존 펌웨어가 교체됩니다.
보드 내장 ST-LINK USB 커넥터(CN1)에 데이터 케이블을 연결하고 SWD 점퍼·전원을 확인하세요.
다른 OpenOCD/GDB 서버는 종료하세요.

## Flash

먼저 [빌드](build.md)를 완료하고 사용하려는 앱의 ELF 경로를 정확히 선택합니다.
아래 세 명령 중 **원하는 한 앱만** 실행하세요. `verify` 성공과 정상 종료를 확인합니다.

C LED Blink:

```bash
cd /Users/hangyeonghyeon/Desktop/Rust/hardware/freeRTOS
/opt/homebrew/bin/openocd -s /opt/homebrew/share/openocd/scripts \
  -f board/st_nucleo_f4.cfg \
  -c 'reset_config srst_only srst_nogate connect_assert_srst' \
  -c 'program C/01_led_blink/build/nucleo_f446re/firmware.elf verify reset exit'
```

Rust UART Hello World:

```bash
/opt/homebrew/bin/openocd -s /opt/homebrew/share/openocd/scripts \
  -f board/st_nucleo_f4.cfg \
  -c 'reset_config srst_only srst_nogate connect_assert_srst' \
  -c 'program Rust/01_hello_world/build/nucleo_f446re/firmware.elf verify reset exit'
```

Rust LED Blink:

```bash
/opt/homebrew/bin/openocd -s /opt/homebrew/share/openocd/scripts \
  -f board/st_nucleo_f4.cfg \
  -c 'reset_config srst_only srst_nogate connect_assert_srst' \
  -c 'program Rust/02_led_blink/build/nucleo_f446re/firmware.elf verify reset exit'
```

HEX도 같은 program 명령에 사용할 수 있습니다. BIN은 주소가 없어 `program 파일.bin 0x08000000 verify reset exit`가 필요합니다.
보통 앱 교체는 필요한 Flash sector만 지우므로 별도 mass erase가 필요하지 않습니다.

## 보드 사용자 Flash 전체 삭제 — 데이터 삭제 주의

**기존 펌웨어와 사용자 Flash 데이터가 모두 지워집니다. 되돌릴 수 없습니다.**
빌드 결과 없이 실행할 수 있습니다.

```bash
/opt/homebrew/bin/openocd -s /opt/homebrew/share/openocd/scripts \
  -f board/st_nucleo_f4.cfg \
  -c 'reset_config srst_only srst_nogate connect_assert_srst' \
  -c init -c 'reset halt' -c 'stm32f2x mass_erase 0' -c shutdown
```

## UART

USART2 TX=PA2, RX=PA3(AF7), 115200 baud, 8N1, 흐름제어 없음입니다.
기본 보드 solder bridge의 ST-LINK VCP 연결(SB13/SB14 등)이 유지되어야 합니다.
UART 출력은 Rust 태스크가 실행한 뒤 나오며, C LED 예제는 로그를 출력하지 않습니다.
시리얼 장치 이름은 연결마다 달라집니다.

```bash
ls /dev/cu.usbmodem*
screen /dev/cu.usbmodem실제장치이름 115200
```

screen은 Ctrl-A, K, Y로 종료합니다. Hello World는 매초 재출력합니다.
Rust Blink는 시작 시 한 줄만 출력하므로 터미널 연결 뒤 NRST를 눌러 관측할 수 있습니다.
현재 TX는 짧은 메시지용 polling입니다. 인터럽트는 켜져 있지만 UART 전송 중 현재 태스크가 TXE를 기다립니다.
여러 태스크가 UART를 쓰면 mutex 또는 전용 로그 태스크가 필요합니다.

## GDB (Flash 없이 연결)

이 작업은 실행 중 보드를 정지·리셋하므로 보드 동작에 영향을 줍니다.
첫 터미널에서 서버를 실행합니다.

```bash
/opt/homebrew/bin/openocd -s /opt/homebrew/share/openocd/scripts \
  -f board/st_nucleo_f4.cfg \
  -c 'reset_config srst_only srst_nogate connect_assert_srst'
```

다른 터미널에서 이미 기록된 펌웨어와 일치하는 ELF를 선택합니다. Rust 앱도 해당 ELF 경로로 바꾸면 됩니다.

```bash
/Users/hangyeonghyeon/zephyr-sdk-1.0.1/gnu/arm-zephyr-eabi/bin/arm-zephyr-eabi-gdb \
  C/01_led_blink/build/nucleo_f446re/firmware.elf
```

GDB 안에서 실행:

```gdb
target extended-remote localhost:3333
monitor reset halt
break blink_task
continue
```

Rust는 `break rust_task`를 사용합니다. 오류로 정지했으면 Ctrl-C 후 다음을 확인하세요.

```gdb
print g_diagnostic
info registers
backtrace
```

`load`는 Flash를 기록합니다. 위 연결 명령과 VS Code launch에는 load가 없습니다.
새 ELF만 열고 기록하지 않았다면 현재 보드 코드와 심볼이 달라집니다.
SDK GDB의 Rust 표현식/pretty printer는 제한될 수 있으며 C ABI 진입점과 기계어 기준으로도 조사할 수 있습니다.
VS Code는 먼저 서버를 실행한 뒤 해당 앱 설정을 선택합니다. 

출처: [ST UM1724](https://www.st.com/resource/en/user_manual/dm00105823.pdf),
[OpenOCD Flash 명령](https://openocd.org/doc/html/Flash-Commands.html),
[OpenOCD GDB](https://openocd.org/doc/html/GDB-and-OpenOCD.html).
