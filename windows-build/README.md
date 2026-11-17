# windows-build

Everything for the Windows 64-bit build (see `../docs/BUILDING.md`):

- `Dockerfile` - the build container (Fedora 42, Qt 6, MinGW64, driver DLLs).
- `collect-dlls.sh` - copies the DLLs the program needs next to the `.exe`.
- `make-portable.sh` - makes the portable folder and the `.zip` with `README.txt`.
