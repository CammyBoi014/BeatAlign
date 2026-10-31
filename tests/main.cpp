#include <OnsetDetector.h>
#include <iostream>
#include <vector>
#include <cmath>
#include <numbers>

int main()
{
    std::cout << "=================================================\n";
    std::cout << "  BeatAlign Phase 1: Spectral Flux Onset Test    \n";
    std::cout << "=================================================\n\n";

    constexpr double sampleRate = 44100.0;
    constexpr double durationSeconds = 1.5;
    const size_t totalSamples = static_cast<size_t>(sampleRate * durationSeconds);

    // Generate test signal with synthetic transients (exponentially decaying sine bursts)
    std::vector<float> audio(totalSamples, 0.0f);
    const std::vector<size_t> expectedOnsets = {
        static_cast<size_t>(0.20 * sampleRate), // ~8820
        static_cast<size_t>(0.60 * sampleRate), // ~26460
        static_cast<size_t>(1.05 * sampleRate)  // ~46305
    };

    constexpr float burstFreq = 1000.0f;
    constexpr float decayRate = 0.002f; // fast exponential decay for sharp attack

    // Let the exponential envelope decay down to silence naturally
    // (exp(-0.002 * 4000) ~ 0.0003, fade out smoothly over the tail to prevent truncation clicks)
    constexpr size_t burstLength = 4000;
    constexpr size_t fadeLength  = 128;

    for (size_t onsetSample : expectedOnsets)
    {
        const size_t endSample = std::min(onsetSample + burstLength, totalSamples);
        for (size_t i = onsetSample; i < endSample; ++i)
        {
            const float t = static_cast<float>(i - onsetSample);
            float envelope = std::exp(-decayRate * t);

            // Apply a smooth fade-out over the final 128 samples to avoid spectral splatter
            if (i >= endSample - fadeLength)
            {
                const float fade = static_cast<float>(endSample - 1 - i) / static_cast<float>(fadeLength);
                envelope *= fade;
            }

            const float sine = std::sin(2.0f * std::numbers::pi_v<float> * burstFreq * (t / static_cast<float>(sampleRate)));
            audio[i] += envelope * sine;
        }
    }

    // Initialize OnsetDetector
    beatalign::dsp::OnsetDetector detector;
    detector.prepare(sampleRate, totalSamples);

    auto detectedOnsets = detector.process(audio);

    std::cout << "Expected Onset Samples:\n";
    for (size_t target : expectedOnsets)
        std::cout << "  - " << target << " (" << (static_cast<double>(target) / sampleRate) << "s)\n";

    std::cout << "\nDetected Onset Samples:\n";
    for (size_t detected : detectedOnsets)
        std::cout << "  - " << detected << " (" << (static_cast<double>(detected) / sampleRate) << "s)\n";

    // Validate that each expected onset has a match within 1 hop size tolerance
    constexpr size_t toleranceSamples = beatalign::dsp::OnsetDetector::getHopSize();
    bool allMatched = true;

    for (size_t target : expectedOnsets)
    {
        bool matched = false;
        for (size_t detected : detectedOnsets)
        {
            if (std::abs(static_cast<int64_t>(target) - static_cast<int64_t>(detected)) <= static_cast<int64_t>(toleranceSamples))
            {
                matched = true;
                break;
            }
        }
        if (!matched)
        {
            std::cerr << "[ERROR] Expected onset at sample " << target << " was not detected!\n";
            allMatched = false;
        }
    }

    if (allMatched && detectedOnsets.size() == expectedOnsets.size())
    {
        std::cout << "\n>>> TEST PASSED: All onsets detected with hop-level accuracy without false positives! <<<\n";
        return 0;
    }

    std::cerr << "\n>>> TEST FAILED: Onset count or location mismatch. <<<\n";
    return 1;
}