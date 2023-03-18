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
#define MIC_SAMPLE_RATE     22000

// The size of the FFT. This should be the same as the number of samples.
// I.e. a 256 point signal.
#define FFT_SAMPLES         256
#define FFT_SAMPLES_HALF    (FFT_SAMPLES / 2)
#define IFFT_FLAG           0

// The bin width (also called line spacing) defines the frequency resolution of the FFT.
#define BIN_WIDTH           ((float) MIC_SAMPLE_RATE / FFT_SAMPLES)

#define NUM_PEAKS           12      // REMOVE
#define CYCLE_SIZE          128     // REMOVE
#define NUM_RUNS_AVERAGE    3 //too big means the notes wont change over to the new one as quickly // REMOVE
#define AVERAGE_THRESH      NUM_RUNS_AVERAGE / 2 // REMOVE

class MicroBitAudioProcessor;

class PeakDataPoint
{
    public:
    int8_t                  value;
    int                     index;
    PeakDataPoint*          pair = NULL;

    public:
    PeakDataPoint(int8_t value, int index);
    PeakDataPoint();
    ~PeakDataPoint();
};

/**
 * TODO finish Javadoc style comments.
 * @link
 * https://arm-software.github.io/CMSIS_5/DSP/html/group__RealFFT.html#ga5d2ec62f3e35575eba467d09ddcd98b5
 */
class MicroBitAudioProcessor : public DataSink, public DataSource
{
    public:
    // On demand activated.
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
    float32_t               firstHarValue;
    uint32_t                firstHarIndex = 0;
    float32_t               secondHarValue;
    uint32_t                secondHarIndex = 0;

    // TODO make these static.
    // complexFFT is the output of performing the FFT calculation.
    // complexFFT = { real[0], imag[0], real[1], imag[1], real[2], imag[2] ... real[(N/2)-1], imag[(N/2)-1 }
    // realFFT and imagFFT are used to separate the real and imaginary parts.
    // powerFFT stores the magnitudes of the result after perfroming the FFT calculation.
    float32_t               complexFFT[FFT_SAMPLES], realFFT[FFT_SAMPLES_HALF], imagFFT[FFT_SAMPLES_HALF],
                            angleFFT[FFT_SAMPLES_HALF], powerFFT[FFT_SAMPLES_HALF], copy[FFT_SAMPLES_HALF];

    int distances[NUM_PEAKS * NUM_PEAKS]; // REMOVE
    int distancesPointer;   // REMOVE

    char secondHarmonic; // REMOVE
    int lastFreq;        // REMOVE
    int lastLastFreq;    // REMOVE
    int secondHarmonicFreq; // REMOVE
    int highestBinBuffer[NUM_RUNS_AVERAGE]; // REMOVE
    int timer = 0;                          // REMOVE
    int timer2 = 0;                         // REMOVE
    char lastEventSent = 'X';               // REMOVE
    char lastDetected = 'X';                // REMOVE
    char lastDetectedS = 'X';               // REMOVE

    int bytesPerSample;
    float32_t *fftInBuffer;

    PeakDataPoint peaks[NUM_PEAKS];

    // Stored (char cast as int)Note..(int)Freq digit 1, freq digit 2, digit 3, freq digit 4, (char cast as int)Note ...
    uint8_t outBuf[11] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
    ManagedBuffer downstreamBuffer;
    ManagedBuffer upstreamBuffer;

    public : MicroBitAudioProcessor(DataSource &source, bool connectImmediately = true);
    ~MicroBitAudioProcessor(); 
    virtual int pullRequest();
    virtual void connect(DataSink &downstream);
    virtual ManagedBuffer pull();
    int getClosestNoteSquare();
    char getClosestNote();
    int getSecondaryFrequency();
    char getSecondaryNote();
    int getFrequency();
    char frequencyToNote(int frequency);
    int setDivisor(int d);
    void startRecording();
    void stopRecording();
    void sendEvent(char letter);
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