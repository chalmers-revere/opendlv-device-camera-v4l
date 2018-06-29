# opendlv-device-camera-v4l
OpenDLV Microservice to interface with v4l-accessible camera devices

Adjusting the maximum size of pages in bytes that can be locked in RAM by adding the following lines to `/etc/security/limits.conf`:

```
*    hard   memlock           unlimited
*    soft   memlock           unlimited
```

To use this microservice, your need to allow access to your X11 server:
```
xhost +
```

Next, you can start the streaming (omit `--verbose` to not display the window showing the raw image; the parameter ulimit specifies that maximum bytes to be locked in RAM and __must__ match with the desired video resolution: Width * Height * BitsPerPixel/8):
```
docker run \
           --rm \
           -ti \
           --init \
           --device /dev/video0 \
           -v /tmp/.X11-unix:/tmp/.X11-unix \
           -e DISPLAY=$DISPLAY \
           -v /dev/shm:/dev/shm \
           --ulimit memlock=2359296:2359296 \
           producer:latest \
               --camera=/dev/video0 \
               --cid=111 \
               --name=camera0 \
               --width=1024 \
               --height=768 \
               --bpp=24 \
               --freq=5 \
               --verbose
```

The directory `example` contains a small program that demonstrates how to access the shared memory that is created from the previous microservice. You can build it as follows:

```
cd example
docker build -t consumer -f Dockerfile.amd64 .
```

Finally, you can run the example as follows (obviously, the parameters must match with the settings for `producer`; the parameter ulimit specifies that maximum bytes to be locked in RAM):

```
docker run \
           --rm \
           -ti \
           --init \
           -v /tmp/.X11-unix:/tmp/.X11-unix \
           -e DISPLAY=$DISPLAY \
           -v /dev/shm:/dev/shm \
           --ulimit memlock=2359296:2359296 \
           example \
               --cid=111 \
               --name=camera0 \
               --width=1024 \
               --height=768 \
               --bpp=24 \
               --verbose
```
