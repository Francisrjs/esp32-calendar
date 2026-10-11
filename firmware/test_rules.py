"""Run constexpr assertions using the installed ESP32 toolchain."""
from pathlib import Path
import os
import subprocess

root = Path(__file__).resolve().parent
compiler = Path(os.environ.get('PLATFORMIO_CORE_DIR', Path.home()/'.platformio')) / 'packages/toolchain-xtensa-esp32s3/bin/xtensa-esp32s3-elf-g++.exe'
output = root/'.pio/rules_test.o'
output.parent.mkdir(exist_ok=True)
subprocess.run([str(compiler), '-std=c++17', '-Wall', '-Wextra', '-Werror', '-c',
                str(root/'tests/input_rules_test.cpp'), '-o', str(output)], check=True)
print('PASS: threshold boundaries, consecutive blocks, KEY grouping, debounce, rollover, Gregorian dates and alarm schedules')

subprocess.run([str(compiler), '-std=c++17', '-Wall', '-Wextra', '-Werror', '-c',
                str(root/'tests/power_rules_test.cpp'), '-o', str(root/'.pio/power_rules_test.o')], check=True)
print('PASS: absence boundaries, overnight, Sunday rollover, overlap/adjacency union, disabled/invalid schedules, continuous-week limit')
