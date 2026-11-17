# Building

The Windows version is **cross-compiled** (MinGW, 64-bit) in a Docker container, so no
Windows compiler is needed. You need **Docker Desktop**; on Windows 10 it runs with WSL
(the commands below are typed in a WSL terminal, or in PowerShell where it says so).

## 1. Get the sources

```
git clone https://github.com/dxingOrg/Dxing.org-drm-receiver.git
cd <this-repository>
```

## 2. Build the container image (once, about 30-60 minutes)

```
docker build -t drm-env -f windows-build/Dockerfile windows-build
```

The image has Qt 6, Qwt, the MinGW64 tool chain, libsndfile, libsamplerate, **fdk-aac
(built from its source, inside your own container: it is needed to compile)**, and the
driver DLLs of the receivers (`airspy.dll`, `airspyhf.dll`, `libhackrf.dll`,
`rtlsdr.dll`), built from the manufacturers' sources. A driver that cannot be built only
prints a warning.

## 3. Compile

```
docker run -it --name drm-win -v "$PWD":/work drm-env bash
```

Inside the container (the repository is `/work`):

```
cd /work
find release -name '*.o' -size 0 -delete 2>/dev/null     # after an interrupted build
x86_64-w64-mingw32-qmake-qt6 drm-receiver.pro
make -j2 2>&1 | tee build.log | grep --line-buffered -E 'error|-o [^ ]+\.exe'
```

Use `-j2`, not more: the machine may run out of memory. The program is
`windows-bin/drm-receiver.exe`.

Later: `docker start drm-win` and `docker exec -it drm-win bash`. A container made from
an old image does not see changes of the image: make a new one after changing the
`Dockerfile`.

## 4. Make the portable package

Still in the container, in `/work`:

```
sed -i 's/\r$//' windows-build/*.sh ; chmod +x windows-build/*.sh   # only if the scripts have Windows line ends
windows-build/collect-dlls.sh windows-bin/drm-receiver.exe
windows-build/make-portable.sh windows-bin 3.3
```

This creates `dist/Dxing-DRM-receiver-win64-3.3.zip`: the program, the Qt libraries
and plugins (`platforms`, `styles`, `imageformats`), the driver DLLs, `README.txt`
and the licence files.

### The AAC library (libfdk-aac)

`libfdk-aac-2.dll` is **not** put into the package by default (it has its own licence;
the README of the package tells the user how to add it). For a package for **your own
use**:

```
INCLUDE_FDK=1 windows-build/collect-dlls.sh windows-bin/drm-receiver.exe
INCLUDE_FDK=1 windows-build/make-portable.sh windows-bin 3.3
```

For the README of a public package you can state the version and the SHA256 sum of the
`libfdk-aac-2.dll` you tested the program with:

```
FDK_VERSION=2.0.3 FDK_SHA256=<sum> windows-build/make-portable.sh windows-bin 3.3
```

## 5. Check

Unzip the package on a clean folder or PC and start `Dxing-DRM-receiver.exe`. The
program must start without a missing-DLL message from Windows. Without
`libfdk-aac-2.dll` it shows its own message "AAC decoder library not found".

## Linux

The program is developed for Windows. A native Linux build follows the instructions of
the original ([README-upstream.md](../README-upstream.md)): Qt 6, qwt-qt6, FFTW,
libsndfile, libsamplerate, PortAudio, libusb and fdk-aac development packages (the
package list of the first part of `windows-build/Dockerfile` is a working Fedora 42
example), then `qmake6 drm-receiver.pro && make`. The Linux path is not maintained
here; reports and patches are welcome.
