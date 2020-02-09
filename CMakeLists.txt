cmake_minimum_required(VERSION 3.15)
project(NESEmulator)

set(CMAKE_CXX_STANDARD 14)

add_executable(NESEmulator main.cpp Bus.cpp Bus.h CPU.cpp CPU.h armTDI.cpp armTDI.h)