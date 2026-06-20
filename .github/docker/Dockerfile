FROM ubuntu:24.04

RUN apt-get update && apt-get install -y \
        ninja-build \
        g++ \
        git \
        qt6-base-dev \
        qt6-tools-dev \
        qt6-tools-dev-tools \
        qt6-multimedia-dev \
    && rm -rf /var/lib/apt/lists/*
