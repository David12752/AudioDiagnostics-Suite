#include "LowBandFftAnalyzer.h"

#include <algorithm>
#include <cmath>

namespace gitpro::dsp
{
    LowBandFftAnalyzer::LowBandFftAnalyzer()
    {
        for (auto index = 0; index < fftSize; ++index)
        {
            const auto position = static_cast<float>(index) / static_cast<float>(fftSize - 1);
            window[static_cast<std::size_t>(index)] = 0.5f - 0.5f * std::cos(juce::MathConstants<float>::twoPi * position);
        }
    }

    void LowBandFftAnalyzer::reset() noexcept
    {
        captureBuffer.fill(0.0f);
        captureIndex = 0;
        blockReady.store(false, std::memory_order_release);
    }

    void LowBandFftAnalyzer::pushBlock(const juce::AudioBuffer<float>& buffer) noexcept
    {
        if (blockReady.load(std::memory_order_acquire))
            return;

        const auto numChannels = buffer.getNumChannels();
        const auto numSamples = buffer.getNumSamples();

        if (numChannels <= 0 || numSamples <= 0)
            return;

        for (auto sample = 0; sample < numSamples; ++sample)
        {
            auto mono = 0.0f;

            for (auto channel = 0; channel < numChannels; ++channel)
                mono += buffer.getReadPointer(channel)[sample];

            mono /= static_cast<float>(numChannels);
            captureBuffer[static_cast<std::size_t>(captureIndex)] = mono;
            ++captureIndex;

            if (captureIndex >= fftSize)
            {
                captureIndex = 0;
                blockReady.store(true, std::memory_order_release);
                return;
            }
        }
    }

    bool LowBandFftAnalyzer::analyzeIfReady(double sampleRate, LowBandSpectralMetrics& output) noexcept
    {
        if (! blockReady.load(std::memory_order_acquire) || sampleRate <= 0.0)
            return false;

        for (auto index = 0; index < fftSize; ++index)
            fftInput[static_cast<std::size_t>(index)] = { captureBuffer[static_cast<std::size_t>(index)] * window[static_cast<std::size_t>(index)], 0.0f };

        fft.perform(fftInput.data(), fftOutput.data(), false);

        output.bandEnergyDb.fill(-120.0f);
        output.bandPhaseRadians.fill(0.0f);
        output.lowBandTotalEnergyDb = -120.0f;
        output.dominantFrequencyHz = 0.0f;
        output.dominantBandIndex = -1;

        auto strongestMagnitude = 0.0f;
        auto totalPower = 0.0f;

        for (std::size_t band = 0; band < LowBandSpectralMetrics::bandCount; ++band)
        {
            auto bandPower = 0.0f;
            auto bandBinCount = 0;
            auto bandStrongestMagnitude = 0.0f;
            auto bandStrongestPhase = 0.0f;

            for (auto bin = 1; bin <= fftSize / 2; ++bin)
            {
                const auto frequency = static_cast<float>((static_cast<double>(bin) * sampleRate) / static_cast<double>(fftSize));

                if (frequency < bandLowerHz[band] || frequency >= bandUpperHz[band])
                    continue;

                const auto value = fftOutput[static_cast<std::size_t>(bin)];
                const auto magnitude = std::abs(value) / static_cast<float>(fftSize);
                bandPower += magnitude * magnitude;
                totalPower += magnitude * magnitude;
                ++bandBinCount;

                if (magnitude > bandStrongestMagnitude)
                {
                    bandStrongestMagnitude = magnitude;
                    bandStrongestPhase = std::atan2(value.imag(), value.real());
                }

                if (magnitude > strongestMagnitude)
                {
                    strongestMagnitude = magnitude;
                    output.dominantFrequencyHz = frequency;
                    output.dominantBandIndex = static_cast<int>(band);
                }
            }

            if (bandBinCount > 0)
            {
                const auto bandRms = std::sqrt(bandPower / static_cast<float>(bandBinCount));
                output.bandEnergyDb[band] = juce::Decibels::gainToDecibels(bandRms, -120.0f);
                output.bandPhaseRadians[band] = bandStrongestPhase;
            }
            else
            {
                output.dominantFrequencyHz = std::max(output.dominantFrequencyHz, bandCentreHz(band));
            }
        }

        output.lowBandTotalEnergyDb = juce::Decibels::gainToDecibels(std::sqrt(totalPower), -120.0f);
        blockReady.store(false, std::memory_order_release);
        return true;
    }

    float LowBandFftAnalyzer::bandCentreHz(std::size_t bandIndex) noexcept
    {
        return (bandLowerHz[bandIndex] + bandUpperHz[bandIndex]) * 0.5f;
    }
}