#pragma once
#include <vector>
#include <string>

namespace elberr {

// GPU-accelerated operations for belief propagation
// Uses AMD ROCm/HIP when available, falls back to CPU
class GPUEngine {
public:
    GPUEngine();
    ~GPUEngine();

    bool isGPUAvailable() const { return gpuAvailable_; }
    std::string deviceName() const { return deviceName_; }

    // Propagate confidence values through rules
    // confidences[i] *= ruleStrengths[j] for matching rules
    void confidencePropagate(std::vector<double>& confidences,
                             const std::vector<double>& ruleStrengths,
                             const std::vector<std::vector<int>>& ruleInputs);

    // Batch evaluate: for each formula, compute truth value
    void batchEvaluate(const std::vector<double>& atomValues,
                       const std::vector<std::vector<int>>& formulas,
                       std::vector<double>& results);

    // Cosine similarity between two vectors
    double cosineSimilarity(const std::vector<double>& a,
                            const std::vector<double>& b);

    std::string info() const;

private:
    bool gpuAvailable_ = false;
    std::string deviceName_ = "CPU (fallback)";

    void initGPU();
};

} // namespace elberr
