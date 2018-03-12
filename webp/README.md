# WebP Image Compression

## Overview
WebP is a new image format developed by Google and supported in Chrome, Opera and Android that is optimized to enable faster and smaller images on the Web.
WebP images are about 30% smaller in size compared to PNG and JPEG images at equivalent visual quality. In addition, the WebP image format has feature parity with other formats as well.

This accelerated WebP encoder project (SDx) is based on `libwebp` open source project. For one input picutre (.png), the output picutre (.webp) is achieved after following six steps:

    <img src="./imgforreadme/webp_steps.png" width="50%" height="50%">

Time-consuming functions are accelerated by 2 FPGA kernels including:

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

* Multi-pictures process. Host code supports multi-pictures process with asynchronous behaviors, which allows to overlap host-device communiations, prediction kernel computation and arithmetic coding kernel computation. This is shown by following demonstration picture and profiling result.

 <img src="./imgforreadme/webp_overlap.png" width="40%" height="40%">
 
 <img src="./imgforreadme/webp_profiling.png" width="70%" height="70%">

## Software and system requirements
The following packages are required to run this application:
* Xilinx SDAccel 2017.1
* GCC 6.x
* make
* DSA: AWS VU9P: xilinx:aws-vu9p-f1:4ddr-xpr-2pr




## Building the accelerated WebP encoder

* Prerequisites: the following steps assume that you have followed the instructions on [how to create, configure and connect to an F1 instance](https://github.com/Xilinx/SDAccel_Examples/wiki/Create,-configure-and-test-an-AWS-F1-instance) and that you are connected to a properly configured F1 instance.

* In a terminal window, execute the following commands to set-up the SDAccel environment
    ```bash
    cd $AWS_FPGA_REPO_DIR  
    source sdaccel_setup.sh
    source $XILINX_SDX/settings64.sh 
    ```

* Build the accelerated WebP encoder with the following command:
    ```bash
    bash xocc.sh
    ```
	- The xocc.sh script compiles both the host application code (`cwebp`) and FPGA binary (kernel.xclbin).
	- The following environment variables can be configured in `xocc.sh` to control the output of compilation flow:
		- WEBP_SDX: SDAccel directory
		- WEBP_DSA: Device DSA
		- WEBP_TARGET: hw or hw_emu
		- WEBP_NBINSTANCES: number of instances. 1,2,3 or 4 are currently supported.
		- WEBP_FREQUENCY: device frequency

* Create the AWS FPGA binary and AFI from the *.xclbin file
   ```bash
   $SDACCEL_DIR/tools/create_sdaccel_afi.sh \
	-xclbin=<xclbin-file-name>.xclbin \
	-s3_bucket=<bucket-name> \
	-s3_dcp_key=<dcp-folder-name> \
	-s3_logs_key=<logs-folder-name>
   ```

* Wait until the AFI becomes available before proceeding to execute the application on the F1 instance.

* Move kernel.xclbin to kernel.xclbin_origin and copy kernel.awsxclbin to kernel.xclbin.

* For more details about the last two steps, refer to [these instructions on how to build an AFI for F1](https://github.com/Xilinx/SDAccel_Examples/wiki/Create,-configure-and-test-an-AWS-F1-instance#build-the-host-application-and-fpga-binary-to-execute-on-f1)

## Running the accelerated WebP encoder

* The `cwebp` application takes the following arguments:
    - list.rst is text file lists input pictures, should be equal to "NPicPool" defined in src_syn/vp8_AsyncConfig.h
    - -use_ocl: should be kept
    - -q: compression quality
    - -o: output directory

* Run the accelerated WebP encoder with the following commands:
    ```sh
    sudo sh
    source /opt/Xilinx/SDx/2017.1.rte/setup.sh   
    ./cwebp list.rst -use_ocl -q 80 -o output 
    ```
