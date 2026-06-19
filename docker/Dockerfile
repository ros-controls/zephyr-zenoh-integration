FROM ros:jazzy-ros-base

ENV DEBIAN_FRONTEND=noninteractive

# Install system dependencies for both Zephyr and ROS 2
RUN apt-get update && apt-get install -y \
    python3-pip python3-venv python3-setuptools \
    python3-colcon-common-extensions \
    cmake ninja-build gperf build-essential \
    ccache dfu-util device-tree-compiler wget \
    python3-dev python3-tk python3-wheel xz-utils file \
    make gcc gcc-multilib g++-multilib libsdl2-dev libmagic1 git \
    ros-jazzy-ros2-control \
    ros-jazzy-ros2-controllers \
    ros-jazzy-rmw-zenoh-cpp \
    ros-jazzy-rosidl-generator-c \
    ros-jazzy-rosidl-generator-cpp \
    && rm -rf /var/lib/apt/lists/*

# Install West
RUN pip3 install --break-system-packages west

# Set up Zephyr workspace with zenoh-pico preloaded
WORKDIR /zephyr_ws
COPY docker/image-west.yml        manifest-repo/west.yml
COPY zenbedded_transport/west.yml manifest-repo/zenbedded_transport.yml
RUN cd manifest-repo && git init -q && git add . \
    && git -c user.email=ci@local -c user.name=ci commit -qm "image manifest"
RUN west init -l manifest-repo && west update && west zephyr-export

# Copy python requirements from zephyr and install
RUN pip3 install --ignore-installed --break-system-packages -r /zephyr_ws/zephyr/scripts/requirements.txt

# Install Zephyr SDK(MINIMAL)
RUN wget -q https://github.com/zephyrproject-rtos/sdk-ng/releases/download/v0.16.8/zephyr-sdk-0.16.8_linux-x86_64_minimal.tar.xz \
    && tar xf zephyr-sdk-0.16.8_linux-x86_64_minimal.tar.xz \
    && rm zephyr-sdk-0.16.8_linux-x86_64_minimal.tar.xz \
    && cd zephyr-sdk-0.16.8 \
    && ./setup.sh -t xtensa-espressif_esp32_zephyr-elf -t xtensa-espressif_esp32s3_zephyr-elf -t arm-zephyr-eabi -c -h

# Set environment variables
ENV ZEPHYR_BASE=/zephyr_ws/zephyr
ENV PATH="/zephyr_ws/zephyr-sdk-0.16.8/arm-zephyr-eabi/bin:/zephyr_ws/zephyr-sdk-0.16.8/xtensa-espressif_esp32_zephyr-elf/bin:/zephyr_ws/zephyr-sdk-0.16.8/xtensa-espressif_esp32s3_zephyr-elf/bin:${PATH}"
ENV RMW_IMPLEMENTATION=rmw_zenoh_cpp

# Set up ROS 2 workspace structure
RUN mkdir -p /ros2_ws/src
WORKDIR /ros2_ws

RUN echo "source /opt/ros/jazzy/setup.bash" >> /root/.bashrc && \
    echo "source /zephyr_ws/zephyr/zephyr-env.sh" >> /root/.bashrc

RUN usermod -a -G dialout root

CMD ["/bin/bash", "-c", "source /opt/ros/jazzy/setup.bash && exec /bin/bash"]
