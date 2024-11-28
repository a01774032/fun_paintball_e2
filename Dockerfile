# Use an official Ubuntu as a parent image
FROM ubuntu:20.04

# Set environment variables to avoid user interaction during package installation
ENV DEBIAN_FRONTEND=noninteractive

# Install necessary packages
RUN apt-get update && apt-get install -y \
    build-essential \
    cmake \
    libsdl2-dev \
    libsdl2-mixer-dev \
    libstdc++6 \
    libstdc++-10-dev \
    && rm -rf /var/lib/apt/lists/*

# Set the working directory
WORKDIR /usr/src/app

# Copy the current directory contents into the container at /usr/src/app
COPY . .

# Compile the juego.cpp file
RUN g++ -o juego juego.cpp -I/usr/include/SDL2 -lSDL2 -lSDL2_mixer -pthread

# Run a shell to keep the container running
CMD ["bash"]