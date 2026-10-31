#include "OnsetDetector.h"
#include <algorithm>
#include <numeric>

namespace beatalign::dsp
{

OnsetDetector::OnsetDetector()
    : fftProcessor(fftOrder),
      window(frameSize, juce::dsp::WindowingFunction<float>::hann, false)
{
    fftTimeAndFreqBuffer.resize(static_cast<size_t>(frameSize) * 2, 0.0f);
    currentMagnitudes.resize(numBins, 0.0f);
    previousMagnitudes.resize(numBins, 0.0f);
}

void OnsetDetector::prepare(double sampleRate, size_t maxAudioLengthInSamples)
{
    currentSampleRate = sampleRate;

    std::fill(fftTimeAndFreqBuffer.begin(), fftTimeAndFreqBuffer.end(), 0.0f);
    std::fill(currentMagnitudes.begin(), currentMagnitudes.end(), 0.0f);
    std::fill(previousMagnitudes.begin(), previousMagnitudes.end(), 0.0f);

    if (maxAudioLengthInSamples > 0)
    {
        const size_t estimatedFrames = (maxAudioLengthInSamples >= static_cast<size_t>(frameSize))
                                           ? ((maxAudioLengthInSamples - static_cast<size_t>(frameSize)) / static_cast<size_t>(hopSize) + 1)
                                           : 0;
        spectralFlux.reserve(estimatedFrames);
        adaptiveThreshold.reserve(estimatedFrames);
    }

    spectralFlux.clear();
    adaptiveThreshold.clear();
}

void OnsetDetector::reset()
{
    std::fill(fftTimeAndFreqBuffer.begin(), fftTimeAndFreqBuffer.end(), 0.0f);
    std::fill(currentMagnitudes.begin(), currentMagnitudes.end(), 0.0f);
    std::fill(previousMagnitudes.begin(), previousMagnitudes.end(), 0.0f);
    spectralFlux.clear();
    adaptiveThreshold.clear();
}

void OnsetDetector::computeFrameSpectralFlux(std::span<const float> frame)
{
    jassert(frame.size() == static_cast<size_t>(frameSize));

    // 1. Copy frame data and pad with zeros for JUCE complex transform
    std::copy_n(frame.data(), static_cast<size_t>(frameSize), fftTimeAndFreqBuffer.data());
    std::fill(fftTimeAndFreqBuffer.begin() + frameSize, fftTimeAndFreqBuffer.end(), 0.0f);

    // 2. Apply Hann Window
    window.multiplyWithWindowingTable(fftTimeAndFreqBuffer.data(), static_cast<size_t>(frameSize));

    // 3. Compute Real-to-Complex In-Place FFT
    fftProcessor.performRealOnlyForwardTransform(fftTimeAndFreqBuffer.data());

    // 4. Calculate Magnitude Spectrum
    // JUCE stores real and imaginary pairs: [r0, i0, r1, i1, ..., rN/2, iN/2]
    for (size_t bin = 0; bin < static_cast<size_t>(numBins); ++bin)
    {
        const float real = fftTimeAndFreqBuffer[2 * bin];
        const float imag = fftTimeAndFreqBuffer[2 * bin + 1];
        currentMagnitudes[bin] = std::sqrt(real * real + imag * imag);
    }

    // 5. Half-wave rectified Spectral Flux: H(diff) = (diff + |diff|) / 2 = max(0.0f, diff)
    float flux = 0.0f;
    for (size_t bin = 0; bin < static_cast<size_t>(numBins); ++bin)
    {
        const float diff = currentMagnitudes[bin] - previousMagnitudes[bin];
        if (diff > 0.0f)
            flux += diff;
    }

    spectralFlux.push_back(flux);

    // Swap magnitudes for next frame
    previousMagnitudes = currentMagnitudes;
}

void OnsetDetector::pickPeaks(std::vector<size_t>& onsetSampleIndices)
{
    const size_t numFrames = spectralFlux.size();
    if (numFrames < 3)
        return;

    adaptiveThreshold.resize(numFrames, 0.0f);
    const int halfWin = config.adaptiveWindowSize / 2;

    // Calculate adaptive local threshold across a moving window
    for (size_t i = 0; i < numFrames; ++i)
    {
        const int start = std::max(0, static_cast<int>(i) - halfWin);
        const int end   = std::min(static_cast<int>(numFrames) - 1, static_cast<int>(i) + halfWin);
        const int count = end - start + 1;

        float sum = 0.0f;
        for (int w = start; w <= end; ++w)
            sum += spectralFlux[static_cast<size_t>(w)];

        const float localMean = sum / static_cast<float>(count);
        adaptiveThreshold[i] = (localMean * config.thresholdMultiplier) + config.thresholdOffset;
    }

    // Peak picking:
    // 1. Must be a local maximum: SF[i] > SF[i-1] and SF[i] >= SF[i+1]
    // 2. Must exceed adaptive threshold: SF[i] > Threshold[i]
    // 3. Enforce minInterOnsetInterval
    int lastOnsetFrame = -config.minInterOnsetInterval;

    for (size_t i = 1; i < numFrames - 1; ++i)
    {
        const float current = spectralFlux[i];
        if (current > spectralFlux[i - 1] &&
            current >= spectralFlux[i + 1] &&
            current > adaptiveThreshold[i])
        {
            if (static_cast<int>(i) - lastOnsetFrame >= config.minInterOnsetInterval)
            {
                // Align detected onset to the window center: i * hopSize + frameSize / 2
                const size_t samplePosition = (i * static_cast<size_t>(hopSize)) + (static_cast<size_t>(frameSize) / 2);
                onsetSampleIndices.push_back(samplePosition);
                lastOnsetFrame = static_cast<int>(i);
            }
        }
    }
}

    std::vector<size_t> OnsetDetector::process(std::span<const float> audio)
{
    reset();

    std::vector<size_t> onsets;
    if (audio.size() < static_cast<size_t>(frameSize))
        return onsets;

    const size_t totalFrames = (audio.size() - static_cast<size_t>(frameSize)) / static_cast<size_t>(hopSize) + 1;
    spectralFlux.reserve(totalFrames);

    // Frame-by-frame STFT sliding by hopSize
    for (size_t offset = 0; offset + static_cast<size_t>(frameSize) <= audio.size(); offset += static_cast<size_t>(hopSize))
    {
        computeFrameSpectralFlux(audio.subspan(offset, static_cast<size_t>(frameSize)));
    }

    pickPeaks(onsets);
    return onsets;
}

} // namespace beatalign::dsp