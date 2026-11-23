/* ST startup은 __libc_init_array()를 호출한다.
 * newlib의 -nostartfiles 링크에서 필요한 빈 C 런타임 초기화/종료 함수.
 * 주변장치/RTOS 처리는 없다. CubeIDE 생성 syscalls.c/sysmem.c는 필요하지 않다.
 */
void _init(void) { }
void _fini(void) { }

#include <stddef.h>
#include <errno.h>
/* RTOS 메모리는 heap_4만 사용한다. libc가 SRAM/MSP 쪽으로 무제한 확장하지
 * 않도록 C heap은 제공하지 않는다. 예제는 정수 snprintf만 사용한다.
 * 부동소수점 printf/malloc이 필요한 앱은 별도의 bounded heap 설계가 필요하다.
 */
void *_sbrk(ptrdiff_t increment)
{
    (void)increment;
    errno = ENOMEM;
    return (void *)-1;
}
