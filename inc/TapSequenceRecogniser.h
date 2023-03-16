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
#include "DataStream.h"
#include "Mixer2.h"
#include "Sequence.h"

#ifndef TAP_RECOGNISER_H
#define TAP_RECOGNISER_H
#define TIMING 500
#define FIRST_SEQUENCE  1
#define SECOND_SEQUENCE 2
#define THIRD_SEQUENCE  3


#include <string>

/**
 * Default configuration values
 */
#define NUM_SAVED_SEQUENCES                             20

namespace codal{
    class TapSequenceRecogniser : public CodalComponent, public DataSink
    {
    public:
        bool            activated;                          // Has this component been connected yet.
        bool            recording;                          // Is the component currently recording audio
        bool            requested = false;
        int             position;                           // Pointer to a position in the savedRecordings array
        DataSource      &upstream;                          // The component producing data to process
        Sequence        *savedSequences[NUM_SAVED_SEQUENCES];// Array of mic buffers that make up a recording
        ManagedBuffer   lastBuffer;                         // Last buffer recieved from the mic
        int             liveBuffer[SEQUENCE_SIZE];
        int             correctCounter = 0;                          
//        Sequence        currentSequence;                    // Current sequence being worked on

        /**
          * Creates a component capable of recording from the mic, storing in a buffer, and playing it back.
          *
          * @param source a DataSource to measure the level of.
          * @param connectImmediately Should this component connect to upstream splitter when started
          */
        TapSequenceRecogniser(DataSource &source,  bool connectImmediately  = true);

        /**
         * Callback provided when data is ready.
         */
    	virtual int pullRequest();

        /*
         * Record a new sequence
         */
        void recordSequence(char name);

        /*
         * Redo a sequence
         */
        void redoSequence(int i);

        /**
         * Destructor.
         */
        ~TapSequenceRecogniser();

    };
}

#endif

