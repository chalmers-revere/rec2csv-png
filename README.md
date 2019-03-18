## OpenDLV Microservice to convert a .rec file from OpenDLV into .csv and .png for h264 video frames

This repository provides source code to convert a .rec file from OpenDLV into .csv
for the contained messages and .png for the contained h264 video frames for the
OpenDLV software ecosystem.

[![License: GPLv3](https://img.shields.io/badge/license-GPL--3-blue.svg
)](https://www.gnu.org/licenses/gpl-3.0.txt)

[OpenH264 Video Codec provided by Cisco Systems, Inc.](https://www.openh264.org/faq.html)

During the Docker-ized build process for this microservice, Cisco's binary
library is downloaded from Cisco's webserver and installed on the user's
computer due to legal implications arising from the patents around the [AVC/h264 format](http://www.mpegla.com/main/programs/avc/pages/intro.aspx).

End user's notice according to [AVC/H.264 Patent Portfolio License Conditions](https://www.openh264.org/BINARY_LICENSE.txt):
**When you are using this software and build scripts from this repository, you are agreeing to and obeying the terms under which Cisco is making the binary library available.**


## Table of Contents
* [Dependencies](#dependencies)
* [Building and Usage](#building-and-usage)
* [License](#license)


## Dependencies
You need a C++14-compliant compiler to compile this project.

The following dependency is part of the source distribution:
* [libcluon](https://github.com/chrberger/libcluon) - [![License: GPLv3](https://img.shields.io/badge/license-GPL--3-blue.svg
)](https://www.gnu.org/licenses/gpl-3.0.txt)
* [lodePNG](https://github.com/lvandeve/lodepng) - [![License: Zlib](https://img.shields.io/badge/License-Zlib-blue.svg)](https://opensource.org/licenses/Zlib)

The following dependencies are will be downloaded and installed during the Docker-ized build:
* [openh264](https://www.openh264.org/index.html) - [![License: BSD 2-Clause](https://img.shields.io/badge/License-BSD%202--Clause-blue.svg)](https://opensource.org/licenses/BSD-2-Clause) - [AVC/H.264 Patent Portfolio License Conditions](https://www.openh264.org/BINARY_LICENSE.txt)
* [libyuv](https://chromium.googlesource.com/libyuv/libyuv/+/master) - [![License: BSD 3-Clause](https://img.shields.io/badge/License-BSD%203--Clause-blue.svg)](https://opensource.org/licenses/BSD-3-Clause) - [Google Patent License Conditions](https://chromium.googlesource.com/libyuv/libyuv/+/master/PATENTS)
* [libjpeg-turbo](https://github.com/libjpeg-turbo/libjpeg-turbo) - [IJG License/BSD 3-Clause/zlib](https://github.com/libjpeg-turbo/libjpeg-turbo/blob/master/LICENSE.md)

## Building and Usage
Due to legal implications arising from the patents around the [AVC/h264 format](http://www.mpegla.com/main/programs/avc/pages/intro.aspx),
we cannot provide and distribute pre-built Docker images. Therefore, we provide
the build instructions in a `Dockerfile` that can be easily integrated in a
`docker-compose.yml` file.

To run this microservice using `docker-compose`, you can use the following
`docker-compose.yml` file to let Docker build this software for you:

```yml
version: '2'
services:
    rec2csv_png:
        build:
            context: https://github.com/chalmers-revere/rec2csv-png.git
            dockerfile: Dockerfile.amd64
        restart: on-failure
        volumes:
        - .:/opt/data
        working_dir: /opt/data
        command: "--rec=YourRecording.rec --odvd=YourMessageSpec.odvd"
```

When you built your Docker image named `transcoder`, you can also call it as follows:

```
docker run --rm -ti --init -v $PWD:/opt/data -w /opt/data transcoder --rec=YourRecording.rec --odvd=YourMessageSpec.odvd
```

The current folder `.` is shared into the Docker container to access the recording
file `YourRecording.rec` and the message specification file `YourMessageSpec.odvd`.


## License

* This project is released under the terms of the GNU GPLv3 License

