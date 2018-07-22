# opendlv-device-camera-v4l
OpenDLV Microservice to interface with v4l-accessible camera devices

To use this microservice, you need to allow access to your X11 server when you
want to display the captured frame:

```
xhost +
```

Next, you can start the streaming (omit `--verbose` to not display the window showing the raw image; the parameter ulimit specifies that maximum bytes to be locked in RAM and __must__ match with the desired video resolution: Width * Height * BitsPerPixel/8):
```
docker run --rm -ti --init --ipc=host -v /tmp:/tmp -e DISPLAY=$DISPLAY --device /dev/video0 v --camera=/dev/video0 --name=cam0 --width=640 --height=480 --freq=20 --verbose

docker run \
           --rm \
           -ti \
           --init \
           --device /dev/video0 \
           --ipc=host
           -v /tmp:/tmp \
           -e DISPLAY=$DISPLAY \
           producer:latest \
               --camera=/dev/video0 \
               --name=camera0 \
               --width=640 \
               --height=480 \
               --freq=20 \
               --verbose
```

