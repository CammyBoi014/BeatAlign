#pragma once

#include <juce_dsp/juce_dsp.h>
#include <juce_core/juce_core.h>
#include <vector>
#include <span>
#include <cmath>
#include <cstddef>

namespace beatalign::dsp
{

/**
 * @brief Configuration parameters for OnsetDetector.
 */
struct OnsetDetectorConfig
{
    float thresholdMultiplier = 1.4f;    ///< Factor multiplying the adaptive local mean/median.
    float thresholdOffset     = 0.05f;   ///< Absolute noise floor offset to prevent false triggers in silence.
    int   adaptiveWindowSize  = 7;       ///< Number of frames in moving window (odd number preferred).
    int   minInterOnsetInterval = 8;     ///< Minimum number of hops between detected onsets (~46 ms at 44.1kHz).
};

/**
 * @brief High-performance Spectral Flux Onset Detector.
 *
 * Utilizes half-wave rectified spectral difference across STFT frames with
 * Hann windowing and adaptive thresholding for peak picking.
 * Pre-allocates all memory during prepare().
 */
class OnsetDetector
{
public:
    static constexpr int fftOrder  = 10;
    static constexpr int frameSize = 1 << fftOrder; // 1024
    static constexpr int hopSize   = 256;
    static constexpr int numBins   = (frameSize / 2) + 1; // 513 unique frequency bins

    OnsetDetector();
    ~OnsetDetector() = default;

    /**
     * @brief Pre-allocates and initializes internal buffers.
     * @param sampleRate The sampling rate in Hz.
     * @param maxAudioLengthInSamples Maximum audio block length expected for offline or buffered processing.
     */
    void prepare(double sampleRate, size_t maxAudioLengthInSamples = 0);

    /**
     * @brief Resets ongoing STFT states and scratch buffers without reallocating.
     */
    void reset();

    /**
     * @brief Sets detection threshold parameters.
     */
    void setConfig(const OnsetDetectorConfig& newConfig) noexcept { config = newConfig; }
    [[nodiscard]] const OnsetDetectorConfig& getConfig() const noexcept { return config; }

    /**
     * @brief Analyzes a contiguous audio stream and returns detected onset locations in sample indices.
     * @param audio Input audio slice (monophonic or downmixed).
     * @return Vector of sample indices corresponding to transient onset points.
     */
    [[nodiscard]] std::vector<size_t> process(std::span<const float> audio);

    /**
     * @brief Returns the raw spectral flux curve computed during the last process() call.
     */
    [[nodiscard]] std::span<const float> getLastSpectralFlux() const noexcept { return spectralFlux; }

    [[nodiscard]] static constexpr int getFrameSize() noexcept { return frameSize; }
    [[nodiscard]] static constexpr int getHopSize() noexcept   { return hopSize; }

private:
    void computeFrameSpectralFlux(std::span<const float> frame);
    void pickPeaks(std::vector<size_t>& onsetSampleIndices);

    OnsetDetectorConfig config;
    double currentSampleRate { 44100.0 };

    juce::dsp::FFT fftProcessor;
    juce::dsp::WindowingFunction<float> window;

    // FFT & Spectrum scratch buffers (2 * frameSize for complex packing in JUCE FFT)
    std::vector<float> fftTimeAndFreqBuffer;
    std::vector<float> currentMagnitudes;
    std::vector<float> previousMagnitudes;

    // Output and detection curves
    std::vector<float> spectralFlux;
    std::vector<float> adaptiveThreshold;
};

} // namespace beatalign::dsp