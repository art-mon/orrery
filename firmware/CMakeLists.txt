cmake_minimum_required(VERSION 3.16)

# Firmware version — stamped into the app descriptor so the OTA client can
# compare it against data/version.json. Bump on every tagged release.
set(PROJECT_VER "1.0.1")

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(orrery)
