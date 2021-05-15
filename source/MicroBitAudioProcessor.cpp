/*
The MIT License (MIT)
Copyright (c) 2020 Arm Limited.
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
#include "MicroBitAudioProcessor.h"
#include <algorithm>
#include <unordered_map>
#include <cstdlib>


MicroBitAudioProcessor::MicroBitAudioProcessor(DataSource& source) : audiostream(source)
{

    this->ifftFlag = 0;
    this->bitReverse = 1;
    
    divisor = 1;
    lastFreq = 0;
    //Init CFFT module
    arm_cfft_radix4_init_f32(&fft_instance, fftSize, ifftFlag, bitReverse);

    position = 0;
    recording = false;

    DMESG("%s %p", "Audio Processor connecting to upstream, Pointer to me : ", this);
    audiostream.connect(*this);
}

MicroBitAudioProcessor::~MicroBitAudioProcessor()
{
    free(buf);
    free(output);
}

int MicroBitAudioProcessor::pullRequest()
{
    //int s;
    //int result;

    ManagedBuffer mic_samples = audiostream.pull();

    if (!recording)
        return DEVICE_OK;

    //Mic samples are uint8_t?

    //int8_t *data = (int8_t *) &mic_samples[0];
    int16_t *data = (int16_t *) &mic_samples[0];

    //Dont divide by 2 if using 8 bit
    for (int i=0; i < mic_samples.length()/2; i++)
    {
        //shuffle buffer
        for(int i = (FFT_SAMPLES/2)-1 ; i > 0  ; i--){
            buf[i] = buf[i-1];
        }

        //put new sample at start of buffer
        buf[0] = (int16_t) *data++;
        position++;

        // we have cycled
        if (position == CYCLE_SIZE && recording)
        {
            position = 0;
            float maxValue = 0;

            // Interlace 0s
            for(int i = 0 ; i < 2048 ; i++){
                input[i] = 0;
            }
            int min = buf[0];
            int max = buf[0];
            int j = 0;
            for(int i = 0 ; i < 1024 ; i++){
                input[j] = buf[i];
                j+=2;
            }


            //DMESGF("===== input =====");
            //for(int i = 0 ; i < 1024 ; i++){
                //DMESGF("%d", (int) input[i]);
            //}

            //auto a = system_timer_current_time();
            arm_cfft_radix4_f32(&fft_instance, input);

            arm_cmplx_mag_f32(input, output, fftSize);

            //DMESGF("===== output =====");
            for(int i = 0 ; i < (int)fftSize / 2 ; i ++ ){
                //DMESGF("%d", (int) output[i]);
                positiveOutput[i] = output[i];
            }

            /* Calculates maxValue and returns corresponding BIN value */ 
            arm_max_f32(positiveOutput, fftSize/2, &maxValue, &resultIndex); 

            //Do Parabolic Interpolation
            float32_t offsetTop = (positiveOutput[resultIndex+1] - positiveOutput[resultIndex-1]);
            float32_t offsetBottom = 2*((2*positiveOutput[resultIndex]) - positiveOutput[resultIndex-1] - positiveOutput[resultIndex+1]);
            float32_t offset = offsetTop/ offsetBottom;

            //get second harmonic - remove first harmonic and run again
            positiveOutput[resultIndex] = 0;
            arm_max_f32(positiveOutput, fftSize/2, &maxValue, &secondHarmonicIndex); 


            // TODO Stack interpolation for greater accuracy//
            // int last highestBin (define in h)
            // if(highest bin is the same as last time){
            //     add offset to list
            //     offset = average of list
            // }

            //DMESGF("Offest for parabolic interpolation = %d %d %d", (int) offsetTop, (int) offsetBottom, (int) offset); 
            float frequencyResolution = (11000.0f/2.0f)/1024.0f;
            float frequencyDetected = frequencyResolution * ((float) resultIndex + (float) offset);
            float secondHarmonicDetected = frequencyResolution * ((float) secondHarmonicIndex);
            lastFreq = frequencyDetected;
            secondHarmonicFreq = secondHarmonicDetected;
            DMESGF("1st Freq %d", (int) frequencyDetected);
            DMESGF("2nd Freq %d", (int) secondHarmonicDetected);


            //Update rolling squre buffer with 0 (so that its reset to 0 if silence and dosnt keep old high peak)
            for(int i = NUM_RUNS_AVERAGE-1 ; i > 0 ; i --){
                highestBinBuffer[i] = highestBinBuffer[i-1];
            }
            highestBinBuffer[0] = 0;

            if(lastFreq<600){
                //sine wave
                closestNote = frequencyToNote(lastFreq);
            }
            else{
                //square wave - probbaly from microbit
                closestNote = frequencyToNote(getClosestNoteSquare());
            }
        }
    }
    return DEVICE_OK;
}

bool sortFunction(PeakDataPoint a, PeakDataPoint b){return (a.index < b.index);}

int MicroBitAudioProcessor::getClosestNoteSquare(){
    //DMESG("Get closest square");

    //create copy so we still have reference to index
    for(int i = 0 ; i < (int) fftSize/2 ; i++){
        cpy[i] = positiveOutput[i];
    }

    //Order peaks
    std::sort(cpy, cpy+(int)fftSize/2);

    //Get highest ones and their index - dont take if value is within <leeway> bins either side of an already stored point
    int i = 0; //position in peaks array
    int p = 0; //position in cpy buffer of magnitudes
    bool pass = true;
    
    int leeway = 6;
    int numPairs = 5;
    //int highestValues[numPairs];
    //until we have <NUM_PEAKS> data points - Should be enough to find the first 5 pairs giving 5 extra leeway for spurious results
    while( i < NUM_PEAKS){
        int target = (int) cpy[(fftSize/2) - (p+1)];
        pass = true;
        for(int j = 0 ; j < (int) fftSize/2 ; j++){
            //get index of peak (j)
            if((int) target == (int) positiveOutput[j]){
                //pre-load first one
                if(i == 0){
                    peaks[i].value = target;
                    peaks[i].index = j;
                    //highestValues[i] = (int) target;
                    i++;
                    p++;
                }
                // check we dont already have this peak
                else{
                    for(int k = 0 ; k < i ; k ++){
                        if(peaks[k].index < j+leeway && peaks[k].index > j-leeway){
                            pass = false;
                        }
                    }
                    if(pass){
                        peaks[i].value = target;
                        peaks[i].index = j;
                        if(i<numPairs){
                            //highestValues[i] = (int) target;
                        }
                        i++;
                    }
                    p++;
                } 
            }
        }
    }

    //order by index
    std::sort(peaks, peaks+NUM_PEAKS, sortFunction);

    //Get distances between all peaks
    std::unordered_map<int, int> hash3;
    int distances[NUM_PEAKS*NUM_PEAKS];
    int distancesPointer = 0;
    for(int i = 0 ; i < NUM_PEAKS ; i++){
        for(int j = 0 ; j < NUM_PEAKS ; j++){
            if(i != j){
                int dist = (int) abs(peaks[i].index - peaks[j].index);
                //48 = middle C, 91 = Middle B
                if(dist > 45 && dist < 100){
                    distances[distancesPointer] = dist;
                    //DMESGF("distance: %d", distances[distancesPointer]);
                    hash3[distances[distancesPointer]]++;
                    distancesPointer++;
                }
            }
        }
    }

    //pick out most common or 'harmonic distance'
    //common base should be most common gap
    int count = 0;
    int result = -1;

    for(auto i : hash3){
        if(count < i.second){
            result = i.first;
            count = i.second;
        }
    }

    //get Second harmonic
    int secondGap = 0;
    int secondHarmonicResult = -1;

    for(auto i : hash3){
        if(secondGap < i.second && i.first != result){
            secondHarmonicResult = i.first;
            secondGap = i.second;
        }
    }

    //result is the FFT bin with peak energy
    DMESGF("Short Result : %d", (int) result);
    DMESGF("2nd harmonic result %d", (int)secondHarmonicResult);
    float binResolution = (float) ((11000.0f/2.0f)/1024.0f);
    float freqDetected = binResolution * (result);

    //DMESGF("freq Detected %d", (int) freqDetected);

    //Add detected frequency to list so anomolies can be removed
    std::unordered_map<int, int> highestBins;
    int binPointer = 0;
    highestBinBuffer[0] = freqDetected;
    for(int i = 0; i < NUM_RUNS_AVERAGE-1 ; i ++){
        highestBins[highestBinBuffer[binPointer++]]++;
    }

    //Average over a few runs to improve accuracy
    int countAverager = 0;
    int averageResult = -1;

    for(auto i : highestBins){
        if(countAverager < i.second && i.first != 0){
            averageResult = i.first;
            countAverager = i.second;
        }
    }

    lastFreq = averageResult;
    //DMESG("frequency : %d", result);
    //clean up
    for(int i = 0 ; i < NUM_PEAKS ; i++){
        peaks[i].value = 0;
        peaks[i].index = 0;
        peaks[i].pair = NULL;
    }

    return result;

}

char MicroBitAudioProcessor::getClosestNote(){
    return closestNote;
}

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

int MicroBitAudioProcessor::getFrequency(){
    return lastFreq;
}


int MicroBitAudioProcessor::setDivisor(int d)
{
    divisor = d;
    return DEVICE_OK;
}


void MicroBitAudioProcessor::startRecording()
{
    this->recording = true;
    DMESG("START RECORDING");
}

void MicroBitAudioProcessor::stopRecording(MicroBit& uBit)
{
    this->recording = false;
    DMESG("STOP RECORDING");
}


// default peak
PeakDataPoint::PeakDataPoint(){

    this->value = 0;
    this->index = 0;

}
//peak data point object used in claculations for square waves
PeakDataPoint::PeakDataPoint(int value, int index){

    this->value = value;
    this->index = index;

}

// destructor
PeakDataPoint::~PeakDataPoint(){

}