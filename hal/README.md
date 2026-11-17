# HAL 버전 (구현 예정)

STM32 HAL 기반 예제를 추가할 폴더입니다. 현재 HAL 소스와 빌드 대상은 없습니다.
기존 예제는 모두 [register/](../register/)의 레지스터 직접 제어 버전에 해당합니다.

향후 폴더 구조:

```text
hal/
├── C/          # STM32 HAL을 사용하는 C 예제
├── Rust/       # C ABI를 통해 STM32 HAL을 사용하는 Rust 예제
└── platform/   # HAL 보드 초기화 및 버전별 설정
```

FreeRTOS 커널과 CMSIS 등 외부 의존성은 루트의 `external/`에서 관리합니다.
HAL 구현 시에는 STM32 HAL 드라이버 의존성과 설정을 별도로 추가해야 합니다.
