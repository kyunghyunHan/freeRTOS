#!/usr/bin/env python3
"""Build all 25 standalone examples and check their actual ELF vector entries.
Run from any directory. --arm-gcc-bin selects STM32CubeIDE GCC/newlib.
No board is accessed. Logs and reports stay under the ignored build directory.
"""
import argparse
import json
from pathlib import Path
import re
import struct
import shlex
import subprocess

ROOT = Path(__file__).resolve().parents[1]
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--arm-gcc-bin', type=Path)
args = parser.parse_args()
variant = 'cube' if args.arm_gcc_bin else 'sdk'
out = ROOT / 'example' / 'build' / variant
out.mkdir(parents=True, exist_ok=True)
reports = []


def run(cmd):
    result = subprocess.run(list(map(str, cmd)), cwd=ROOT, text=True,
                            stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    if result.returncode:
        raise RuntimeError(result.stdout)
    return result.stdout


for number in range(1, 26):
    app = f'example{number:02}'
    build = out / app
    cmd = ['cmake', '-S', ROOT / 'example', '-B', build, '-G', 'Ninja',
           f'-DAPP={app}', '-DCMAKE_BUILD_TYPE=Debug']
    if args.arm_gcc_bin:
        cmd += [f'-DCMAKE_TOOLCHAIN_FILE={ROOT}/example/arm-none-eabi.cmake',
                f'-DARM_GCC_BIN={args.arm_gcc_bin}']
    else:
        cmd += [f'-DCMAKE_TOOLCHAIN_FILE={ROOT}/cmake/arm-gcc.cmake']
    log = run(cmd) + run(['cmake', '--build', build])
    (build / 'verification.log').write_text(log)
    commands = json.loads((build / 'compile_commands.json').read_text())
    compiler = Path(shlex.split(commands[0]['command'])[0])
    prefix = str(compiler).removesuffix('gcc')
    elf = build / 'firmware.elf'
    nm = run([prefix + 'nm', '-n', elf])
    symbols = {m[2]: (int(m[0], 16), m[1]) for m in
               re.findall(r'^([0-9a-f]+)\s+(\w)\s+(\S+)$', nm, re.MULTILINE)}
    vector = build / 'vectors.bin'
    run([prefix + 'objcopy', '--dump-section', f'.isr_vector={vector}', elf])
    data = vector.read_bytes()
    for slot, name in [(1, 'Reset_Handler'), (11, 'SVC_Handler'),
                       (14, 'PendSV_Handler'), (15, 'SysTick_Handler')]:
        address, kind = symbols[name]
        assert struct.unpack_from('<I', data, slot * 4)[0] == address | 1, (app, name)
        if slot != 1:
            assert kind == 'T', (app, name, kind)
    assert struct.unpack_from('<I', data)[0] == 0x20030000
    assert symbols['main'][1] == 'T'
    assert symbols['SystemCoreClock'][1] in ('B', 'D')
    if number in (16, 17, 18, 19, 24, 25):
        address, kind = symbols['EXTI15_10_IRQHandler']
        assert kind == 'T'
        assert struct.unpack_from('<I', data, (16 + 40) * 4)[0] == address | 1
    if number == 7:
        assert symbols['vApplicationIdleHook'][1] == 'T'
    assert not re.search(r'\b(?:HAL_\w+|osKernelStart|osThreadNew|ex_log|example_start)\b', nm)
    for ext in ('hex', 'bin'):
        assert (build / f'firmware.{ext}').stat().st_size > 0
    source = (ROOT / 'example' / f'{app}.c').read_text()
    assert not re.search(r'configASSERT\([^;]*\b(?:xTaskCreate|xQueueSend|xTimerStart|xTimerReset|xSemaphoreGive)\(', source)
    size = run([prefix + 'size', elf]).splitlines()[-1].split()[:3]
    reports.append(dict(example=app, result='PASS', text=int(size[0]), data=int(size[1]), bss=int(size[2])))
    print(f'{variant}/{app}: PASS (build, vectors, symbols, ELF/HEX/BIN)', flush=True)
(out / 'verification.json').write_text(json.dumps(reports, indent=2) + '\n')
print(f'{len(reports)}/25 passed; report: {out}/verification.json')
