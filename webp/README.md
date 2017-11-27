# WebP Image Compression

## Overview
WebP is a new image format developed by Google and supported in Chrome, Opera and Android that is optimized to enable faster and smaller images on the Web.
WebP images are about 30% smaller in size compared to PNG and JPEG images at equivalent visual quality. In addition, the WebP image format has feature parity with other formats as well.

This accelerated WebP encoder project (SDx) is based on `libwebp` open source project. Time-consuming functions are shifted into 2 FPGA kernels including:

  ```bash
  Kernel-1: intra-prediction and probability counting
  Kernel-2: arithmetic coding
  ```
The above two kernels form one instance. The project supports 1~4 instances.

The main feature of configuration used is 'method=4', which uses 'Token' based arithmetic coding, and can give better performance of compression.
Some original algorithms are modified for hardware efficiency, but without effect on decoding process.

## Performance
* One instance achieves about 8~10 times acceleration. The complexity of image texture may affect the throughput. But for average performance, 9 times can be achieved. Here is two examples:

  - An image with middle complex texture
  
    | Kernel | width x height (pix) | -q | hw | time (ms) | Freq (MHz) | Throughput (MB/sec) |
    | ------ | -------------------- | -- | -- | --------- | --------- |  ------------------ |
    | kernel-1 | 3840 x 2160 | 80 | aws-vu9p | 74.96 | 250 | 165.9 |
    | kerenl-2 | 3840 x 2160 | 80 | aws-vu9p | 87.93 | 250 | 141.4 |
    
  - An image with simple texture

    | Kernel | width x height (pix) | -q | hw | time (ms) | Freq (MHz) | Throughput (MB/sec)|
    | ------ | -------------------- | -- | -- | --------- | ---------- | ---------- |
    | kernel-1 | 3840 x 2160 | 80 | aws-vu9p | 74.4 | 250 | 167.2 |
    | kerenl-2 | 3840 x 2160 | 80 | aws-vu9p | 45.4 | 250 | 274.0 |

* One instance takes about 6% resource of VU9P, flowing is the detail:

    | Utilizations  |   Kernel-1   |   Kernel-2   | Kernel-1 + Kernel-2 |
    | ------------  |   --------   |   --   | ----------- |
    | LUT |           46693  |  14037 |  5.14%  |
    | FF  |           51828  |  18781 |  2.99%  |
    | DSP |            388   |    3   |  5.72%  |
    | BRAM |           97    |   27   |  5.74%  |
    | DDR bandwidth |        |        |  5.80%  |


## Software and system requirements
The following packages are required to run this application:
* Xilinx SDAccel 2017.1
* GCC 6.x
* make
* DSA: AWS VU9P: xilinx:aws-vu9p-f1:4ddr-xpr-2pr


## Building the accelerated WebP encoder
* To build the accelerated WebP encoder, run the following command:
    ```bash
    bash xocc.sh
    ```
    - The `xocc.sh` script compiles both the host application code (cwebp) and kernel code (kernel.xclbin). 
    - It sets environment variables and uses `make` to compile the application.

    
* The following environment variables can be configured in `xocc.sh`:
    - VP8_SDX: SDAccel directory
    - DSA: Device DSA
    - VP8_TARGET: hw or hw_emu
    - VP8_NBINSTANCES: number of instances. 1,2,3 or 4 are currently supported.
    - VP8_FREQUENCY: device frequency


* Follow the guide to create Amazon FPGA Image (AFI):
    - [AWS F1 Application Execution on Xilinx Virtex UltraScale Devices](https://github.com/aws/aws-fpga/blob/master/SDAccel/README.md)


* Move kernel.xclbin to kernel.xclbin_origin and copy kernel.awsxclbin to kernel.xclbin
   

## Running the accelerated WebP encoder
* Put following command in script `run.sh`
    ```bash
    source /opt/Xilinx/SDx/2017.1.rte/setup.sh
    ./cwebp list.rst -use_ocl -q 80 -o output
    ```
    - list.rst is text file lists input pictures, should be equal to "NPicPool" defined in src_syn/vp8_AsyncConfig.h
    - -use_ocl: should be kept
    - -q: compression quality
    - -o: output directory
  

* Execute the script as follows:
    ```bash
    sudo ./run.sh
    ```
