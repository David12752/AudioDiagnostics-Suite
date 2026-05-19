#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_dsp/juce_dsp.h>

#include <array>
#include <atomic>

namespace gitpro::dsp
{
    struct LowBandSpectralMetrics
    {
        static constexpr std::size_t bandCount = 6;

        std::array<float, bandCount> bandEnergyDb {};
        std::array<float, bandCount> bandPhaseRadians {};
        float lowBandTotalEnergyDb = -120.0f;
        float dominantFrequencyHz = 0.0f;
        int dominantBandIndex = -1;
    };

    class LowBandFftAnalyzer final
    {
    public:
        static constexpr int fftOrder = 10;
        static constexpr int fftSize = 1 << fftOrder;
        static constexpr int queuedFrameCount = 4;
        static constexpr std::array<float, LowBandSpectralMetrics::bandCount> bandLowerHz { 40.0f, 55.0f, 70.0f, 90.0f, 115.0f, 135.0f };
        static constexpr std::array<float, LowBandSpectralMetrics::bandCount> bandUpperHz { 55.0f, 70.0f, 90.0f, 115.0f, 135.0f, 150.0f };

        LowBandFftAnalyzer();

        void reset() noexcept;
        void pushBlock(const juce::AudioBuffer<float>& buffer) noexcept;
        [[nodiscard]] bool analyzeIfReady(double sampleRate, LowBandSpectralMetrics& output) noexcept;

    private:
        [[nodiscard]] static float bandCentreHz(std::size_t bandIndex) noexcept;

        juce::dsp::FFT fft { fftOrder };
        std::array<float, fftSize> captureBuffer {};
        std::array<std::array<float, fftSize>, queuedFrameCount> queuedFrames {};
        std::array<juce::dsp::Complex<float>, fftSize> fftInput {};
        std::array<juce::dsp::Complex<float>, fftSize> fftOutput {};
        std::array<float, fftSize> window {};
        juce::AbstractFifo readyFrameFifo { queuedFrameCount };
        int captureIndex = 0;
    };
}