# Xilinx LZ4  

Xilinx LZ4 compression/decompression is FPGA based implementation of standard LZ4. 
Xilinx implementation of LZ4 application is aimed at achieving high throughput for both compression and decompression.
This Xilinx LZ4 application is developed and tested on AWS F1 instance. To know
more about standard LZ4 application please refer https://github.com/lz4/lz4

## Results

### Resource Utilization <br />

Table below presents resource utilization of Xilinx LZ4 compress/decompress
kernels with 8 cores. It is possible to extend number of cores to achieve higher throughput.


| Design | LUT | LUTMEM | REG | BRAM | URAM| DSP | Fmax (MHz) |
| --------------- | --- | ------ | --- | ---- | --- | -----| -----|
| Compression     | 79502(7.07%) | 28317(4.90%)|60756(2.8%)|24(1.28%) | 48(5%)|1(0.01%)|250|
| Decompression     | 44451(4.31%) | 22413(3.88%)|40649(1.87%)|146(7.79%)|0|1(0.01%)|250|


### Throughput & Compression Ratio

Table below presents the best throughput achieved during execution of this application.

| Topic| Results| 
|-------|--------|
|Best Compression Throughput|1.85 GB/s|
|Best Decompression Throughput| 1.83 GB/s |
|Average Compression Ratio| 1.87x (Silesia Benchmark)|

Note: This throughput is reported for Single Kernel. Overall throughput can be increased by multiple compute units. 


## Software & Hardware

```
  Software: Xilinx SDx 2017.4
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
        This file is placed in ./xclbin directory under LZ4 folder. To execute it on AWS F1 instance, 
        please follow instructions specific to AWS F1 deployment process.

```

### Execution Steps

While using PARALLEL_BLOCK (8 default) the generated executable would be
"xil_lz4_8b"

```
  Input Arguments: 
    
        1. To execute single file for compression :  ./xil_lz4_8b -c <file_name>
        2. To execute single file for decompression: ./xil_lz4_8b -d <file_name.lz4>
        3. To validate various files together:       ./xil_lz4_8b -l <files.list>
            3.a. <files.list>: Contains various file names with current path    
        
   Note: Default arguments are set in Makefile
```

### Limitations

#### Decompression

No support for block dependence case




