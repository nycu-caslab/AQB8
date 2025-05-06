# AQB8: Energy-Efficient Ray Tracing Accelerator through Multi-Level Quantization



## 🐳 AQB8 Vulkan Simulation Docker Image

[![Docker Pulls](https://img.shields.io/docker/pulls/ycpin/aqb8-vulkan-sim)](https://hub.docker.com/r/ycpin/aqb8-vulkan-sim)

A pre-configured Docker image containing all necessary tools and environments for AQB8 Vulkan-based GPU simulation and analysis. Designed for research, reproducibility, and development.

---

## ✅ Features

- Ubuntu 20.04 base
- CUDA 11.1
- Vulkan SDK + mesa Vulkan drivers
- GPGPU-Sim with supporting libraries
- Preloaded Vulkan simulation configurations:
  - `vulkan-sim-baseline-{2,6}wide`
  - `vulkan-sim-compress-{2,6}wide`
  - `vulkan-sim-quantized-{2,6}wide`

---

## 🚀 Quick Start

Run interactively:

```bash
docker pull ycpin/aqb8-vulkan-sim
docker run -it --rm ycpin/aqb8-vulkan-sim bash
