# codal-microbit-v2
CODAL target for the micro:bit v2.x series of devices

See https://github.com/lancaster-university/microbit-v2-samples for details on building and using CODAL.

## Tags and Releases
We tag fairly often, and these may include changes which we are currently testing across the various ecosystems that use CODAL, including Microsoft MakeCode and The Foundation Python editors.
Consequently, these, while generally more stable than the `feature/` or `tests/` branches are also not _guaranteed_ to be stable and complete.
# codal-microbit-v2
CODAL target for the micro:bit v2.x series of devices


This Branch contains all work related to:

Recording API
Frequency Detection

This code will only run if using the a version of codal with the splitter included.
https://github.com/JoshuaAHill/codal-core/tree/recordingAPI
https://github.com/JoshuaAHill/codal-nrf52/tree/adc-live-demux

Releases are selected tags intended to be stable and production-ready; and are the recommended ones to use for anyone implementing or using the CODAL codebase.
Each GitHub Release changelog will include the changes since the previous Release (not for just that specific tag only), and the changelog will also include changes in the project dependencies (codal-core, codal-nrf52, codal-nrf-sdk) as these don't have individual changelogs.