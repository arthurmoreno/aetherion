// my_module.cpp

#include <Python.h>
#include <cuda_runtime.h>
#include <numpy/arrayobject.h>

#include <iostream>

#include "my_kernel.cuh"

void check_cuda_error(const char* message) {
    cudaError_t err = cudaGetLastError();
    if (err != cudaSuccess) {
        std::cerr << "CUDA error after " << message << ": " << cudaGetErrorString(err) << std::endl;
    }
}

static PyObject* add_scalar(PyObject* self, PyObject* args) {
    PyArrayObject* input_array;
    float scalar;

    // Parse the input tuple
    if (!PyArg_ParseTuple(args, "O!f", &PyArray_Type, &input_array, &scalar)) {
        return NULL;
    }

    // Ensure the input is a float32 array
    if (PyArray_TYPE(input_array) != NPY_FLOAT32) {
        PyErr_SetString(PyExc_TypeError, "Input array must be of type float32");
        return NULL;
    }

    // Get the number of elements in the array
    int n = (int)PyArray_SIZE(input_array);
    std::cout << "Number of elements in array: " << n << std::endl;

    // Get a pointer to the array data
    float* host_array = (float*)PyArray_DATA(input_array);
    std::cout << "First element before CUDA operation: " << host_array[0] << std::endl;

    // Allocate memory on the GPU
    float* device_array;
    cudaMalloc((void**)&device_array, n * sizeof(float));
    check_cuda_error("cudaMalloc");

    // Copy data from the host to the device
    cudaMemcpy(device_array, host_array, n * sizeof(float), cudaMemcpyHostToDevice);
    check_cuda_error("cudaMemcpy to device");

    // Define grid and block dimensions
    int blockSize = 256;
    int numBlocks = (n + blockSize - 1) / blockSize;
    std::cout << "Launching kernel with " << numBlocks << " blocks of " << blockSize
              << " threads each." << std::endl;

    // Launch the kernel (call the function from the .cu file)
    launch_add_scalar(device_array, scalar, n, numBlocks, blockSize);
    check_cuda_error("launch_add_scalar");

    // Copy the result back to the host
    cudaMemcpy(host_array, device_array, n * sizeof(float), cudaMemcpyDeviceToHost);
    check_cuda_error("cudaMemcpy to host");

    std::cout << "First element after CUDA operation: " << host_array[0] << std::endl;

    // Free device memory
    cudaFree(device_array);
    check_cuda_error("cudaFree");

    // Return None
    Py_RETURN_NONE;
}

// Module method table
static PyMethodDef MyMethods[] = {{"add_scalar", (PyCFunction)add_scalar, METH_VARARGS,
                                   "Add a scalar to each element of the array"},
                                  {NULL, NULL, 0, NULL}};

// Module definition structure
static struct PyModuleDef mymodule = {PyModuleDef_HEAD_INIT,
                                      "my_module",  // Name of the module
                                      NULL,         // Module documentation, may be NULL
                                      -1,  // Size of per-interpreter state of the module, or -1 if
                                           // the module keeps state in global variables.
                                      MyMethods};

// Module initialization function
PyMODINIT_FUNC PyInit_my_module(void) {
    import_array();  // Required for NumPy
    return PyModule_Create(&mymodule);
}