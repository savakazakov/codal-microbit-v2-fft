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

std::map<char, int> eventCode = {{'C', 1}, {'D', 2}, {'E', 3}, {'F', 4}, {'G', 5}, {'A', 6}, {'B', 7}};

MicroBitAudioProcessor::MicroBitAudioProcessor(DataSource& source, bool connectImmediately) : 
    upstream(source)
{
    this->ifftFlag = IFFT_FLAG;

    this->lastLastFreq = -1;

    lastFreq = 0;

    // Init RFFT module.
    arm_rfft_fast_init_f32(&fftInstance, FFT_SAMPLES);

    ManagedBuffer downstreamBuffer(outBuf, 11);
    this->downstreamBuffer = downstreamBuffer;

    ManagedBuffer upstreamBuffer;

    DMESG("%s %p\n", "Audio Processor connecting to upstream, Pointer to me : ", this);

    if(connectImmediately)
    {
        upstream.connect(*this);
        activated = true;
    }
}

MicroBitAudioProcessor::~MicroBitAudioProcessor()
{
    //TODO fill out these? or not needed now malloc is gone?
}

/**
 * Return latest FFT Data.
 *
 * @return DEVICE_OK on success.
 */
ManagedBuffer MicroBitAudioProcessor::pull()
{
    if(!recording)
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
int MicroBitAudioProcessor::pullRequest()
{
    // DMESG("In MicroBitAudioProcessor - pullRequest - Start."); // REMOVE
    fiber_sleep(1);

    upstreamBuffer = upstream.pull();
    
    // Return promptly if the FFT is not recording.
    if (!recording)
        return DEVICE_OK;

    // Transform the data in the required input format for the arm_rfft_fast_f32, i.e. float32_t*.
    this->bytesPerSample = DATASTREAM_FORMAT_BYTES_PER_SAMPLE(this->upstream.getFormat());
    // DMESG("upstream format = %d", this->bytesPerSample); // REMOVE should be 2.
    int sampleCount = upstreamBuffer.length() / bytesPerSample;
    // DMESG("sampleCount = %d", sampleCount); // REMOVE should be 512 / 2 = 256

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
    arm_max_f32(&powerFFT[1], FFT_SAMPLES_HALF - 1, &firstHarValue, &firstHarIndex);

    // Correcting index.
    firstHarIndex += 1;

    // TODO check if this really works.
    // Do Parabolic Interpolation.
    float32_t offsetTop = (powerFFT[firstHarIndex + 1] - powerFFT[firstHarIndex - 1]);
    float32_t offsetBottom = 2 * ((2 * powerFFT[firstHarIndex]) - powerFFT[firstHarIndex - 1] - powerFFT[firstHarIndex + 1]);
    float32_t parabolicOffset = offsetTop / offsetBottom;

    // // Get second harmonic - remove first harmonic and run again.
    // powerFFT[firstHarIndex] = 0;
    
    float frequencyDetected = BIN_WIDTH * ((float) firstHarIndex + (float) parabolicOffset);
    // Print the frequency value.
    lastFreq = (int) frequencyDetected;
    DMESG("frequencyDetected = %d", (int) frequencyDetected); // REMOVE

    // TODO
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

    // DMESG("In MicroBitAudioProcessor - pullRequest - End."); // REMOVE
    return DEVICE_OK;
}

/**
 * Sends an event that a note was detected
 *
 * @param letter Which letter was detected
 */
void MicroBitAudioProcessor::sendEvent(char letter)
{
    std::map<char, int>::iterator it;

    for(it = eventCode.begin(); it != eventCode.end(); ++it)
    {
        if(it->first == letter && it->first != lastEventSent)
        {
            lastEventSent = it->first;
            Event e(DEVICE_ID_AUDIO_PROCESSOR, it->second);
            return;
        }
    }
}

/**
 * Determines the harmonic frequency from a square wave FFT output
 *
 * @return DEVICE_OK on success.
 */
int MicroBitAudioProcessor::getClosestNoteSquare()
{
    // Create copy so we still have reference to index.
    for(int i = 0; i < (int) FFT_SAMPLES / 2; i++)
    {
        copy[i] = powerFFT[i];
    }

    // Get highest ones and their index - don't take if value is within <leeway> bins either side of an already stored point.
    // Position in peaks array.
    int i = 0;
    bool pass = true;
    int leeway = 6;

    while(i < NUM_PEAKS)
    {
        arm_max_f32(copy , FFT_SAMPLES / 2, &firstHarValue, &firstHarIndex);       
        pass = true;
        if(i < 1)
        {
            peaks[i].value = (int) firstHarValue;
            peaks[i].index = firstHarIndex;
            i++;
        }
        else
        {
            for(int k = 0; k < i; k ++)
            {
                if((int) peaks[k].index < (int) firstHarIndex + leeway && (int) peaks[k].index > (int) firstHarIndex - leeway)
                {
                    pass = false;
                }
            }
            if(pass)
            {
                peaks[i].value = (int) firstHarValue;
                peaks[i].index = firstHarIndex;
                i++;
            }
        }

        copy[firstHarIndex] = 0;
    }
    
    std::unordered_map<int, int> hash3;
    distancesPointer = 0;
    for(int i = 0; i < NUM_PEAKS; i++)
    {
        for(int j = 0; j < NUM_PEAKS; j++)
        {
            if(i != j)
            {
                int dist = (int) abs(peaks[i].index - peaks[j].index);
                //48 = middle C, 91 = Middle B - EXPAND if expanding range
                if(dist > 45 && dist < 100)
                {
                    distances[distancesPointer] = dist;
                    hash3[(int)distances[(int)distancesPointer]]++;
                    distancesPointer++;
                }
            }
        }
    }

    // Pick out most common (primary and secondary) or 'harmonic distance'.
    // Common base should be most common gap.
    int count = 0;
    int result = -1;
    int secondCount = 0;
    int secondHarmonicResult = -1;

    for(auto i : hash3)
    {
        if(count < i.second)
        {
            result = i.first;
            count = i.second;
        }
        if(secondCount < i.second && i.first != result)
        {
            secondHarmonicResult = i.first;
            secondCount = i.second;
        }
    }

    secondHarmonicFreq = BIN_WIDTH * secondHarmonicResult;

    float freqDetected = BIN_WIDTH * (result);

    // Add detected frequency to list so average can be found.

    std::unordered_map<int, int> highestBins;
    int binPointer = 0;
    highestBinBuffer[0] = (int) freqDetected;

    for(int i = 0; i < NUM_RUNS_AVERAGE-1 ; i ++)
    {
        highestBins[highestBinBuffer[binPointer++]]++;
    }

    // Average over a few runs to improve accuracy.
    if(NUM_RUNS_AVERAGE > 2)
    {
        int countAverager = 0;
        int averageResult = -1;

        for(auto i : highestBins)
        {
            if(countAverager < i.second && i.first != 0)
            {
                averageResult = i.first;
                countAverager = i.second;
            }
        }

        if(countAverager > AVERAGE_THRESH)
        {
            // Confidence in average should be over AVERAGE_THRESH of total samples.
            lastFreq = (int) averageResult;
        }
        else
        {
            lastFreq = 0; //else give best guess freq detected? or dont give anything?
        }
    }
    else
    {
        // Just check if current is same as the last.
        lastFreq = (int) freqDetected;

        if((int) lastFreq == (int) lastLastFreq)
        {
            lastLastFreq = (int) lastFreq;
        }
        else
        {
            lastLastFreq = (int) lastFreq;
            lastFreq = 0;
        }
    }

    // Clean up.
    for(int i = 0; i < NUM_PEAKS; i++)
    {
        peaks[i].value = 0;
        peaks[i].index = 0;
        peaks[i].pair = NULL;
    }

    return lastFreq;
}

/**
 * Returns a letter note that the given frequency is closest too
 *
 * @param frequency integer frequency to get the closest note too
 * @return Char closest to frequency
 */
char MicroBitAudioProcessor::frequencyToNote(int frequency){
    if(250<frequency && frequency <277){
        return 'C'; //C4
    }
    if(277<frequency && frequency <311){
        return 'D'; //D4
    }
    if(311<frequency && frequency <340){
        return 'E'; //E4
    }
    if(340<frequency && frequency <370){
        return 'F'; //F4
    }
    if(370<frequency && frequency <415){
        return 'G'; //G4
    }
    if(415<frequency && frequency <466){
        return 'A'; //A4
    }
    if(466<frequency && frequency <515){
        return 'B'; //B4
    }
    return 'X';
}

/**
 * Returns a frequency that the letter note is equivalent too
 *
 * @param note what note to look up
 * @return char frequency of note
 */
int MicroBitAudioProcessor::noteToFrequency(char note){
    if('C'){
        return 261; //C4
    }
    if('D'){
        return 293; //D4
    }
    if('E'){
        return 329; //E4
    }
    if('F'){
        return 349; //F4
    }
    if('G'){
        return 391; //G4
    }
    if('A'){
        return 440; //A4
    }
    if('B'){
        return 493; //B4
    }
    return 0;
}

/**
 * Get the last detected primary frequency
 *
 * @return lastFreq - Freqeuncy detected
 */
int MicroBitAudioProcessor::getFrequency()
{
    // DMESG("In MicroBitAudioProcessor - getFrequency - Start\n"); // REMOVE
    if(!recording)
    {
        // DMESG("In MicroBitAudioProcessor - getFrequency - in check if recording\n"); // REMOVE
        startRecording();
    }

    return lastFreq;
}

/**
 * Get the last detected secondary frequency
 *
 * @return lastFreq - Freqeuncy detected
 */
int MicroBitAudioProcessor::getSecondaryFrequency()
{
    if(!recording)
    {
        startRecording();
    }

    return secondHarmonicFreq;
}

/**
 * Start the FFT.
 */
void MicroBitAudioProcessor::startRecording()
{
    if(!activated)
    {
        upstream.connect(*this);
        activated = true;
    }

    this->recording = true;
    DMESG("START RECORDING\n");
}

/**
 * Stop the FFT.
 */
void MicroBitAudioProcessor::stopRecording()
{
    this->recording = false;
    DMESG("STOP RECORDING\n");
}

/**
 * Play Note through onboard speaker
 *
 * @param note what note to play
 * @param ms hopw long to play for
 */
void MicroBitAudioProcessor::playFrequency(char note, int ms)
{
    int frequency = noteToFrequency(note);
    playFrequency(frequency, ms);
}

// Default peak constructor.
PeakDataPoint::PeakDataPoint()
{
    this->value = 0;
    this->index = 0;
}

// Peak data point object used in claculations for square waves.
PeakDataPoint::PeakDataPoint(int8_t value, int index)
{
    this->value = value;
    this->index = index;
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

// destructor
PeakDataPoint::~PeakDataPoint()
{

}