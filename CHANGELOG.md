# Changelog

All notable changes of this fork compared with the original
[drm-receiver](https://github.com/JvanKatwijk/drm-receiver). Version numbers are those
of the Windows packages. (Versions up to 2.7 are summarised in one entry.)

## [3.3] - unreleased
### Changed
- If `libfdk-aac-2.dll` cannot be loaded, the program now says so (a message with the
  missing file and a pointer to the README) instead of staying silent.
- The public package no longer contains `libfdk-aac-2.dll`; the README of the package
  explains where to get it and how to add it (MSYS2 route, optional SHA256 check).
  `collect-dlls.sh` / `make-portable.sh` copy it only with `INCLUDE_FDK=1`.

## [3.2]
### Added
- Pictures (JPEG, PNG, GIF) that arrive as files of the **Chinese data broadcast** are
  shown in the **Slideshow** with the MOT slides (format recognised from the content;
  the type code of the file description is shown but not interpreted; length and MD5
  of the file name are checked and reported).

## [3.1]
### Fixed
- **Service info** showed a phantom service after noise passed the 8-bit CRC of the FAC.
  A FAC record is now accepted after 3 identical repetitions and forgotten when not
  seen for about 30 seconds; an alarm needs a confirmed record.

## [3.0]
### Added
- **Service info** window: services, labels, language and programme type, date/time
  (UTC and local), alternative frequencies (double click tunes), announcements and
  alarm (the button turns red).
- Labels and the chosen service are kept **per service**, not per stream: two services
  can share one audio stream. The chosen service is marked and its name goes to the
  station list; the choice survives a re-synchronisation.
- **Carousel assembler** for Chinese data broadcast packets: large packets (up to
  92 DRM packets) are built from several repetitions of the station's loop, each
  accepted only if its own CRC16 is right. Progress of a file is shown ("3 of 6
  packets").
- Service diagnostics in **Data info** (FAC services, SDC entities, streams, labels).
### Changed
- One **Record raw data for analysis** box (in Data info); the button "MOT files" is
  now **Files and data**.
- The time shown in the decoder window is UTC with two digits (the local time offset
  was never applied; it is now shown in the Service info window).
### Fixed
- A label of the first service of a two-service station was lost.

## [2.x] - up to 2.7
### Added
- Drivers for the **AirSpy R2** and **AirSpy HF+ Discovery**.
- Native **Windows 64-bit** build (MinGW) and portable package (`collect-dlls.sh`,
  `make-portable.sh`, `Dockerfile`).
- Own name, logo and icon; keypad "Select frequency"; 5 kHz step; slider 0-100 (Auto).
- **Data hub** and **Data info** window with a raw recording for analysis.
- **Journaline** (TS 102 979), tested on BBC WS.
- **MOT** (EN 301 234) headers, directory, **slide show** (TS 101 499), files; tested on
  TDF; repetitions of the carousel are combined.
- **Chinese data broadcast** packets (GY/T 423-2025, 6.7) as plain text.
- xHE-AAC speed diagnostics and error concealment.
### Fixed
- Crash when a receiver could not be opened; crash on closing with the file reader;
  old station name after a frequency change; wrong characters in texts; station list
  as UTF-8 text; spectrum scale; uninitialised fields and a division by zero in the
  file reader; channel index of xHE-AAC stereo (not tested with a stereo stream).
