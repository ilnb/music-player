# DESCRIPTION

A simple music player made in C using Raylib.

## REQUIREMENTS

raylib, ffmpeg

## INSTALLATION

mkdir bin
gcc src/main.c -o bin/mp -lraylib -lavformat -lavcodec -lavutil -lswscale -lswresample -lm -pthread

## USAGE

mp \<filename\>
