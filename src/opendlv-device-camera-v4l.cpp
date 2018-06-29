/*
 * Copyright (C) 2018  Christian Berger
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "cluon-complete.hpp"
#include "opendlv-standard-message-set.hpp"

#ifndef WIN32
# if !defined(__OpenBSD__) && !defined(__NetBSD__)
#  pragma GCC diagnostic push
# endif
# pragma GCC diagnostic ignored "-Weffc++"
#endif
    #include "jpgd.hpp"
#ifndef WIN32
# if !defined(__OpenBSD__) && !defined(__NetBSD__)
#  pragma GCC diagnostic pop
# endif
#endif

#include <X11/Xlib.h>

#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/select.h>
#include <unistd.h>

#include <linux/videodev2.h>

extern "C" {
    #include <libswscale/swscale.h>
}

#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>
#include <thread>

unsigned char* decompress(const unsigned char *src, const uint32_t &srcSize, int *width, int *height, int *actualBytesPerPixel, const uint32_t &requestedBytesPerPixel, bool bgr2rgb, unsigned char *pDestBuffer, int dest_buffer_size) {
    unsigned char* imageData = NULL;

    if ( (src != NULL) && 
         (srcSize > 0) && 
         (requestedBytesPerPixel > 0) ) {
        imageData = jpgd::decompress_jpeg_image_from_memory(src, srcSize, width, height, actualBytesPerPixel, requestedBytesPerPixel, bgr2rgb, pDestBuffer, dest_buffer_size);
    }
    return imageData;
}


int32_t main(int32_t argc, char **argv) {
    int32_t retCode{0};
    auto commandlineArguments = cluon::getCommandlineArguments(argc, argv);
    if ( (0 == commandlineArguments.count("camera")) || (0 == commandlineArguments.count("cid")) || (0 == commandlineArguments.count("width")) || (0 == commandlineArguments.count("height")) || (0 == commandlineArguments.count("bpp")) || (0 == commandlineArguments.count("freq")) ) {
        std::cerr << argv[0] << " interfaces with the given V4L camera (e.g., /dev/video0) and publishes it to a running OpenDaVINCI session using the OpenDLV Standard Message Set." << std::endl;
        std::cerr << "Usage:   " << argv[0] << " --camera=<V4L dev node> --cid=<OpenDaVINCI session> --width=<width> --height=<height> --bpp=<bits per pixel> [--name=<unique name for the associated shared memory>] [--id=<Identifier in case of multiple cameras>] [--verbose]" << std::endl;
        std::cerr << "         --freq:    desired bits per pixel of a frame (must be either 8 or 24)" << std::endl;
        std::cerr << "         --width:   desired width of a frame" << std::endl;
        std::cerr << "         --height:  desired height of a frame" << std::endl;
        std::cerr << "         --bpp:     desired bits per pixel of a frame (must be either 8 or 24)" << std::endl;
        std::cerr << "         --bgr2rgb: convert BGR to RGB" << std::endl;
        std::cerr << "         --name:    when omitted, '/cam0' is chosen" << std::endl;
        std::cerr << "         --verbose: when set, the raw image is displayed" << std::endl;
        std::cerr << "Example: " << argv[0] << " --cid=111 --camera=/dev/video0 --name=cam0" << std::endl;
        retCode = 1;
    }
    else {
        const uint32_t WIDTH{static_cast<uint32_t>(std::stoi(commandlineArguments["width"]))};
        const uint32_t HEIGHT{static_cast<uint32_t>(std::stoi(commandlineArguments["height"]))};
        const uint32_t BPP{static_cast<uint32_t>(std::stoi(commandlineArguments["bpp"]))};
        const float FREQ{static_cast<float>(std::stof(commandlineArguments["freq"]))};

        if ( (BPP != 24) && (BPP != 8) ) {
            std::cerr << argv[0] << ": bits per pixel must be either 24 or 8; found " << BPP << "." << std::endl;
            return retCode = 1;
        }
        if ( !(FREQ > 0) ) {
            std::cerr << argv[0] << ": freq must be larger than 0; found " << FREQ << "." << std::endl;
            return retCode = 1;
        }
        const uint32_t SIZE{WIDTH * HEIGHT * BPP/8};
        const std::string NAME{(commandlineArguments["name"].size() != 0) ? commandlineArguments["name"] : "/cam0"};
        const uint32_t ID{(commandlineArguments["id"].size() != 0) ? static_cast<uint32_t>(std::stoi(commandlineArguments["id"])) : 0};
        (void)ID;
        const bool VERBOSE{commandlineArguments.count("verbose") != 0};
        const bool BGR2RGB{commandlineArguments.count("bgr2rgb") != 0};


        int videoDevice = open(commandlineArguments["camera"].c_str(), O_RDWR);
        if (-1 == videoDevice) {
            std::cerr << argv[0] << ": Failed to open capture device: " << commandlineArguments["camera"] << std::endl;
            return retCode = 1;
        }

        struct v4l2_capability v4l2_cap;
        ::memset(&v4l2_cap, 0, sizeof(struct v4l2_capability));

        if (0 > ::ioctl(videoDevice, VIDIOC_QUERYCAP, &v4l2_cap)) {
            std::cerr << argv[0] << ": Failed to query capture device: " << commandlineArguments["camera"] << std::endl;
            return retCode = 1;
        }

        if (!(v4l2_cap.capabilities & V4L2_CAP_VIDEO_CAPTURE)) {
            std::cerr << argv[0] << ": Capture device: " << commandlineArguments["camera"] << " does not support V4L2_CAP_CAPTURE." << std::endl;
            return retCode = 1;
        }

        if (!(v4l2_cap.capabilities & V4L2_CAP_STREAMING)) {
            std::cerr << argv[0] << ": Capture device: " << commandlineArguments["camera"] << " does not support V4L2_CAP_STREAMING." << std::endl;
            return retCode = 1;
        }

        struct v4l2_format v4l2_fmt;
        ::memset(&v4l2_fmt, 0, sizeof(struct v4l2_format));

        v4l2_fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        v4l2_fmt.fmt.pix.width = WIDTH;
        v4l2_fmt.fmt.pix.height = HEIGHT;
        v4l2_fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_MJPEG;
        v4l2_fmt.fmt.pix.field = V4L2_FIELD_ANY;

        if (0 > ::ioctl(videoDevice, VIDIOC_S_FMT, &v4l2_fmt)) {
            std::cerr << argv[0] << ": Capture device: " << commandlineArguments["camera"] << " does not support requested format." << std::endl;
            return retCode = 1;
        }

        if ((v4l2_fmt.fmt.pix.width != WIDTH) ||
          (v4l2_fmt.fmt.pix.height != HEIGHT)) {
            std::cerr << argv[0] << ": Capture device: " << commandlineArguments["camera"] << " does not support requested " << WIDTH << " x " << HEIGHT << std::endl;
            return retCode = 1;
        }

        bool isMJPEG{false};
        bool isYUYV422{false};
        if (v4l2_fmt.fmt.pix.pixelformat == V4L2_PIX_FMT_MJPEG) {
            std::clog << argv[0] << ": Capture device: " << commandlineArguments["camera"] << " provides MJPEG stream." << std::endl;
            isMJPEG = true;
        }
        if (v4l2_fmt.fmt.pix.pixelformat == V4L2_PIX_FMT_YUYV) {
            std::clog << argv[0] << ": Capture device: " << commandlineArguments["camera"] << " provides YUYV 4:2:2 stream." << std::endl;
            isYUYV422 = true;
        }

        struct v4l2_streamparm v4l2_stream_parm;
        ::memset(&v4l2_stream_parm, 0, sizeof(struct v4l2_streamparm));

        v4l2_stream_parm.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        v4l2_stream_parm.parm.capture.timeperframe.numerator = 1;
        v4l2_stream_parm.parm.capture.timeperframe.denominator = static_cast<uint32_t>(FREQ);

        if (0 > ::ioctl(videoDevice, VIDIOC_S_PARM, &v4l2_stream_parm) ||
          v4l2_stream_parm.parm.capture.timeperframe.numerator != 1 ||
          v4l2_stream_parm.parm.capture.timeperframe.denominator != static_cast<uint32_t>(FREQ)) {
            std::cerr << argv[0] << ": Capture device: " << commandlineArguments["camera"] << " does not support requested " << FREQ << " fps." << std::endl;
            return retCode = 1;
        }

        constexpr uint32_t BUFFER_COUNT{30};

        struct v4l2_requestbuffers v4l2_req_bufs;
        ::memset(&v4l2_req_bufs, 0, sizeof(struct v4l2_requestbuffers));

        v4l2_req_bufs.count = BUFFER_COUNT;
        v4l2_req_bufs.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        v4l2_req_bufs.memory = V4L2_MEMORY_MMAP;

        if (0 > ::ioctl(videoDevice, VIDIOC_REQBUFS, &v4l2_req_bufs)) {
            std::cerr << argv[0] << ": Could not allocate buffers for capture device: " << commandlineArguments["camera"] << std::endl;
            return retCode = 1;
        }

        struct buffer {
            uint32_t length;
            void *buf;
        };

        struct buffer buffers[BUFFER_COUNT];


        for (uint8_t i = 0; i < BUFFER_COUNT; i++) {
            struct v4l2_buffer v4l2_buf;
            ::memset(&v4l2_buf, 0, sizeof(struct v4l2_buffer));

            v4l2_buf.index = i;
            v4l2_buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
            v4l2_buf.memory = V4L2_MEMORY_MMAP;

            if (0 > ::ioctl(videoDevice, VIDIOC_QUERYBUF, &v4l2_buf)) {
                std::cerr << argv[0] << ": Could not query buffer " << i <<  " for capture device: " << commandlineArguments["camera"] << std::endl;
                return retCode = 1;
            }

            buffers[i].length = v4l2_buf.length;

            buffers[i].buf = mmap(0, buffers[i].length, PROT_READ, MAP_SHARED, videoDevice, v4l2_buf.m.offset);
            if (MAP_FAILED == buffers[i].buf) {
                std::cerr << argv[0] << ": Could not map buffer " << i <<  " for capture device: " << commandlineArguments["camera"] << std::endl;
                return retCode = 1;
            }
        }

        for (uint8_t i = 0; i < BUFFER_COUNT; ++i) {
            struct v4l2_buffer v4l2_buf;
            memset(&v4l2_buf, 0, sizeof(struct v4l2_buffer));

            v4l2_buf.index = i;
            v4l2_buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
            v4l2_buf.memory = V4L2_MEMORY_MMAP;

            if (0 > ::ioctl(videoDevice, VIDIOC_QBUF, &v4l2_buf)) {
                std::cerr << argv[0] << ": Could not queue buffer " << i <<  " for capture device: " << commandlineArguments["camera"] << std::endl;
                return retCode = 1;
            }
        }

        int type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        if (0 > ::ioctl(videoDevice, VIDIOC_STREAMON, &type)) {
            std::cerr << argv[0] << ": Could not start video stream for capture device: " << commandlineArguments["camera"] << std::endl;
            return retCode = 1;
        }


        // Interface to a running OpenDaVINCI session (ignoring any incoming Envelopes).
        cluon::OD4Session od4{static_cast<uint16_t>(std::stoi(commandlineArguments["cid"]))};

        std::unique_ptr<cluon::SharedMemory> sharedMemory(new cluon::SharedMemory{NAME, SIZE});
        if (sharedMemory && sharedMemory->valid()) {
            std::clog << argv[0] << ": Data from camera '" << commandlineArguments["camera"]<< "' available in shared memory '" << sharedMemory->name() << "' (" << sharedMemory->size() << ")." << std::endl;

            // Define timeout for select system call.
            struct timeval timeout {};
            fd_set setOfFiledescriptorsToReadFrom{};

            struct SwsContext *yuv2rgbContext{nullptr};
            if (BGR2RGB) {
                yuv2rgbContext = sws_getContext(WIDTH, HEIGHT, AV_PIX_FMT_YUYV422, WIDTH, HEIGHT, AV_PIX_FMT_BGR24, 0, 0, 0, 0);
            }
            else {
                yuv2rgbContext = sws_getContext(WIDTH, HEIGHT, AV_PIX_FMT_YUYV422, WIDTH, HEIGHT, AV_PIX_FMT_RGB24, 0, 0, 0, 0);
            }

            struct SwsContext *rgb2rgbaContext{nullptr};
            if (BGR2RGB) {
                rgb2rgbaContext = sws_getContext(WIDTH, HEIGHT, AV_PIX_FMT_BGR24, WIDTH, HEIGHT, AV_PIX_FMT_RGB32, 0, 0, 0, 0);
            }
            else {
                rgb2rgbaContext = sws_getContext(WIDTH, HEIGHT, AV_PIX_FMT_RGB24, WIDTH, HEIGHT, AV_PIX_FMT_RGB32, 0, 0, 0, 0);
            }


            // Accessing the low-level X11 data display.
            char *imageRGBA = static_cast<char*>(malloc(WIDTH*HEIGHT*4));
            if (nullptr == imageRGBA) {
                std::cerr << argv[0] << ": Could not allocate memory for RGBA image." << std::endl;
                return retCode = 1;
            }
            Display* display{nullptr};
            Visual* visual{nullptr};
            Window window{0};
            XImage* ximage{nullptr};
            if (VERBOSE) {
                display = XOpenDisplay(NULL);
                visual = DefaultVisual(display, 0);
                window = XCreateSimpleWindow(display, RootWindow(display, 0), 0, 0, WIDTH, HEIGHT, 1, 0, 0);
                ximage = XCreateImage(display, visual, 24, ZPixmap, 0, imageRGBA, WIDTH, HEIGHT, 32, 0);
                XMapWindow(display, window);
            }

            while (od4.isRunning()) {
                timeout.tv_sec  = 1;
                timeout.tv_usec = 0;
                FD_ZERO(&setOfFiledescriptorsToReadFrom);
                FD_SET(videoDevice, &setOfFiledescriptorsToReadFrom);
                ::select(videoDevice + 1, &setOfFiledescriptorsToReadFrom, nullptr, nullptr, &timeout);
                if (FD_ISSET(videoDevice, &setOfFiledescriptorsToReadFrom)) {
                    struct v4l2_buffer v4l2_buf;
                    ::memset(&v4l2_buf, 0, sizeof(struct v4l2_buffer));

                    v4l2_buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
                    v4l2_buf.memory = V4L2_MEMORY_MMAP;

                    if (0 > ::ioctl(videoDevice, VIDIOC_DQBUF, &v4l2_buf)) {
                        std::cerr << argv[0] << ": Could not dequeue buffer for capture device: " << commandlineArguments["camera"] << std::endl;
                        return false;
                    }

                    cluon::data::TimeStamp ts;
                    ts.seconds(v4l2_buf.timestamp.tv_sec).microseconds(v4l2_buf.timestamp.tv_usec/1000);

                    const uint8_t bufferIndex = v4l2_buf.index;
                    const uint32_t bufferSize = v4l2_buf.bytesused;
                    unsigned char *bufferStart = (unsigned char *) buffers[bufferIndex].buf;

                    if (0 < bufferSize) {
                        sharedMemory->lock();

                        if (isMJPEG) {
                            int width = 0;
                            int height = 0;
                            int actualBytesPerPixel = 0;
                            int requestedBytesPerPixel = 3;
                            decompress(bufferStart, bufferSize, &width, &height, &actualBytesPerPixel, requestedBytesPerPixel, BGR2RGB, reinterpret_cast<unsigned char*>(sharedMemory->data()), sharedMemory->size());
                        }
                        if (isYUYV422) {
                            const uint8_t *const inData[1] = { bufferStart };
                            int inLinesize[1] = { static_cast<int>(WIDTH * 2 /* 2*WIDTH for YUYV 422*/) };
                            int outLinesize[1] = { static_cast<int>(WIDTH * BPP/8 /* RGB is 3 pixels */) };
                            uint8_t *dst = reinterpret_cast<uint8_t*>(sharedMemory->data());
                            sws_scale(yuv2rgbContext, inData, inLinesize, 0, HEIGHT, &dst, outLinesize);
                        }

                        if (VERBOSE && (isMJPEG || isYUYV422)) {
                            if ( (nullptr != ximage) && (nullptr != display) && (nullptr != rgb2rgbaContext) ) {
                                // First, transform to RGBA.
                                uint8_t *src = reinterpret_cast<uint8_t*>(sharedMemory->data());
                                const uint8_t *const inData[1] = { src };
                                int inLinesize[1] = { static_cast<int>(WIDTH * BPP/8 /* RGB is 3 pixels */) };
                                int outLinesize[1] = { static_cast<int>(WIDTH * (BPP/8+1) /* RGBA is 4 pixels */) };
                                uint8_t *dst = reinterpret_cast<uint8_t*>(imageRGBA);
                                sws_scale(rgb2rgbaContext, inData, inLinesize, 0, HEIGHT, &dst, outLinesize);

                                // Now, display.
                                XPutImage(display, window, DefaultGC(display, 0), ximage, 0, 0, 0, 0, WIDTH, HEIGHT);
                            }
                        }

                        sharedMemory->unlock();
                        sharedMemory->notifyAll();
                    }

                    if (0 > ::ioctl(videoDevice, VIDIOC_QBUF, &v4l2_buf)) {
                        std::cerr << argv[0] << ": Could not requeue buffer for capture device: " << commandlineArguments["camera"] << std::endl;
                        return false;
                    }
                }
            }
            if (VERBOSE) {
                XCloseDisplay(display);
            }
        }
        else {
            std::cerr << argv[0] << ": Failed to create shared memory '" << NAME << "'." << std::endl;
        }

        type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        if (0 > ::ioctl(videoDevice, VIDIOC_STREAMOFF, &type)) {
            std::cerr << argv[0] << ": Could not stop video stream for capture device: " << commandlineArguments["camera"] << std::endl;
            return retCode = 1;
        }

        for (uint8_t i = 0; i < BUFFER_COUNT; i++) {
            ::munmap(buffers[i].buf, buffers[i].length);
        }

        ::close(videoDevice);
    }
    return retCode;
}

