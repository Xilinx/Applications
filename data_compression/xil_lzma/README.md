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
| Compression     | 27463(2.78%) | 9619(1.69%)|34102(1.64%)|38(2.00%) | 0(0%)|3(0.04%)|250|


### Throughput & Compression Ratio

Table below presents the best throughput achieved during execution of this application.

| Topic| Results| 
|-------|--------|
|Best Compression Throughput|29MB/s|
|Average Compression Ratio Xilinx LZMA| 1.54x (Genomic data)|
|Average Compression Ratio CPU LZMA| 1.96x (Genomic data)|
|Average Compression Ratio GZIP| 1.26x (Genomic data)|

Note: This throughput is reported for buffer to buffer using one compute units. Overall throughput can still be increased with multiple compute units. 


## Software & Hardware

```
  Software: Xilinx SDx 2018.2
  Hardware: xilinx_aws-vu9p-f1_dynamic_5_0 (AWS VU9p F1 DSA)
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
        ===============================================================================================

```


### Limitations

It divides file into 2GB chuncks and process each chunch as sapereate stream.


