#!/bin/bash
# Build the portable Windows package: a clean folder + a .zip.
#
# Run from the PROJECT folder (the one with drm-receiver.pro) inside the
# container, after the exe was built and collect-dlls.sh was run:
#
#     ./make-portable.sh                # uses ./windows-bin
#     ./make-portable.sh windows-bin 1.0
#
#
# The AAC decoder library libfdk-aac-2.dll is NOT put into the package (it has
# its own licence). The README tells the user how to add it. For a package for
# your own use:   INCLUDE_FDK=1 ./make-portable.sh windows-bin 3.3
# (run collect-dlls.sh with INCLUDE_FDK=1 as well, so that the file is copied)
# For the README: FDK_VERSION=2.0.3 FDK_SHA256=<sum> ./make-portable.sh ...
# the sum of the libfdk-aac-2.dll that you tested the program with.
INCLUDE_FDK="${INCLUDE_FDK:-0}"
FDK_VERSION="${FDK_VERSION:-2.x}"
FDK_SHA256="${FDK_SHA256:-(see the release page of this version)}"
SRC="${1:-windows-bin}"
VER="${2:-$(date +%Y%m%d)}"
NAME="Dxing-DRM-receiver-win64-$VER"
DEST="$(pwd)/dist/$NAME"

[ -f "$SRC/drm-receiver.exe" ] || { echo "ERROR: $SRC/drm-receiver.exe not found"; exit 1; }

rm -rf "$DEST" "$DEST.zip"
mkdir -p "$DEST"
cp -r "$SRC"/. "$DEST"/
mv "$DEST/drm-receiver.exe" "$DEST/Dxing-DRM-receiver.exe"

# --- sanity check: the files the program cannot work without ---
missing=0
vmissing=0
if [ "$INCLUDE_FDK" != "1" ]; then
	rm -f "$DEST"/libfdk-aac-*.dll	# never in a package for the public
	echo "NOTE: libfdk-aac-2.dll is NOT in the package (public package); the README explains how to add it."
fi
for f in Dxing-DRM-receiver.exe platforms/qwindows.dll imageformats/qjpeg.dll \
         Qt6Core.dll Qt6Widgets.dll qwt-qt6.dll libusb-1.0.dll; do
	if [ ! -e "$DEST/$f" ]; then echo "WARNING: missing in package: $f"; missing=1; fi
done
if [ "$INCLUDE_FDK" = "1" ] && [ ! -e "$DEST/libfdk-aac-2.dll" ]; then
	echo "WARNING: missing in package: libfdk-aac-2.dll (INCLUDE_FDK=1)"; missing=1
fi
# the receiver drivers: a missing one only means that receiver cannot be used
for f in airspyhf.dll airspy.dll libhackrf.dll rtlsdr.dll; do
	if [ ! -e "$DEST/$f" ]; then echo "WARNING: receiver DLL missing (that receiver will not work): $f"; vmissing=1; fi
done

cp COPYING "$DEST/LICENSE-GPL-2.0.txt" 2>/dev/null
cp AUTHORS "$DEST/AUTHORS.txt" 2>/dev/null

# --- README (UTF-8 with BOM + Windows line ends, so Notepad shows it right) ---
{
printf '\xEF\xBB\xBF'
cat << 'README_EOF'
Dxing.org DRM receiver  -  Windows 64-bit, portable
===================================================

START  /  СТАРТИРАНЕ
  1. Unzip the WHOLE folder anywhere (Desktop, USB stick ...). Keep all files
     and the "platforms", "styles" and "imageformats" sub-folders together
     with the .exe.
  2. Double-click  Dxing-DRM-receiver.exe
     No installation and no administrator rights are needed.
     Windows 10 or 11, 64-bit. No Visual C++ runtime needed.

  Windows may say "Windows protected your PC" because the program is not
  digitally signed. Click "More info", then "Run anyway".

  BG: Разархивирайте ЦЯЛАТА папка някъде и пуснете Dxing-DRM-receiver.exe.
      Не се инсталира нищо. Не местете .exe файла без останалите файлове.

RECEIVERS  /  ПРИЕМНИЦИ
  The drivers (DLL files) of these receivers are included, 64-bit:
     AirSpy HF+ Discovery     airspyhf.dll
     AirSpy R2                airspy.dll
     HackRF                   libhackrf.dll
     RTL-SDR                  rtlsdr.dll        (they all use libusb-1.0.dll)

  Each receiver also needs its USB driver (WinUSB). If another program such as
  SDR# already works with the receiver on this PC, the driver is in place. If
  not, install WinUSB for it with Zadig (https://zadig.akeo.ie).
  Only ONE program can use a receiver at a time - close SDR# (or any other)
  first.

  SDRplay: not included. Install the SDRplay API from sdrplay.com; it provides
  the driver and the DLL the program looks for.
  SpyServer and the sound-card input need no extra files.

  If a receiver cannot be opened, the program just shows the device list again.

USE  /  РАБОТА
  select freq      Type the frequency with the on-screen keypad or the keyboard:
                   digits, then Enter (or K) = kHz, M = MHz, Backspace = correct,
                   Esc = clear.
  Journaline       The "Journaline" button at the bottom of the "drm decoder" window
                   becomes active when the station sends Journaline (text pages and
                   menus). It opens its own window; the audio keeps playing. Pages
                   arrive slowly, in a loop: the main menu can take a minute or more.
  Slideshow        The "Slideshow" button shows pictures sent with the broadcast (MOT
                   slide show; also pictures that arrive as files of the Chinese data
                   broadcast). "Files and data" lists other files; nothing is saved
                   unless you click Save or turn on automatic saving. Files go to
                   C:\Users\<your name>\Dxing-DRM\Download and are never opened or run.
  Data broadcast   Services that use the Chinese data broadcast format (GY/T 423-2025,
                   clause 6.7), e.g. emergency broadcast messages, appear in the
                   third tab ("Data broadcast") of the "Files and data" window, as
                   plain text. A big file needs several repetitions of the station's
                   loop; the tab shows how many pieces have arrived.
  Service info     The "Service info" button shows what the station says about itself:
                   services, language, programme type, date and time, alternative
                   frequencies (double click a DRM frequency to tune to it) and
                   announcements. The button turns red during an alarm.
  Raw data         "Data info" has the box "Record raw data for analysis": it writes the
                   received data to Dxing-DRM\data-groups.txt in your user folder (for
                   bug reports; a file without data lines means the station sends no
                   data service).
  frequency save   Saves the station name decoded from the signal, together
                   with its frequency. You can edit the name before saving.
  Quit             Use the X of the main window "Dxing.org DRM receiver" and
                   answer Yes. The station list is saved on a clean exit.
                   (The X of the "drm decoder" window does nothing, on purpose.)

AAC DECODER (libfdk-aac)  /  AAC ДЕКОДЕР
  Stations with AAC or xHE-AAC sound (almost all DRM stations) need the library
  libfdk-aac-2.dll. It is NOT part of this program or this package: it has its
  own licence (Fraunhofer FDK AAC), separate from the licence of this program.
  Without it such stations are silent (data, Journaline, slide show and service
  information still work), and the program says so at the start with the
  message "AAC decoder library not found".

  How to get it (64-bit Windows):
   1. Install MSYS2 from https://www.msys2.org (free).
   2. Start "MSYS2 MINGW64" from the Start menu and type:
          pacman -S mingw-w64-x86_64-fdk-aac
   3. The file is in the MSYS2 folder, normally
          C:\msys64\mingw64\bin\libfdk-aac-2.dll
      Take the one from the "mingw64" folder (64-bit), not from "mingw32".
   4. Copy it into THIS program folder, next to Dxing-DRM-receiver.exe, and
      restart the program.
  The source code of the library: https://github.com/mstorsjo/fdk-aac

  Checking the file (optional), in PowerShell:
      Get-FileHash C:\msys64\mingw64\bin\libfdk-aac-2.dll -Algorithm SHA256
  The sum of the file this version of the program was tested with
  (libfdk-aac @FDK_VERSION@):
      @FDK_SHA256@
  A different sum is normal when MSYS2 has meanwhile published a newer build
  of the library (check the version with: pacman -Q mingw-w64-x86_64-fdk-aac).

  BG: Копирайте libfdk-aac-2.dll (64-битовия, от C:\msys64\mingw64\bin след
      "pacman -S mingw-w64-x86_64-fdk-aac" в MSYS2 MINGW64) в папката на
      програмата, до Dxing-DRM-receiver.exe, и рестартирайте програмата.
      Без този файл станциите с AAC / xHE-AAC са безмълвни.

YOUR DATA  /  ВАШИТЕ ДАННИ
  Settings and the station list are NOT stored in this folder but in your user
  folder:   C:\Users\<your name>\.drm-radio.ini
            C:\Users\<your name>\.drm-radio-stations.txt
            C:\Users\<your name>\Dxing-DRM\   (logs and recorded data)
  Deleting this program folder does not delete them.

IF SOMETHING GOES WRONG  /  АКО НЕЩО НЕ СТАНЕ
  "... .dll was not found" when starting
        A file was not copied. Copy the complete folder again.
  Receiver list reappears: "failed to open airspyhf.dll"
        airspyhf.dll or libusb-1.0.dll is missing next to the .exe.
  Receiver list reappears: "Problem opening airspyhf device"
        The driver is not installed for this PC or another program (SDR#)
        is using the receiver.
  Station name is shown but there is no sound
        Pick the sound output in the drop-down at the top of the main window.
        Also check that libfdk-aac-2.dll is in this folder: see "AAC DECODER"
        above. The program shows the message "AAC decoder library not found"
        when it is missing.

LICENSE
  Free software under the GNU General Public License, version 2 (see
  LICENSE-GPL-2.0.txt). Based on "drm-receiver" by Jan van Katwijk:
  https://github.com/JvanKatwijk/drm-receiver
  Modified by Dxing.org (AirSpy R2 / AirSpy HF+ Discovery support, Windows
  build, user-interface changes). The source code of this build must be made
  available together with it.

  The folder also contains third-party libraries, each under its own licence:
  Qt 6, Qwt, FFTW, PortAudio, libsndfile, libsamplerate, libusb and the receiver
  drivers (libairspy, libairspyhf, libhackrf, librtlsdr). The Fraunhofer FDK AAC
  codec (libfdk-aac) is NOT included: see "AAC DECODER" above.
README_EOF
} | sed "s|@FDK_VERSION@|$FDK_VERSION|; s|@FDK_SHA256@|$FDK_SHA256|" \
  | sed 's/$/\r/' > "$DEST/README.txt"

# --- the archive ---
( cd "$(dirname "$DEST")" &&
  if command -v zip >/dev/null 2>&1 && [ -z "$FORCE_PYZIP" ]; then
	zip -r -q "$NAME.zip" "$NAME"
  else
	python3 -m zipfile -c "$NAME.zip" "$NAME"
  fi )

echo
echo "Package folder : dist/$NAME"
echo "Archive        : dist/$NAME.zip   ($(du -h "$DEST.zip" | cut -f1))"
echo "Files in folder: $(find "$DEST" -type f | wc -l)"
[ $missing -eq 0 ] && echo "All required files present." || echo "SOME REQUIRED FILES ARE MISSING - see warnings above."
[ $vmissing -eq 0 ] && echo "All four receiver DLLs present." || echo "Some receiver DLLs are missing - see warnings above."
