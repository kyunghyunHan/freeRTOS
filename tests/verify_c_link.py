import pathlib
import re
import struct
import subprocess
import tempfile

ROOT = pathlib.Path(__file__).resolve().parents[1]
CACHE = ROOT / '.cache/verification'
SOURCE = '''#include <stdint.h>
#include "FreeRTOS.h"
#include "task.h"

static void task(void *argument)
{
    (void)argument;
    for (;;) {
        *(volatile uint32_t *)0x40020018 = 1U << 5;
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

int main(void)
{
    *(volatile uint32_t *)0x40023830 |= 1U;
    (void)*(volatile uint32_t *)0x40023830;
    *(volatile uint32_t *)0x40020000 =
        (*(volatile uint32_t *)0x40020000 & ~(3U << 10)) | (1U << 10);
    if (xTaskCreate(task, "task", 256, 0, 1, 0) != pdPASS)
        for (;;) {}
    vTaskStartScheduler();
    for (;;) {}
}
'''


def run(*args):
    result = subprocess.run(args, text=True, capture_output=True)
    if result.returncode:
        raise RuntimeError(result.stdout + result.stderr)
    return result.stdout


def tool(build, name):
    config = next(build.glob('CMakeFiles/*/CMakeCCompiler.cmake')).read_text()
    compiler = re.search(r'set\(CMAKE_C_COMPILER "([^"]+)"\)', config)[1]
    return compiler.removesuffix('gcc') + name


def inspect(build, override=False, disabled=False):
    elf = build / 'firmware.elf'
    symbols = {}
    for line in run(tool(build, 'nm'), str(elf)).splitlines():
        match = re.match(r'([0-9a-f]+) ([A-Za-z]) (\S+)$', line)
        if match:
            symbols[match[3]] = (int(match[1], 16), match[2])
    assert not run(tool(build, 'nm'), '-u', str(elf)).strip()
    for suffix in ('elf', 'hex', 'bin'):
        assert (build / ('firmware.' + suffix)).stat().st_size
    for name in ('firmware_assert', 'firmware_fail'):
        assert symbols[name][1] == 'W', (name, symbols.get(name))
    if not disabled:
        assert symbols['vApplicationMallocFailedHook'][1] == ('T' if override else 'W')
        assert symbols['vApplicationStackOverflowHook'][1] == 'W'
    else:
        assert 'vApplicationMallocFailedHook' not in symbols
        assert 'vApplicationStackOverflowHook' not in symbols
    image = (build / 'firmware.bin').read_bytes()
    assert struct.unpack_from('<I', image)[0] == 0x20020000
    for index, name in ((3, 'HardFault_Handler'), (11, 'SVC_Handler'),
                        (14, 'PendSV_Handler'), (15, 'SysTick_Handler')):
        assert struct.unpack_from('<I', image, index * 4)[0] == symbols[name][0] | 1
        assert symbols[name][0] != symbols['Default_Handler'][0]
    for name in ('SVC_Handler', 'PendSV_Handler', 'SysTick_Handler'):
        assert symbols[name][1] == 'T'
    offset = symbols['__bothinit_array_start'][0] - 0x08000000
    end = symbols['__bothinit_array_end'][0] - 0x08000000
    pointers = struct.unpack('<' + 'I' * ((end - offset) // 4), image[offset:end])
    assert symbols['freertos_runtime_init'][0] | 1 in pointers
    assert 'runtime_init' in symbols
    code = run(tool(build, 'objdump'), '-d', '--disassemble=__wrap_vTaskStartScheduler', str(elf))
    assert code.index('<SystemCoreClockUpdate>') < code.index('<vTaskStartScheduler>')
    code = run(tool(build, 'objdump'), '-d', '--disassemble=main', str(elf))
    assert '<__wrap_vTaskStartScheduler>' in code
    attrs = run(tool(build, 'readelf'), '-A', str(elf))
    assert 'Tag_ABI_VFP_args: VFP registers' in attrs
    assert 'Tag_FP_arch: VFPv4-D16' in attrs
    assert 'Tag_CPU_name: "7E-M"' in attrs
    assert '_init' not in symbols and '_fini' not in symbols


def main():
    CACHE.mkdir(parents=True, exist_ok=True)
    for variant in ('no_hooks', 'partial_override', 'hooks_disabled'):
        with tempfile.TemporaryDirectory(prefix=variant + '-', dir=CACHE) as directory:
            fixture = pathlib.Path(directory)
            build = fixture / 'build'
            source = SOURCE
            if variant == 'partial_override':
                source += '\nvoid vApplicationMallocFailedHook(void) { for (;;) {} }\n'
            (fixture / 'main.c').write_text(source)
            config = (ROOT / 'cmake/FreeRTOSConfig.h').read_text()
            if variant == 'hooks_disabled':
                config = config.replace('configUSE_MALLOC_FAILED_HOOK 1', 'configUSE_MALLOC_FAILED_HOOK 0')
                config = config.replace('configCHECK_FOR_STACK_OVERFLOW 2', 'configCHECK_FOR_STACK_OVERFLOW 0')
            (fixture / 'FreeRTOSConfig.h').write_text(config)
            (fixture / 'CMakeLists.txt').write_text(f'''cmake_minimum_required(VERSION 3.24)
project(link_fixture C ASM)
set(FIRMWARE_LINKER_SCRIPT "{ROOT}/cmake/stm32f446re.ld" CACHE FILEPATH "")
include("{ROOT}/cmake/firmware.cmake")
target_sources(firmware PRIVATE main.c)
''')
            log = run('cmake', '-S', str(fixture), '-B', str(build), '-G', 'Ninja',
                      '-DCMAKE_TOOLCHAIN_FILE=' + str(ROOT / 'cmake/arm-gcc.cmake'),
                      '-DCMAKE_BUILD_TYPE=Debug')
            log += run('cmake', '--build', str(build))
            (CACHE / (variant + '.log')).write_text(log)
            inspect(build, variant == 'partial_override', variant == 'hooks_disabled')
            print(variant + ': PASS (link, vectors, preinit, clock refresh, hard-float, no _init/_fini)')


if __name__ == '__main__':
    main()
