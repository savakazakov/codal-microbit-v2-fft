#include "MicroBitSerial.h"
#include "MicroBit.h"
#include "CodalDmesg.h"

MicroBit uBit;

void
recordingTest()
{
	uBit.display.print("R");

    while(1)
    {
        if (uBit.buttonA.isPressed())
            uBit.audio.recorder->startRecording();

        if (uBit.buttonB.isPressed())
            uBit.audio.recorder->playback();
        
        uBit.sleep(100);
    }
}


int 
main()
{
    uBit.init();
    recordingTest();
}
