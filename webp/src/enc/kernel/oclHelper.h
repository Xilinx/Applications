// Copyright 2014, Xilinx Inc.
// All rights reserved.

#ifndef _OCL_HELP_H_
#define _OCL_HELP_H_

#include <CL/cl.h>
#include "../../../src_syn/vp8_AsyncConfig.h"

struct oclHardware {
    cl_platform_id mPlatform;
    cl_context mContext;
    cl_device_id mDevice;
    cl_command_queue mQueue;
    short mMajorVersion;
    short mMinorVersion;
};

struct oclSoftware {
    cl_program mProgram;
    char mCompileOptions[1024];
    char mFileName[1024];
};

struct oclKernelInfo {
    cl_kernel mKernel;
    char mKernelName[128];
    cl_kernel mKernel2;
    char mKernelName2[128];

    cl_kernel mKernelPred[NasyncDepth*Ninstances];
    cl_kernel mKernelAC[NasyncDepth*Ninstances];
};

extern "C" oclHardware getOclHardware(cl_device_type type, char *target_device);

extern "C" int getOclSoftware(oclSoftware &software, const oclHardware &hardware);

extern "C" void releaseSoftware(oclSoftware& software);

extern "C" void releaseKernel(oclKernelInfo& kernelinfo);

extern "C" void releaseHardware(oclHardware& hardware);

extern "C" const char *oclErrorCode(cl_int code);

#endif
