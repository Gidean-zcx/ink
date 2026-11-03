# Phase-1 measurement firmware for the Metalio E-Ink4 Plus (ESP32-S31). See README.md.
cmake_minimum_required(VERSION 3.22)
set(PROJECT_VER "1") # fixed, with APP_REPRODUCIBLE_BUILD: the image hash depends on the sources only
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(ink-hw-probe)
