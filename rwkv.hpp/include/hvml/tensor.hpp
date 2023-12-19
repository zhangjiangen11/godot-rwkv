#ifndef TENSOR_HPP
#define TENSOR_HPP

#include "intrinsics.hpp"
#include <vector>
#include <iostream>
#include <algorithm>
#include <cassert>

#include "nlohmann/json.hpp"

#include <vuda_runtime.hpp>

#define ALIGNMENT 64

#define bfloat16 short






enum TENSORTYPE
{
    /// Boolean type
    kBOOL,
    /// Unsigned byte
    kUINT_8,
    /// Signed byte
    kINT_8,
    /// Signed integer (16-bit)
    kINT_16,
    /// Unsigned integer (16-bit)
    kUINT_16,
    /// Half-precision floating point
    kFLOAT_16,
    /// Brain floating point
    kBFLOAT_16,
    /// Signed integer (32-bit)
    kINT_32,
    /// Unsigned integer (32-bit)
    kUINT_32,
    /// Floating point (32-bit)
    kFLOAT_32,
    /// Floating point (64-bit)
    kFLOAT_64,
    /// Signed integer (64-bit)
    kINT_64,
    /// Unsigned integer (64-bit)
    kUINT_64,

};

typedef struct{uint16_t i;} HVMLCPU;
typedef struct{uint16_t i;} HVMLVULKAN;
typedef struct{uint16_t i;} HVMLDYNAMIC;


HVMLCPU KHVMLCPU = {0};
HVMLVULKAN KHVMLVULKAN = {1};

template <typename INP = HVMLCPU>
struct VKTensorInfo
{
    struct{u_int16_t i;} device_type = KHVMLCPU;
    int offset = 0;
};




NLOHMANN_JSON_SERIALIZE_ENUM(TENSORTYPE, {
                                             {kBOOL, "BOOL"},
                                             {kUINT_8, "U8"},
                                             {kINT_8, "I8"},
                                             {kINT_16, "I16"},
                                             {kUINT_16, "U16"},
                                             {kFLOAT_16, "F16"},
                                             {kBFLOAT_16, "BF16"},
                                             {kINT_32, "I32"},
                                             {kUINT_32, "U32"},
                                             {kFLOAT_32, "F32"},
                                             {kFLOAT_64, "F64"},
                                             {kINT_64, "I64"},
                                             {kUINT_64, "U64"},
                                         })

// can be float, double, int, etc.
template <typename DataType, typename DeviceData = HVMLDYNAMIC>
class Tensor
{
public:
    // DataType *data __attribute__((aligned(ALIGNMENT)));
    // DataType if DeviceData is HVMLCPU
    // VKTensorInfo if DeviceData is VKTensorInfo
    DataType* data __attribute__((aligned(ALIGNMENT)));
    uint64_t data_size_in_elements;
    uint64_t data_size_in_bytes;
    uint64_t total_original_allocated_bytes;

    VKTensorInfo<DeviceData> device = {
        .device_type = {
            .i = KHVMLCPU.i,
        },
        .offset = 0,
    };

    std::vector<uint64_t> shape = {};

    Tensor()
    {
        this->data = nullptr;
        this->data_size_in_elements = 0;
        this->data_size_in_bytes = 0;
        this->total_original_allocated_bytes = 0;
    }

    uint64_t get_data_size_in_elements(std::vector<uint64_t> shape)
    {
        uint64_t size = 1;
        for (size_t i = 0; i < shape.size(); i++)
        {
            size *= shape[i];
        }
        this->data_size_in_elements = size;
        return size;
    }

    uint64_t get_data_size_in_bytes()
    {
        return this->data_size_in_elements * sizeof(DataType);
    }

    void fill(DataType value)
    {
        // fill data with value using std::fill
        if (this->device.device_type.i == KHVMLCPU.i){
        std::fill(this->data, this->data + this->data_size_in_elements, value);
        }
        else{
            auto stream_id = 0;
            const int CHUNKSIZE = 256;
            auto kernalparams = vuda::dim3(this->data_size_in_elements/CHUNKSIZE, 1, 1);
            vuda::launchKernel("./shaders/set1.glsl.spv", "main", stream_id, kernalparams, this->data_size_in_elements, CHUNKSIZE, value, this->data);
            vuda::streamSynchronize(stream_id);
        }
    }

    

    TENSORTYPE get_type()
    {
        if (std::is_same<DataType, bool>::value)
        {
            return kBOOL;
        }
        else if (std::is_same<DataType, uint8_t>::value)
        {
            return kUINT_8;
        }
        else if (std::is_same<DataType, int8_t>::value)
        {
            return kINT_8;
        }
        else if (std::is_same<DataType, int16_t>::value)
        {
            return kINT_16;
        }
        else if (std::is_same<DataType, uint16_t>::value)
        {
            return kUINT_16;
        }
        else if (std::is_same<DataType, float>::value)
        {
            return kFLOAT_32;
        }
        else if (std::is_same<DataType, double>::value)
        {
            return kFLOAT_64;
        }
        else if (std::is_same<DataType, int32_t>::value)
        {
            return kINT_32;
        }
        else if (std::is_same<DataType, uint32_t>::value)
        {
            return kUINT_32;
        }
        else if (std::is_same<DataType, int64_t>::value)
        {
            return kINT_64;
        }
        else if (std::is_same<DataType, uint64_t>::value)
        {
            return kUINT_64;
        }
        else if (std::is_same<DataType, bfloat16>::value)
        {
            return kBFLOAT_16;
        }
        else
        {
            throw std::runtime_error("Unknown type");
        }
    }

    void clone(Tensor<DataType> tensor)
    {
        if (this->data_size_in_bytes != tensor.data_size_in_bytes)
        {
            // throw std::runtime_error("Tensor sizes do not match during clone");
            std::cout << "Tensor sizes do not match during clone" << std::endl;

        }
        this->shape.clear();
        for (size_t i = 0; i < tensor.shape.size(); i++)
        {
            this->shape.push_back(tensor.shape[i]);
        }
        this->data_size_in_elements = tensor.data_size_in_elements;
        this->data_size_in_bytes = tensor.data_size_in_bytes;
        for (uint64_t i = 0; i < this->data_size_in_elements; i += SIMD_WIDTH)
        {
            STORE(this->data + i, LOAD(tensor.data + i));
        }
    }

    template <typename Odata = DataType>
    Tensor<Odata,HVMLVULKAN> sendToVulkan()
    {
        if (this->device.device_type.i == KHVMLVULKAN.i)
        {
            return *(Tensor<Odata,HVMLVULKAN>*)this;
        }

        this->data_size_in_bytes = this->data_size_in_elements * sizeof(Odata);

        Odata* data;

        auto allocerror = cudaMalloc((void**)&data, this->total_original_allocated_bytes);

        if (allocerror != cudaSuccess)
        {
            std::cout << "cudaMalloc failed: " << cudaGetErrorString(allocerror) << std::endl;
        }

        auto memerror = cudaMemcpy(data, (DataType*)this->data, this->data_size_in_bytes, cudaMemcpyHostToDevice);

        if (memerror != cudaSuccess)
        {
            std::cout << "cudaMemcpy failed: " << cudaGetErrorString(memerror) << std::endl;
        }

        // create vulkan tensor info
    
        this->data = (DataType*)data;
        this->device.device_type.i = KHVMLVULKAN.i;


        return *(Tensor<Odata,HVMLVULKAN>*)(this);
        
       
    }

   
    Tensor<DataType> receiveFromVulkan()
    {
        if (this->device.device_type.i == KHVMLCPU.i)
        {
            return *(Tensor<DataType>*)this;
        }
        // create host tensor
        DataType* host_data = (DataType*)malloc(this->data_size_in_bytes);

        // copy data from vulkan tensor to host tensor
        auto error = cudaMemcpy(host_data,this->data, this->data_size_in_bytes, cudaMemcpyDeviceToHost);

        if (error != cudaSuccess)
        {
            std::cout << "cudaMemcpy failed: " << cudaGetErrorString(error) << std::endl;
        }
        // return host tensor
        this->data = host_data;
        this->device.device_type.i = KHVMLCPU.i;

        // std::cout << "receiveFromVulkan: " << this->data_size_in_bytes << std::endl;
        // std::cout << "this->data: " << *(this->data) << std::endl;

        return *(Tensor<DataType>*)this;

    }


    void unloadVKBuffer(Tensor<float> &buffer)
    {

        if (this->device.device_type.i == KHVMLCPU.i)
        {
            buffer.clone(*(Tensor<DataType>*)this);
            return;
        }
        // copy data from vulkan tensor to host tensor
        auto error = cudaMemcpy(buffer.data, this->data, this->data_size_in_bytes, cudaMemcpyDeviceToHost);

        if (error != cudaSuccess)
        {
            std::cout << "cudaMemcpy failed: " << cudaGetErrorString(error) << std::endl;
        }
    }

    void loadVKBuffer(Tensor<float> &buffer)
    {
        if (this->device.device_type.i == KHVMLCPU.i)
        {
            this->clone(*(Tensor<DataType>*)&buffer);
            return;
        }
        // copy data from vulkan tensor to host tensor
        auto error = cudaMemcpy(this->data, buffer.data, this->data_size_in_bytes, cudaMemcpyHostToDevice);

        if (error != cudaSuccess)
        {
            std::cout << "cudaMemcpy failed: " << cudaGetErrorString(error) << std::endl;
        }
    }

    Tensor(std::vector<uint64_t> shape)
    {

        // copy the shape
        this->shape.clear();
        for (size_t i = 0; i < shape.size(); i++)
        {
            this->shape.push_back(shape[i]);
        }
        this->data_size_in_elements = get_data_size_in_elements(this->shape);
        this->data_size_in_bytes = get_data_size_in_bytes();
        this->total_original_allocated_bytes = this->data_size_in_bytes;

        // make sure alignment is correct to 64 bytes
        // malloc
        this->data = (DataType *)aligned_alloc(ALIGNMENT, this->data_size_in_bytes);
    }

    Tensor(std::vector<uint64_t> shape, DataType value)
    {
        // call the other constructor

        this->shape.clear();
        for (size_t i = 0; i < shape.size(); i++)
        {
            // std::cout << "shape: " << shape[i] << std::endl;
            if (shape[i] > 5960578998)
            {
                // throw std::runtime_error("Tensor size is too large");
                std::cout << "Tensor size is too large" << std::endl;
            }
            this->shape.push_back(shape[i]);
        }
        this->data_size_in_elements = get_data_size_in_elements(this->shape);
        this->data_size_in_bytes = get_data_size_in_bytes();
        this->total_original_allocated_bytes = this->data_size_in_bytes;
        assert(this->data_size_in_bytes > 0);
        // make sure alignment is correct to 64 bytes
        // malloc
        this->data = (DataType *)aligned_alloc(ALIGNMENT, this->data_size_in_bytes);
        this->fill(value);
    }

    Tensor(std::vector<uint64_t> shape, DataType *data)
    {
        this->shape.clear();
        for (size_t i = 0; i < shape.size(); i++)
        {
            this->shape.push_back(shape[i]);
        }
        this->data_size_in_elements = get_data_size_in_elements(this->shape);
        this->data_size_in_bytes = get_data_size_in_bytes();
        this->total_original_allocated_bytes = this->data_size_in_bytes;
        this->data = data;
    }

    // copy constructor
    Tensor(const Tensor<DataType> &tensor)
    {
        this->shape.clear();
        for (size_t i = 0; i < tensor.shape.size(); i++)
        {
            this->shape.push_back(tensor.shape[i]);
        }
        this->data_size_in_elements = tensor.data_size_in_elements;
        this->data_size_in_bytes = tensor.data_size_in_bytes;
        this->data = tensor.data;
        this->device.device_type.i = tensor.device.device_type.i;
        this->total_original_allocated_bytes = tensor.total_original_allocated_bytes;
    }

    void add(Tensor<float,HVMLCPU> &tensor, Tensor<float,HVMLCPU> &result);
    void add(Tensor<float,HVMLVULKAN> &tensor, Tensor<float,HVMLVULKAN> &result);
    void add(Tensor<float,HVMLDYNAMIC> &tensor, Tensor<float,HVMLDYNAMIC> &result){
        //assert all on same device
        assert(this->device.device_type.i == tensor.device.device_type.i && this->device.device_type.i == result.device.device_type.i);
        // dynamic routing
        if (this->device.device_type.i == KHVMLCPU.i){
            ((Tensor<float,HVMLCPU>*)this)->add(*(Tensor<float,HVMLCPU>*)&tensor, *(Tensor<float,HVMLCPU>*)&result);
        }
        else if (this->device.device_type.i == KHVMLVULKAN.i){
            ((Tensor<float,HVMLVULKAN>*)this)->add(*(Tensor<float,HVMLVULKAN>*)&tensor, *(Tensor<float,HVMLVULKAN>*)&result);
        }
        else{
            std::cout << "device add not implemented yet" << std::endl;
        }
    }
    

    void multiply(Tensor<DataType> &tensor, Tensor<DataType> &result)
    {
        if (this->device.device_type.i == KHVMLCPU.i){
// #pragma omp parallel for
        for (uint64_t i = 0; i < this->data_size_in_elements; i += SIMD_WIDTH)
        {
            STORE(result.data + i, MULTIPLY(LOAD(this->data + i), LOAD(tensor.data + i)));
        }
        }
        else{
            auto stream_id = 0;
            const int CHUNKSIZE = 256;
            auto kernalparams = vuda::dim3(this->data_size_in_elements/CHUNKSIZE, 1, 1);
            vuda::launchKernel("./shaders/multiply.glsl.spv", "main", stream_id, kernalparams, this->data_size_in_elements, CHUNKSIZE, this->data, tensor.data, result.data);
            vuda::streamSynchronize(stream_id);
        }
    }

    void multiply(float input, Tensor<DataType> &result)
    {
        if (this->device.device_type.i == KHVMLCPU.i){
        // #pragma omp parallel for
        for (uint64_t i = 0; i < this->data_size_in_elements; i += SIMD_WIDTH)
        {
            STORE(result.data + i, MULTIPLY(LOAD(this->data + i), SET1(input)));
        }
        }
        else{
            auto stream_id = 0;
            const int CHUNKSIZE = 256;
            auto kernalparams = vuda::dim3(this->data_size_in_elements/CHUNKSIZE, 1, 1);
            vuda::launchKernel("./shaders/multiply1.glsl.spv", "main", stream_id, kernalparams, this->data_size_in_elements, CHUNKSIZE,input, this->data, result.data);
            vuda::streamSynchronize(stream_id);
        }
    }

   

    void reshape(std::vector<uint64_t> new_shape)
    {

        this->shape.clear();
        ulong newsize = 1;
        for (size_t i = 0; i < new_shape.size(); i++)
        {
            this->shape.push_back(new_shape[i]);
            newsize *= new_shape[i];
        }
        assert(newsize == this->data_size_in_elements);
        this->data_size_in_elements = newsize;
        this->data_size_in_bytes = get_data_size_in_bytes();
    }

    void unsafereshape(std::vector<uint64_t> new_shape)
    {

        this->shape.clear();
        ulong newsize = 1;
        for (size_t i = 0; i < new_shape.size(); i++)
        {
            this->shape.push_back(new_shape[i]);
            newsize *= new_shape[i];
        }
        this->data_size_in_elements = newsize;
        this->data_size_in_bytes = get_data_size_in_bytes();
    }

    void matmul(Tensor<float,HVMLVULKAN> &in, Tensor<float,HVMLVULKAN> &result)
    {
        auto A = this->data;
        auto B = in.data;
        auto C = result.data;

        // Dimensions
        const long IN = in.shape[2];
        const long OUT = result.shape[2];

        long BBT = 1;

        if (in.shape.size() > 1)
        {
            for (int i = in.shape.size() - 2; i >= 0; i--)
            {
                BBT *= in.shape[i];
            }
        }

        // confirm result length
        this->data_size_in_bytes = get_data_size_in_bytes();
        // std::cout << "result.data_size_in_elements: " << result.data_size_in_elements << std::endl;

        assert(result.data_size_in_elements == BBT * OUT);

        // Parallel computation using vulkan


        const int stream_id = 0;

        const int BLOCKSIZE = 1;

        assert(OUT % BLOCKSIZE == 0);

        vuda::dim3 kernalparams = vuda::dim3(BBT, OUT, 1);
        vuda::launchKernel("./shaders/matmul.glsl.spv", "main", stream_id, kernalparams, BBT,IN,OUT, BLOCKSIZE, B, A, C);
        
        vuda::streamSynchronize(stream_id);
    }

    void matmul(Tensor<float,HVMLVULKAN> &aor,Tensor<float,HVMLVULKAN> &aoo,Tensor<float,HVMLVULKAN> &in, Tensor<float,HVMLVULKAN> &result)
    {
        result.fill(0.0f);
        uint8_t* A = (uint8_t*)this->data;
        float* Ar = (float*)aor.data;
        float* Ao = (float*)aoo.data;
        float* B = (float*)in.data;
        float* C = (float*)result.data;

        // Dimensions
        const long IN = in.shape[2];
        const long OUT = result.shape[2];

        // std::cout << "IN: " << IN << std::endl;
        // std::cout << "OUT: " << OUT << std::endl;

        long BBT = in.shape[0] * in.shape[1];

       

        // confirm result length
        // std::cout << "result.data_size_in_elements: " << result.data_size_in_elements << std::endl;

        // assert(result.data_size_in_elements == BBT * OUT);
        // std::cout << "BBT: " << BBT << std::endl;
        // Parallel computation using vulkan
       
        const int stream_id = 0;

        const int BLOCKSIZE = 64;

        assert(OUT % BLOCKSIZE == 0);

        

        vuda::dim3 kernalparams = vuda::dim3(BBT, OUT/(32), IN/(32));
        vuda::launchKernel("./shaders/matmul8.glsl.spv", "main", stream_id, kernalparams, BBT,IN,OUT, BLOCKSIZE, B, A, Ar, Ao, C);
        
        vuda::streamSynchronize(stream_id);
    }

    void matmul(Tensor<float> &in, Tensor<float> &result, bool thisisbf16 = false)
    {
        // Pointers to the data

        // std::cout << "Is this bf16? " << thisisbf16 << std::endl;
        // A is float* or bf16*,

        // A is either bf16* or float*, check if it is bf16*

        auto B = in.data;
        float *C = result.data;

        // Dimensions
        const long IN = in.shape[2];
        const long OUT = result.shape[2];

        long BBT = 1;

        if (in.shape.size() > 1)
        {
            for (int i = in.shape.size() - 2; i >= 0; i--)
            {
                BBT *= in.shape[i];
            }
        }

        // confirm result length
        // std::cout << "result.data_size_in_elements: " << result.data_size_in_elements << std::endl;
        // std::cout << "BBT: " << BBT << std::endl;
        assert(result.data_size_in_elements == BBT * OUT);

        if (thisisbf16)
        {
            bfloat16 *A = (bfloat16 *)this->data;
// Parallel computation
// #pragma omp parallel for collapse(2) schedule(dynamic, 32) shared(A, B, C)
            for (long i = 0; i < OUT; i += 1)
            {

                for (long bbj = 0; bbj < BBT; bbj += 1)
                {

                    auto acc = SET1(0.0f);
#pragma unroll(16)
                    for (long k = 0; k < IN; k += 32) // let intrinsics handle the unrolling
                    {

                        acc = DOTBF16(
                            LOADBF16(&A[i * IN + k]),
                            LOADFP32BF16(B + bbj * IN + k),
                            acc);
                    }
                    C[bbj * OUT + i] = REDUCE(acc);
                }
            }
        }
        else
        {
            float *A = (float *)this->data;
// Parallel computation for float tensors
// #pragma omp parallel for collapse(2) schedule(dynamic, 32) shared(A, B, C)
            for (long i = 0; i < OUT; i += 1)
            {

                for (long bbj = 0; bbj < BBT; bbj += 1)
                {

                    auto acc = SET1(0.0f);
#pragma unroll(16)
                    for (long k = 0; k < IN; k += 32) // let intrinsics handle the unrolling
                    {

                        acc = DOTBF16F32(
                            LOADFP32BF16(A + i * IN + k),
                            LOADFP32BF16(B + bbj * IN + k),
                            acc);
                    }
                    C[bbj * OUT + i] = REDUCE(acc);
                }
            }
        }
    }

    Tensor<DataType, HVMLCPU> cpu () {
        if (this->device.device_type.i == KHVMLCPU.i){
            return (Tensor<DataType, HVMLCPU>*)this;
        }
        else{
            throw std::runtime_error("Not implemented");
        }
        
    }

    void matmul(Tensor<float,HVMLCPU> &Art, Tensor<float,HVMLCPU> &Aot,
                Tensor<float,HVMLCPU> &Bt, Tensor<float,HVMLCPU> &Ct);
    void matmul(Tensor<float> &Art, Tensor<float> &Aot,
                Tensor<float> &Bt, Tensor<float> &Ct){
                    // test all are on same device
                    assert(this->device.device_type.i == Art.device.device_type.i && this->device.device_type.i == Aot.device.device_type.i && this->device.device_type.i == Bt.device.device_type.i && this->device.device_type.i == Ct.device.device_type.i);
                    // dynamic routing
                    if (this->device.device_type.i == KHVMLCPU.i){
                        ((Tensor<uint8_t,HVMLCPU>*)this)->matmul(*(Tensor<float,HVMLCPU>*)&Art, *(Tensor<float,HVMLCPU>*)&Aot, *(Tensor<float,HVMLCPU>*)&Bt, *(Tensor<float,HVMLCPU>*)&Ct);
                    }
                    else if (this->device.device_type.i == KHVMLVULKAN.i){
                        ((Tensor<uint8_t,HVMLVULKAN>*)this)->matmul(*(Tensor<float,HVMLVULKAN>*)&Art, *(Tensor<float,HVMLVULKAN>*)&Aot, *(Tensor<float,HVMLVULKAN>*)&Bt, *(Tensor<float,HVMLVULKAN>*)&Ct);
                    }
                    else{
                        std::cout << "device matmul not implemented yet" << std::endl;
                    }
                }
    
    DataType sum()
    {
        DataType sum = 0;
        for (int i = 0; i < this->data_size_in_elements; i++)
        {
            sum += this->data[i];
        }
        return sum;
    }

    DataType expsum()
    {
        DataType sum = 0;
        for (int i = 0; i < this->data_size_in_elements; i++)
        {
            sum += exp(this->data[i]);
        }
        return sum;
    }

    void softmax()
    {
        // Pointers to the data
        auto A = this->data;

        // Dimensions
        auto dim = this->data_size_in_elements;

        // Parallel computation

        auto sum = this->expsum();

        auto sumhold = SET1(sum);

        for (long i = 0; i < dim; i += SIMD_WIDTH)
        {
            STORE(A + i, DIVIDE(EXP(LOAD(A + i)), sumhold));
        }
    }

    void gather(std::vector<std::vector<ulong>> indicies, Tensor<DataType> &buffer)
    {
        // Assume copy

        // Pointers to the data
        auto A = this->data;

        // Dimensions
        auto BATCH = indicies.size();
        auto T = indicies[0].size();
        auto OUT = this->shape[1];

        // print initial shape
        // std::cout << "buffer.size: " << buffer.data_size_in_elements << std::endl;

        buffer.shape.clear();
        buffer.shape.push_back(BATCH);
        buffer.shape.push_back(T);
        buffer.shape.push_back(OUT);
        buffer.data_size_in_elements = get_data_size_in_elements(buffer.shape);
        buffer.data_size_in_bytes = buffer.get_data_size_in_bytes();

        // std::cout << "BATCH: " << BATCH << std::endl;
        // std::cout << "T: " << T << std::endl;
        // std::cout << "OUT: " << OUT << std::endl;
        // std::cout << "out.size: " << BATCH*T*OUT << std::endl;
        // std::cout << "buffer.data_size_in_elements: " << buffer.data_size_in_elements << std::endl;
        // std::cout << "buffer.data_size_in_bytes: " << buffer.data_size_in_bytes << std::endl;

        // Parallel computation
        // #pragma omp parallel for collapse(2) schedule(dynamic, 32) shared(A, B)
        if(buffer.device.device_type.i == KHVMLCPU.i){
        for (u_int64_t i = 0; i < BATCH; i += 1)
        {

            for (u_int64_t j = 0; j < T; j += 1)
            {

                for (u_int64_t k = 0; k < OUT; k += SIMD_WIDTH)
                {
                    auto acc = LOAD(A + indicies[i][j] * OUT + k);
                    STORE(&buffer.data[i * T * OUT + j * OUT + k], acc);
                }
            }
        }}
        else{
            // Parallel computation
            std::cout << "Gather on GPU not implemented yet" << std::endl;
        }
    }

    template <typename T>
    void layernorm(const Tensor<DataType,T> &weight, const Tensor<DataType,T> &bias, const Tensor<DataType,T> &result, float eps = 1e-5)
    {
        uint64_t BTT = 1;

        if (this->shape.size() > 1)
        {
            for (int i = this->shape.size() - 2; i >= 0; i--)
            {
                BTT *= this->shape[i];
            }
        }

        uint64_t OUT = this->shape[this->shape.size() - 1];

        // confirm result length
        // std::cout << "result.data_size_in_elements: " << result.data_size_in_elements << std::endl;
        // std::cout << "this->data_size_in_elements: " << this->data_size_in_elements << std::endl;
        // std::cout << "OUT:" << OUT << std::endl;
        // std::cout << "BTT:" << BTT << std::endl;
        assert(result.data_size_in_elements == this->data_size_in_elements);

        // Pointers to the data
        auto A = this->data;
        auto W = weight.data;
        auto B = bias.data;
        auto C = result.data;

        auto devicemap = this->device.device_type.i + weight.device.device_type.i + bias.device.device_type.i + result.device.device_type.i;

        assert(devicemap % 4 == 0); // assert all tensors are on the same device
        // std::cout << "devicemap: " << devicemap << std::endl;
        if (this->device.device_type.i == KHVMLCPU.i){
            for (uint64_t i = 0; i < BTT; i += 1)
            {
                float mean = 0.0f;
                float var = 0.0f;
                for (uint64_t j = 0; j < OUT; j += SIMD_WIDTH)
                {
                    mean += REDUCE(LOAD(A + i * OUT + j));
                }
                mean /= OUT;

                for (uint64_t j = 0; j < OUT; j += SIMD_WIDTH)
                {
                    auto acc = ADD(LOAD(A + i * OUT + j), SET1(-1.0f * mean));
                    acc = MULTIPLY(acc, acc);
                    var += REDUCE(acc);
                }
                var /= OUT;

                for (uint64_t j = 0; j < OUT; j += SIMD_WIDTH)
                {
                    // std::cout << "level1: " <<j<< std::endl;
                    auto acc = ADD(LOAD(A + i * OUT + j), SET1(-1.0f * mean));
                    // std::cout << "level1mm: " <<j<< std::endl;
                    acc = MULTIPLY(acc, SET1(1.0f / sqrt(var + eps)));
                    // std::cout << "level1acc: " <<j<< std::endl;

                    STORE(C + i * OUT + j, MULTADD(LOAD(W + j), acc, LOAD(B + j)));
                    // std::cout << "level1store: " <<j<< std::endl;
                }
            }
        }else{
            // Parallel computation
            const int stream_id = 0;
            // std::cout << "BTT: " << BTT << std::endl;

            const int BLOCKSIZE = 32;

            assert(OUT % BLOCKSIZE == 0);

            vuda::dim3 kernalparams = vuda::dim3(BTT, 1, 1);
            vuda::launchKernel("./shaders/layernorm.glsl.spv", "main", stream_id, kernalparams, BTT,OUT, BLOCKSIZE, A, W, B, C);
            
            vuda::streamSynchronize(stream_id);
        }
    }

    void lerp(Tensor<DataType> &tensor1, Tensor<DataType> &tensor2, Tensor<DataType> &result)
    {
        // Pointers to the data, assume (this) is the weights, lerp across last dimension
        auto A = tensor1.data;
        auto B = tensor2.data;
        auto C = result.data;

        result.unsafereshape(tensor2.shape);

        // Dimensions

        auto IN = this->shape[0];

        if (this->device.device_type.i == KHVMLCPU.i){
            

// #pragma omp parallel for schedule(static, 32)
        for (uint64_t i = 0; i < result.data_size_in_elements; i += SIMD_WIDTH)
        {
            STORE(C + i, MULTADD(LOAD(B + i), LOAD(this->data + (i % IN)), MULTIPLY(LOAD(A + i), SET1(1.0f) - LOAD(this->data + (i % IN)))));
        }}

        else{
            // Parallel computation
            const int stream_id = 0;

            const int BLOCKSIZE = 32;

            const int entries = result.data_size_in_elements;

            vuda::dim3 kernalparams = vuda::dim3(entries/BLOCKSIZE, 1, 1);
            vuda::launchKernel("./shaders/lerp.glsl.spv", "main", stream_id, kernalparams, entries, BLOCKSIZE, IN, this->data, B, A, C);
            
            vuda::streamSynchronize(stream_id);
        }

        // Parallel computation
    }

    void relu(Tensor<DataType> &result)
    {
        // Pointers to the data
        auto A = this->data;
        auto C = result.data;

        // Dimensions
        result.unsafereshape(this->shape);

        if (this->device.device_type.i == KHVMLCPU.i){

// #pragma omp parallel for schedule(static, 256)
        for (uint64_t i = 0; i < this->data_size_in_elements; i += SIMD_WIDTH)
        {
            STORE(C + i, MAX(LOAD(A + i), SET1(0.0f)));
        }
        }
        else{
            auto stream_id = 0;
            const int CHUNKSIZE = 128;
            auto kernalparams = vuda::dim3(this->data_size_in_elements/CHUNKSIZE, 1, 1);
            vuda::launchKernel("./shaders/relu.spv", "main", stream_id, kernalparams, this->data_size_in_elements, CHUNKSIZE, this->data, result.data);
            vuda::streamSynchronize(stream_id);
        }

        // Parallel computation
    }

    void relusquare(Tensor<DataType> &result)
    {
        // Pointers to the data
        auto A = this->data;
        auto C = result.data;

        // Dimensions
        result.unsafereshape(this->shape);

        if (this->device.device_type.i == KHVMLCPU.i){

// #pragma omp parallel for schedule(static, 32)
        for (uint64_t i = 0; i < this->data_size_in_elements; i += SIMD_WIDTH)
        {
            auto a = MAX(LOAD(A + i), SET1(0.0f));
            STORE(C + i, MULTIPLY(a, a));
        }
        }
        else{
            auto stream_id = 0;
            const auto B = this->shape[0];
            const auto T = this->shape[1];
            const auto C = this->shape[2];
            auto kernalparams = vuda::dim3(B, T, 1);
            vuda::launchKernel("./shaders/relusquare.glsl.spv", "main", stream_id, kernalparams, B,T,C, this->data, result.data);
            vuda::streamSynchronize(stream_id);
        }

        // Parallel computation
    }

    void sigmoid(Tensor<DataType> &result)
    {
        // Pointers to the data
        auto A = this->data;
        auto C = result.data;

        // Dimensions
        result.unsafereshape(this->shape);

        // std::cout << "result.sigmoid.data_size_in_elements: " << result.data_size_in_elements << std::endl;
if (this->device.device_type.i == KHVMLCPU.i){
// #pragma omp parallel for schedule(static, 32)
        for (uint64_t i = 0; i < this->data_size_in_elements; i += SIMD_WIDTH)
        {
            STORE(C + i, DIVIDE(SET1(1.0f), ADD(SET1(1.0f), EXP(MULTIPLY(SET1(-1.0f), LOAD(A + i))))));
        }
        }else{
            auto stream_id = 0;
            const int CHUNKSIZE = 256;
            auto kernalparams = vuda::dim3(this->data_size_in_elements/CHUNKSIZE, 1, 1);
            vuda::launchKernel("./shaders/sigmoid.glsl.spv", "main", stream_id, kernalparams, this->data_size_in_elements, CHUNKSIZE, this->data, result.data);
            vuda::streamSynchronize(stream_id);
        }
        // Parallel computation
    }

    void sigmoidmult(Tensor<DataType> &mult,Tensor<DataType> &result)
    {
        // Pointers to the data
        auto A = this->data;
        auto B = mult.data;
        auto C = result.data;

        // Dimensions
        result.unsafereshape(this->shape);

        // std::cout << "result.sigmoid.data_size_in_elements: " << result.data_size_in_elements << std::endl;
if (this->device.device_type.i == KHVMLCPU.i){
// #pragma omp parallel for schedule(static, 32)
        for (uint64_t i = 0; i < this->data_size_in_elements; i += SIMD_WIDTH)
        {
            STORE(C + i,DIVIDE(LOAD(B+i), ADD(SET1(1.0f), EXP(MULTIPLY(SET1(-1.0f), LOAD(A + i))))));
        }
        }else{
            auto stream_id = 0;
            const int CHUNKSIZE = 256;
            auto kernalparams = vuda::dim3(this->data_size_in_elements/CHUNKSIZE, 1, 1);
            vuda::launchKernel("./shaders/sigmoidmult.glsl.spv", "main", stream_id, kernalparams, this->data_size_in_elements, CHUNKSIZE, this->data, mult.data, result.data);
            vuda::streamSynchronize(stream_id);
        }
        // Parallel computation
    }

    void swishmult(Tensor<DataType> &mult,Tensor<DataType> &result)
    {
        // Pointers to the data
        auto A = this->data;
        auto B = mult.data;
        auto C = result.data;

        // Dimensions
        result.unsafereshape(this->shape);

        // std::cout << "result.sigmoid.data_size_in_elements: " << result.data_size_in_elements << std::endl;
if (this->device.device_type.i == KHVMLCPU.i){
// #pragma omp parallel for schedule(static, 32)
        for (uint64_t i = 0; i < this->data_size_in_elements; i += SIMD_WIDTH)
        {
            STORE(C + i,DIVIDE(MULTIPLY(LOAD(B+i),LOAD(A + i)), ADD(SET1(1.0f), EXP(MULTIPLY(SET1(-1.0f), LOAD(A + i))))));
        }
        }else{
            auto stream_id = 0;
            const int CHUNKSIZE = 256;
            auto kernalparams = vuda::dim3(this->data_size_in_elements/CHUNKSIZE, 1, 1);
            vuda::launchKernel("./shaders/swishmult.glsl.spv", "main", stream_id, kernalparams, this->data_size_in_elements, CHUNKSIZE, this->data, mult.data, result.data);
            vuda::streamSynchronize(stream_id);
        }
        // Parallel computation
    }
    void wkv5(Tensor<float,HVMLVULKAN> &r, Tensor<float,HVMLVULKAN> &k, Tensor<float,HVMLVULKAN> &v, Tensor<float,HVMLVULKAN> &w, Tensor<float,HVMLVULKAN> &u, Tensor<float,HVMLVULKAN> &y);
    void wkv5(Tensor<float,HVMLCPU> &r, Tensor<float,HVMLCPU> &k, Tensor<float,HVMLCPU> &v, Tensor<float,HVMLCPU> &w, Tensor<float,HVMLCPU> &u, Tensor<float,HVMLCPU> &y);
    void wkv5(Tensor<float,HVMLDYNAMIC> &r, Tensor<float,HVMLDYNAMIC> &k, Tensor<float,HVMLDYNAMIC> &v, Tensor<float,HVMLDYNAMIC> &w, Tensor<float,HVMLDYNAMIC> &u, Tensor<float,HVMLDYNAMIC> &y){
        if (this->device.device_type.i == KHVMLCPU.i){
            ((Tensor<float,HVMLCPU>*)this)->wkv5(*(Tensor<float,HVMLCPU>*)&r, *(Tensor<float,HVMLCPU>*)&k, *(Tensor<float,HVMLCPU>*)&v, *(Tensor<float,HVMLCPU>*)&w, *(Tensor<float,HVMLCPU>*)&u, *(Tensor<float,HVMLCPU>*)&y);
        }
        else if (this->device.device_type.i == KHVMLVULKAN.i){
            ((Tensor<float,HVMLVULKAN>*)this)->wkv5(*(Tensor<float,HVMLVULKAN>*)&r, *(Tensor<float,HVMLVULKAN>*)&k, *(Tensor<float,HVMLVULKAN>*)&v, *(Tensor<float,HVMLVULKAN>*)&w, *(Tensor<float,HVMLVULKAN>*)&u, *(Tensor<float,HVMLVULKAN>*)&y);
        }
        else{
            std::cout << "device wkv5 not implemented yet" << std::endl;
        }
    }

    // [] operator
    Tensor<DataType> operator[](const uint64_t index)
    {
        uint64_t stride = 1;
        std::vector<uint64_t> new_shape = {};
        for (size_t i = 1; i < this->shape.size(); i++)
        {
            stride *= this->shape[i];
            new_shape.push_back(this->shape[i]);
        }

        return Tensor<DataType>(new_shape, this->data + index * stride);
    }

    // << operator for printing
    friend std::ostream &operator<<(std::ostream &os, const Tensor &tensor)
    {
        DataType* outdata;

        if (tensor.device.device_type.i == KHVMLCPU.i)
        {
            outdata = tensor.data;
        }
        else
        {
            outdata = (DataType *)aligned_alloc(ALIGNMENT, tensor.data_size_in_bytes);
            auto error = cudaMemcpy(outdata, tensor.data, tensor.data_size_in_bytes, cudaMemcpyDeviceToHost);

            if (error != cudaSuccess)
            {
                std::cout << "cudaMemcpy failed: " << cudaGetErrorString(error) << std::endl;
            }
        }

        if (tensor.shape.size() == 0)
        {
            os << outdata[0];
            return os;
        }

        os << "Tensor(";
        for (int i = 0; i < std::min(tensor.data_size_in_elements, 4UL); i++)
        {
            os << outdata[i];
            if (i != tensor.data_size_in_elements - 1)
            {
                os << ", ";
            }
        }
        if (tensor.data_size_in_elements > 4)
        {
            os << ", ...";
            if (tensor.data_size_in_elements > 8)
            {
                os << ", ";
                for (int i = tensor.data_size_in_elements - 4; i < tensor.data_size_in_elements; i++)
                {
                    os << outdata[i];
                    if (i != tensor.data_size_in_elements - 1)
                    {
                        os << ", ";
                    }
                }
            }

        }
        os << ", shape=(";
        for (int i = 0; i < tensor.shape.size(); i++)
        {
            os << tensor.shape[i];
            if (i != tensor.shape.size() - 1)
            {
                os << ", ";
            }
        }
        os << "))";
        return os;
    }

    // here we make it so, that if shape is {} then it is treated as a scalar by the compiler
    // this is useful for the [] operator
    operator DataType()
    {
        if (this->shape.size() != 0)
        {
            throw std::runtime_error("Tensor is not a scalar");
        }
        return this->data[0];
    }

    
};

#include "hvml/operations/avx512/matmul8.hpp"
#include "hvml/operations/generic/genericadd.hpp"
#include "hvml/operations/vulkan/vulkanadd.hpp"
#include "hvml/operations/vulkan/vulkanwkv5.hpp"
#endif