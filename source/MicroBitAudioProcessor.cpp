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
    int s;
    int result;

    auto mic_samples = audiostream.pull();

    if (!recording)
        return DEVICE_OK;

    //using 8 bits produces more accurate to input results (not 2x like using 16) but issue with 
    //F and G both producing 363hz?

    // A 440 matches perfectly, but the rest of the notes dont?
    //int8_t *data = (int8_t *) &mic_samples[0];

    //Legacy Version
    int16_t *data = (int16_t *) &mic_samples[0];

    int samples = mic_samples.length() / 2;

    for (int i=0; i < samples; i++)
    {


        s = (int) *data;
        result = s;

        data++;
        buf[position++] = (float32_t)result;

        // we have enough samples (no windowing here)
        if (position == FFT_SAMPLES/2)
        {
            float maxValue = 0;
            uint32_t index = 0;

            // Interlace 0s
            for(int i = 0 ; i < 2048 ; i++){
                input[i] = 0;
            }
            int j = 0;
            for(int i = 0 ; i < 1024 ; i++){
                input[j] = buf[i];
                j+=2;
            }

            DMESGF("===== input =====");
            for(int i = 0 ; i < 2048 ; i++){
                //DMESGF("%d", (int) input[i]);
            }

            //DMESG("Run FFT, %d", offset);
            //auto a = system_timer_current_time();
            arm_cfft_radix4_f32(&fft_instance, input);

            arm_cmplx_mag_f32(input, output, fftSize);

            DMESGF("===== output =====");
            for(int i = 0 ; i < (int)fftSize / 2 ; i ++ ){
                DMESGF("%d", (int) output[i]);
                positiveOutput[i] = output[i];
            }

            /* Calculates maxValue and returns corresponding BIN value */ 
            arm_max_f32(positiveOutput, fftSize/2, &maxValue, &resultIndex); 
            DMESGF("Highest energy bin: %d", (int) resultIndex);
            DMESGF("%d %d %d",(int) positiveOutput[resultIndex-1], (int) positiveOutput[resultIndex], (int) positiveOutput[resultIndex+1] );
            
            //Do Parabolic Interpolation
            float32_t offsetTop = (positiveOutput[resultIndex+1] - positiveOutput[resultIndex-1]);
            float32_t offsetBottom = 2*((2*positiveOutput[resultIndex]) - positiveOutput[resultIndex-1] - positiveOutput[resultIndex+1]);
            float32_t offset = offsetTop/ offsetBottom;

            DMESGF("Offest for parabolic interpolation = %d %d %d", (int) offsetTop, (int) offsetBottom, (int) offset); 
            float32_t frequencyResolution = 11000/1024;
            float32_t frequencyDetected = frequencyResolution * ((float32_t) resultIndex + (float32_t) offset);
            DMESGF("Freq %d", (int) frequencyDetected);
            if(420<frequencyDetected && frequencyDetected <450){
                    DMESG("A 440hz Detected");
            }

            position = 0;
        }


    }

    return DEVICE_OK;
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