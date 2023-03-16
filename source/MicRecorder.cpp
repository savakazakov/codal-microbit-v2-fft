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
#include "Event.h"
#include "CodalCompat.h"
#include "Timer.h"
#include "MicRecorder.h"
#include "ErrorNo.h"
#include "CodalDmesg.h"
#include "MemorySource.h"
#include "MicroBitAudio.h"
#include "Mixer2.h"
#include "../source/samples/SerialStreamer.h"

using namespace codal;
static MemorySource *sampleSource = NULL;
static SerialStreamer *streamer = NULL;
auto t1 = system_timer_current_time();
auto t2 = system_timer_current_time();

MicRecorder::MicRecorder(DataSource &source, Mixer2 &mixer, bool connectImmediately) : upstream(source), mixer(mixer)
{
    this->activated = false;
    this->recording = false;
    this->position  = 0;
 	
    if(connectImmediately){
    	DMESG("Mic recorder connecting");
        upstream.connect(*this);
        activated = true;
    }
}

/**
 * If recording - save the mic buffer to an array - else ignore it.
 */
int MicRecorder::pullRequest()
{
    lastBuffer = upstream.pull();

    if(recording && position < BUFFER_SIZE){
        savedRecording[position] = lastBuffer;
        position++;
    }
    if(recording && position >= BUFFER_SIZE){
    	//t2 = system_timer_current_time();
    	//DMESG("%s", "Done Recording ");
    	//DMESG("%s %d", "recorded for ", (int)(t2-t1));
    	recording = false;
    }

    return DEVICE_OK;
}

// Start recording a clip - will overwrite if one already exists
void MicRecorder::startRecording()
{

    if(!activated){
    	DMESG("Mic recorder connecting");
        upstream.connect(*this);
        activated = true;
	}

    if(!recording){
		DMESG("Start Recording");
        clearStored();
    	position = 0;
    	recording = true;
    	//t1 = system_timer_current_time();
    }

}

//Stop recording - to allow variable length recordings
void MicRecorder::stopRecording()
{
    if(recording){
	    position = 0;
    	recording = false;
    }
}

//Denoise the stored sample by applying a low-pass filter
void MicRecorder::denoise(){
    int FILTER_SHIFT = 6; //lower = less smoothing
    int32_t filter_reg = 0;
    int16_t filter_input;
    int16_t filter_output;

    for(int i = 0 ; i < BUFFER_SIZE ; i++)
    {
        for(int j = 0 ; j < savedRecording[i].length() ; j++){
            if((int8_t)savedRecording[i][j] < 8 && (int8_t) savedRecording[i][j] > -8){
                filter_input = (int8_t) savedRecording[i][j];
                filter_reg = filter_reg - (filter_reg >> FILTER_SHIFT) + filter_input;
                filter_output = filter_reg >> FILTER_SHIFT;
                savedRecording[i][j] = filter_output; //set to 0 to completley remove these noisy low level frequencies
            }
            
        }
    }
}

// Play back the stored sample
void MicRecorder::playback(int sampleRate)
{
	DMESG("playback");
  
    if (sampleSource == NULL){
        sampleSource = new MemorySource();
        sampleSource->setFormat(DATASTREAM_FORMAT_8BIT_SIGNED);
        sampleSource->setBufferSize(256);
        mixer.addChannel(*sampleSource, sampleRate, 255);
        mixer.setVolume(1023);
        MicroBitAudio::requestActivation();
    }
    // TODO - this will set the global mixer sample rate
    //mixer.setSampleRate(sampleRate);
    
    /*
    // Used to debug what we are actually sending to the speaker - save output as a binary and import to audacity to inspect
    if (streamer == NULL)
        streamer = new SerialStreamer(*sampleSource, SERIAL_STREAM_MODE_BINARY);
    */

    for(int i = 0 ; i < BUFFER_SIZE ; i++)
    {
		sampleSource->play(savedRecording[i]);
    }
    
}


void MicRecorder::clearStored()
{
    //Clear the array
}

/**
 * Destructor.
 */
MicRecorder::~MicRecorder()
{
}
