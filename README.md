# AQB8 Project Hardware Code

**Note:** This is the `RTL` branch of the AQB8 project, focusing on the **hardware implementation**. For full paper information, including citation, author details, and a comprehensive overview of the AQB8 project, please refer to the `main` branch of this repository.

This repository contains the hardware source code and experimental setup for the AQB8 project.

The codebase implements the hardware aspects of AQB8, a ray tracing (RT) accelerator designed to operate on a quantized Bounding Volume Hierarchy (BVH) tree using a multi-level quantization technique. This approach enables RT to operate directly on low-bit integers, aiming to reduce memory traffic, energy consumption, and hardware area while improving performance.

## Repository Structure

The repository is organized into three main configurations, reflecting different experimental setups for the hardware:

* **`baseline-2`**: Hardware implementation of a baseline ray tracing accelerator using standard FP32 BVH for a 2-ary BVH tree.
* **`compress-2`**: Hardware implementation of a ray tracing accelerator using conventional low-bit (e.g., INT8) BVH compression (without multi-level quantization) for a 2-ary BVH tree.
* **`AQB8-2`**: Hardware implementation of the proposed AQB8 accelerator with multi-level quantization for a 2-ary BVH tree.
* **`.gitmodules`**: Specifies Git submodules used in the project.

Each of these configuration directories (`baseline-2`, `compress-2`, `AQB8-2`) generally contains the following subdirectories:

* **`data/`**: Contains BVH construction logics and scripts for data generation (i.e., `generate.cpp`).
    * Notably, `AQB8-2/data/bvh-quantize/` holds core components for the multi-level quantization, BVH building, traversal, and utility tools.
* **`include/`**: Header files for data types (e.g., `datatypes.h` for HLS, `datatypes.svh` for SystemVerilog), parameters (e.g., `params.h`, `params.vh`), and simulation utilities.
* **`src/`**: Contains **synthesizable hardware source code**. This includes:
    * C++ files (e.g., `rtcore.cpp`, `bbox.h`) intended for High-Level Synthesis (HLS) to generate RTL.
    * SystemVerilog/Verilog files (e.g., `rtcore_top.sv`, `trv.sv`, FIFOs, SRAMs) for the RTL design.
* **`test/`**: Contains **testing code for hardware verification**. This includes:
    * C++ testbenches (e.g., `tb_rtcore.cpp`) for HLS components.
    * SystemVerilog/Verilog testbenches (e.g., `tb_*.sv`) for RTL modules and system-level verification.
* **`util/`**: Utility programs and scripts.
* **`work/`**: Scripts for synthesis (e.g., `syn.tcl`), power analysis (e.g., `pwr.tcl`), and simulation workflows for the hardware.

## Features

* Hardware implementation of the **AQB8 ray tracing accelerator** architecture.
* Hardware support for **multi-level quantization** in BVH trees.
* BVH traversal logic optimized for **low-bit integer arithmetic** (INT8 for quantized BBs) in hardware.
* Comparative hardware models:
    * **Baseline FP32 BVH** accelerator.
    * **Conventional INT8 compressed BVH** accelerator.
* Support for **2-ary BVH trees** (as indicated by the `*-2` suffix in directory names).

## Dependencies

* **TSMC 40nm Cell Library:** Access to the TSMC 40nm standard cell libraries is necessary for the synthesis and power analysis steps to accurately target the technology node used in the paper.
* **QuestaSim:** For cycle-accurate hardware simulation.
* **Design Compiler:** For RTL synthesis.
* **PrimePower:** For power analysis.
* **Catapult HLS (Optional):** This tool is required if you intend to modify the HLS C++ sources (e.g., `rtcore.cpp`, `bbox.h`, `ist.h`) and regenerate the RTL code. The repository already provides pre-generated RTL from HLS (e.g., in `src/concat_rtl.v`), so this tool is not strictly necessary if you are only using the existing HLS-generated RTL.

## Building and Running

1.  **Clone the repository:**
    Since the project uses Git submodules, clone the repository recursively:
    ```bash
    git clone -b RTL --depth 1 --recursive https://github.com/nycu-caslab/AQB8.git
    cd AQB8
    ```

2.  **Running Simulations, Synthesis, and Other Tasks:**
    Most tasks like HLS C-simulation, RTL simulation, synthesis, and power analysis are initiated from the `work/` directory within each specific configuration (e.g., `baseline-2/work/`, `AQB8-2/work/`).
    Navigate to the desired `work` directory:
    ```bash
    cd <configuration_directory>/work  # e.g., cd AQB8-2/work/
    ```
    Then, use `make` with a specific target to perform the intended operation. For example:
    * To run HLS C-simulation:
        ```bash
        make c_sim
        ```
    * To synthesize the RTL code:
        ```bash
        make syn
        ```
    * Other targets for RTL simulation, power analysis, etc., are defined in the `Makefile` within the `work/` directory. Refer to the `Makefile` for a list of available targets.

## Key Components

The core hardware units of the RT accelerator as described in the paper correspond to the following files in the codebase (located in the `src/` directory of each configuration):

* **BOX Unit** (Handles FP32 ray-box intersection tests):
    * `baseline-2/src/bbox.h`
    * `compress-2/src/bbox.h`
    * `AQB8-2/src/clstr.h` (Specifically for anchor box intersection in AQB8)
* **QBOX Unit** (Handles INT8 quantized ray-box intersection tests in AQB8):
    * `AQB8-2/src/bbox.h`
* **TRV Unit** (Handles BVH traversal logic):
    * `src/trv.sv` (Common across `AQB8-2`, `baseline-2`, `compress-2`)
* **TRIG Unit** (Handles ray-triangle intersection tests):
    * `src/ist.h` (Common across `AQB8-2`, `baseline-2`, `compress-2`)

## License

Please refer to the license information provided in the `main` branch.