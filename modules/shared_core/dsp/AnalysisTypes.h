#pragma once

#include <juce_dsp/juce_dsp.h>

#include <array>
#include <cstdint>

namespace gitpro::dsp
{
    enum class InstrumentProfile : std::uint8_t
    {
        unknown,
        electricGuitar,
        bassGuitar,
        kick,
        synthBass,
        pad,
        vocal,
        drums,
        masterBus
    };

    struct MeterSnapshot
    {
        double sampleRate = 0.0;
        std::uint32_t blockSize = 0;
        float peakDbfs = -120.0f;
        float rmsDbfs = -120.0f;
        float truePeakDbfs = -120.0f;
        float crestFactorDb = 0.0f;
        float stereoCorrelation = 0.0f;
        std::uint64_t sequenceNumber = 0;
    };

    struct SignalQualitySnapshot
    {
        MeterSnapshot meters;
        float snrDb = 0.0f;
        float dynamicRangeUtilization = 0.0f;
        float clippingRisk = 0.0f;
        float scorePercent = 0.0f;
    };

    struct LatencyPingRequest
    {
        std::uint64_t pingId = 0;
        std::int64_t sourceSampleTime = 0;
        float amplitude = 0.0f;
    };

    struct LatencyPingResult
    {
        std::uint64_t pingId = 0;
        std::int64_t sourceSampleTime = 0;
        std::int64_t detectedSampleTime = 0;
        std::int32_t latencySamples = 0;
        float confidence = 0.0f;
    };

    struct SpectrumSnapshot
    {
        static constexpr std::size_t maxBins = 1024;

        double sampleRate = 0.0;
        std::uint32_t fftOrder = 0;
        std::uint32_t binCount = 0;
        std::array<float, maxBins> magnitudesDb {};
        std::uint64_t sequenceNumber = 0;
    };

    struct MatchmakerSnapshot
    {
        InstrumentProfile sourceProfile = InstrumentProfile::unknown;
        InstrumentProfile targetProfile = InstrumentProfile::unknown;
        float maskingScore = 0.0f;
        float phaseCancellationScore = 0.0f;
        float arrangementCrowdingScore = 0.0f;
        float overallClashScore = 0.0f;
    };
}