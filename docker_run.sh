#!/bin/bash

# docker_run.sh - Run the AQB8 Vulkan-Sim Docker container

IMAGE="ycpin/aqb8-vulkan-sim"
CONTAINER_NAME="aqb8-vulkan-sim-container"

echo "Pulling Docker image: $IMAGE"
docker pull "$IMAGE"

echo "Running Docker container: $CONTAINER_NAME"
docker run -it --rm --name "$CONTAINER_NAME" "$IMAGE"
