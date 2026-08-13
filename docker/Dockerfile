ARG UBUNTU_VERSION="26.04"

###################################
# Build qemu plugin
#
FROM ubuntu:$UBUNTU_VERSION AS plugin-build

# enable sources
RUN sed -i 's/^Types: deb$/Types: deb deb-src/' /etc/apt/sources.list.d/ubuntu.sources

# install dependencies and qemu source code.
# note that this all has to be done in a single step to make sure the sources
# match the version of the installed qemu
RUN apt-get update && DEBIAN_FRONTEND=noninteractive apt-get install --yes \
    qemu-system-arm build-essential pkg-config libglib2.0-dev dpkg-dev \
    && mkdir /build && cd /build && apt-get source qemu \
    && apt-get clean && rm -rf /var/lib/apt/lists/* /tmp/* /var/tmp/*

# build the plugin
RUN --mount=type=bind,source=bench-plugin.c,target=/build/bench-plugin.c \
    gcc \
    -std=c11 \
    -O2 \
    -Wall \
    -Wextra \
    -shared \
    -fPIC \
    -fvisibility=hidden \
    $(pkg-config --cflags glib-2.0) \
    -I /build/qemu-*+ds/include/qemu \
    /build/bench-plugin.c \
    -o /build/bench-plugin.so


###################################
# Build final system
#
FROM ubuntu:$UBUNTU_VERSION

LABEL org.opencontainers.image.source=https://github.com/Finomnis/qemu-embedded-bench

# install dependencies
RUN apt-get update && DEBIAN_FRONTEND=noninteractive apt-get install --yes \
    qemu-system-arm llvm \
    && apt-get clean && rm -rf /var/lib/apt/lists/* /tmp/* /var/tmp/*

COPY --from=plugin-build /build/bench-plugin.so /bench-plugin.so

COPY run.sh /usr/bin/run.sh

ENTRYPOINT ["/bin/bash", "/usr/bin/run.sh"]
