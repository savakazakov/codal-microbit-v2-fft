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

using namespace codal;
static MemorySource *sampleSource = NULL;

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
 * IF recording - save the mic buffer to an array - else ignore it.
 */
int MicRecorder::pullRequest()
{
    ManagedBuffer b = upstream.pull();

    if(recording && position < BUFFER_SIZE){
        //savedRecording[position] = (uint8_t*) &b[0];
        savedRecording[position] = (int16_t*) &b[0];
        position++;
    }
    if(recording && position >= BUFFER_SIZE){
    	DMESG("%s", "Done Recording ");
    	recording = false;
    }

    return DEVICE_OK;
}

void MicRecorder::startRecording()
{
	if(!activated){
    	DMESG("Mic recorder connecting");
        upstream.connect(*this);
        activated = true;
	}

    if(!recording){
		DMESG("Start Recording");
    	position = 0;
    	recording = true;
    }

}

void MicRecorder::stopRecording()
{
    if(recording){
	    position = 0;
    	recording = false;
    }
}

void MicRecorder::playback()
{
	DMESG("playback");
   
    //Take the array and give it to mixer 2
    if (sampleSource == NULL){
        sampleSource = new MemorySource();
        //DATASTREAM_FORMAT_UNKNOWN - DATASTREAM_FORMAT_16BIT_UNSIGNED - DATASTREAM_FORMAT_8BIT_SIGNED - DATASTREAM_FORMAT_8BIT_UNSIGNED
        //Managed buffer payload is uint8_t
        sampleSource->setFormat(DATASTREAM_FORMAT_16BIT_SIGNED);
        sampleSource->setBufferSize(sizeof(savedRecording[0]));
    }
    
    mixer.addChannel(*sampleSource, 22000, 255);
    MicroBitAudio::requestActivation();
   
    for(int i = 0 ; i < BUFFER_SIZE ; i++)
    {
    	//Do we need to extract the payload bytes?
    	//uint8_t byteArray[32];
    	//savedRecording[i]->readBytes(byteArray, 0, 32, false);

    	//Only 4 bytes?
    	//DMESG("%d", sizeof(savedRecording[i]));
		sampleSource->play(savedRecording[i], sizeof(savedRecording[i]));
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
