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

#include <opencv2/highgui/highgui.hpp>
#include <opencv2/imgproc/imgproc.hpp>

#include <cstdint>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>
#include <thread>

int32_t main(int32_t argc, char **argv) {
    int32_t retCode{0};
    auto commandlineArguments = cluon::getCommandlineArguments(argc, argv);
    if ( (0 == commandlineArguments.count("name")) || (0 == commandlineArguments.count("cid")) || (0 == commandlineArguments.count("width")) || (0 == commandlineArguments.count("height")) || (0 == commandlineArguments.count("bpp")) ) {
        std::cerr << argv[0] << " accesses video data using shared memory provided using the command line parameter --name=." << std::endl;
        std::cerr << "Usage:   " << argv[0] << " --cid=<OpenDaVINCI session> --width=<width> --height=<height> --bpp=<bits per pixel> --name=<name for the associated shared memory> [--id=<Identifier in case of multiple video streams>] [--verbose]" << std::endl;
        std::cerr << "         --width:   width of a frame" << std::endl;
        std::cerr << "         --height:  height of a frame" << std::endl;
        std::cerr << "         --bpp:     bits per pixel of a frame (either 8 or 24)" << std::endl;
        std::cerr << "         --name:    name of the shared memory to use" << std::endl;
        std::cerr << "         --verbose: when set, the image contained in the shared memory is displayed" << std::endl;
        std::cerr << "Example: " << argv[0] << " --cid=111 --name=cam0 --width=640 --height=480 --bpp=24" << std::endl;
        retCode = 1;
    }
    else {
        const uint32_t WIDTH{static_cast<uint32_t>(std::stoi(commandlineArguments["width"]))};
        const uint32_t HEIGHT{static_cast<uint32_t>(std::stoi(commandlineArguments["height"]))};
        const uint32_t BPP{static_cast<uint32_t>(std::stoi(commandlineArguments["bpp"]))};

        if ( (BPP != 24) && (BPP != 8) ) {
            std::cerr << argv[0] << ": bits per pixel must be either 24 or 8; found " << BPP << "." << std::endl;
        }
        else {
            const uint32_t SIZE{WIDTH * HEIGHT * BPP/8};
            const std::string NAME{(commandlineArguments["name"].size() != 0) ? commandlineArguments["name"] : "/cam0"};
            const uint32_t ID{(commandlineArguments["id"].size() != 0) ? static_cast<uint32_t>(std::stoi(commandlineArguments["id"])) : 0};
            const bool VERBOSE{commandlineArguments.count("verbose") != 0};

            (void)ID;
            (void)SIZE;

            // Interface to a running OpenDaVINCI session (ignoring any incoming Envelopes).
            cluon::OD4Session od4{static_cast<uint16_t>(std::stoi(commandlineArguments["cid"]))};

            std::unique_ptr<cluon::SharedMemory> sharedMemory(new cluon::SharedMemory{NAME});
            if (sharedMemory && sharedMemory->valid()) {
                std::clog << argv[0] << ": Found shared memory '" << sharedMemory->name() << "' (" << sharedMemory->size() << " bytes)." << std::endl;

                CvSize size;
                size.width = WIDTH;
                size.height = HEIGHT;

                IplImage *image = cvCreateImageHeader(size, IPL_DEPTH_8U, BPP/8);
                sharedMemory->lock();
                image->imageData = sharedMemory->data();
                image->imageDataOrigin = image->imageData;
                sharedMemory->unlock();

                while (od4.isRunning()) {
                    // The shared memory uses a pthread broadcast to notify us; just sleep to get awaken up.
                    sharedMemory->wait();
                    sharedMemory->lock();
                    if (VERBOSE) {
                        cvShowImage(sharedMemory->name().c_str(), image);
                    }
                    sharedMemory->unlock();
                    cv::waitKey(1);
                }

                cvReleaseImageHeader(&image);
            }
            else {
                std::cerr << argv[0] << ": Failed to access shared memory '" << NAME << "'." << std::endl;
            }
        }
    }
    return retCode;
}

