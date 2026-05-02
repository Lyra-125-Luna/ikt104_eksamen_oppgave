
# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.20.0)
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(blinky)

target_sources(app PRIVATE
        src/main.c
        src/rgb_lcd.c
        src/screen.c
        src/tem_hum.c
        src/network_config.c
        )
