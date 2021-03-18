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

#ifndef MIC_RECORDER_H
#define MIC_RECORDER_H


/**
 * Default configuration values
 */
#define BUFFER_SIZE                                     128

namespace codal{
    class MicRecorder : public CodalComponent, public DataSink
    {
    public:
        bool            activated;                      // Has this component been connected yet.
        bool            recording;                      // Is the component currently recording audio
        int             position;                       // Pointer to a position in the savedRecordings array
        DataSource      &upstream;                      // The component producing data to process
        ManagedBuffer   savedRecording[BUFFER_SIZE];   // Array of mic buffers that make up a recording

        Mixer2          &mixer;

        /**
          * Creates a component capable of recording from the mic, storing in a buffer, and playing it back.
          *
          * @param source a DataSource to measure the level of.
          * @param connectImmediately Should this component connect to upstream splitter when started
          */
        MicRecorder(DataSource &source, Mixer2 &mixer, bool connectImmediately  = true);

        /**
         * Callback provided when data is ready.
         */
    	virtual int pullRequest();

        /*
         * Start recording from the mic
         */
        void startRecording();

        /*
         * Stop recording from the mic
         */
        void stopRecording();

        /*
         * Playback Saved Recording
         */
        void playback();

        /*
         * Clear Saved Recording
         */
        void clearStored();

        /**
         * Destructor.
         */
        ~MicRecorder();

    };
}

#endif
