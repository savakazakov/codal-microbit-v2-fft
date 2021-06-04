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

static Pin *pin = NULL;
static uint8_t pitchVolume = 0xff;
std::map<char, int> eventCode = {{'C', 1}, {'D', 2}, {'E', 3}, {'F', 4}, {'G', 5}, {'A', 6}, {'B', 7}};

MicroBitAudioProcessor::MicroBitAudioProcessor(DataSource& source, Pin &p, bool connectImmediately) : audiostream(source)
{

    this->ifftFlag = 0;
    this->bitReverse = 1;
    this->lastLastFreq = -1;
    this->activated = false;
    pin = &p;

    divisor = 1;
    lastFreq = 0;
    //Init CFFT module
    arm_rfft_fast_init_f32(&fft_instance, FFT_SAMPLES);

    ManagedBuffer outputBuffer(outBuf, 11);
    this->outputBuffer = outputBuffer;  

    this->position = FFT_SAMPLES; //first one in to the cycle size overflow buffer (so wrap arround works correctly)
    this->offset = 0;
    recording = false;

    DMESG("%s %p", "Audio Processor connecting to upstream, Pointer to me : ", this);
    if(connectImmediately){
        audiostream.connect(*this);
        activated = true;
    }
    
}

MicroBitAudioProcessor::~MicroBitAudioProcessor()
{
    //TODO fill out these? or not needed now malloc is gone?
    free(output);
}

/**
 * Return latest FFT Data
 *
 * @return DEVICE_OK on success.
 */
ManagedBuffer MicroBitAudioProcessor::pull(){
    if(!recording){
        this->recording = true;
    }
    return outputBuffer;
}

/**
 * Set the downstream component to recieve pull requests
 *
 */
void MicroBitAudioProcessor::connect(DataSink &downstream){
    this->downstream = &downstream;
}

/**
 * Do something when recieving data from Mic
 *
 * @return DEVICE_OK on success.
 */
int MicroBitAudioProcessor::pullRequest()
{
    auto mic_samples = audiostream.pull();

    //DMESGF("Time between PR %d", (int) (system_timer_current_time() - timer));
    timer = system_timer_current_time();

    if (!recording)
        return DEVICE_OK;


    int8_t *data = (int8_t *) &mic_samples[0];

    for (int i=0; i < mic_samples.length(); i++)
    {

        input[position++] = (int8_t)*data++;

        if (!(position % CYCLE_SIZE))
        {
            if(position >= (FFT_SAMPLES + CYCLE_SIZE) -1 )
                position= 0;

            offset+=CYCLE_SIZE;
            if(offset >= (FFT_SAMPLES + CYCLE_SIZE) -1){
                offset = 0;
            }

            int temp_offset = offset;
            int pos = 0;
            for(int i = 0; i < FFT_SAMPLES ; i++ ){
                if(temp_offset + pos >= (FFT_SAMPLES + CYCLE_SIZE)){
                    temp_offset = 0;
                    pos = 0;
                }
                samples[i] = input[temp_offset + pos++];
            }

            arm_rfft_fast_f32(&fft_instance, samples, output, 0);
            arm_cmplx_mag_f32(output, magOut, FFT_SAMPLES / 2);
            // DMESGF("===== output =====");
            // for(int i = 0 ; i <FFT_SAMPLES / 2 ; i ++ ){
            //     DMESGF("%d", (int) magOut[i]);
            // }

            arm_max_f32(magOut , FFT_SAMPLES / 2, &maxValue, &resultIndex);

            //Do Parabolic Interpolation
            float32_t offsetTop = (magOut[resultIndex+1] - magOut[resultIndex-1]);
            float32_t offsetBottom = 2*((2*magOut[resultIndex]) - magOut[resultIndex-1] - magOut[resultIndex+1]);
            float32_t parabolicOffset = offsetTop/ offsetBottom;

            //get second harmonic - remove first harmonic and run again
            magOut[resultIndex] = 0;

            //TODO filter out so that 2nd harmonic isnt within 5 hz each way of primary
            arm_max_f32(magOut, FFT_SAMPLES/2, &maxValue, &secondHarmonicIndex);

            float frequencyDetected = binResolution * ((float) resultIndex + (float) parabolicOffset);
            secondHarmonicFreq = binResolution * ((float) secondHarmonicIndex);
            lastFreq = (int) frequencyDetected;
            secondHarmonicFreq = secondHarmonicFreq;

            //Update rolling square buffer with 0 (so that its reset to 0 if silence and dosnt keep old high peak)
            for(int i = NUM_RUNS_AVERAGE-1 ; i > 0 ; i --){
                highestBinBuffer[i] = highestBinBuffer[i-1];
            }
            highestBinBuffer[0] = 0;

            if((int)lastFreq<800){
                //sine wave
                closestNote = frequencyToNote(lastFreq);
                secondHarmonic = frequencyToNote(secondHarmonicFreq);
            }
            else{
                //square wave - probbaly from microbit
                closestNote = frequencyToNote(getClosestNoteSquare());
                secondHarmonic = frequencyToNote(secondHarmonicFreq);
            }

            //Send detected events (only if 2 in a row are the same to remove noise)
            if(lastDetected == closestNote){
                sendEvent(closestNote);
                
            }
            if(lastDetectedS == secondHarmonic){
                sendEvent(secondHarmonic);
            }         

            lastDetected = closestNote;
            lastDetectedS = secondHarmonic;


            uint8_t notIdle = 0;
            //IDLE Freq is ~42 so use 60 to have leeway (spoken frequencies are around 150+)
            if(lastFreq > 60){
                notIdle = 1;
            }
            if(downstream != NULL){
                outputBuffer.setByte(0, (uint8_t) closestNote);
                outputBuffer.setByte(1, (uint8_t) (lastFreq/1000)%10);
                outputBuffer.setByte(2, (uint8_t) (lastFreq/100)%10);
                outputBuffer.setByte(3, (uint8_t) (lastFreq/10) %10);
                outputBuffer.setByte(4, (uint8_t) (lastFreq % 10));
                outputBuffer.setByte(5, (uint8_t) secondHarmonic);
                outputBuffer.setByte(6, (uint8_t) (secondHarmonicFreq/1000)%10);
                outputBuffer.setByte(7, (uint8_t) (secondHarmonicFreq/100)%10);
                outputBuffer.setByte(8, (uint8_t) (secondHarmonicFreq/10)%10);
                outputBuffer.setByte(9, (uint8_t) (secondHarmonicFreq%10));
                outputBuffer.setByte(10,(uint8_t) notIdle);
                downstream->pullRequest();
            }
        }
    }
    return DEVICE_OK;
}

/**
 * Sends an event that a note was detected
 *
 * @param letter Which letter was detected
 */
void MicroBitAudioProcessor::sendEvent(char letter){
    std::map<char, int>::iterator it;
    for(it=eventCode.begin(); it!=eventCode.end(); ++it){
      if(it->first == letter && it->first != lastEventSent){
        lastEventSent = it->first;
        Event e(DEVICE_ID_AUDIO_PROCESSOR, it->second );
        return;
      }
    }
}

bool sortFunction(PeakDataPoint a, PeakDataPoint b){return (a.index < b.index);}

/**
 * Determines the harmonic frequency from a square wave FFT output
 *
 * @return DEVICE_OK on success.
 */
int MicroBitAudioProcessor::getClosestNoteSquare(){

    //create copy so we still have reference to index
    for(int i = 0 ; i < (int) FFT_SAMPLES/2 ; i++){
        cpy[i] = magOut[i];
    }

    //Get highest ones and their index - dont take if value is within <leeway> bins either side of an already stored point
    int i = 0; //position in peaks array
    bool pass = true;
    int leeway = 6;

    while(i < NUM_PEAKS){
        arm_max_f32(cpy , FFT_SAMPLES / 2, &maxValue, &resultIndex);       
        pass = true;
        if(i < 1){
            peaks[i].value = (int) maxValue;
            peaks[i].index = resultIndex;
            i++;
        }
        else{
            for(int k = 0 ; k < i ; k ++){
                if((int) peaks[k].index < (int) resultIndex+leeway && (int) peaks[k].index > (int) resultIndex-leeway){
                    pass = false;
                }
            }
            if(pass){
                peaks[i].value = (int) maxValue;
                peaks[i].index = resultIndex;
                i++;
            }
        }
        cpy[resultIndex] = 0;
    }
    
    std::unordered_map<int, int> hash3;
    distancesPointer = 0;
    for(int i = 0 ; i < NUM_PEAKS ; i++){
        for(int j = 0 ; j < NUM_PEAKS ; j++){
            if(i != j){
                int dist = (int) abs(peaks[i].index - peaks[j].index);
                //48 = middle C, 91 = Middle B - EXPAND if expanding range
                if(dist > 45 && dist < 100){
                    distances[distancesPointer] = dist;
                    hash3[(int)distances[(int)distancesPointer]]++;
                    distancesPointer++;
                }
            }
        }
    }

    //pick out most common (primary and secondary) or 'harmonic distance'
    //common base should be most common gap
    int count = 0;
    int result = -1;
    int secondCount = 0;
    int secondHarmonicResult = -1;

    for(auto i : hash3){
        if(count < i.second){
            result = i.first;
            count = i.second;
        }
        if(secondCount < i.second && i.first != result){
            secondHarmonicResult = i.first;
            secondCount = i.second;
        }
    }
    secondHarmonicFreq = binResolution * secondHarmonicResult;

    float freqDetected = binResolution * (result);


    //Add detected frequency to list so average can be found
    std::unordered_map<int, int> highestBins;
    int binPointer = 0;
    highestBinBuffer[0] = (int) freqDetected;
    for(int i = 0; i < NUM_RUNS_AVERAGE-1 ; i ++){
        highestBins[highestBinBuffer[binPointer++]]++;
    }


    //Average over a few runs to improve accuracy
    if(NUM_RUNS_AVERAGE > 2){
        int countAverager = 0;
        int averageResult = -1;

        for(auto i : highestBins){
            if(countAverager < i.second && i.first != 0){
                averageResult = i.first;
                countAverager = i.second;
            }
        }

        if(countAverager > AVERAGE_THRESH){
            // confidence in average should be over AVERAGE_THRESH of total samples
            lastFreq = (int) averageResult;

        }
        else{
            lastFreq = 0; //else give best guess freq detected? or dont give anything?
        }
    }
    // Just check if current is same as the last
    else{
        lastFreq = (int) freqDetected;
        if((int) lastFreq == (int) lastLastFreq){
            lastLastFreq = (int) lastFreq;
        }
        else{
            lastLastFreq = (int) lastFreq;
            lastFreq = 0;

        }
    }

    //clean up
    for(int i = 0 ; i < NUM_PEAKS ; i++){
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
int MicroBitAudioProcessor::getFrequency(){
    if(!recording){
        startRecording();
    }
    return lastFreq;
}

/**
 * Returns the last detected primary note
 *
 * @return closestNote - note detected
 */
char MicroBitAudioProcessor::getClosestNote(){
    if(!recording){
        startRecording();;
    }
    return closestNote;
}

/**
 * Get the last detected secondary frequency
 *
 * @return lastFreq - Freqeuncy detected
 */
int MicroBitAudioProcessor::getSecondaryFrequency(){
    if(!recording){
        startRecording();
    }
    return secondHarmonicFreq;
}

/**
 * Returns the last detected secondary note
 *
 * @return closestNote - note detected
 */
char MicroBitAudioProcessor::getSecondaryNote(){
    if(!recording){
        startRecording();
    }
    return secondHarmonic;
}

/**
 * Start the FFT
 *
 */
void MicroBitAudioProcessor::startRecording()
{
    if(!activated){
        audiostream.connect(*this);
        activated = true;
    }
    this->recording = true;
    DMESG("START RECORDING");
}

/**
 * Stop the FFT
 *
 */
void MicroBitAudioProcessor::stopRecording()
{
    this->recording = false;
    DMESG("STOP RECORDING");
}

/**
 * Play frequency through onboard speaker
 *
 * @param frequency what freq to play
 * @param ms hopw long to play for
 */
// From samples/AudioTest.cpp
void MicroBitAudioProcessor::playFrequency(int frequency, int ms) {
    if (frequency <= 0 || pitchVolume == 0) {
        pin->setAnalogValue(0);
    } else {
        // I don't understand the logic of this value.
        // It is much louder on the real pin.
        int v = 1 << (pitchVolume >> 5);
        // If you flip the order of these they crash on the real pin with E030.
        pin->setAnalogValue(v);
        pin->setAnalogPeriodUs(1000000/frequency);
    }
    if (ms > 0) {
        fiber_sleep(ms);
        pin->setAnalogValue(0);
        fiber_sleep(5);
    }
}

/**
 * Play Note through onboard speaker
 *
 * @param note what note to play
 * @param ms hopw long to play for
 */
void MicroBitAudioProcessor::playFrequency(char note, int ms) {
    int frequency = noteToFrequency(note);

    playFrequency(frequency, ms);
}


// default peak constructor
PeakDataPoint::PeakDataPoint(){

    this->value = 0;
    this->index = 0;

}
//peak data point object used in claculations for square waves
PeakDataPoint::PeakDataPoint(int8_t value, int index){

    this->value = value;
    this->index = index;

}

/**
 *  Determine the data format of the buffers streamed out of this component.
 */
int MicroBitAudioProcessor::getFormat()
{
    return audiostream.getFormat();
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
PeakDataPoint::~PeakDataPoint(){

}