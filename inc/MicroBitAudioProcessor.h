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

#include "MicroBit.h"
#include "DataStream.h"
#define ARM_MATH_CM4
#include "arm_math.h"

#ifndef MICROBIT_AUDIO_PROCESSOR_H
#define MICROBIT_AUDIO_PROCESSOR_H

#define MIC_SAMPLE_RATE     (11 * 1024)
#define FFT_SAMPLES         2048
#define NUM_PEAKS           15
#define CYCLE_SIZE          512
#define NUM_RUNS_AVERAGE    8


class MicroBitAudioProcessor;

class PeakDataPoint
{
    public:
    int value;
    int index;
    PeakDataPoint* pair = NULL;
    public:
    PeakDataPoint(int value, int index);
    PeakDataPoint();
    ~PeakDataPoint();


};

class MicroBitAudioProcessor : public DataSink
{

public:
        bool recording;
private:
    DataSource      &audiostream;          
    int             zeroOffset;             // unsigned value that is the best effort guess of the zero point of the data source
    int             divisor;                // Used for LINEAR modes
    arm_cfft_radix4_instance_f32 fft_instance;
    float32_t input[FFT_SAMPLES];
    float32_t output[FFT_SAMPLES/2];
    float32_t positiveOutput [FFT_SAMPLES/4];
    float32_t buf[FFT_SAMPLES/2];
    float32_t buf8[FFT_SAMPLES/2];
    float32_t cpy [FFT_SAMPLES/4];

    int distances[NUM_PEAKS*NUM_PEAKS];
    int distancesPointer;

    uint32_t fftSize = FFT_SAMPLES/2;
    uint32_t ifftFlag;
    uint32_t bitReverse;
    uint32_t resultIndex;
    uint32_t secondHarmonicIndex;
    uint16_t position;
    float maxValue;
    char closestNote;
    char secondHarmonic;
    int lastFreq;
    int secondHarmonicFreq;
    int highestBinBuffer[NUM_RUNS_AVERAGE];

    PeakDataPoint peaks[NUM_PEAKS];

    public:
    MicroBitAudioProcessor(DataSource& source); 
    ~MicroBitAudioProcessor(); 
    virtual int pullRequest();
    int getClosestNoteSquare();
    char getClosestNote();
    int getFrequency();
    char frequencyToNote(int frequency);
    int setDivisor(int d);
    void startRecording();
    void stopRecording(MicroBit& uBit);
};



#endif