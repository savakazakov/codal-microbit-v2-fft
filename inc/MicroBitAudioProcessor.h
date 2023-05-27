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

#include "DataStream.h"
#define ARM_MATH_CM4
#include "arm_math.h"
#include <map>

#ifndef MICROBIT_AUDIO_PROCESSOR_H
#define MICROBIT_AUDIO_PROCESSOR_H

// Mic's adc is usually set to a 45.(45) microseconds sampling period.
// This translates to a sample rate of 22000 Hz.
// TODO Make this dynamic and hence not a macro.
#define MIC_SAMPLE_RATE     22000

// The size of the FFT. This should be the same as the number of samples.
// I.e. a 256 point signal.
// TODO Make this dynamic and hence not a macro.
#define FFT_SAMPLES         256
#define FFT_SAMPLES_HALF    (FFT_SAMPLES / 2)
#define IFFT_FLAG           0

// The bin width (also called line spacing) defines the frequency resolution of the FFT.
#define BIN_WIDTH           ((float) MIC_SAMPLE_RATE / FFT_SAMPLES)
#define BIN_WIDTH_HALF      ((float) MIC_SAMPLE_RATE / FFT_SAMPLES / 2)
#define FREQ_TO_IDX(X)      (int) (X / BIN_WIDTH)

#define NUM_PEAKS           12      // REMOVE
#define CYCLE_SIZE          128     // REMOVE
#define NUM_RUNS_AVERAGE    3 //too big means the notes won't change over to the new one as quickly // REMOVE
#define AVERAGE_THRESH      NUM_RUNS_AVERAGE / 2 // REMOVE

// Events.
#define MICROBIT_RADAR_EVT_DATAGRAM 1 // Event to signal that a new datagram has been received. // REMOVE

class MicroBitAudioProcessor;

// A Plain Old Data (POD) class used as a tuple to register
// frequency levels that should produce events when detected.
class FrequencyLevel
{
    public:
    uint16_t frequency;
    uint16_t threshold;

    public:
    FrequencyLevel(uint16_t frequency, uint16_t threshold);
    FrequencyLevel();
    ~FrequencyLevel();
};

/**
 * TODO finish Javadoc style comments.
 * @link
 * https://arm-software.github.io/CMSIS_5/DSP/html/group__RealFFT.html#ga5d2ec62f3e35575eba467d09ddcd98b5
 */
class MicroBitAudioProcessor : public DataSink, public DataSource
{
    // On demand activated.
    public:
    bool                    recording = false;
    bool                    activated = false;

    private:
    DataSource              &upstream;   
    DataSink                *downstream = NULL;

    // Unsigned value that is the best effort guess of the zero point of the data source
    int                     zeroOffset;

    // The direction of the FFT algorithm value = 0 : RFFT value = 1 : RIFFT
    uint8_t                 ifftFlag;
    arm_rfft_fast_instance_f32 fftInstance;
    arm_status              status;

    // Used to extract the bin of the first/second harmonics.
    float32_t               firstHarmonic;
    float32_t               firstHarPower;
    uint32_t                firstHarIndex = 0;
    float32_t               secondHarPower;
    uint32_t                secondHarIndex = 0;

    // complexFFT is the output of performing the FFT calculation.
    // complexFFT = { real[0], imag[0], real[1], imag[1], real[2], imag[2] ... real[(N/2)-1], imag[(N/2)-1 }
    // realFFT and imagFFT are used to separate the real and imaginary parts.
    // powerFFT stores the magnitudes of the result after perfroming the FFT calculation.
    float32_t               complexFFT[FFT_SAMPLES], realFFT[FFT_SAMPLES_HALF], imagFFT[FFT_SAMPLES_HALF],
                            angleFFT[FFT_SAMPLES_HALF], powerFFT[FFT_SAMPLES_HALF], copy[FFT_SAMPLES_HALF];

    // Used for the detection of frequencies.
    CODAL_TIMESTAMP         timestamp;

    // A map to hold any frequencies that user code is interested in.
    std::map<uint8_t, FrequencyLevel *> eventFrequencyLevels;

    int                     bytesPerSample;
    float32_t               *fftInBuffer;

    ManagedBuffer downstreamBuffer;
    ManagedBuffer upstreamBuffer;

    public:
    MicroBitAudioProcessor(DataSource &source, bool connectImmediately = true);
    ~MicroBitAudioProcessor(); 
    virtual int pullRequest();
    virtual void connect(DataSink &downstream);
    virtual ManagedBuffer pull();

    // Subsriber pattern.
    uint8_t subscribe(uint16_t lowerBound, uint16_t upperBound);
    void unsubscribe(uint8_t eventCode);

    // int getClosestNoteSquare();
    // char getClosestNote();
    // int getSecondaryFrequency();
    // char getSecondaryNote();

    int getFrequency();

    // char frequencyToNote(int frequency);

    void startRecording();
    void stopRecording();

    void playFrequency(int frequency, int ms);
    void playFrequency(char note, int ms);
    int noteToFrequency(char note);
    /**
     *  Determine the data format of the buffers streamed out of this component.
     */
    virtual int getFormat();

    /**
     * Defines the data format of the buffers streamed out of this component.
     * @param format the format to use, one of
     */
    virtual int setFormat(int format);
};

#endif