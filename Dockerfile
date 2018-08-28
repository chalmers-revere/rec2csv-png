# Copyright (C) 2018  Christian Berger
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <http://www.gnu.org/licenses/>.

# Part to build rec2csv+png.
FROM ubuntu:18.04 as builder
MAINTAINER Christian Berger "christian.berger@gu.se"
RUN apt-get update -y && \
    apt-get upgrade -y && \
    apt-get dist-upgrade -y && \
    apt-get install -y --no-install-recommends \
        ca-certificates \
        cmake \
        build-essential \
        git \
        libx11-dev \
        nasm \
        wget
RUN cd tmp && \
    git clone --depth 1 https://chromium.googlesource.com/libyuv/libyuv && \
    cd libyuv &&\
    make -f linux.mk libyuv.a && cp libyuv.a /usr/lib/x86_64-linux-gnu && cd include && cp -r * /usr/include
RUN cd tmp && \
    git clone --depth 1 --branch v1.7.0 https://github.com/webmproject/libvpx.git && \
    mkdir build && cd build && \
    ../libvpx/configure --disable-docs --disable-tools --enable-vp8 --enable-vp9 --enable-libyuv --disable-unit-tests --disable-webm-io --disable-postproc && \
    make -j4 && make install
RUN cd tmp && \
    git clone --depth 1 --branch v1.8.0 https://github.com/cisco/openh264.git && \
    cd openh264 && mkdir b && cd b \
    make -j2 -f ../Makefile libraries && make -f ../Makefile install
ADD . /opt/sources
WORKDIR /opt/sources
RUN mkdir build && \
    cd build && \
    cmake -D CMAKE_BUILD_TYPE=Release -D CMAKE_INSTALL_PREFIX=/tmp .. && \
    make && make install
RUN cd /tmp && wget http://ciscobinary.openh264.org/libopenh264-1.8.0-linux64.4.so.bz2


# Part to deploy rec2csv+png.
FROM ubuntu:18.04
MAINTAINER Christian Berger "christian.berger@gu.se"

RUN apt-get update -y && \
    apt-get upgrade -y && \
    apt-get dist-upgrade -y && \
    apt-get install -y --no-install-recommends \
        bc \
        libx11-6 \
        mjpegtools \
        vpx-tools && \
    apt-get clean

WORKDIR /usr/lib/x86_64-linux-gnu
COPY --from=builder /tmp/libopenh264-1.8.0-linux64.4.so.bz2 .
RUN bunzip2 libopenh264-1.8.0-linux64.4.so.bz2 && \
    ln -sf libopenh264-1.8.0-linux64.4.so libopenh264.so.4

ADD generate_webm.sh /usr/bin

WORKDIR /usr/bin
COPY --from=builder /tmp/bin/rec2csv+png .
ENTRYPOINT ["/usr/bin/rec2csv+png"]

