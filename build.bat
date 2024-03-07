@echo off

clang++ -std=c++17 -O3 src/main.cpp src/utility/perfTimer.cpp -o cli
cli.exe
