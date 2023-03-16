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

#ifndef SEQUENCE_H
#define SEQUENCE_H

#include <string>

#define SEQUENCE_SIZE                                   10

namespace codal{
    class Sequence : public CodalComponent
    {
    public:
        char     name;                           // name for sequence
        int             sequence[SEQUENCE_SIZE];        // Sequence data
        bool            live;                           // Is this sequence live          

        /**
          * Creates a component capable of recording from the mic, storing in a buffer, and playing it back.
          *
          * @param source a DataSource to measure the level of.
          * @param connectImmediately Should this component connect to upstream splitter when started
          * @param level A level detector to read volume level from the mic
          */
        Sequence(char name);

        // Default constructor
        Sequence();

        /**
         * Destructor.
         */
        ~Sequence();

    };
}

#endif