@echo off

mkdir build
pushd build

cl -FC -Zi ..\src\main.c

popd
