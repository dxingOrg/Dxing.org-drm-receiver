#!/bin/bash
# Collect every DLL a cross-compiled (mingw64) .exe needs into the folder
# where the .exe lives, so that folder can be copied to any Windows PC.
#
# usage:  ./collect-dlls.sh windows-bin/drm-receiver.exe
#
# Finds dependencies recursively (the exe, the DLLs it needs, the DLLs
# those need ...) plus the Qt "windows" platform plugin, without which
# a Qt program will not start on Windows.
EXE="$1"
[ -f "$EXE" ] || { echo "usage: $0 path/to/program.exe"; exit 1; }
OUT="$(cd "$(dirname "$EXE")" && pwd)"
SYSROOT="${SYSROOT:-/usr/x86_64-w64-mingw32/sys-root/mingw}"
OBJDUMP="${OBJDUMP:-x86_64-w64-mingw32-objdump}"
SEARCH_DIRS="$SYSROOT/bin $SYSROOT/lib"

declare -A seen
system_dlls=()
copied=()

find_dll () {		# case-insensitive lookup in the sysroot
	local d f
	for d in $SEARCH_DIRS; do
	   f=$(find "$d" -maxdepth 1 -iname "$1" 2>/dev/null | head -1)
	   [ -n "$f" ] && { echo "$f"; return; }
	done
}

resolve () {		# resolve <file> : copy its DLL dependencies, recurse
	local file="$1" dll key path
	for dll in $($OBJDUMP -p "$file" 2>/dev/null | awk '/DLL Name:/ {print $3}' | tr -d '\r'); do
	   key=$(echo "$dll" | tr 'A-Z' 'a-z')
	   [ -n "${seen[$key]}" ] && continue
	   seen[$key]=1
	   path=$(find_dll "$dll")
	   if [ -n "$path" ]; then
	      [ -e "$OUT/$(basename "$path")" ] || cp "$path" "$OUT/"
	      copied+=("$(basename "$path")")
	      resolve "$path"
	   else
	      system_dlls+=("$dll")
	   fi
	done
}

resolve "$EXE"

# DLLs the program loads by itself at run time (LoadLibrary) are not in the
# .exe's import table, so the scan above cannot see them. Add them here.
#   libfdk-aac-*  : AAC / xHE-AAC audio decoder (aac-handler.cpp asks for
#                   "libfdk-aac-2.dll"); only with INCLUDE_FDK=1 (see below)
#   libusb-1.0    : needed by the vendor DLLs (airspy/airspyhf/hackrf/rtlsdr)
#                   that the user adds later
#
#   libfdk-aac is only copied when INCLUDE_FDK=1 is set (a package for your own
#   use). A package for the public does not contain it: its licence (Fraunhofer
#   FDK AAC) is separate, and the README of the package tells the user where to
#   get it (see make-portable.sh).
patterns='libusb-1.0.dll'
[ "$INCLUDE_FDK" = "1" ] && patterns="libfdk-aac-*.dll $patterns"
for pattern in $patterns; do
	path=$(find_dll "$pattern")
	if [ -n "$path" ]; then
	   [ -e "$OUT/$(basename "$path")" ] || cp "$path" "$OUT/"
	   copied+=("$(basename "$path")")
	   seen[$(basename "$path" | tr 'A-Z' 'a-z')]=1
	   echo "run-time DLL copied: $(basename "$path")"
	   resolve "$path"
	else
	   echo "WARNING: run-time DLL '$pattern' NOT FOUND in $SYSROOT"
	fi
done

# Receiver driver DLLs built in the Docker image (see the Dockerfile). They are
# already named the way the program asks for them: airspy.dll, airspyhf.dll,
# libhackrf.dll, rtlsdr.dll. Their own dependencies (libusb ...) are added too.
VENDOR_DIR="${VENDOR_DIR:-/opt/vendor-dlls}"
if ls "$VENDOR_DIR"/*.dll >/dev/null 2>&1; then
	for f in "$VENDOR_DIR"/*.dll; do
	   cp -f "$f" "$OUT/"
	   seen[$(basename "$f" | tr 'A-Z' 'a-z')]=1
	   copied+=("$(basename "$f")")
	   echo "receiver DLL copied: $(basename "$f")"
	   resolve "$f"
	done
else
	echo "NOTE: no receiver DLLs in $VENDOR_DIR (rebuild the Docker image to get them)"
fi

# Qt platform plugin (+ style plugin if present), the JPEG and GIF image
# plugins (Qt cannot show a JPEG slide without imageformats/qjpeg.dll), and
# their own dependencies
for plugin in platforms/qwindows.dll styles/qmodernwindowsstyle.dll \
		 imageformats/qjpeg.dll imageformats/qgif.dll; do
	src=$(find "$SYSROOT" -path "*plugins/$plugin" 2>/dev/null | head -1)
	if [ -n "$src" ]; then
	   mkdir -p "$OUT/$(dirname "$plugin")"
	   [ -e "$OUT/$plugin" ] || cp "$src" "$OUT/$plugin"
	   echo "plugin copied: $plugin"
	   resolve "$src"
	else
	   echo "NOTE: plugin $plugin not found in $SYSROOT"
	fi
done

echo
echo "Copied ${#copied[@]} DLLs next to $(basename "$EXE"):"
printf '   %s\n' "${copied[@]}" | sort
echo
echo "Not found in the mingw sysroot (should all be Windows system DLLs):"
printf '   %s\n' "${system_dlls[@]}" | sort
