#ifndef MY_KERNEL_CUH
#define MY_KERNEL_CUH

// Declare the CUDA kernel
__global__ void add_scalar(float* array, float scalar, int n);

// Declare the launcher function that will be called from the C++ code
extern "C" void launch_add_scalar(float* device_array, float scalar, int n, int numBlocks, int blockSize);

#endif // MY_KERNEL_CUH