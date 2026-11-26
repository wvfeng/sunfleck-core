FROM ubuntu:24.04
RUN apt-get update && apt-get install -y --no-install-recommends build-essential cmake ninja-build pkgconf libx11-dev libxext-dev libxtst6 xvfb xauth xdotool libfreetype-dev fonts-dejavu-core fonts-noto-cjk && rm -rf /var/lib/apt/lists/*
WORKDIR /src
