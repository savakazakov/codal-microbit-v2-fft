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
#include "TapSequenceRecogniser.h"
#include "Sequence.h"
#include <string>
#include <cmath>
using namespace codal;

static Sequence *currentSequence = NULL;
static int32_t filter_reg = 0;
static int16_t filter_input;
static int16_t filter_output;

TapSequenceRecogniser::TapSequenceRecogniser(DataSource &source, LevelDetector &level, bool connectImmediately) : upstream(source), level(level)
{
    this->activated = false;
    this->recording = false;
    this->signalRecord = false;
    this->position  = 0;

    if(connectImmediately){
    	DMESG("Tap Sequence Recogniser connecting");
        upstream.connect(*this);
        activated = true;
    }
    for(int i = 0 ; i < NUM_SAVED_SEQUENCES ; i++){
        savedSequences[i] = NULL;
    }
    for(int i = 0 ; i < SEQUENCE_SIZE ; i++){
        liveBuffer[i] = 0;
    }
    level.getValue();

    //check sequences
    for(int i = 0 ; i < NUM_SAVED_SEQUENCES ; i++){
        if(savedSequences[i] != NULL){
            DMESG("%s", savedSequences[i]->live ? "true" : "false");
            DMESG("%s", savedSequences[i]->name);
            for(int j = 0 ; j < SEQUENCE_SIZE ; j++){
                DMESG("%d",savedSequences[i]->sequence[j]);
            }
        }
        else{
            DMESG("Sequence is null");
        }
    }
}

/**
 * Check against stored sequences - change sample rate?
 */
int TapSequenceRecogniser::pullRequest()
{

    lastBuffer = upstream.pull();
    auto t1 = system_timer_current_time();

    int value = level.getValue();

    //Limits - set noise to 0 and set louds to a max (so no values in the 1000s corrupt the readings)
    if (value > 100)
        value = 100;
    if (value < 10)
        value = 0;

    //Smooth input
    int FILTER_SHIFT = 2; //lower = less smoothing


    filter_input = value;
    filter_reg = filter_reg - (filter_reg >> FILTER_SHIFT) + filter_input;
    filter_output = filter_reg >> FILTER_SHIFT;
    //value = filter_output;

    //update Live buffer//

    //shuffle
    for(int i = 0 ; i < SEQUENCE_SIZE-1 ; i++){
        liveBuffer[i] = liveBuffer[i+1];
    }
    //add new
    liveBuffer[SEQUENCE_SIZE-1] = value;

    //Match to saved sequences //TODO - only do this if heard a loud sound and fill up from there?
    for(int i = 0 ; i < NUM_SAVED_SEQUENCES ; i++){
        if(savedSequences[i] != NULL && savedSequences[i]->live == true){
            // DMESG("%s", savedSequences[i]->live ? "true" : "false");
            // DMESG("%s,", savedSequences[i]->name);
            //Euclidean Distance Calculation
            double ans = 0;
            for(int j = 0 ; j < SEQUENCE_SIZE ; j++){
                ans += pow(liveBuffer[j] - savedSequences[i]->sequence[j], 2);
                DMESG("%s, %d - %d, %d", "ans ", liveBuffer[j] , savedSequences[i]->sequence[j], (int)ans);
            }
            double distance = sqrt(ans);
            DMESG("%s, %d", "distance ", (int) distance);
            if(distance < 10){
                DMESG("Match Found");
            }
        }
    }

    //TODO make it start when heard the first tap - so they will hopefully line up more accuratly
    if(signalRecord == true && position == 0){
        if(value > 10){
            signalRecord = false;
            recording = true;
            while(system_timer_current_time() < t1+200){
                //sleep to stop button press counting as first beat
            }   
        }
    }

    if(recording){

        DMESG("%d", value);
        currentSequence->sequence[position] = value;
        position++;

        if(position >= SEQUENCE_SIZE){
            recording = false;
            currentSequence->live = true;
            DMESGF("Done recording");
        }
    }

    while(system_timer_current_time() < t1+100){
        //sleep
    }

    return DEVICE_OK;
}

// Start recording a clip - will overwrite if one already exists
void TapSequenceRecogniser::recordSequence(std::string name)
{
    //Check for name clash - if we already have a sequenc with this name, then redo it.
    for(int i = 0 ; i < NUM_SAVED_SEQUENCES ; i++){
        if(savedSequences[i] == NULL){
            savedSequences[i] = new Sequence(name);
            currentSequence = savedSequences[i];
            signalRecord = true;
            position = 0;
            return;
        }
        if(savedSequences[i]->name == name){
            DMESG("%s", name);
            redoSequence(name);
            return;
        }
    } 

    DMESG("Max sequences reached"); 

}

// record over an existing sequence
void TapSequenceRecogniser::redoSequence(std::string name){

    DMESGF("redo sequence");

}

/**
 * Destructor.
 */
TapSequenceRecogniser::~TapSequenceRecogniser()
{
}



