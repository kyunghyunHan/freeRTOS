# HAL 버전 (구현 예정)

STM32 HAL 기반 예제를 추가할 폴더입니다. 현재 HAL 소스와 빌드 대상은 없습니다.
기존 예제는 모두 [register/](../register/)의 레지스터 직접 제어 버전입니다.

HAL 예제는 향후 `C/`, `Rust/` 아래에 추가합니다.
FreeRTOS 커널과 CMSIS 등 외부 의존성은 루트의 `external/`에서 관리합니다.
HAL 구현 시에는 STM32 HAL 드라이버 의존성과 설정을 별도로 추가해야 합니다.
