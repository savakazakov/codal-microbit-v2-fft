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
#include "MorseCode.h"
#include "MicroBitAudioProcessor.h"
#include "MicroBitAudio.h"
#include <algorithm>
#include <chrono>
#include <thread>

static Pin *pin = NULL;
static uint8_t pitchVolume = 0xff;
static auto timePeriod = system_timer_current_time();


MorseCode::MorseCode(DataSource& source, char primaryNote, char secondaryNote, Pin &p) :audiostream(source)
{
    DMESGF("hello morse detector");
    //if(note != NULL){
        //std::find returns the last item in the array if its not found
    this->primaryNote = primaryNote;
    this->secondaryNote = secondaryNote;
    pin = &p;
    //}
    // if(processor == NULL){
    //     //TODO use ubit splitter
    //     this->processor = (MicroBitAudioProcessor *) new MicroBitAudioProcessor(*audio.splitter);
    // }
    supportedCheck();

    this->recognise = false;

    DMESG("%s %p On Note %c", "Morse Code connecting to upstream processor, Pointer to me : ", this, primaryNote);
    audiostream.connect(*this);
}

MorseCode::~MorseCode()
{
    //default to any frequency but assign if there is one set

}

int MorseCode::supportedCheck(){
        bool found = (std::find(supportedNotes.begin(), supportedNotes.end(), primaryNote) != supportedNotes.end());
        if(found){
            DMESGF("ok");
            return DEVICE_OK; 
        }
        else{
            DMESGF("bad parameter");
            return DEVICE_INVALID_PARAMETER; 
        }
}

int MorseCode::pullRequest()
{
    //pull from audio processor that is set up as a data source
    ManagedBuffer noteData = audiostream.pull();
    //DMESGF("length %d", noteData.length());

    int8_t *data = (int8_t *) &noteData[0];

    if(system_timer_current_time() > timePeriod+DOT_LENGTH){
        DMESGF("correct %d", correctCounterP);

        if(correctCounterP > 10){
            DMESGF("1");
            bufP[INPUT_BUF_LEN-1] = (int) 1;
        }
        else{
            DMESGF("0");
            bufP[INPUT_BUF_LEN-1] = (int) 0;
        }

        if(correctCounterS > 10){
            bufS[INPUT_BUF_LEN-1] = (int) 1;
        }
        else{
            bufS[INPUT_BUF_LEN-1] = (int) 0;
        }
        correctCounterP = 0;
        correctCounterS = 0;
        DMESGF("----------");
        timePeriod = system_timer_current_time();
    
    }
    // //Print managed buffer recieved
    // for(int i = 0 ; i < noteData.length() ; i++){
    //     if(!(i%5))
    //         DMESGF("%c", (char) *data++);
    //     else
    //         DMESGF("%d", (int) *data++);
    // }

    for(int i = 0 ; i < noteData.length() ; i++){
        if(!(i%5)){
            //DMESGF("%c", (char) *data++);
            if((char)*data == (char) primaryNote){
                correctCounterP++;
            }
            else{
                if((char)*data == (char) secondaryNote){
                    correctCounterS++;
                }
            }
        }
        data++;

    }
    
    
    return DEVICE_OK;

}

void MorseCode::startRecognise(){

    this->recognise = true;
    doRecognise();
}


void MorseCode::stopRecognise(){
    this->recognise = false;

}

void MorseCode::doRecognise(){

    int letterPos = 0;
    int wordPos = 0;

    while(recognise){
        //auto a = system_timer_current_time();
        fiber_sleep(DOT_LENGTH);
        DMESGF("listen"); //crashing after 44 runs?
        char detected =  /*(char) processor->getClosestNote();*/ 'A';
        if(detected == primaryNote){
            //add dot
            primaryLetter[letterPos++] = '.';
        }
        else{
            //add space
            primaryLetter[letterPos++] = ' ';
        }
        //check for end of letter
        if(letterPos > 3){
            if(primaryLetter[letterPos] == ' ' && primaryLetter[letterPos-1] == ' ' && primaryLetter[letterPos-2] == ' '){
                //end of letter - convert and add to word
                char temp[5] = {'X','X','X','X','X'}; // 5 is max length of a single letter (in dots or dashes). Prefill with X's to avoid left over data
                int tempPos = 0;
                int i = 0;
                bool calculating = true;
                while(calculating){
                    if(primaryLetter[i] == '.' && primaryLetter[i+1] == ' '){
                        temp[tempPos++] = '.';
                        i+=2;
                    }
                    else if(primaryLetter[i] == '.' && primaryLetter[i+1] == '.' && primaryLetter[i+2] == '.'){
                        temp[tempPos++] = '-';
                        i+=3;
                    }

                    else if(primaryLetter[i] == ' ' && primaryLetter[i+1] == ' ' && primaryLetter[i+2] == ' '){
                        calculating = false;
                    }

                }
                primaryWord[wordPos++] = bufToChar(temp);
                DMESG("detected letter %c", bufToChar(temp));
            }
        }

    }
    return;

}

char MorseCode::bufToChar(char letterParts[5]){
    //look up in map
    if(letterParts[0] == '.' && letterParts[1] == '-' && letterParts[2] == 'X'){
        return 'A';
    }
    else{
        return 'X';
    }
}

// From samples/AudioTest.cpp
void MorseCode::playFrequency(int frequency, int ms) {
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

void MorseCode::playChar(char c){

    //look up in map
    if(c == 'A'){
        playFrequency(440, 550);
        fiber_sleep(500);
        playFrequency(440, 1550);
    }

}

int MorseCode::syncTest(){
    if(!(system_timer_current_time() % 500))
        return 1;
    else
        return 0;


}



