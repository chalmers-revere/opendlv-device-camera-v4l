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

# Part to build opendlv-device-camera-v4l.
FROM pipill/armhf-alpine:edge as builder
MAINTAINER Christian Berger "christian.berger@gu.se"

RUN [ "cross-build-start" ]

RUN echo http://dl-4.alpinelinux.org/alpine/v3.8/main > /etc/apk/repositories && \
    echo http://dl-4.alpinelinux.org/alpine/v3.8/community >> /etc/apk/repositories && \
    apk update && \
    apk --no-cache add \
        cmake \
        g++ \
        git \
        libx11-dev \
        linux-headers \
        make \
        nasm \
        wget
RUN cd /tmp && \
    wget https://github.com/libjpeg-turbo/libjpeg-turbo/archive/1.5.90.tar.gz && \
    tar xvzf 1.5.90.tar.gz && cd libjpeg-turbo-1.5.90 && mkdir build && cd build && \
    cmake -D CMAKE_INSTALL_PREFIX=/usr -DENABLE_SHARED=0 .. && \
    make -j4 && make install
RUN cd /tmp && \
    git clone --depth 1 https://chromium.googlesource.com/libyuv/libyuv && \
    cd libyuv &&\
    make -f linux.mk CFLAGS="-O2 -fomit-frame-pointer -Iinclude/ -DHAVE_JPEG" CXXFLAGS="-O2 -fomit-frame-pointer -Iinclude/ -DHAVE_JPEG" libyuv.a && cp libyuv.a /usr/lib && cd include && cp -r * /usr/include
ADD . /opt/sources
WORKDIR /opt/sources
RUN mkdir build && \
    cd build && \
    cmake -D CMAKE_BUILD_TYPE=Release -D CMAKE_INSTALL_PREFIX=/tmp .. && \
    make && make install

RUN [ "cross-build-end" ]


# Part to deploy opendlv-device-camera-v4l.
FROM pipill/armhf-alpine:edge
MAINTAINER Christian Berger "christian.berger@gu.se"

RUN [ "cross-build-start" ]

RUN echo http://dl-4.alpinelinux.org/alpine/v3.8/main > /etc/apk/repositories && \
    echo http://dl-4.alpinelinux.org/alpine/v3.8/community >> /etc/apk/repositories && \
    apk update && \
    apk --no-cache add \
        libx11

RUN [ "cross-build-end" ]

WORKDIR /usr/bin
COPY --from=builder /tmp/bin/opendlv-device-camera-v4l .
ENV NO_AT_BRIDGE=1
ENTRYPOINT ["/usr/bin/opendlv-device-camera-v4l"]

