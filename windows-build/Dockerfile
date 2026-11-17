FROM fedora:42

# ---- native Linux build toolchain (unchanged from before) ----
RUN dnf install -y \
        gcc-c++ make \
        qt6-qtbase-devel qt6-qttools-devel \
        qwt-qt6-devel \
        eigen3-devel \
        fftw-devel libsndfile-devel libsamplerate-devel \
        portaudio-devel libusb1-devel alsa-lib-devel \
        airspyone_host airspyone_host-devel \
        unzip which \
    && dnf install -y https://download1.rpmfusion.org/nonfree/fedora/rpmfusion-nonfree-release-$(rpm -E %fedora).noarch.rpm \
    && dnf install -y fdk-aac-devel \
    && dnf clean all

RUN dnf install -y usbutils cmake git && dnf clean all

RUN git clone https://github.com/airspy/airspyhf.git /tmp/airspyhf \
    && mkdir -p /tmp/airspyhf/build \
    && cd /tmp/airspyhf/build \
    && cmake .. -DCMAKE_INSTALL_PREFIX=/usr -DCMAKE_INSTALL_LIBDIR=lib64 \
    && make -j$(nproc) \
    && make install \
    && ldconfig \
    && rm -rf /tmp/airspyhf

RUN dnf install -y pulseaudio-utils alsa-plugins-pulseaudio && dnf clean all
RUN printf 'pcm.!default {\n    type pulse\n}\nctl.!default {\n    type pulse\n}\n' > /etc/asound.conf

# ---- NEW: MinGW64 cross-compile toolchain, for a real Windows .exe ----
# AirSpy/HackRF/RTL-SDR libraries are deliberately NOT cross-built here:
# those handlers dlopen() the vendor DLL at runtime only (same as the
# Linux build), so nothing vendor-specific is needed to compile the exe.
RUN dnf install -y \
        mingw64-gcc-c++ \
        mingw64-qt6-qtbase mingw64-qt6-qttools \
        mingw64-qwt-qt6 \
        mingw64-libusb1 mingw64-portaudio \
        mingw64-fftw \
        mingw64-filesystem mingw64-binutils \
        autoconf automake libtool \
    && dnf clean all

# libsndfile and libsamplerate have no Fedora mingw64 package either -
# cross-build them from source (external codecs switched off: the
# receiver only needs plain WAV-type file support).
RUN git clone --depth 1 https://github.com/libsndfile/libsamplerate.git /tmp/libsamplerate \
    && cd /tmp/libsamplerate \
    && mingw64-cmake -B build -S . \
         -DBUILD_SHARED_LIBS=ON -DBUILD_TESTING=OFF -DLIBSAMPLERATE_EXAMPLES=OFF \
    && cmake --build build -j$(nproc) \
    && cmake --install build \
    && rm -rf /tmp/libsamplerate

RUN git clone --depth 1 https://github.com/libsndfile/libsndfile.git /tmp/libsndfile \
    && cd /tmp/libsndfile \
    && mingw64-cmake -B build -S . \
         -DBUILD_SHARED_LIBS=ON -DBUILD_TESTING=OFF -DBUILD_PROGRAMS=OFF \
         -DBUILD_EXAMPLES=OFF -DENABLE_EXTERNAL_LIBS=OFF -DENABLE_MPEG=OFF \
    && cmake --build build -j$(nproc) \
    && cmake --install build \
    && rm -rf /tmp/libsndfile

# fdk-aac has no Fedora mingw64 package - cross-build it from source
RUN git clone https://github.com/mstorsjo/fdk-aac.git /tmp/fdk-aac \
    && cd /tmp/fdk-aac \
    && autoreconf -fiv \
    && mingw64-configure --enable-static=no \
    && mingw64-make -j$(nproc) \
    && mingw64-make install \
    && rm -rf /tmp/fdk-aac

# ---- Receiver driver DLLs for the Windows build ----
# The program loads these at run time (LoadLibrary) under EXACTLY these names,
# so nothing here is needed to compile it:
#     airspy.dll    AirSpy R2        airspyhf.dll  AirSpy HF+ Discovery
#     libhackrf.dll HackRF           rtlsdr.dll    RTL-SDR
# They are built here, 64-bit, from the manufacturers' sources and kept in
# /opt/vendor-dlls until collect-dlls.sh puts them next to the .exe.
# A driver that fails to build only prints a WARNING; the image build goes on.
RUN mkdir -p /opt/vendor-dlls

RUN git clone --depth 1 https://github.com/airspy/airspyone_host.git /tmp/s \
    && cd /tmp/s \
    && mingw64-cmake -B build -S . -DCMAKE_POLICY_VERSION_MINIMUM=3.5 \
    && ( cmake --build build --target airspy -j$(nproc) \
         || cmake --build build -k -j$(nproc) || true ) \
    && cp "$(find build -iname '*airspy*.dll' | head -1)" /opt/vendor-dlls/airspy.dll \
    || echo "WARNING: airspy.dll (AirSpy R2) could not be built"; \
    rm -rf /tmp/s

RUN git clone --depth 1 https://github.com/airspy/airspyhf.git /tmp/s \
    && cd /tmp/s \
    && mingw64-cmake -B build -S . -DCMAKE_POLICY_VERSION_MINIMUM=3.5 \
    && ( cmake --build build --target airspyhf -j$(nproc) \
         || cmake --build build -k -j$(nproc) || true ) \
    && cp "$(find build -iname '*airspyhf*.dll' | head -1)" /opt/vendor-dlls/airspyhf.dll \
    || echo "WARNING: airspyhf.dll (AirSpy HF+) could not be built"; \
    rm -rf /tmp/s

# HackRF: first try the whole host tree, then libhackrf alone
RUN git clone --depth 1 https://github.com/greatscottgadgets/hackrf.git /tmp/s \
    && ( ( cd /tmp/s/host \
           && mingw64-cmake -B build -S . -DCMAKE_POLICY_VERSION_MINIMUM=3.5 \
           && ( cmake --build build --target hackrf -j$(nproc) \
                || cmake --build build -k -j$(nproc) || true ) \
           && cp "$(find build -iname '*hackrf*.dll' | head -1)" /opt/vendor-dlls/libhackrf.dll ) \
         || ( cd /tmp/s/host/libhackrf \
           && mingw64-cmake -B build2 -S . -DCMAKE_POLICY_VERSION_MINIMUM=3.5 \
           && ( cmake --build build2 -j$(nproc) || true ) \
           && cp "$(find build2 -iname '*hackrf*.dll' | head -1)" /opt/vendor-dlls/libhackrf.dll ) ) \
    || echo "WARNING: libhackrf.dll (HackRF) could not be built"; \
    rm -rf /tmp/s

RUN git clone --depth 1 https://github.com/osmocom/rtl-sdr.git /tmp/s \
    && cd /tmp/s \
    && mingw64-cmake -B build -S . -DCMAKE_POLICY_VERSION_MINIMUM=3.5 \
    && ( cmake --build build --target rtlsdr -j$(nproc) \
         || cmake --build build -k -j$(nproc) || true ) \
    && cp "$(find build -iname '*rtlsdr*.dll' | head -1)" /opt/vendor-dlls/rtlsdr.dll \
    || echo "WARNING: rtlsdr.dll (RTL-SDR) could not be built"; \
    rm -rf /tmp/s

WORKDIR /work
