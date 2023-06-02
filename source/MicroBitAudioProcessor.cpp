/*
The MIT License (MIT)
Copyright (c) 2021 Lancaster University.
Permission is hereby granted, free of charge, to any person obtaining a
copy of this software and associated documentation files (the "Software"),
to deal in the Software without restriction, including without limitation
the rights to use, copy, modify, merge, publish, distribute, sublicense,
and/or sell copies of the Software, and to permit persons to whom the
Software is furnished to do so, subject to the following conditions:
The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
DEALINGS IN THE SOFTWARE.
*/

#include "CodalConfig.h"
#include "MicroBit.h"
#include "MicroBitAudioProcessor.h"
#include "Event.h"
#include <algorithm>
#include <unordered_map>
#include <cstdlib>
#include <map>
#include <string>

MicroBitAudioProcessor::MicroBitAudioProcessor(DataSource& source, bool connectImmediately) : upstream(source)
{
    this->ifftFlag = IFFT_FLAG;
    this->firstHarmonic = 0;

    // Init RFFT module.
    arm_rfft_fast_init_f32(&fftInstance, FFT_SAMPLES);

    // TODO fix downstream.
    // ManagedBuffer downstreamBuffer(outBuf, 11);
    // this->downstreamBuffer = downstreamBuffer;

    ManagedBuffer upstreamBuffer;

    if (connectImmediately)
    {
        upstream.connect(*this);
        activated = true;
    }
}

MicroBitAudioProcessor::~MicroBitAudioProcessor() {}

/**
 * Return latest FFT Data.
 *
 * @return DEVICE_OK on success.
 */
ManagedBuffer MicroBitAudioProcessor::pull()
{
    if (!recording)
        this->recording = true;

    return downstreamBuffer;
}

/**
 * Set the downstream component to recieve pull requests.
 */
void MicroBitAudioProcessor::connect(DataSink &downstream)
{
    this->downstream = &downstream;
}

/**
 * Do something when recieving data from Mic.
 *
 * @return DEVICE_OK on success.
 */
REAL_TIME_FUNC
int MicroBitAudioProcessor::pullRequest()
{
    // Take a timestamp as soon as the data is received.
    this->timestamp = system_timer_current_time_us();

    upstreamBuffer = upstream.pull();
    
    // Return promptly if the FFT is not recording.
    if (!recording)
        return DEVICE_OK;

    // Transform the data in the required input format for the arm_rfft_fast_f32, i.e. float32_t*.
    // DMESG("upstream format = %d", this->bytesPerSample); // Should be 2.
    this->bytesPerSample = DATASTREAM_FORMAT_BYTES_PER_SAMPLE(this->upstream.getFormat());
    int sampleCount = upstreamBuffer.length() / bytesPerSample;
    // DMESG("sampleCount = %d", sampleCount); // Should be 512 / 2 = 256

    // Initialize the exact amount of space needed.
    float32_t fftInBuffer[sampleCount];

    uint8_t *in = upstreamBuffer.getBytes();

    // Construct the FFT input given the type conversion of the data.
    for (int i = 0; i < sampleCount; i++)
    {
        float value = (float32_t) StreamNormalizer::readSample[this->upstream.getFormat()](in);
        fftInBuffer[i] = value;

        in += bytesPerSample;
    }

    status = ARM_MATH_SUCCESS;
    status = arm_rfft_fast_init_f32(&fftInstance, FFT_SAMPLES);

    // Input is real, output is alternating between real and complex.
    arm_rfft_fast_f32(&fftInstance, fftInBuffer, complexFFT, ifftFlag);

    // First entry is all real DC offset.
    float32_t DCoffset = complexFFT[0];

    // Separate real and complex values.
    for (int i = 0; i < FFT_SAMPLES_HALF - 1; i++)
    {
        realFFT[i] = complexFFT[i * 2];
        imagFFT[i] = complexFFT[(i * 2) + 1];
    }

    // Find angle of FFT.
    for (int i = 0; i < FFT_SAMPLES_HALF; i++)
    {
        angleFFT[i] = atan2f(imagFFT[i], realFFT[i]);
    }

    // Compute power.
    arm_cmplx_mag_squared_f32(complexFFT, powerFFT, FFT_SAMPLES_HALF);
    // Make sure to start from the second entry.
    arm_max_f32(&powerFFT[1], FFT_SAMPLES_HALF - 1, &firstHarPower, &firstHarIndex);

    // Correcting index.
    firstHarIndex++;

    // TODO check if this really works.
    // Parabolic Interpolation.
    float32_t offsetTop = (powerFFT[firstHarIndex + 1] - powerFFT[firstHarIndex - 1]);
    float32_t offsetBottom = 2 * ((2 * powerFFT[firstHarIndex]) - powerFFT[firstHarIndex - 1] - powerFFT[firstHarIndex + 1]);
    float32_t parabolicOffset = offsetTop / offsetBottom;

    firstHarmonic = BIN_WIDTH * ((float) firstHarIndex + (float) parabolicOffset) + BIN_WIDTH_HALF;

    // Loop through the values of the map.
    for (const auto &pair : eventFrequencyLevels)
    {
        FrequencyLevel* level = pair.second;

        if (powerFFT[FREQ_TO_IDX(level->frequency)] >= level->threshold)
        {
            Event e(DEVICE_ID_AUDIO_PROCESSOR, pair.first, this->timestamp, CREATE_AND_FIRE);
        }
    }

    // TODO fix the downstream communication.
    /* if (downstream != NULL)
    {
        outputBuffer.setByte(0, (uint8_t)closestNote);
        outputBuffer.setByte(1, (uint8_t)(lastFreq / 1000) % 10);
        outputBuffer.setByte(2, (uint8_t)(lastFreq / 100) % 10);
        outputBuffer.setByte(3, (uint8_t)(lastFreq / 10) % 10);
        outputBuffer.setByte(4, (uint8_t)(lastFreq % 10));
        outputBuffer.setByte(5, (uint8_t)secondHarmonic);
        outputBuffer.setByte(6, (uint8_t)(secondHarmonicFreq / 1000) % 10);
        outputBuffer.setByte(7, (uint8_t)(secondHarmonicFreq / 100) % 10);
        outputBuffer.setByte(8, (uint8_t)(secondHarmonicFreq / 10) % 10);
        outputBuffer.setByte(9, (uint8_t)(secondHarmonicFreq % 10));
        outputBuffer.setByte(10, (uint8_t)notIdle);
        downstream->pullRequest();
    } */

    return DEVICE_OK;
}

/**
 * Subscribe to a specific frequency with a given threshold level.
 *
 * @param frequency The desired frequency to be tracked.
 * @param threshold The sensitivity at which the frequency is reported as harmonic. Subject to calibration.
 * 
 * @return An event code used to register an event handler.
 * @note This should be followed by a call to listen() with the returned event code.
 * 
 * TODO: need to make this call safe. I.e. no fix corner cases.
 * TODO: Switch to try_emplace with C++17.
 */
uint8_t MicroBitAudioProcessor::subscribe(uint16_t frequency, uint32_t threshold)
{
    // Reserve 0 for the "all" event. 0 is not used due to the pre-increment.
    uint8_t ctr = 0;
    bool result = false;

    while (!result)
    {
        std::tie(std::ignore, result) = eventFrequencyLevels.emplace(++ctr, new FrequencyLevel(frequency, threshold));

        if (ctr == 255 && !result)
            return DEVICE_NO_RESOURCES;
    }

    // If there is an available event code, return it.
    return ctr;
}

/**
 * Unsubscribe from a specific event.
 *
 * @param eventCode The code of the event that should stop producing such events.
 * 
 * @note This should be followed by a call to ignore() with the provided event code.
 */
void MicroBitAudioProcessor::unsubscribe(uint8_t eventCode)
{
    eventFrequencyLevels.erase(eventCode);
}

/**
 * Get the last detected first harmonic.
 *
 * @return The primary frequency detected.
 */
int MicroBitAudioProcessor::getFrequency()
{
    if (!recording)
        startRecording();

    return this->firstHarmonic;
}

/**
 * Start the FFT.
 */
REAL_TIME_FUNC
void MicroBitAudioProcessor::startRecording()
{
    if (!activated)
    {
        upstream.connect(*this);
        activated = true;
    }

    this->recording = true;
    DMESG("Start Recording\n");
}

/**
 * Stop the FFT.
 */
REAL_TIME_FUNC
void MicroBitAudioProcessor::stopRecording()
{
    this->recording = false;
    DMESG("Stop Recording\n");
}

/**
 * Constructor.
 * 
 * @param frequency The desired frequency to be tracked.
 * @param threshold The power threshold at which the event should be triggered.
*/
FrequencyLevel::FrequencyLevel(uint16_t frequency, uint32_t threshold)
{
    this->frequency = frequency;
    this->threshold = threshold;
}

/**
 * Default constructor.
 */
FrequencyLevel::FrequencyLevel()
{
    this->frequency = 0;
    this->threshold = 0;
}

/**
 *  Determine the data format of the buffers streamed out of this component.
 */
int MicroBitAudioProcessor::getFormat()
{
    return upstream.getFormat();
}

/**
 * Defines the data format of the buffers streamed out of this component.
 * @param format the format to use
 */
int MicroBitAudioProcessor::setFormat(int format)
{
    return DEVICE_NOT_SUPPORTED;
}