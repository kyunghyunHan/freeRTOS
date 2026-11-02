# 의존성과 복원

커널·CMSIS 버전은 `external/dependencies.lock.json`에 고정합니다.
보드 초기화는 `platform/board.c`, 장치 헤더와 startup은 ST CMSIS를 사용합니다.

| 폴더 | 공식 출처 | 태그 | commit |
|---|---|---|---|
| FreeRTOS-Kernel | [FreeRTOS-Kernel](https://github.com/FreeRTOS/FreeRTOS-Kernel) | V11.2.0 | `0adc196d4bd52a2d91102b525b0aafc1e14a2386` |
| cmsis-device-f4 | [cmsis-device-f4](https://github.com/STMicroelectronics/cmsis-device-f4) | v2.6.10 | `5f41fb29d22773896c780052bf61e47fc924d524` |
| CMSIS_5 | [CMSIS_5](https://github.com/ARM-software/CMSIS_5) | 5.9.0 | `2b7495b8535bdcb306dac29b9ded4cfb679d7e5c` |

`external/`의 소스가 없을 때 프로젝트 루트에서 복원합니다.
빈 대상 폴더를 사용하고, SHA256 검사가 OK인 아카이브만 압축 해제하세요.

```bash
cd /Users/hangyeonghyeon/Desktop/Rust/hardware/freeRTOS
mkdir -p .cache/downloads
```

```bash
curl -fL --retry 2 'https://codeload.github.com/FreeRTOS/FreeRTOS-Kernel/tar.gz/0adc196d4bd52a2d91102b525b0aafc1e14a2386' -o .cache/downloads/FreeRTOS-Kernel.tar.gz
printf '%s  %s\n' '6af5d78e39a5237b63faaafdc31672c87b27b68e713e2a57d3528aa3a4c1cee8' '.cache/downloads/FreeRTOS-Kernel.tar.gz' | shasum -a 256 -c -
```

위 검사가 OK인 경우에만 실행합니다.

```bash
mkdir -p external/FreeRTOS-Kernel
tar -xzf .cache/downloads/FreeRTOS-Kernel.tar.gz -C external/FreeRTOS-Kernel --strip-components=1
```

```bash
curl -fL --retry 2 'https://codeload.github.com/STMicroelectronics/cmsis-device-f4/tar.gz/5f41fb29d22773896c780052bf61e47fc924d524' -o .cache/downloads/cmsis-device-f4.tar.gz
printf '%s  %s\n' '7b70f5e3613eac5472426b206765620aa4192d96df1f21a3a5d84ea999f70c41' '.cache/downloads/cmsis-device-f4.tar.gz' | shasum -a 256 -c -
```

위 검사가 OK인 경우에만 실행합니다.

```bash
mkdir -p external/cmsis-device-f4
tar -xzf .cache/downloads/cmsis-device-f4.tar.gz -C external/cmsis-device-f4 --strip-components=1
```

```bash
curl -fL --retry 2 'https://codeload.github.com/ARM-software/CMSIS_5/tar.gz/5.9.0' -o .cache/downloads/CMSIS_5.tar.gz
printf '%s  %s\n' '9ce693dd15c41f02506adda79323c077912b3211303ca0d9b28b8ebe735a7031' '.cache/downloads/CMSIS_5.tar.gz' | shasum -a 256 -c -
```

위 검사가 OK인 경우에만 실행합니다.

```bash
mkdir -p external/CMSIS_5
tar -xzf .cache/downloads/CMSIS_5.tar.gz -C external/CMSIS_5 --strip-components=1
```

커널은 MIT, CMSIS_5는 Apache-2.0, ST 장치 패키지는 저장소 LICENSE.md와 파일별 고지를 따릅니다.
재배포할 때 원본 라이선스 파일을 함께 보존하세요.

Rust는 외부 crate가 없으며 `platform/rust-api` path 의존성만 사용합니다.
두 앱의 Cargo.lock을 추적하고 `--locked --offline`으로 빌드합니다.
도구 버전·SDK 출처와 Rust 정확 버전 선택은 [설치 문서](setup.md)에 있습니다.
