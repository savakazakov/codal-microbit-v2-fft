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

#include "MicroBitAudioProcessor.h"
#include "DataStream.h"
#include <list>
#include <map>

#ifndef MORSE_CODE_H
#define MORSE_CODE_H

#define DOT_LENGTH 500 //milliseconds
#define INPUT_BUF_LEN 7 // max is 7 spaces for a word gap
#define LETTER_LEN 5
#define WORD_LEN 20
#define NUM_SAMPLES 22

// class MicroBitAudioProcessor;

// class PeakDataPoint
// {
//     public:
//     int value;
//     int index;
//     PeakDataPoint* pair = NULL;
//     public:
//     PeakDataPoint(int value, int index);
//     PeakDataPoint();
//     ~PeakDataPoint();


// };

class MorseCode : public DataSink
{
    MicroBitAudioProcessor  &audiostream;
    char                    primaryNote;
    char                    secondaryNote;
    std::map<char, int>     supportedNotes = {{'C', 261},{'D', 293},{'E', 329},{'F', 349},{'G', 391},{'A', 440},{'B', 493}};
    bool                    recognise;
    char                    primaryLetter[LETTER_LEN];   //5 is max we can have
    char                    primaryWord[WORD_LEN];     //support up to 20 letter words 
    char                    secondaryLetter[LETTER_LEN];   //20 is max we can have 5 sets of dashes (3 dots each) with 1 dot after each and then 3 gaps after letter
    char                    secondaryWord[WORD_LEN];     //support up to 20 letter words 
    int                     correctCounterP = 0;
    int                     correctCounterS = 0;
    int                     bufP[INPUT_BUF_LEN];
    int                     bufS[INPUT_BUF_LEN];
    int                     letterPosP = 0;
    int                     letterPosS = 0;
    int                     wordPosP = 0;
    int                     wordPosS = 0;
    int                     skipP = 0;
    int                     skipS = 0;
    int                     frequencyP = 440;
    int                     frequencyS = 329;
    int                     avgThresh;
    int                     samples = 0;
    int                     oneBeforeP = 0;
    int                     oneBeforeS = 0;
    int                     twoBeforeP = 0;
    int                     twoBeforeS = 0;
    int                     firstHalfP = 0;
    int                     secondHalfP = 0;
    int                     firstHalfS = 0;
    int                     secondHalfS = 0;
    bool                    secondary;
    bool                    voiceMode = false;
    bool                    activated = false;
    bool                    topHeavyP;
    std::string             messageP = "";
    std::string             messageS = "";

    public:
    //Init for voice activation - anything above idle will be registered as a primary sound, allows humans to beep their own morse
    MorseCode(MicroBitAudioProcessor& source, Pin &p, bool connectImmediately = true);
    //standard init, allows the listening for 2 notes at a time, primary and secondary
    MorseCode(MicroBitAudioProcessor& source, char primaryNote, char secondaryNote, Pin &p, bool connectImmediately = true); 
    ~MorseCode(); 
    virtual int pullRequest();
    int supportedCheck();
    void startRecognise();
    void stopRecognise();
    void doRecognise(int input[INPUT_BUF_LEN], char letter[LETTER_LEN], std::string& message, int& skip, int& letterPos, int& wordPos, int oneBeforeValue, int twoBeforeValue);
    char lookupLetter(char letterParts[LETTER_LEN]);
    void playFrequency(int frequency, int ms);
    void playChar(char c, bool primary);
    void playString(std::string c, bool primary);
    void serialPrintStored(bool primary);
    char getLastChar(bool primary);
    void clearStored(bool primary);
    std::string getStored(bool primary);
};
#endif