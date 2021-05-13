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

    //Mic samples are uint8_t

    //int8_t *data = (int8_t *) &mic_samples[0];
    int16_t *data = (int16_t *) &mic_samples[0];

    for (int i=0; i < mic_samples.length() / 2; i++)
    {

        buf[position++] = (int16_t) *data++;;

        // we have enough samples (no windowing here)
        if (position == FFT_SAMPLES/2)
        {
            float maxValue = 0;

            // Interlace 0s
            for(int i = 0 ; i < 2048 ; i++){
                input[i] = 0;
            }
            int16_t min = buf[0];
            int16_t max = buf[0];
            int j = 0;
            for(int i = 0 ; i < 1024 ; i++){
                input[j] = buf[i];
                if(buf[i]<min){
                    min = buf[i];
                }
                if(buf[i]>max){
                    max = buf[i];
                }
                j+=2;
            }

            // int FILTER_SHIFT = 6; //lower = less smoothing
            // int32_t filter_reg = 0;
            // int8_t filter_input;
            // int8_t filter_output;          

            // normalize every other value (the input buffers not the 0s)
            //for(int i = 0 ; i  < 2048 ; i+=2){
                //normalise
                //input[i] = 2*((input[i] - min)/(max-min)) -1;

                //smooth with low pass filter
                // filter_input = input[i];
                // filter_reg = filter_reg - (filter_reg >> FILTER_SHIFT) + filter_input;
                // filter_output = filter_reg >> FILTER_SHIFT;
                // input[i] = filter_output; //set to 0 to completley remove these noisy low level frequencies

            //}

            //DMESGF("===== input =====");
            //for(int i = 0 ; i < 1024 ; i++){
                //DMESGF("%d", (int) input[i]);
            //}

            //DMESG("Run FFT, %d", offset);
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
            //DMESGF("Highest energy bin: %d", (int) resultIndex);
            //DMESGF("%d %d %d",(int) positiveOutput[resultIndex-1], (int) positiveOutput[resultIndex], (int) positiveOutput[resultIndex+1] );
            
            //Do Parabolic Interpolation
            float32_t offsetTop = (positiveOutput[resultIndex+1] - positiveOutput[resultIndex-1]);
            float32_t offsetBottom = 2*((2*positiveOutput[resultIndex]) - positiveOutput[resultIndex-1] - positiveOutput[resultIndex+1]);
            float32_t offset = offsetTop/ offsetBottom;

                // TODO Stack interpolation for greater accuracy//
            // int last highestBin (define in h)
            // if(highest bin is the same as last time){
            //     add offset to list
            //     offset = average of list
            // }

            //DMESGF("Offest for parabolic interpolation = %d %d %d", (int) offsetTop, (int) offsetBottom, (int) offset); 
            float frequencyResolution = (11000.0f/2.0f)/1024.0f;
            float frequencyDetected = frequencyResolution * ((float) resultIndex + (float) offset);
            DMESGF("Freq %d", (int) frequencyDetected);
            lastFreq = frequencyDetected;
            if(lastFreq<600){
                //sine wave
                closestNote = frequencyToNote(lastFreq);
            }
            else{
                //square wave - probbaly from microbit
                closestNote = frequencyToNote(getClosestNoteSquare());
            }
            
            position = 0;
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
    //untill we have <NUM_PEAKS> data points - Should be enough to find the first 5 pairs giving 5 extra leeway for spurious results
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

    //result is the FFT bin with peak energy
    DMESGF("Short Result : %d", (int) result);
    float binResolution = (float) ((11000.0f/2.0f)/1024.0f);
    float freqDetected = binResolution * (result);
    DMESGF("freq Detected %d", (int) freqDetected);

/*

    //put into pairs
        //retrieve highest peaks
    for(int i = 0 ; i < numPairs ; i++){
        //pair with closest high peak
        for(int j = 0 ; j < NUM_PEAKS ; j++){
            if((int) peaks[j].value == (int) highestValues[i]){
                if(j == 0){
                    peaks[j].pair = &peaks[j+1];
                }
                else if(j == NUM_PEAKS-1){
                    peaks[j].pair = &peaks[j-1];
                }
                else{
                    //check top distance but remove circular reference i.e. A->B B->A is the same pair
                    int distance = (int) peaks[j+1].index - (int) peaks[j].index;
                    //check bottom distance
                    if((int) ((int) peaks[j].index - (int) peaks[j-1].index) < (int) distance){

                        peaks[j].pair = &peaks[j-1];
                    }
                    else{
                        peaks[j].pair = &peaks[j+1];
                    }
                }
            }
        }
    }

    //calcualte inter-pair distance
        
    int ipDistances[NUM_PEAKS]; //inter-peak distances
    int distancePointer = 0;
    std::unordered_map<int, int> hash;

    for(int i = NUM_PEAKS-1 ; i > 0 ; i--){
        if(peaks[i].pair != NULL){
            int distance = (int) ((int) peaks[i].index - (int) peaks[i].pair->index);
            //We want 'right peaks' as the main, and 'left peaks' as the pair
            if((int) distance > 0){
                ipDistances[distancePointer] = (int)distance;
                hash[ipDistances[distancePointer]]++;
                distancePointer++;
                //DMESGF("top value %d with index %d", peaks[i].value, peaks[i].index);
                //DMESGF("Paired with value %d with index %d", peaks[i].pair->value, peaks[i].pair->index);
            };
        }
    }
    //remove any anomolous pairs (to account for single peak start)
    //std::sort(ipDistances, ipDistances+distancePointer);

    count = 0;
    result = -1;

    for(auto i : hash){
        if(count < i.second){
            result = i.first;
            count = i.second;
        }
    }

    // for(int i = 0 ; i < distancePointer ; i++){
    //     DMESGF("inter-peak distance %d", ipDistances[i]);
    // }
    // DMESGF("Most common %d", (int) result);

    //Remove anomolies to get a list of the final peak points, right spike then left in sets of 2.
    int numFinalPoints = numPairs*2;
    int finalPoints[numFinalPoints] = {-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1};
    int fpPointer = 0;
    for(int i = NUM_PEAKS-1 ; i > 0 ; i--){
        if(peaks[i].pair != NULL){
            int distance = (int) ((int) peaks[i].index - (int) peaks[i].pair->index);
            if((int) distance > 0 && (int)distance < result+2 && (int) distance > result-2){
                finalPoints[fpPointer] = peaks[i].index;
                finalPoints[fpPointer+1] = peaks[i].pair->index;
                fpPointer+=2;
                //DMESGF("top value %d with index %d", peaks[i].value, peaks[i].index);
                //DMESGF("Paired with value %d with index %d", peaks[i].pair->value, peaks[i].pair->index);
            };
        }
    }

    std::unordered_map<int, int> hash2;
    //calcualte distance between peaks
    for(int i = 0 ; i < numFinalPoints ; i+=2){
        if(finalPoints[i] > -1 && finalPoints[i+2] > 1){
            //DMESGF("Inter Peak value %d", finalPoints[i] - finalPoints[i+2]);
            //DMESGF("Inter Peak value %d", finalPoints[i+1] - finalPoints[i+3]);
            hash2[finalPoints[i] - finalPoints[i+2]]++;
            hash2[finalPoints[i+1] - finalPoints[i+3]]++;
        }
    }

    count = 0;
    result = -1;

    for(auto i : hash2){
        if(count < i.second){
            result = i.first;
            count = i.second;
        }
    }

    DMESGF("long result %d", result);

    //get frequency as int
    float frequencyResolution = (11000/2)/1024;
    float frequencyDetected = frequencyResolution * (result);
    //DMESGF("Freq %d", (int) frequencyDetected);


*/
    //clean up
    for(int i = 0 ; i < NUM_PEAKS ; i++){
        peaks[i].value = 0;
        peaks[i].index = 0;
        peaks[i].pair = NULL;
    }

    return freqDetected;

}

char MicroBitAudioProcessor::getClosestNote(){
    if(closestNote != NULL){
        return closestNote;
    }
    else return NULL;  
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