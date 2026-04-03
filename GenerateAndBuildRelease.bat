@echo off
call conan install . --build=missing -pr=release20
echo Install Successful
call cmake --preset=conan-default
echo Cmake Preset Successful
call cmake --build --preset=conan-release
echo Build Successful

pause