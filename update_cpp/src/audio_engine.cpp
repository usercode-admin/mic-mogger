#define MINIAUDIO_IMPLEMENTATION
#include "miniaudio.h"
#include "audio_engine.h"
#include <cstring>
#include <cmath>

AudioEngine::AudioEngine() {
    ma_context_config cc = ma_context_config_init();
    auto* ctx = new ma_context();
    if (ma_context_init(nullptr, 0, &cc, ctx) != MA_SUCCESS) {
        delete ctx; ctx_ = nullptr; return;
    }
    ctx_ = ctx;
}

AudioEngine::~AudioEngine() {
    stop();
    if (ctx_) { ma_context_uninit((ma_context*)ctx_); delete (ma_context*)ctx_; }
}

std::vector<AudioEngine::Dev> AudioEngine::listInputs() {
    std::vector<Dev> v;
    if (!ctx_) return v;
    ma_device_info* pb; ma_uint32 pbc;
    ma_device_info* cp; ma_uint32 cpc;
    ma_context_get_devices((ma_context*)ctx_, &pb, &pbc, &cp, &cpc);
    for (ma_uint32 i = 0; i < cpc; ++i) v.push_back({cp[i].name, (int)i});
    return v;
}

std::vector<AudioEngine::Dev> AudioEngine::listOutputs() {
    std::vector<Dev> v;
    if (!ctx_) return v;
    ma_device_info* pb; ma_uint32 pbc;
    ma_device_info* cp; ma_uint32 cpc;
    ma_context_get_devices((ma_context*)ctx_, &pb, &pbc, &cp, &cpc);
    for (ma_uint32 i = 0; i < pbc; ++i) v.push_back({pb[i].name, (int)i});
    return v;
}

bool AudioEngine::autoSelect(std::string& inName, std::string& outName) {
    if (!ctx_) return false;
    ma_device_info* pb; ma_uint32 pbc;
    ma_device_info* cp; ma_uint32 cpc;
    ma_context_get_devices((ma_context*)ctx_, &pb, &pbc, &cp, &cpc);
    bool okIn = false, okOut = false;

    for (ma_uint32 i = 0; i < pbc; ++i)
        if (strstr(pb[i].name, "CABLE Input")) { outName = pb[i].name; okOut = true; break; }

    for (ma_uint32 i = 0; i < cpc; ++i)
        if (!strstr(cp[i].name, "CABLE Output")) { inName = cp[i].name; okIn = true; break; }

    return okIn && okOut;
}

void AudioEngine::sCallback(void* user, float* out, const float* in, unsigned frames) {
    static_cast<AudioEngine*>(user)->process(out, in, frames);
}

bool AudioEngine::start(int inIdx, int outIdx) {
    stop();
    if (!ctx_) { lastErr_ = "Audio context not initialized"; return false; }

    ma_context* ctx = (ma_context*)ctx_;
    ma_device_info* pb; ma_uint32 pbc;
    ma_device_info* cp; ma_uint32 cpc;
    ma_context_get_devices(ctx, &pb, &pbc, &cp, &cpc);

    if (inIdx < 0 || inIdx >= (int)cpc) { lastErr_ = "Bad input index"; return false; }
    if (outIdx < 0 || outIdx >= (int)pbc) { lastErr_ = "Bad output index"; return false; }

    ma_device_config cfg = ma_device_config_init(ma_device_type_duplex);
    cfg.capture.pDeviceID  = &cp[inIdx].id;
    cfg.playback.pDeviceID = &pb[outIdx].id;
    cfg.capture.format     = ma_format_f32;
    cfg.playback.format    = ma_format_f32;
    cfg.capture.channels   = 1;
    cfg.playback.channels  = 1;
    cfg.sampleRate         = 0;
    cfg.periodSizeInMilliseconds = 8;
    cfg.performanceProfile = ma_performance_profile_low_latency;
    cfg.dataCallback = [](ma_device* d, void* o, const void* i, ma_uint32 f) {
        sCallback(d->pUserData, (float*)o, (const float*)i, f);
    };
    cfg.pUserData = this;

    auto* dev = new ma_device();
    if (ma_device_init(ctx, &cfg, dev) != MA_SUCCESS) {
        delete dev; lastErr_ = "ma_device_init failed"; return false;
    }

    sampleRate_ = (int)dev->sampleRate;
    eq_.sr = (float)sampleRate_;
    eq_.reconfigure();
    eq_.reset();
    comp_.setSampleRate((float)sampleRate_);
    comp_.reset();

    if (ma_device_start(dev) != MA_SUCCESS) {
        ma_device_uninit(dev); delete dev;
        lastErr_ = "ma_device_start failed"; return false;
    }

    dev_ = dev;
    running_ = true;
    lastErr_.clear();
    return true;
}

void AudioEngine::stop() {
    if (!running_.exchange(false)) return;
    auto* dev = (ma_device*)dev_;
    dev_ = nullptr;
    if (dev) {
        ma_device_stop(dev);
        ma_device_uninit(dev);
        delete dev;
    }
}

float AudioEngine::noise() {
    rng_ = rng_ * 1664525u + 1013904223u;
    return ((float)((rng_ >> 8) & 0x00FFFFFF) / 8388608.0f) - 1.0f;
}

void AudioEngine::process(float* out, const float* in, unsigned frames) {
    const bool buff = buffActive.load();
    const bool play = playing_.load();
    const float g   = gain.load();
    const float userMk = makeup.load();

    if (buff && in) std::memcpy(out, in, frames * sizeof(float));
    else std::memset(out, 0, frames * sizeof(float));

    if (play) {
        std::lock_guard<std::mutex> lk(fileMu_);
        if (!fileData_.empty() && fileSr_ > 0) {
            const double step = (double)fileSr_ / (double)sampleRate_;
            const size_t total = fileData_.size();
            for (unsigned i = 0; i < frames; ++i) {
                size_t idx = (size_t)filePos_;
                size_t nxt = (idx + 1) % total;
                float frac = (float)(filePos_ - (double)idx);
                float s = fileData_[idx] * (1.f - frac) + fileData_[nxt] * frac;
                out[i] += s;
                filePos_ += step;
                while (filePos_ >= (double)total) filePos_ -= (double)total;
            }
        }
    }

    if (!buff && !play) { meterPeak.store(0.f); return; }

    float l = lowDb.load(), m = midDb.load(), h = highDb.load();
    float mf = midFreq.load(), hf = hpfFreq.load();
    if (l != eq_.lowDb || m != eq_.midDb || h != eq_.highDb ||
        mf != eq_.midF || hf != eq_.hpfF) {
        eq_.lowDb = l; eq_.midDb = m; eq_.highDb = h;
        eq_.midF = mf; eq_.hpfF = hf;
        eq_.reconfigure();
    }

    comp_.threshold = compThresh.load();
    comp_.ratio     = compRatio.load();
    comp_.makeup    = 1.0f;

    const float preGainMult = 1.0f + std::min(1.6f, (g - 1.0f) * 0.15f);
    const float preGain     = g * preGainMult;

    const float compMix = std::min(0.72f, 0.30f + (g - 1.0f) * 0.05f);

    const float makeupFinal = (1.0f + std::min(2.8f, std::log10(std::max(g, 1.0f)) * 1.6f)) * userMk;

    const float drive    = 1.6f + (g - 1.0f) * 0.12f;
    const float satMix   = std::min(0.42f, 0.10f + (g - 1.0f) * 0.04f);
    const float satNorm  = std::tanh(drive);

    const bool stat = staticOn.load();
    float peak = 0.f;

    for (unsigned i = 0; i < frames; ++i) {
        float x = out[i] * preGain;
        x = eq_.process(x);

        float comped = comp_.process(x);
        x = x * (1.0f - compMix) + comped * compMix;

        x *= makeupFinal;

        if (stat) x += noise() * noiseAmt_ * std::min(1.f, g * 0.10f);

        float sat = std::tanh(x * drive) / satNorm;
        x = x * (1.0f - satMix) + sat * satMix;

        x = lim_.process(x);

        out[i] = x;
        peak = std::max(peak, std::fabs(x));
    }

    meterPeak.store(peak);
}

bool AudioEngine::loadFile(const std::string& path, std::string& errOut) {
    ma_decoder dec;
    ma_decoder_config cfg = ma_decoder_config_init(ma_format_f32, 1, 0);
    if (ma_decoder_init_file(path.c_str(), &cfg, &dec) != MA_SUCCESS) {
        errOut = "Không đọc được file"; return false;
    }

    ma_uint64 totalFrames = 0;
    ma_decoder_get_length_in_pcm_frames(&dec, &totalFrames);
    if (totalFrames == 0) {
        ma_decoder_uninit(&dec); errOut = "File rỗng"; return false;
    }

    std::vector<float> data(totalFrames);
    ma_uint64 read = 0;
    ma_decoder_read_pcm_frames(&dec, data.data(), totalFrames, &read);
    int sr = (int)dec.outputSampleRate;
    ma_decoder_uninit(&dec);

    if (read == 0) { errOut = "Không đọc được mẫu"; return false; }
    data.resize((size_t)read);

    {
        std::lock_guard<std::mutex> lk(fileMu_);
        fileData_ = std::move(data);
        fileSr_ = sr;
        filePos_ = 0.0;
        fileName_ = path.substr(path.find_last_of("\\/") + 1);
    }
    playing_.store(false);
    return true;
}

void AudioEngine::togglePlay() {
    std::lock_guard<std::mutex> lk(fileMu_);
    if (fileData_.empty()) return;
    playing_.store(!playing_.load());
    if (!playing_.load()) filePos_ = 0.0;
}

std::string AudioEngine::fileName() const {
    std::lock_guard<std::mutex> lk(fileMu_);
    return fileName_;
}

std::string AudioEngine::lastError() const {
    std::lock_guard<std::mutex> lk(fileMu_);
    return lastErr_;
}
