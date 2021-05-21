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
#include <thread>
#include <map>

static Pin *pin = NULL;
static uint8_t pitchVolume = 0xff;
static auto timePeriod = system_timer_current_time();

std::map<char, std::string> lookup = {
    {'A', ".-000"}, {'B', "-...0"}, {'C', "-.-.0"}, {'D', "-..00"}, {'E', ".0000"}, {'F', "..-.0"}, {'G', "--.00"}, {'H', "....0"}, 
    {'I', "..000"}, {'J', ".---0"}, {'K', "-.-00"}, {'L', ".-..0"}, {'M', "--000"}, {'N', "-.000"}, {'O', "---00"}, {'P', ".--.0"},
    {'Q', "--.-0"}, {'R', ".-.00"}, {'S', "...00"}, {'T', "-0000"}, {'U', "..-00"}, {'V', "...-0"}, {'W', ".--00"}, {'X', "-..-0"},
    {'Y', "-.--0"}, {'Z', "--..0"}, {'1', ".----"}, {'2', "..---"}, {'3', "...--"}, {'4', "....-"}, {'5', "....."}, {'6', "-...."},
    {'7', "--..."}, {'8', "---.."}, {'9', "----."}, {'0', "-----"}, {'&', ".-..."}};

MorseCode::MorseCode(DataSource& source, char primaryNote, char secondaryNote, Pin &p) :audiostream(source)
{
    DMESGF("hello morse detector");
    //if(note != NULL){
        //std::find returns the last item in the array if its not found
    this->primaryNote = primaryNote;
    this->secondaryNote = secondaryNote;
    //TOOD set based off fft cycle size (128 - 18, 256 - 10, 512 - 5)
    this->avgThresh = 10;
    pin = &p;
    //}
    // if(processor == NULL){
    //     //TODO use ubit splitter
    //     this->processor = (MicroBitAudioProcessor *) new MicroBitAudioProcessor(*audio.splitter);
    // }
    supportedCheck();

    this->recognise = false;

    //setup Buffers
    for(int i = 0 ; i < 5 ; i++){
        primaryLetter[i] = '0';
        secondaryLetter[i] = '0';
    }

    for(int i = 0 ; i < 20 ; i++){
        primaryWord[i] = '0';
        secondaryWord[i] = '0';
    }

    DMESG("%s %p On Note %c", "Morse Code connecting to upstream processor, Pointer to me : ", this, primaryNote);
    audiostream.connect(*this);
}

MorseCode::~MorseCode()
{
    //default to any frequency but assign if there is one set

}

int MorseCode::supportedCheck(){
        bool foundP = false;
        bool foundS = false;
        std::map<char, int>::iterator it;
        for(it=supportedNotes.begin(); it!=supportedNotes.end(); ++it){
          if(it->first == primaryNote){
            frequencyP = it->second;
            foundP = true;
          }
          if(it->first == secondaryNote){
            frequencyS = it->second;
            foundS = true;
          }
        }

        if(foundP && foundS){
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
    
    if(!recognise)
        return DEVICE_OK;

    int8_t *data = (int8_t *) &noteData[0];
    samples++;
    if(system_timer_current_time() > timePeriod+DOT_LENGTH){
        //shuffle buffer
        // for(int i = 0 ; i < INPUT_BUF_LEN-2 ; i++){
        //     bufP[i] = (int) bufP[i+1];
        //     DMESGF("%d", (int) bufP[i]);
        //     bufS[i] = (int) bufS[i+1];
        //     DMESGF("%d", (int) bufS[i]);
        // }

        //shuffle buff
        twoBeforeP = oneBeforeP;
        twoBeforeS = oneBeforeS;
        oneBeforeP = bufP[INPUT_BUF_LEN-1];
        oneBeforeS = bufS[INPUT_BUF_LEN-1];
        for(int i = 0 ; i < INPUT_BUF_LEN-1 ; i++){
            bufP[i] = bufP[i+1];
            bufS[i] = bufS[i+1];
        }

        DMESGF("correct %d", correctCounterP);

        //check for primary note
        if(correctCounterP > avgThresh){
            DMESGF("Add: 1");
            bufP[INPUT_BUF_LEN-1] = 1;
        }
        else{
            DMESGF("Add: 0");
            bufP[INPUT_BUF_LEN-1] = 0;
        }

        //check for secondary note
        if(correctCounterS > avgThresh){
            bufS[INPUT_BUF_LEN-1] = 1;
        }
        else{
            bufS[INPUT_BUF_LEN-1] = 0;
        }

        //auto a = system_timer_current_time();
        doRecognise(bufP, primaryLetter, primaryWord, skipP, letterPosP, wordPosP, oneBeforeP, twoBeforeP);
        doRecognise(bufS,secondaryLetter, secondaryWord, skipS, letterPosS, wordPosS, oneBeforeS, twoBeforeS);
        //DMESGF("time taken %d", (int) (system_timer_current_time() - a));
        DMESGF("samples : %d", samples);
        correctCounterP = 0;
        correctCounterS = 0;
        samples = 0;
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
            else if((char)*data == (char) secondaryNote){
                correctCounterS++;
            }

        }
        data++;
    }

   
    return DEVICE_OK;

}

void MorseCode::startRecognise(){

    this->recognise = true;
    //doRecognise();
}


void MorseCode::stopRecognise(){
    this->recognise = false;

}

void MorseCode::doRecognise(int input[INPUT_BUF_LEN], char letter[LETTER_LEN], char word[WORD_LEN], int& skip, int& letterPos, int& wordPos, int oneBefore, int twoBefore){

    bool letterEmpty = true;
    bool wordEmpty = true;

    for(int i = 0 ; i < LETTER_LEN ; i++){
        if(letter[i] == '.' || letter[i] == '-'){
            letterEmpty = false;
        }
    }
    for(int i = 0 ; i < WORD_LEN ; i++){
        if(word[i] != '0'){
            wordEmpty = false;
        }
    }


    if(!(skip > 0)){

        //TODO decide order
        if(/*oneBefore == 0 && */input[0] == 1 && input[1] == 0){
            //add dot
            DMESGF("add dot");
            letter[letterPos++] = '.';
            skip = 1;
        }
        else if(oneBefore == 0 && input[0] == 1 && input[1] == 1 && input[2] == 1 && input[3] == 0) {
            //add dash
            DMESGF("add dash");
            letter[letterPos++] = '-';
            skip = 3;
        }
        //only 2 as one will be skipped as part of inter-character wait?
        else if(input[0] == 0 && input[1] == 0 && input[2] == 0 && !letterEmpty){
            //add end of letter
            DMESGF("end of letter");
            word[wordPos++] = lookupLetter(letter);
            skip = 2;
            letterPos = 0;
            //clear letter buffer
            for(int i = 0 ; i < LETTER_LEN ; i++){
                letter[i] = '0';
            }
        }
        //only 6 becasue 7th will be skipped as part of end of inter-character wait
        else if(oneBefore == 0 && input[0] == 0 && input[1] == 0 && input[2] == 0 && input[3] == 0 && input[4] == 0 &&
        input[5] == 0 && !wordEmpty){
            //add end of word
            DMESGF("end of word");
            skip = 6;
            letterPos = 0;
            wordPos = 0;
            //Print then clear word buffer
            for(int i = 0 ; i < WORD_LEN ; i++){
                DMESGF("%c", (char) word[i]);
                word[i] = '0';
            }
        }
        else if(oneBefore == 0 && input[0] == 0 && input[1] == 1 && input[2] == 1 && input[3] == 0) {
            //add dash error correct
            DMESGF("add dash 1 ec!");
            letter[letterPos++] = '-';
            skip = 3;
        }
        else if(oneBefore == 0 && input[0] == 1 && input[1] == 1 && input[2] == 0 && input[2] == 0) {
            //add dash error correct
            DMESGF("add dash 2 ec!");
            letter[letterPos++] = '-';
            skip = 3;
        }
        else if(oneBefore == 1 && input[0] == 1 && input[1] == 1 && input[2] == 1 && input[3] == 1 && input[4] == 1) {
            //add dash error correct
            DMESGF("add dash 3 ec!");
            letter[letterPos++] = '-';
            skip = 3;
        }
        // else if(twoBefore == 1 && input[0] == 0 && input[1] == 0 && input[2] == 0 && !letterEmpty ) {
        //     //add dot error correct
        //     DMESGF("add end dot ec!");
        //     letter[letterPos++] = '.';
        //     skip = 2;
        // }
        else if(oneBefore == 0 && input[0] == 1 && input[1] == 1 && input[2] == 1 && input[3] == 1 && input[4] == 1 ) {
            //add dot error correct
            DMESGF("add dot 1 ec!");
            letter[letterPos++] = '.';
            skip = 1;
        }
        //needs re-work as catches the end of letter (3 x 0's)
        // else if(oneBefore == 0 && input[0] == 0 && input[1] == 0 && input[2] == 1 && !letterEmpty) {
        //     //add dot error correct
        //     DMESGF("add dot 2 ec!");
        //     letter[letterPos++] = '.';
        //     skip = 1;
        // }
        else if(input[0] == 1 && input[1] == 1 && input[2] == 1 && input[3] == 1 && input[4] == 0) {
            //add gap error correct (dont add a dot or dash)
            DMESGF("add gap 1 ec");
        }
        else if(oneBefore == 1 && input[0] == 1 && input[1] == 1 && input[2] == 1 && input[3] == 0) {
            //add gap error correct (dont add a dot or dash)
            DMESGF("add gap 2 ec");
        }
        //links with below
        else if(oneBefore == 1 && input[0] == 1 && input[1] == 0 && input[2] == 1 && input[3] == 1) {
            //add dash error correct
            DMESGF("add dash 4 ec!");
            letter[letterPos++] = '-';
            skip = 3;
        }
        //could happen if a previous 1101101 - > 11(1)1101 has happened
        else if(oneBefore == 1 && input[0] == 1 && input[1] == 1 && input[2] == 0 && input[3] == 1) {
            //add gap error correct (dont add a dot or dash)
            DMESGF("add gap 3 ec");
        }
        else{
            DMESGF("Operation Not Found");
            //Print out buffer to see what we missed
            DMESGF("%d", twoBefore);
            DMESGF("%d", oneBefore);
            DMESGF("---");
            for(int i = 0 ; i < INPUT_BUF_LEN ; i ++){
                DMESGF("%d", input[i]);
            }
            DMESGF("---");

        }


    }
    else{
        DMESGF("skip");
        skip--;
    }

    for(int i = 0 ; i < LETTER_LEN ; i++){
        DMESGF("%c", (char) letter[i]);
    }

    return;

}

char MorseCode::lookupLetter(char letterParts[LETTER_LEN]){
    //make letters into string
    std::string letter;
    for(int i = 0 ; i < LETTER_LEN ; i++){

        letter.push_back((char) letterParts[i]);

    }

    std::map<char, std::string>::iterator it;
    for(it=lookup.begin(); it!=lookup.end(); ++it){
      if(it->second == letter){
        return it->first;
      }
    }
    //error - letter not found
    return '@';
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

void MorseCode::playChar(char c, bool primary){

    int frequency = 0;
    if(primary)
        frequency = frequencyP;
    else
        frequency = frequencyS;

    DMESGF("play char %c", c);
    DMESGF("frequency %d", frequency);

    std::string sequence;
    std::map<char, std::string>::iterator it;
    for(it=lookup.begin(); it!=lookup.end(); ++it){
        if(it->first == c){
            sequence = it->second;
        }
    }

    for(char& c : sequence){
        DMESGF("found %c", c);
        if(c == '.')
            playFrequency(frequency, DOT_LENGTH);
        else if (c == '-')
            playFrequency(frequency, DOT_LENGTH*3);
        //gap between sections of each note
        fiber_sleep(DOT_LENGTH);
    }
    //wait inter-letter gap (3 x time unit)
    fiber_sleep(DOT_LENGTH*3);

}

void MorseCode::playString(std::string input, bool primary){

    for(char& c : input) {
        playChar(c, primary);
    }

}