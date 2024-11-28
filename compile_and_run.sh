#!/bin/bash

# Script to compile and run the game

# Compilation command
g++ -std=c++11 juego.cpp -o juego -lncurses \
    -I/opt/homebrew/opt/sdl2/include/SDL2 \
    -I/opt/homebrew/opt/sdl2_mixer/include/SDL2 \
    -L/opt/homebrew/opt/sdl2/lib \
    -L/opt/homebrew/opt/sdl2_mixer/lib \
    -L/opt/homebrew/lib \
    -lSDL2 -lSDL2_mixer

# Check if compilation was successful
if [ $? -eq 0 ]; then
    echo "Compilation successful! Running the game..."
    ./juego
else
    echo "Compilation failed. Please check your code for errors."
fi