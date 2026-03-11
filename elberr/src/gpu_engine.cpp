#include "gpu_engine.hpp"
#include <cmath>
#include <numeric>
#include <iostream>
#include <sstream>

#ifdef ELBERR_HIP
#include <hip/hip_runtime.h>

// HIP kernels
__global__ void hipConfidencePropagate(double* confidences, const double* strengths,
                                        const int* inputIndices, const int* inputOffsets,
                                        int numRules, int numAtoms) {
    int rid = blockIdx.x * blockDim.x + threadIdx.x;
    if (rid >= numRules) return;

    int start = inputOffsets[rid];
    int end = inputOffsets[rid + 1];
    double minConf = 1.0;
    for (int i = start; i < end; ++i) {
        int atomIdx = inputIndices[i];
        if (atomIdx >= 0 && atomIdx < numAtoms) {
            minConf = fmin(minConf, confidences[atomIdx]);
        }
    }
    confidences[numAtoms + rid] = minConf * strengths[rid];
}

__global__ void hipBatchEval(const double* atomValues, const int* formulas,
                              const int* formulaOffsets, double* results,
                              int numFormulas, int numAtoms) {
    int fid = blockIdx.x * blockDim.x + threadIdx.x;
    if (fid >= numFormulas) return;

    int start = formulaOffsets[fid];
    int end = formulaOffsets[fid + 1];
    double val = 1.0;
    for (int i = start; i < end; ++i) {
        int atomIdx = formulas[i];
        if (atomIdx >= 0 && atomIdx < numAtoms) {
            val = fmin(val, atomValues[atomIdx]);
        }
    }
    results[fid] = val;
}

__global__ void hipCosineSim(const double* a, const double* b, double* partialDot,
                              double* partialNormA, double* partialNormB, int n) {
    int tid = blockIdx.x * blockDim.x + threadIdx.x;
    if (tid >= n) return;
    atomicAdd(partialDot, a[tid] * b[tid]);
    atomicAdd(partialNormA, a[tid] * a[tid]);
    atomicAdd(partialNormB, b[tid] * b[tid]);
}
#endif // ELBERR_HIP

namespace elberr {

GPUEngine::GPUEngine() {
    initGPU();
}

GPUEngine::~GPUEngine() {}

void GPUEngine::initGPU() {
#ifdef ELBERR_HIP
    int deviceCount = 0;
    hipError_t err = hipGetDeviceCount(&deviceCount);
    if (err == hipSuccess && deviceCount > 0) {
        hipDeviceProp_t prop;
        hipGetDeviceProperties(&prop, 0);
        gpuAvailable_ = true;
        deviceName_ = prop.name;
        std::cout << "[GPU] AMD GPU detected: " << deviceName_ << "\n";
        std::cout << "[GPU] Compute units: " << prop.multiProcessorCount << "\n";
    } else {
        std::cout << "[GPU] No AMD GPU found, using CPU fallback\n";
    }
#else
    std::cout << "[GPU] Built without HIP support, using CPU fallback\n";
    std::cout << "[GPU] Rebuild with -DELBERR_ENABLE_HIP=ON for AMD GPU acceleration\n";
#endif
}

void GPUEngine::confidencePropagate(std::vector<double>& confidences,
                                     const std::vector<double>& ruleStrengths,
                                     const std::vector<std::vector<int>>& ruleInputs) {
    if (ruleStrengths.empty() || confidences.empty()) return;

#ifdef ELBERR_HIP
    if (gpuAvailable_) {
        // Flatten ruleInputs
        std::vector<int> flatInputs;
        std::vector<int> offsets;
        offsets.push_back(0);
        for (auto& ri : ruleInputs) {
            flatInputs.insert(flatInputs.end(), ri.begin(), ri.end());
            offsets.push_back(static_cast<int>(flatInputs.size()));
        }

        int numAtoms = static_cast<int>(confidences.size());
        int numRules = static_cast<int>(ruleStrengths.size());
        confidences.resize(numAtoms + numRules, 0.0);

        double *d_conf, *d_str;
        int *d_inputs, *d_offsets;
        hipMalloc(&d_conf, confidences.size() * sizeof(double));
        hipMalloc(&d_str, ruleStrengths.size() * sizeof(double));
        hipMalloc(&d_inputs, flatInputs.size() * sizeof(int));
        hipMalloc(&d_offsets, offsets.size() * sizeof(int));

        hipMemcpy(d_conf, confidences.data(), confidences.size() * sizeof(double), hipMemcpyHostToDevice);
        hipMemcpy(d_str, ruleStrengths.data(), ruleStrengths.size() * sizeof(double), hipMemcpyHostToDevice);
        hipMemcpy(d_inputs, flatInputs.data(), flatInputs.size() * sizeof(int), hipMemcpyHostToDevice);
        hipMemcpy(d_offsets, offsets.data(), offsets.size() * sizeof(int), hipMemcpyHostToDevice);

        int threads = 256;
        int blocks = (numRules + threads - 1) / threads;
        hipLaunchKernelGGL(hipConfidencePropagate, dim3(blocks), dim3(threads), 0, 0,
                           d_conf, d_str, d_inputs, d_offsets, numRules, numAtoms);

        hipMemcpy(confidences.data(), d_conf, confidences.size() * sizeof(double), hipMemcpyDeviceToHost);

        hipFree(d_conf); hipFree(d_str); hipFree(d_inputs); hipFree(d_offsets);
        return;
    }
#endif

    // CPU fallback
    size_t numAtoms = confidences.size();
    size_t numRules = ruleStrengths.size();
    confidences.resize(numAtoms + numRules, 0.0);

    for (size_t r = 0; r < numRules; ++r) {
        double minConf = 1.0;
        for (int idx : ruleInputs[r]) {
            if (idx >= 0 && static_cast<size_t>(idx) < numAtoms) {
                minConf = std::min(minConf, confidences[idx]);
            }
        }
        confidences[numAtoms + r] = std::max(0.0, std::min(1.0, minConf * ruleStrengths[r]));
    }
}

void GPUEngine::batchEvaluate(const std::vector<double>& atomValues,
                               const std::vector<std::vector<int>>& formulas,
                               std::vector<double>& results) {
    results.resize(formulas.size());

#ifdef ELBERR_HIP
    if (gpuAvailable_) {
        std::vector<int> flatFormulas;
        std::vector<int> offsets;
        offsets.push_back(0);
        for (auto& f : formulas) {
            flatFormulas.insert(flatFormulas.end(), f.begin(), f.end());
            offsets.push_back(static_cast<int>(flatFormulas.size()));
        }

        double *d_atoms, *d_results;
        int *d_formulas, *d_offsets;
        int numFormulas = static_cast<int>(formulas.size());
        int numAtoms = static_cast<int>(atomValues.size());

        hipMalloc(&d_atoms, atomValues.size() * sizeof(double));
        hipMalloc(&d_results, results.size() * sizeof(double));
        hipMalloc(&d_formulas, flatFormulas.size() * sizeof(int));
        hipMalloc(&d_offsets, offsets.size() * sizeof(int));

        hipMemcpy(d_atoms, atomValues.data(), atomValues.size() * sizeof(double), hipMemcpyHostToDevice);
        hipMemcpy(d_formulas, flatFormulas.data(), flatFormulas.size() * sizeof(int), hipMemcpyHostToDevice);
        hipMemcpy(d_offsets, offsets.data(), offsets.size() * sizeof(int), hipMemcpyHostToDevice);

        int threads = 256;
        int blocks = (numFormulas + threads - 1) / threads;
        hipLaunchKernelGGL(hipBatchEval, dim3(blocks), dim3(threads), 0, 0,
                           d_atoms, d_formulas, d_offsets, d_results, numFormulas, numAtoms);

        hipMemcpy(results.data(), d_results, results.size() * sizeof(double), hipMemcpyDeviceToHost);

        hipFree(d_atoms); hipFree(d_results); hipFree(d_formulas); hipFree(d_offsets);
        return;
    }
#endif

    // CPU fallback
    for (size_t f = 0; f < formulas.size(); ++f) {
        double val = 1.0;
        for (int idx : formulas[f]) {
            if (idx >= 0 && static_cast<size_t>(idx) < atomValues.size()) {
                val = std::min(val, atomValues[idx]);
            }
        }
        results[f] = val;
    }
}

double GPUEngine::cosineSimilarity(const std::vector<double>& a,
                                    const std::vector<double>& b) {
    if (a.size() != b.size() || a.empty()) return 0.0;

#ifdef ELBERR_HIP
    if (gpuAvailable_) {
        int n = static_cast<int>(a.size());
        double *d_a, *d_b, *d_dot, *d_na, *d_nb;
        hipMalloc(&d_a, n * sizeof(double));
        hipMalloc(&d_b, n * sizeof(double));
        hipMalloc(&d_dot, sizeof(double));
        hipMalloc(&d_na, sizeof(double));
        hipMalloc(&d_nb, sizeof(double));

        double zero = 0.0;
        hipMemcpy(d_a, a.data(), n * sizeof(double), hipMemcpyHostToDevice);
        hipMemcpy(d_b, b.data(), n * sizeof(double), hipMemcpyHostToDevice);
        hipMemcpy(d_dot, &zero, sizeof(double), hipMemcpyHostToDevice);
        hipMemcpy(d_na, &zero, sizeof(double), hipMemcpyHostToDevice);
        hipMemcpy(d_nb, &zero, sizeof(double), hipMemcpyHostToDevice);

        int threads = 256;
        int blocks = (n + threads - 1) / threads;
        hipLaunchKernelGGL(hipCosineSim, dim3(blocks), dim3(threads), 0, 0,
                           d_a, d_b, d_dot, d_na, d_nb, n);

        double dot, na, nb;
        hipMemcpy(&dot, d_dot, sizeof(double), hipMemcpyDeviceToHost);
        hipMemcpy(&na, d_na, sizeof(double), hipMemcpyDeviceToHost);
        hipMemcpy(&nb, d_nb, sizeof(double), hipMemcpyDeviceToHost);

        hipFree(d_a); hipFree(d_b); hipFree(d_dot); hipFree(d_na); hipFree(d_nb);

        double denom = std::sqrt(na) * std::sqrt(nb);
        return denom > 0 ? dot / denom : 0.0;
    }
#endif

    // CPU fallback
    double dot = 0, normA = 0, normB = 0;
    for (size_t i = 0; i < a.size(); ++i) {
        dot += a[i] * b[i];
        normA += a[i] * a[i];
        normB += b[i] * b[i];
    }
    double denom = std::sqrt(normA) * std::sqrt(normB);
    return denom > 0 ? dot / denom : 0.0;
}

std::string GPUEngine::info() const {
    std::ostringstream ss;
    ss << "GPU Engine: " << deviceName_;
    if (gpuAvailable_) {
        ss << " (HIP/ROCm active)";
    } else {
        ss << " (CPU mode)";
    }
    return ss.str();
}

} // namespace elberr
