@echo off
call conan install . --build=missing -pr=debug20
echo Install Successful
call cmake --preset=conan-default
echo Cmake Preset Successful
call cmake --build --preset=conan-debug
echo Build Successful

pause