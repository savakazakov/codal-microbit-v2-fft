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
static auto timePeriod = system_timer_current_time();

TapSequenceRecogniser::TapSequenceRecogniser(DataSource &source, bool connectImmediately) : upstream(source)
{
    this->activated = false;
    this->recording = false;
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
 * Do something when recieving data from FFT - check against saved patterns
 *
 * @return DEVICE_OK on success.
 */
int TapSequenceRecogniser::pullRequest()
{
    //pull from audio processor that is set up as a data source
    ManagedBuffer noteData = upstream.pull();
    int8_t *data = (int8_t *) &noteData[0];

    if(system_timer_current_time() > timePeriod+TIMING){

        if(correctCounter > 15 && requested){
            DMESGF("starting record");
            recording = true;
            requested = false;
        }

        if(recording){
            if(correctCounter > 15){
                currentSequence->sequence[position] = correctCounter;
            }
            else{
                currentSequence->sequence[position] = 0;
            }

            position++;
            if(position >= SEQUENCE_SIZE){
                recording = false;
                currentSequence->live = true;
                DMESGF("Done recording Sequence");
            }
        }
        //update Live buffer//

        //shuffle
        for(int i = 0 ; i < SEQUENCE_SIZE-1 ; i++){
            liveBuffer[i] = liveBuffer[i+1];
        }
        //add new
        if(correctCounter > 15){
            liveBuffer[SEQUENCE_SIZE-1] = correctCounter;
        }
        else{
            liveBuffer[SEQUENCE_SIZE-1] = 0;
        }

        //Match to saved sequences
        for(int i = 0 ; i < NUM_SAVED_SEQUENCES ; i++){
            if(savedSequences[i] != NULL && savedSequences[i]->live == true){
                //Euclidean Distance Calculation
                double ans = 0;
                for(int j = 0 ; j < SEQUENCE_SIZE ; j++){
                    ans += pow(liveBuffer[j] - savedSequences[i]->sequence[j], 2);
                    DMESG("%s, %d - %d, %d", "ans ", liveBuffer[j] , savedSequences[i]->sequence[j], (int)ans);
                }
                double distance = sqrt(ans);
                DMESG("%s, %d", "distance ", (int) distance);
                //Distance threshold - determines fuzziness
                if(distance < 20){
                    DMESG("!Match Found! %d", i);
                    Event e(DEVICE_ID_TAP, i+1);
                }
            }
        }
        correctCounter = 0;
        timePeriod = system_timer_current_time();
    }

    for(int i = 0 ; i < noteData.length() ; i++){
        //check last one
        if(i == noteData.length()-1){
            if((int8_t)*data == 1){
                correctCounter++;
            }
        }
        data++;
    }

    return DEVICE_OK;
}


/**
 * Start recording a clip - will overwrite if one already exists
 *
 * @param name name of sequence to record
 */
void TapSequenceRecogniser::recordSequence(char name)
{
    if(!activated){
        upstream.connect(*this);
        activated = true;
    }
    //Check for name clash - if we already have a sequence with this name, then redo it.
    for(int i = 0 ; i < NUM_SAVED_SEQUENCES ; i++){
        if(savedSequences[i] == NULL){
            savedSequences[i] = new Sequence(name);
            currentSequence = savedSequences[i];
            this->requested = true;
            position = 0;
            return;
        }
        if(savedSequences[i]->name == name){
            DMESG("%s", name);
            redoSequence(i);
            return;
        }
    } 

    DMESG("Max sequences reached"); 

}

 
/**
 * Record over an existing sequence
 *
 * @param name name of sequence to re-record
 */
void TapSequenceRecogniser::redoSequence(int i){

    DMESGF("redo sequence");
    currentSequence = savedSequences[i];
    this->requested = true;
    position = 0;
    return;

}

/**
 * Destructor.
 */
TapSequenceRecogniser::~TapSequenceRecogniser()
{
}



