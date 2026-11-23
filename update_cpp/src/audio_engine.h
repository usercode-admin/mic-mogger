#pragma once
#include <atomic>
#include <mutex>
#include <string>
#include <vector>
#include "dsp.h"

class AudioEngine {
public:
    AudioEngine();
    ~AudioEngine();

    struct Dev { std::string name; int index; };
    std::vector<Dev> listInputs();
    std::vector<Dev> listOutputs();
    bool autoSelect(std::string& inName, std::string& outName);

    bool start(int inIdx, int outIdx);
    void stop();
    bool running() const { return running_.load(); }

    std::atomic<float> gain{1.0f};
    std::atomic<float> lowDb{0.f}, midDb{0.f}, highDb{0.f};
    std::atomic<float> midFreq{1500.f}, hpfFreq{80.f};
    std::atomic<float> compThresh{0.38f}, compRatio{3.8f};
    std::atomic<bool>  buffActive{false};
    std::atomic<bool>  staticOn{true};
    std::atomic<float> meterPeak{0.f};
    std::atomic<float> makeup{1.0f};

    bool loadFile(const std::string& path, std::string& errOut);
    void togglePlay();
    bool isPlaying() const { return playing_.load(); }
    std::string fileName() const;
    int streamSampleRate() const { return sampleRate_; }
    std::string lastError() const;

private:
    static void sCallback(void* user, float* out, const float* in, unsigned frames);
    void process(float* out, const float* in, unsigned frames);
    float noise();

    void* ctx_ = nullptr;
    void* dev_ = nullptr;
    std::atomic<bool> running_{false};
    int sampleRate_ = 48000;

    EQChain eq_;
    Compressor comp_;
    Limiter lim_;
    float noiseAmt_ = 0.012f;
    unsigned rng_ = 0x12345678u;

    mutable std::mutex fileMu_;
    std::vector<float> fileData_;
    double filePos_ = 0.0;
    int fileSr_ = 0;
    std::string fileName_;
    std::atomic<bool> playing_{false};
    mutable std::string lastErr_;
};
