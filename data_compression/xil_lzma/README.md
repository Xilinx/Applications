# Xilinx LZMA 

Xilinx LZMA compression is FPGA based implementation of standard LZMA. 
Xilinx implementation of LZMA application is aimed at achieving high throughput for both compression.
This Xilinx LZMA application is developed and tested on AWS F1 instance. To know
more about standard LZMA application please refer https://github.com/lzma/lzma

This application is accelerated using generic hardware architecture for LZ based data compression algorithms.

![LZx compress select](../img/lzx_comp.png) <br />

For more details refer this [link](https://gitenterprise.xilinx.com/heeran/xil_lzma/blob/master/README.md)


## Results

### Resource Utilization <br />

Table below presents resource utilization of Xilinx LZMA compress


| Design | LUT | LUTMEM | REG | BRAM | URAM| DSP | Fmax (MHz) |
| --------------- | --- | ------ | --- | ---- | --- | -----| -----|
| Compression     | 30441(3.02%) | 9036(1.58%)|47448(2.24%)|39(2.17%) | 0(0%)|3(0.04%)|250|


### Throughput & Compression Ratio

Table below presents the best throughput achieved during execution of this application.

| Topic| Results| 
|-------|--------|
|Best Compression Throughput|27MB/s|
|Average Compression Ratio Xilinx LZMA| 1.82x (Genomic data)|
|Average Compression Ratio CPU LZMA| 2.07x (Genomic data)|

Note: This throughput is reported for buffer to buffer using one compute units. Overall throughput can still be increased with multiple compute units. 


## Software & Hardware

```
  Software: Xilinx SDx 2018.3
  Hardware: xilinx_u200_xdma_201830_1 (Xilinx Alveo U200)
```
 
## Usage


### Build Steps

#### Emulation flows
```
  make check TARGETS=<sw_emu/hw_emu> DEVICES=$AWS_PLATFORM
  
  Note: This command compiles for targeted emulation mode and executes the
        application. To execute it on AWS F1 instance, please follow instructions
        specific to AWS F1 emulation.
```
#### Hardware

```
  make all TARGETS=hw DEVICES=$AWS_PLATFORM

  Note: This command compiles for hardware execution. It generates kernel binary ".xclbin" file. 
        This file is placed in ./xclbin directory under LZMA folder. To execute it on AWS F1 instance, 
        please follow instructions specific to AWS F1 deployment process.

```

### Execution Steps

The generated executable would be
"xil_lzma_1b"

```
  Input Arguments: 
    
        1. To execute single file for compression :  ./xil_lzma_1b -c <file_name>
        
   Note: Default arguments are set in Makefile

  Help:

        ===============================================================================================
        Usage: application.exe -[-h-c]

                --help,         -h      Print Help Options   Default: [false]
                --compress,     -c      Compress
				--cu			-x		CU used for compression
        ===============================================================================================

```


### Limitations

It divides file into 2GB chuncks and process each chunch as sapereate stream.


