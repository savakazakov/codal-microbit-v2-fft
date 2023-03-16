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

//Init for voice activation - anything above idle will be registered as a primary sound, allows humans to beep their own morse
MorseCode::MorseCode(MicroBitAudioProcessor& source, Pin &p, bool connectImmediately) :audiostream(source)
{
    DMESGF("hello morse detector");

    this->primaryNote = '0';
    this->secondaryNote = '0';
    this->secondary = false;
    this->avgThresh = 20; //18
    pin = &p;

    this->recognise = false;
    this->activated = false;
    this->voiceMode = true;

    //setup Buffers
    for(int i = 0 ; i < LETTER_LEN ; i++){
        primaryLetter[i] = '0';
        secondaryLetter[i] = '0';
    }

    for(int i = 0 ; i < WORD_LEN ; i++){
        primaryWord[i] = '0';
        secondaryWord[i] = '0';
    }

    if(connectImmediately){
        DMESGF("connecting Immediately");
        audiostream.connect(*this);
        this->activated = true;
        if(!audiostream.activated){
            audiostream.startRecording();
        }
    }
}

//standard init, allows the listening for 2 notes at a time, primary and secondary
MorseCode::MorseCode(MicroBitAudioProcessor& source, char primaryNote, char secondaryNote, Pin &p, bool connectImmediately) :audiostream(source)
{

    this->primaryNote = primaryNote;
    this->secondaryNote = secondaryNote;
    this->secondary = true;
    //TOOD set based off fft cycle size (128 - 15-18, 256 - 10, 512 - 5)
    this->avgThresh = 20; //18
    pin = &p;

    supportedCheck();

    this->recognise = false;
    this->activated = false;

    //setup Buffers
    for(int i = 0 ; i < LETTER_LEN ; i++){
        primaryLetter[i] = '0';
        secondaryLetter[i] = '0';
    }

    for(int i = 0 ; i < WORD_LEN ; i++){
        primaryWord[i] = '0';
        secondaryWord[i] = '0';
    }

    if(connectImmediately){
        DMESGF("connecting Immediately");
        audiostream.connect(*this);
        this->activated = true;
        if(!audiostream.activated){
            audiostream.startRecording();
        }
    }
}

MorseCode::~MorseCode()
{
    //destructor - free variables
}

/**
 * Check the supplied primary and secondary frequencies are valid
 *
 */
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


/**
 * Do something when recieving data from FFT
 *
 * @return DEVICE_OK on success.
 */
int MorseCode::pullRequest()
{
    //pull from audio processor that is set up as a data source
    ManagedBuffer noteData = audiostream.pull();

    if(!recognise)
        return DEVICE_OK;

    int8_t *data = (int8_t *) &noteData[0];
    samples++;
    if(system_timer_current_time() > timePeriod+DOT_LENGTH){

        //shuffle buffer
        twoBeforeP = oneBeforeP;
        twoBeforeS = oneBeforeS;
        oneBeforeP = bufP[0];
        oneBeforeS = bufS[0];
        for(int i = 0 ; i < INPUT_BUF_LEN-1 ; i++){
            bufP[i] = bufP[i+1];
            bufS[i] = bufS[i+1];
        }

        DMESGF("correct %d", correctCounterP);
        //DMESGF("correct %d", correctCounterS);

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
            //DMESGF("Add: 1");
        }
        else{
            bufS[INPUT_BUF_LEN-1] = 0;
            //DMESGF("Add: 0");
        }

        doRecognise(bufP, primaryLetter, messageP, skipP, letterPosP, wordPosP, oneBeforeP, twoBeforeP);
        if(secondary)
            doRecognise(bufS,secondaryLetter, messageS, skipS, letterPosS, wordPosS, oneBeforeS, twoBeforeS);
        DMESGF("samples : %d", samples);

        // -- Timing Correction -- if we arnt getting many samples, then run slightly quicker next time to try and mesh
        // multiply so that bigger differences are changed more than small ones

        // was correct but too small (only correct for primary or else primary and secondary will clash with correction?)
        topHeavyP = false;
        if(correctCounterP < 30 && correctCounterP > avgThresh){
            if(firstHalfP > secondHalfP){
                topHeavyP = true;
                DMESGF("correcting + ");
                timePeriod = system_timer_current_time()+((NUM_SAMPLES - correctCounterP)*6);
            }
            else{
                DMESGF("correcting - ");
                timePeriod = system_timer_current_time()-((NUM_SAMPLES - correctCounterP)*6);
            }
        }
        //was incorrect but too big
        else if (correctCounterP <= avgThresh && correctCounterP > 12){
            if(firstHalfP > secondHalfP){
                topHeavyP = true;
                DMESGF("correcting + ");
                timePeriod = system_timer_current_time()+(correctCounterP*6);
            }
            else{
                DMESGF("correcting - ");
                timePeriod = system_timer_current_time()-(correctCounterP*6);
            }

        }
        else
            timePeriod = system_timer_current_time();

        correctCounterP = 0;
        correctCounterS = 0;
        firstHalfP = 0;
        secondHalfP = 0;
        firstHalfS = 0;
        secondHalfS = 0;
        samples = 0;
        DMESGF("----------");
    }

    for(int i = 0 ; i < noteData.length()-1 ; i++){
        if(!(i%5) && i < 10){
            if((char)*data == (char) primaryNote){
                correctCounterP++;
                if(samples < NUM_SAMPLES/2){
                    firstHalfP++;
                }
                else{
                    secondHalfP++;
                }
            }
            else if((char)*data == (char) secondaryNote){
                correctCounterS++;
                if(samples < NUM_SAMPLES/2){
                    firstHalfS++;
                }
                else{
                    secondHalfS++;
                }
            }

        }
        data++;
    }

    if(voiceMode){
        //if not idle
        if((int8_t)*data == 1){
            correctCounterP++;
            if(samples < NUM_SAMPLES/2){
                firstHalfP++;
            }
            else{
                secondHalfP++;
            }
        }
    }
   
    return DEVICE_OK;

}

/**
 * Starts Morse recognition
 *
 */
void MorseCode::startRecognise(){
    DMESGF("start recognise");
    if(!activated){
        DMESGF("wasnt activated");
        audiostream.connect(*this);
        activated = true;
        if(!audiostream.activated){
            audiostream.startRecording();
        }
    }
    this->recognise = true;
    //doRecognise();
}

/**
 * Stops Morse recognition
 *
 */
void MorseCode::stopRecognise(){
    this->recognise = false;

}

/**
 * Recognise a given buffer input (consisting of 1 and 0s) into morse . and -
 *
 * @param input current input buffer to recognise
 * @param letter current state of letter being detected
 * @param string current output buffer
 * @param skip how many inputs to skip
 * @param letterPos  position in letter array
 * @param wordPos position in word array
 * @param oneBefore value one before input
 * @param twoBefore value two before input
 */
void MorseCode::doRecognise(int input[INPUT_BUF_LEN], char letter[LETTER_LEN], std::string& message, int& skip, int& letterPos, int& wordPos, int oneBefore, int twoBefore){

    bool letterEmpty = true;
    bool wordEmpty = true;

    if(wordPos >= WORD_LEN){
        DMESGF("buffer full");
        return;
    }

    for(int i = 0 ; i < LETTER_LEN ; i++){
        if(letter[i] == '.' || letter[i] == '-'){
            letterEmpty = false;
        }
    }
    if(message.length() > 0)
        wordEmpty = false;

    if(!(skip > 0)){

        if(input[0] == 1 && input[1] == 0){
            DMESGF("add dot");
            letter[letterPos++] = '.';
            skip = 1;
        }
        else if(input[0] == 1 && input[1] == 1 && input[2] == 1 && input[3] == 0) {
            DMESGF("add dash");
            letter[letterPos++] = '-';
            skip = 3;
        }
        //only 2 as one will be skipped as part of inter-character wait?
        else if(input[0] == 0 && input[1] == 0 /*&& input[2] == 0 */&& !letterEmpty){
            //add end of letter
            //DMESGF("end of letter");
            //word[wordPos++] = lookupLetter(letter);
            DMESGF("adding letter %c", lookupLetter(letter));
            message.push_back(lookupLetter(letter));
            DMESGF("last message %c", message[message.length()-1]);
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
            
            //only add if there is letters before (dont add double space)
            if(! (message[message.length()-1] == ' ')){
                //DMESGF("end of word - add space");
                //word[wordPos++] = ' ';
                message.push_back(' ');
                skip = 6;
                letterPos = 0;
            }
            else{
                //TODO send message recieved event
                //DMESGF("end of word - waiting for more");
            }

        }
        //Error correction Cases
        else if(oneBefore == 0 && input[0] == 0 && input[1] == 1 && input[2] == 1 && input[3] == 0 && topHeavyP) {
            //add dash error correct
            DMESGF("add dash 1 ec!");
            letter[letterPos++] = '-';
            skip = 3;
        }
        else if(oneBefore == 0 && input[0] == 1 && input[1] == 1 && input[2] == 0 && input[3] == 0) {
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
        else if(twoBefore == 1 && oneBefore == 0 && input[0] == 0 && input[1] == 0 && input[2] == 1 && !letterEmpty){
            //add dot error correct
            DMESGF("add dot 1 ec!");
            letter[letterPos++] = '.';
            skip = 1;
        }
        else if(twoBefore == 1 && oneBefore == 0 && input[0] == 0 && input[1] == 1 && input[2] == 0 && !letterEmpty){
            //add dot error correct
            DMESGF("add dot 2 ec!");
            letter[letterPos++] = '.';
            skip = 1;
        }
        else if(oneBefore == 0 && input[0] == 1 && input[1] == 1 && input[2] == 1 && input[3] == 1 && input[4] == 1 ) {
            //add dot error correct
            DMESGF("add dot 3 ec!");
            letter[letterPos++] = '.';
            skip = 1;
        }
        else if(oneBefore == 0 && input[0] == 1 && input[1] == 1 && input[2] == 0 && input[3] == 1 && !topHeavyP) {
            //add dot error correct
            DMESGF("add dot 4 ec!");
            letter[letterPos++] = '.';
            skip = 2;
        }
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
        }
    }
    else{
        skip--;
     }
    return;
}

/**
 * Converts array of . and - into alphabet letter
 *
 * @param letterParts array of . and -
 * @return Char letter represented in morse code
 */
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

/**
 * Play frequency through onboard speaker
 *
 * @param frequency what freq to play
 * @param ms hopw long to play for
 */
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

/**
 * Play letter through onboard speaker
 *
 * @param c what letter to play
 * @param primary primary (true) or secondary frequency (false)
 */
void MorseCode::playChar(char c, bool primary){

    int frequency = 0;
    if(primary)
        frequency = frequencyP;
    else
        frequency = frequencyS;

    //DMESGF("play char %c", c);
    //DMESGF("frequency %d", frequency);

    std::string sequence;
    std::map<char, std::string>::iterator it;
    for(it=lookup.begin(); it!=lookup.end(); ++it){
        if(it->first == c){
            sequence = it->second;
        }
    }

    for(char& c : sequence){
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

/**
 * Play String of letters through onboard speaker
 *
 * @param string String to play
 * @param primary primary (true) or secondary frequency (false)
 */
void MorseCode::playString(std::string input, bool primary){

    for(char& c : input) {
        playChar(c, primary);
    }

}

/**
 * Returns the detected message string so far
 *
 * @param primary primary (true) or secondary frequency (false)
 * @return message - result string
 */
std::string MorseCode::getStored(bool primary){
    if(!activated){
        startRecognise();
    }

    std::string ret;
    if(primary){
        // for(int i = 0 ; i < WORD_LEN ; i++)
        //     ret.push_back((char) primaryWord[i]);
        return messageP;
    }
    else{
        // for(int i = 0 ; i < WORD_LEN ; i++)
        //     ret.push_back((char) secondaryWord[i]);   
        return messageS;
    }
    //return ret;

}

/**
 * Prints results thorugh serial port
 *
 * @param primary primary (true) or secondary frequency (false)
 */
void MorseCode::serialPrintStored(bool primary){
    if(!activated){
        startRecognise();
    }

    if(primary){
        // for(int i = 0 ; i < WORD_LEN ; i++)
        //     DMESGF("%c", (char) primaryWord[i]);
        for(char& c : messageP) {
            DMESGF("%c", (char) c);
        }
    }
    else{
        // for(int i = 0 ; i < WORD_LEN ; i++)
        //     DMESGF("%c", (char) secondaryWord[i]);
        for(char& c : messageS) {
            DMESGF("%c", (char) c);
        }
    }
}

/**
 * Returns just the last char detected
 *
 * @param primary primary (true) or secondary frequency (false)
 * @return char - last character detected string
 */
char MorseCode::getLastChar(bool primary){
    if(!activated){
        startRecognise();
    }

    if(primary){
        //return primaryWord[wordPosP-1];
        //messageP.getLast
        return messageP[messageP.length()-1];
    }
    else{
        //return secondaryWord[wordPosS-1];
        //messageS.getLast
        return messageS[messageS.length()-1];
    }
}

/**
 * Clears the stored message strings
 *
 * @param primary primary (true) or secondary frequency (false)
 */
void MorseCode::clearStored(bool primary){
    if(!activated){
        startRecognise();
    }


    if(primary){

        messageP = "";

    }
    else{

        messageS = "";

    }
}