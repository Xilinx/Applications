# WebP Image Compression

## Overview
WebP is a new image format developed by Google and supported in Chrome, Opera and Android that is optimized to enable faster and smaller images on the Web. WebP images are about 30% smaller in size compared to PNG and JPEG images at equivalent visual quality. In addition, the WebP image format has feature parity with other formats as well. It supports:

* Lossy compression: The lossy compression is based on VP8 key frame encoding.
* Lossless compression: The lossless compression format is developed by the WebP team.
* Transparency: 8-bit alpha channel is useful for graphical images. The Alpha channel can be used along with lossy RGB, a feature that's currently not available with any other format.
* Animation: It supports true-color animated images.
* Metadata: It may have EXIF and XMP metadata.
* Color Profile: It may have an embedded ICC profile.


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
