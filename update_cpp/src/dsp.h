#pragma once
#include <cmath>
#include <algorithm>

struct BiquadCoeffs { float b0, b1, b2, a1, a2; };

inline BiquadCoeffs mkHighpass(float sr, float f, float Q = 0.707f) {
    float w = 2.f * 3.14159265f * f / sr;
    float cw = std::cos(w), sw = std::sin(w);
    float al = sw / (2.f * Q);
    float b0 = (1 + cw) * 0.5f, b1 = -(1 + cw), b2 = (1 + cw) * 0.5f;
    float a0 = 1 + al, a1 = -2 * cw, a2 = 1 - al;
    return { b0/a0, b1/a0, b2/a0, a1/a0, a2/a0 };
}

inline BiquadCoeffs mkLowShelf(float sr, float f, float gainDb, float Q = 0.707f) {
    float A = std::pow(10.f, gainDb / 40.f);
    float w = 2.f * 3.14159265f * f / sr;
    float cw = std::cos(w), sw = std::sin(w);
    float al = sw / (2.f * Q);
    float sq = 2.f * std::sqrt(A) * al;
    float b0 =  A*((A+1)-(A-1)*cw + sq);
    float b1 = 2*A*((A-1)-(A+1)*cw);
    float b2 =  A*((A+1)-(A-1)*cw - sq);
    float a0 =     (A+1)+(A-1)*cw + sq;
    float a1 = -2*((A-1)+(A+1)*cw);
    float a2 =     (A+1)+(A-1)*cw - sq;
    return { b0/a0, b1/a0, b2/a0, a1/a0, a2/a0 };
}

inline BiquadCoeffs mkPeak(float sr, float f, float gainDb, float Q = 1.f) {
    float A = std::pow(10.f, gainDb / 40.f);
    float w = 2.f * 3.14159265f * f / sr;
    float cw = std::cos(w), sw = std::sin(w);
    float al = sw / (2.f * Q);
    float b0 = 1 + al*A, b1 = -2*cw, b2 = 1 - al*A;
    float a0 = 1 + al/A, a1 = -2*cw, a2 = 1 - al/A;
    return { b0/a0, b1/a0, b2/a0, a1/a0, a2/a0 };
}

inline BiquadCoeffs mkHighShelf(float sr, float f, float gainDb, float Q = 0.707f) {
    float A = std::pow(10.f, gainDb / 40.f);
    float w = 2.f * 3.14159265f * f / sr;
    float cw = std::cos(w), sw = std::sin(w);
    float al = sw / (2.f * Q);
    float sq = 2.f * std::sqrt(A) * al;
    float b0 =  A*((A+1)+(A-1)*cw + sq);
    float b1 =-2*A*((A-1)+(A+1)*cw);
    float b2 =  A*((A+1)+(A-1)*cw - sq);
    float a0 =     (A+1)-(A-1)*cw + sq;
    float a1 =  2*((A-1)-(A+1)*cw);
    float a2 =     (A+1)-(A-1)*cw - sq;
    return { b0/a0, b1/a0, b2/a0, a1/a0, a2/a0 };
}

class Biquad {
    BiquadCoeffs c{1,0,0,0,0};
    float x1=0,x2=0,y1=0,y2=0;
public:
    void set(const BiquadCoeffs& n) { c = n; }
    inline float process(float x) {
        float y = c.b0*x + c.b1*x1 + c.b2*x2 - c.a1*y1 - c.a2*y2;
        x2=x1; x1=x; y2=y1; y1=y; return y;
    }
    void reset() { x1=x2=y1=y2=0; }
};

struct EQChain {
    Biquad hpf, lowS, midP, highS;
    float sr = 48000.f;
    float lowDb = 0.f, midDb = 0.f, highDb = 0.f;
    float midF = 1500.f, hpfF = 80.f;

    void reconfigure() {
        hpf.set(mkHighpass(sr, hpfF));
        lowS.set(mkLowShelf(sr, 120.f, lowDb));
        midP.set(mkPeak(sr, midF, midDb, 1.2f));
        highS.set(mkHighShelf(sr, 6000.f, highDb));
    }
    inline float process(float x) {
        return highS.process(midP.process(lowS.process(hpf.process(x))));
    }
    void reset() { hpf.reset(); lowS.reset(); midP.reset(); highS.reset(); }
};

class Compressor {
    float sr = 48000.f;
    float ac = 0.9f, rc = 0.99f;
    float env = 0.f, gain = 1.f;
public:
    float threshold = 0.15f;
    float ratio     = 5.0f;
    float makeup    = 1.0f;
    float attackMs  = 8.f;
    float releaseMs = 100.f;

    void setSampleRate(float s) { sr = s; update(); }
    void update() {
        ac = std::exp(-1.f / (sr * attackMs  * 0.001f));
        rc = std::exp(-1.f / (sr * releaseMs * 0.001f));
    }
    inline float process(float x) {
        float lv = std::fabs(x);
        env = lv > env ? ac*env + (1-ac)*lv : rc*env + (1-rc)*lv;
        float des = 1.f;
        if (env > threshold)
            des = std::pow(threshold / std::max(env, 1e-6f), 1.f - 1.f/ratio);
        gain = des < gain ? ac*gain + (1-ac)*des : rc*gain + (1-rc)*des;
        return x * gain * makeup;
    }
    void reset() { env = 0; gain = 1; }
};

class Limiter {
public:
    float ceiling  = 0.97f;
    float softKnee = 0.68f;
    inline float process(float x) {
        float a = std::fabs(x);
        if (a <= softKnee) return x;
        float t = (a - softKnee) / (ceiling - softKnee);
        float y = softKnee + std::tanh(t) * (ceiling - softKnee);
        return std::copysign(std::min(y, ceiling), x);
    }
};
