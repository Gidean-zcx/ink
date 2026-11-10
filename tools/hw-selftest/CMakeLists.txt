# Hardware self-test for the Metalio E-Ink4 Plus (ESP32-S31), run from ota_1.
cmake_minimum_required(VERSION 3.22)
set(PROJECT_VER "1") # fixed, with APP_REPRODUCIBLE_BUILD: the image depends on the sources only
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(ink-hw-selftest)
