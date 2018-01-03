# GZip 

Gzip is a file format used for file compression and decompression. 

## Overview

Gzip is based on the DEFLATE algorithm. The complete GZip application has two parts:

1. LZ77 compression 
2. Huffman encoding

In this example GZip implementation, the LZ77 module has been accelerated on the
FPGA device available on the AWS F1 instance. The accelerated LZ77 algorithm
provides a throughput of 3.5GBytes/s using 2 compute units. Higher performance
can be achieved by using additional compute units. This GZip example achieves around 
2x compression ratio overall (Silesia Benchmark).


### Resource Utilization

The LZ77 kernel runs at 250MHz and uses the following resources:

| Design | Compute Units | LUT | LUTMEM | REG | BRAM | DSP |
| ------ | ------------- | --- | ------ | --- | ---- | --- |
| 8 Bytes/clock | 1 | 31842(3.57%) | 1202(0.22%) |35577(1.79%) | 277(17.15%) |0|
| 8 Bytes/clock | 2 | 63669(7.18%) | 2404(0.44%) | 71154(3.60%) | 554(34.30%)|0|

## Algorithm Description

In this example, the Huffman encoding is still being run as part of the host
application and it is not yet accelerated. The LZ77 kernel is implemented as a C/C++
kernel and compiled using SDAccel.

The following  are various internal dataflow stages (pipeline stages)
into which the kernel implementation has been split:  
  

1. Read Input and Update Present Window
2. Read/Write into History Hash Table
3. Optimal Match Finder
4. Byte Packing

  
Following are some of the key architectural decisions taken to achieve the
necessary performance:

1.  The whole LZ77 pipeline has been designed to process 8bytes/cycle. To
achieve this, we read 8byte in parallel from DDR memory, and then perform
multiple comparison in the history hash table to find the optimal match. Every cycle, we
compare multiple sub-strings in an 8-byte length to find the optimal match.
2.  To perform multiple comparisons every cycle, several copies of the
history hash table data is maintained. The history hash table is also updated every cycle to
avoid conditional writes.
3.  The last stage of LZ77 compression performs a byte-level packing of the
data. This step by definition has a throughput of 1Byte per cycle. Hence, to
match the throughput of earlier stages of 8Bytes/cycle, the incoming data stream
is split into 8 parallel stream of 1KB each and sent to separate pipelines. In
the end, streams from multiple pipes are written to DDR.  

  
## Software & Hardware

```
  Software: Xilinx SDx 2017.1
  Hardware: xilinx:aws-vu9p-f1:4ddr-xpr-2pr (AWS VU9p DSA)
```
 
## Execution Steps

This example is provided with two popular compression benchmarks. 

1. [Silesia](http://sun.aei.polsl.pl/~sdeor/index.php?page=silesia)
2. [Canterbury](http://corpus.canterbury.ac.nz/descriptions/#cantrbry)

```
  Input Arguments: 
    
        1. To execute list/batch of files: -l <files.list> (files.list contains list of files)
        2. To execute single file: -i <file>
        
   Note: Default arguments are set in Makefile
```

### Emulation flows
```
  make check TARGETS=<sw_emu/hw_emu> DEVICES=$AWS_PLATFORM
  
  Note: This command compiles for targeted emulation mode and executes the
        application. To execute it on AWS F1 instance, please follow instructions
        specific to AWS F1 emulation.

```
### Hardware

```
  make all TARGETS=hw DEVICES=$AWS_PLATFORM

  Note: This command compiles for hardware execution. It generates kernel binary ".xclbin" file. 
        This file is placed in ./xclbin directory under GZip folder. To execute it on AWS F1 instance, 
        please follow instructions specific to AWS F1 deployment process.

```

### Expected Output

This section presents various steps involved to generate encode and decode output. The encoded stream of this example is not compatible with standard GZip implementation at the moment and kindly make use of custom decoder provided with this example.

```
1. Encode input file 
   a. LZ77 (FPGA) encode
   b. Huffman (CPU) encode
2. Generate encoded file (.xgzip) format
3. Decode (.xgzip) file generated in previous steps
   a. Huffman & LZ77 (CPU) decode
4. Generate original file (.orig) format
5. Validate results (input file & .orig file) 
   a. PASSED: Status is passed it means both files matches
   b. FAILED: Status is failed it means there is mismatch
```

#### Sample Result 

This section presents sample output format produced by this application for a given input file. Below is the result of executing couple of the silesia benchmark input files on AWS platform (hardware)

```
------------------------------------------------------------------------------

E2E(MBps)  KT(MBps)   CR      STATUS   File Size(MB)   File Name

1290.73    3331.64    3.31    PASSED   33.6            ./benchmark/silesia/nci
1234.91    3505.20    1.87    PASSED   51.2            ./benchmark/silesia/mozilla

-------------------------------------------------------------------------------

E2E(MBps) - End to End throughput
KT(MBps)  - LZ77 Kernel throughput 
CR        - Compression Ratio
STATUS    - Test case validation status
File Size - Input file size in MBs
File Name - Input file name

Note: Sample results presented above are produced by executing this application on real hardware (AWS F1)
```
  

