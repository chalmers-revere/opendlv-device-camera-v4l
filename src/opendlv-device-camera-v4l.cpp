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

#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/select.h>
#include <time.h>
#include <unistd.h>

#include <linux/videodev2.h>
#include <X11/Xlib.h>
#include <libyuv.h>

#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>
#include <thread>

int32_t main(int32_t argc, char **argv) {
    int32_t retCode{0};
    auto commandlineArguments = cluon::getCommandlineArguments(argc, argv);
    if ( (0 == commandlineArguments.count("camera")) ||
         (0 == commandlineArguments.count("width")) ||
         (0 == commandlineArguments.count("height")) ||
         (0 == commandlineArguments.count("freq")) ) {
        std::cerr << argv[0] << " interfaces with the given V4L camera (e.g., /dev/video0) and provides the captured image in two shared memory areas: one in I420 format and one in ARGB format." << std::endl;
        std::cerr << "Usage:   " << argv[0] << " --camera=<V4L dev node> --width=<width> --height=<height> [--name.i420=<unique name for the shared memory in I420 format>] [--name.argb=<unique name for the shared memory in ARGB format>] [--verbose]" << std::endl;
        std::cerr << "         --camera:    V4L camera device node to be used" << std::endl;
        std::cerr << "         --name.i420: name of the shared memory for the I420 formatted image; when omitted, the last part of the device node + '.i420' is chosen" << std::endl;
        std::cerr << "         --name.argb: name of the shared memory for the I420 formatted image; when omitted, the last part of the device node + '.argb' is chosen" << std::endl;
        std::cerr << "         --width:     desired width of a frame" << std::endl;
        std::cerr << "         --height:    desired height of a frame" << std::endl;
        std::cerr << "         --freq:      desired frame rate" << std::endl;
        std::cerr << "         --verbose:   display captured image" << std::endl;
        std::cerr << "Example: " << argv[0] << " --camera=/dev/video0 --width=640 --height=480 --freq=20 --verbose" << std::endl;
        retCode = 1;
    }
    else {
        const uint32_t WIDTH{static_cast<uint32_t>(std::stoi(commandlineArguments["width"]))};
        const uint32_t HEIGHT{static_cast<uint32_t>(std::stoi(commandlineArguments["height"]))};
        const bool VERBOSE{commandlineArguments.count("verbose") != 0};

        const float FREQ{static_cast<float>(std::stof(commandlineArguments["freq"]))};
        if ( !(FREQ > 0) ) {
            std::cerr << "[opendlv-device-camera-v4l]: freq must be larger than 0; found " << FREQ << "." << std::endl;
            return retCode = 1;
        }

        // Set up the names for the shared memory areas.
        std::string NAME_I420{commandlineArguments["camera"]};
        auto pos = NAME_I420.find_last_of('/');
        std::string NAME_ARGB = NAME_I420.substr(pos+1);
        NAME_I420 = NAME_ARGB + ".i420";
        if ((commandlineArguments["name.i420"].size() != 0)) {
            NAME_I420 = commandlineArguments["name.i420"];
        }
        NAME_ARGB += ".argb";
        if ((commandlineArguments["name.argb"].size() != 0)) {
            NAME_ARGB = commandlineArguments["name.argb"];
        }

        // V4L initialization.
        int videoDevice = open(commandlineArguments["camera"].c_str(), O_RDWR);
        if (-1 == videoDevice) {
            std::cerr << "[opendlv-device-camera-v4l]: Failed to open capture device: " << commandlineArguments["camera"] << std::endl;
            return retCode = 1;
        }

        struct v4l2_capability v4l2_cap;
        ::memset(&v4l2_cap, 0, sizeof(struct v4l2_capability));
        if (0 > ::ioctl(videoDevice, VIDIOC_QUERYCAP, &v4l2_cap)) {
            std::cerr << "[opendlv-device-camera-v4l]: Failed to query capture device: " << commandlineArguments["camera"] << ", error: " << errno << ": " << strerror(errno) << std::endl;
            return retCode = 1;
        }
        if (!(v4l2_cap.capabilities & V4L2_CAP_VIDEO_CAPTURE)) {
            std::cerr << "[opendlv-device-camera-v4l]: Capture device: " << commandlineArguments["camera"] << " does not support V4L2_CAP_CAPTURE." << std::endl;
            return retCode = 1;
        }
        if (!(v4l2_cap.capabilities & V4L2_CAP_STREAMING)) {
            std::cerr << "[opendlv-device-camera-v4l]: Capture device: " << commandlineArguments["camera"] << " does not support V4L2_CAP_STREAMING." << std::endl;
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
            std::cerr << "[opendlv-device-camera-v4l]: Capture device: " << commandlineArguments["camera"] << " does not support requested format." << ", error: " << errno << ": " << strerror(errno) << std::endl;
            return retCode = 1;
        }

        if ((v4l2_fmt.fmt.pix.width != WIDTH) ||
          (v4l2_fmt.fmt.pix.height != HEIGHT)) {
            std::cerr << "[opendlv-device-camera-v4l]: Capture device: " << commandlineArguments["camera"] << " does not support requested " << WIDTH << " x " << HEIGHT << std::endl;
            return retCode = 1;
        }

        bool isMJPEG{false};
        bool isYUYV422{false};
        if (v4l2_fmt.fmt.pix.pixelformat == V4L2_PIX_FMT_MJPEG) {
            std::clog << "[opendlv-device-camera-v4l]: Capture device: " << commandlineArguments["camera"] << " provides MJPEG stream." << std::endl;
            isMJPEG = true;
        }
        if (v4l2_fmt.fmt.pix.pixelformat == V4L2_PIX_FMT_YUYV) {
            std::clog << "[opendlv-device-camera-v4l]: Capture device: " << commandlineArguments["camera"] << " provides YUYV 4:2:2 stream." << std::endl;
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
            std::cerr << "[opendlv-device-camera-v4l]: Capture device: " << commandlineArguments["camera"] << " does not support requested " << FREQ << " fps." << std::endl;
            return retCode = 1;
        }

        const uint32_t BUFFER_COUNT{32};

        struct v4l2_requestbuffers v4l2_req_bufs;
        ::memset(&v4l2_req_bufs, 0, sizeof(struct v4l2_requestbuffers));
        v4l2_req_bufs.count = BUFFER_COUNT;
        v4l2_req_bufs.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        v4l2_req_bufs.memory = V4L2_MEMORY_MMAP;
        if (0 > ::ioctl(videoDevice, VIDIOC_REQBUFS, &v4l2_req_bufs)) {
            std::cerr << "[opendlv-device-camera-v4l]: Could not allocate buffers for capture device: " << commandlineArguments["camera"] << ", error: " << errno << ": " << strerror(errno) << std::endl;
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
                std::cerr << "[opendlv-device-camera-v4l]: Could not query buffer " << +i <<  " for capture device: " << commandlineArguments["camera"] << ", error: " << errno << ": " << strerror(errno) << std::endl;
                return retCode = 1;
            }

            buffers[i].length = v4l2_buf.length;

            buffers[i].buf = mmap(0, buffers[i].length, PROT_READ, MAP_SHARED, videoDevice, v4l2_buf.m.offset);
            if (MAP_FAILED == buffers[i].buf) {
                std::cerr << "[opendlv-device-camera-v4l]: Could not map buffer " << +i <<  " for capture device: " << commandlineArguments["camera"] << ", error: " << errno << ": " << strerror(errno) << std::endl;
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
                std::cerr << "[opendlv-device-camera-v4l]: Could not queue buffer " << +i <<  " for capture device: " << commandlineArguments["camera"] << ", error: " << errno << ": " << strerror(errno) << std::endl;
                return retCode = 1;
            }
        }

        int type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        if (0 > ::ioctl(videoDevice, VIDIOC_STREAMON, &type)) {
            std::cerr << "[opendlv-device-camera-v4l]: Could not start video stream for capture device: " << commandlineArguments["camera"] << ", error: " << errno << ": " << strerror(errno) << std::endl;
            return retCode = 1;
        }

        std::unique_ptr<cluon::SharedMemory> sharedMemoryI420(new cluon::SharedMemory{NAME_I420, WIDTH * HEIGHT * 3/2});
        if (!sharedMemoryI420 || !sharedMemoryI420->valid()) {
            std::cerr << "[opendlv-device-camera-v4l]: Failed to create shared memory '" << NAME_I420 << "'." << std::endl;
            return retCode = 1;
        }

        std::unique_ptr<cluon::SharedMemory> sharedMemoryARGB(new cluon::SharedMemory{NAME_ARGB, WIDTH * HEIGHT * 4});
        if (!sharedMemoryARGB || !sharedMemoryARGB->valid()) {
            std::cerr << "[opendlv-device-camera-v4l]: Failed to create shared memory '" << NAME_ARGB << "'." << std::endl;
            return retCode = 1;
        }

        if ( (sharedMemoryI420 && sharedMemoryI420->valid()) &&
             (sharedMemoryARGB && sharedMemoryARGB->valid()) ) {
            std::clog << "[opendlv-device-camera-v4l]: Data from camera '" << commandlineArguments["camera"]<< "' available in I420 format in shared memory '" << sharedMemoryI420->name() << "' (" << sharedMemoryI420->size() << ") and in ARGB format in shared memory '" << sharedMemoryARGB->name() << "' (" << sharedMemoryARGB->size() << ")." << std::endl;

            int64_t uptimeToEpochOffsetIn_ms{0};
            {
                struct timeval epochTime{};
                gettimeofday(&epochTime, nullptr);

                struct timespec upTime{};
                clock_gettime(CLOCK_MONOTONIC, &upTime);

                int64_t uptimeIn_ms = upTime.tv_sec * 1000 + static_cast<int64_t>(round(upTime.tv_nsec/(1000.0f*1000.0f)));
                int64_t epochTimeIn_ms =  epochTime.tv_sec * 1000 + static_cast<int64_t>(round(epochTime.tv_usec/1000.0f));
                uptimeToEpochOffsetIn_ms = epochTimeIn_ms - uptimeIn_ms;
            }

            // Define timeout for select system call.
            struct timeval timeout {};
            fd_set setOfFiledescriptorsToReadFrom{};

            // Accessing the low-level X11 data display.
            Display* display{nullptr};
            Visual* visual{nullptr};
            Window window{0};
            XImage* ximage{nullptr};
            if (VERBOSE) {
                display = XOpenDisplay(NULL);
                visual = DefaultVisual(display, 0);
                window = XCreateSimpleWindow(display, RootWindow(display, 0), 0, 0, WIDTH, HEIGHT, 1, 0, 0);
                sharedMemoryARGB->lock();
                {
                    ximage = XCreateImage(display, visual, 24, ZPixmap, 0, sharedMemoryARGB->data(), WIDTH, HEIGHT, 32, 0);
                }
                sharedMemoryARGB->unlock();
                XMapWindow(display, window);
            }

            while (!cluon::TerminateHandler::instance().isTerminated.load()) {
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
                        std::cerr << "[opendlv-device-camera-v4l]: Could not dequeue buffer for capture device: " << commandlineArguments["camera"] << ", error: " << errno << ": " << strerror(errno) << std::endl;
                        return false;
                    }

                    cluon::data::TimeStamp ts{cluon::time::now()};
                    if ( (v4l2_buf.flags & V4L2_BUF_FLAG_TIMESTAMP_MASK) == V4L2_BUF_FLAG_TIMESTAMP_MONOTONIC ) {
                        int64_t temp_ms = v4l2_buf.timestamp.tv_sec * 1000 + static_cast<int64_t>(round(v4l2_buf.timestamp.tv_usec/1000.0f));
                        int64_t epochTimeStampIn_ms = temp_ms + uptimeToEpochOffsetIn_ms;
                        ts = cluon::time::fromMicroseconds(epochTimeStampIn_ms*1000);
                    }

                    const uint8_t bufferIndex = v4l2_buf.index;
                    const uint32_t bufferSize = v4l2_buf.bytesused;
                    unsigned char *bufferStart = (unsigned char *) buffers[bufferIndex].buf;

                    if (0 < bufferSize) {
                        // Transform data as I420 in sharedMemoryI420.
                        sharedMemoryI420->lock();
                        sharedMemoryI420->setTimeStamp(ts);
                        {
                            if (isMJPEG) {
                                libyuv::MJPGToI420(bufferStart, bufferSize,
                                                   reinterpret_cast<uint8_t*>(sharedMemoryI420->data()), WIDTH,
                                                   reinterpret_cast<uint8_t*>(sharedMemoryI420->data()+(WIDTH * HEIGHT)), WIDTH/2,
                                                   reinterpret_cast<uint8_t*>(sharedMemoryI420->data()+(WIDTH * HEIGHT + ((WIDTH * HEIGHT) >> 2))), WIDTH/2,
                                                   WIDTH, HEIGHT,
                                                   WIDTH, HEIGHT);
                            }
                            if (isYUYV422) {
                                libyuv::YUY2ToI420(bufferStart, WIDTH * 2 /* 2*WIDTH for YUYV 422*/,
                                                   reinterpret_cast<uint8_t*>(sharedMemoryI420->data()), WIDTH,
                                                   reinterpret_cast<uint8_t*>(sharedMemoryI420->data()+(WIDTH * HEIGHT)), WIDTH/2,
                                                   reinterpret_cast<uint8_t*>(sharedMemoryI420->data()+(WIDTH * HEIGHT + ((WIDTH * HEIGHT) >> 2))), WIDTH/2,
                                                   WIDTH, HEIGHT);
                            }
                        }
                        sharedMemoryI420->unlock();

                        sharedMemoryARGB->lock();
                        sharedMemoryARGB->setTimeStamp(ts);
                        {
                            libyuv::I420ToARGB(reinterpret_cast<uint8_t*>(sharedMemoryI420->data()), WIDTH,
                                               reinterpret_cast<uint8_t*>(sharedMemoryI420->data()+(WIDTH * HEIGHT)), WIDTH/2,
                                               reinterpret_cast<uint8_t*>(sharedMemoryI420->data()+(WIDTH * HEIGHT + ((WIDTH * HEIGHT) >> 2))), WIDTH/2,
                                               reinterpret_cast<uint8_t*>(sharedMemoryARGB->data()), WIDTH * 4, WIDTH, HEIGHT);

                            if (VERBOSE) {
                                XPutImage(display, window, DefaultGC(display, 0), ximage, 0, 0, 0, 0, WIDTH, HEIGHT);
                                std::clog << "[opendlv-device-camera-v4l]: Acquired new frame from capture device: " << commandlineArguments["camera"] << " at " << cluon::time::toMicroseconds(ts) << " microseconds." << std::endl;
                            }
                        }
                        sharedMemoryARGB->unlock();

                        sharedMemoryI420->notifyAll();
                        sharedMemoryARGB->notifyAll();
                    }

                    if (0 > ::ioctl(videoDevice, VIDIOC_QBUF, &v4l2_buf)) {
                        std::cerr << "[opendlv-device-camera-v4l]: Could not requeue buffer for capture device: " << commandlineArguments["camera"] << ", error: " << errno << ": " << strerror(errno) << std::endl;
                        return false;
                    }
                }
            }
            if (VERBOSE) {
                XCloseDisplay(display);
            }
        }

        type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        if (0 > ::ioctl(videoDevice, VIDIOC_STREAMOFF, &type)) {
            std::cerr << "[opendlv-device-camera-v4l]: Could not stop video stream for capture device: " << commandlineArguments["camera"] << ", error: " << errno << ": " << strerror(errno) << std::endl;
            return retCode = 1;
        }

        for (uint8_t i = 0; i < BUFFER_COUNT; i++) {
            ::munmap(buffers[i].buf, buffers[i].length);
        }

        ::close(videoDevice);
    }
    return retCode;
}

