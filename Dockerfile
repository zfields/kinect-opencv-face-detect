# Container build parameters
# `docker build --tag head-hunter .`

# Must allow Docker to access the host's X11 server (lasts until reboot)
# `xhost +local:docker`

# Container run parameters
# `docker run --device /dev/snd --env DISPLAY --interactive --net host --privileged --rm --tty head-hunter`

ARG DEBIAN_FRONTEND=noninteractive
ARG GIT_TAG_LIBFREENECT="v0.6.0"
ARG GIT_TAG_OPENCV="4.3.0"

# Base Image
FROM debian:10-slim

# Pull in global args
ARG DEBIAN_FRONTEND
ARG GIT_TAG_LIBFREENECT
ARG GIT_TAG_OPENCV

# Install environmental dependencies
RUN ["dash", "-c" ,"\
    apt-get update \
 && apt-get install --no-install-recommends --show-progress --verbose-versions --yes \
      ca-certificates \
      nano \
      udev \
"]

# Install `libfreenect` build dependencies
# https://github.com/OpenKinect/libfreenect#linux
RUN ["dash", "-c" ,"\
    apt-get install --no-install-recommends --show-progress --verbose-versions --yes \
      build-essential \
      cmake \
      git \
      libusb-1.0-0-dev \
"]

# Install `libfreenect` example dependencies
RUN ["dash", "-c" ,"\
    apt-get install --no-install-recommends --show-progress --verbose-versions --yes \
      freeglut3-dev \
      libxi-dev \
      libxmu-dev \
"]

# Install `opencv` build dependencies
RUN ["dash", "-c" ,"\
    apt-get install --no-install-recommends --show-progress --verbose-versions --yes \
      build-essential \
      cmake \
      git \
      libavcodec-dev \
      libavformat-dev \
      libgtk2.0-dev \
      libswscale-dev \
      pkg-config \
"]

# Install application dependencies
RUN ["dash", "-c" ,"\
    apt-get install --no-install-recommends --show-progress --verbose-versions --yes \
      libcanberra-gtk-module \
 && rm -rf /var/lib/apt/lists \
"]

WORKDIR /src

# Clone `OpenKinect/libfreenect` git repository
RUN git clone https://github.com/OpenKinect/libfreenect.git --recursive --branch ${GIT_TAG_LIBFREENECT}

# Clone `OpenCV` git repository
RUN git clone https://github.com/opencv/opencv.git --recursive --branch ${GIT_TAG_OPENCV}

# Copy udev rules
RUN ["cp", "--no-clobber", "/src/libfreenect/platform/linux/udev/51-kinect.rules", "/etc/udev/rules.d/"]

WORKDIR /build/libfreenect

# Generate `libfreenect` build system
RUN ["cmake", "/src/libfreenect"]

# Build `libfreenect`
RUN ["make", "all", "install", "--directory=/build/libfreenect"]

WORKDIR /build/opencv

# Generate `opencv` build system
RUN ["cmake", "-DCMAKE_BUILD_TYPE=Release", "-DCMAKE_INSTALL_PREFIX=/usr/local", "/src/opencv"]

# Build `opencv`
RUN ["make", "all", "install", "--directory=/build/opencv", "-j7"]

ENV LD_LIBRARY_PATH=/usr/local/lib

WORKDIR /build
COPY Makefile .
COPY kinect_opencv_head_hunter.cpp .

# Build Head Hunter
RUN ["make", "all"]

CMD ["/build/head_hunter"]