FROM ros:indigo-ros-base
# install ros tutorials packages
RUN apt-get update && apt-get install -y
    ros-indigo-ros-tutorials \
    ros-indigo-common-tutorials \
    && rm -rf /var/lib/apt/lists/

docker build --tag ros:ros-tutorials .

