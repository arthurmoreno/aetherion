// #include "my_kernel.cuh"

// my_kernel.cu
__global__ void add_scalar(float* array, float scalar, int n) {
    int idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx < n) {
        array[idx] += scalar;
    }
}

extern "C" void launch_add_scalar(float* device_array, float scalar, int n, int numBlocks, int blockSize) {
    add_scalar<<<numBlocks, blockSize>>>(device_array, scalar, n);
}
