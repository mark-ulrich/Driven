@echo off

mkdir build
pushd build

cl /std:c11 /FC /Zi ..\src\main.c

popd
