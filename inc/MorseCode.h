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
#include "MicroBitAudioProcessor.h"
#include "DataStream.h"
#include <list>

#ifndef MORSE_CODE_H
#define MORSE_CODE_H

#define DOT_LENGTH 500 //milliseconds
#define INPUT_BUF_LEN 5

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
    DataSource              &audiostream;
    char                    primaryNote;
    char                    secondaryNote;
    std::list<char>         supportedNotes = {'C','D','E','F','G','A','B'};
    bool                    recognise;
    char                    primaryLetter[22];   //20 is max we can have 5 sets of dashes (3 dots each) with 1 dot after each and then 3 gaps after letter
    char                    primaryWord[20];     //support up to 20 letter words 
    char                    secondaryLetter[22];   //20 is max we can have 5 sets of dashes (3 dots each) with 1 dot after each and then 3 gaps after letter
    char                    secondaryWord[20];     //support up to 20 letter words 
    int                     correctCounterP = 0;
    int                     correctCounterS = 0;
    int                     bufP[5];
    int                     bufS[5];
    int                     skipP = 0;
    int                     skipS = 0;
    public:
        //TODO default to no note (NULL) - so that any sound can be used (by checking like we do at the start of the constructor)
        //i.e. if (X) then use any sound
    MorseCode(DataSource& source, char primaryNote, char secondaryNote, Pin &p); 
    ~MorseCode(); 
    virtual int pullRequest();
    int supportedCheck();
    void startRecognise();
    void stopRecognise();
    void doRecognise();
    char bufToChar(char letterParts[5]);
    void playFrequency(int frequency, int ms);
    void playChar(char c);
    int syncTest();

};



#endif