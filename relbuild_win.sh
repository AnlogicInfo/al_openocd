export OCDLIB_DIR=/home/ocdlib
export VER_LIBUSB="1.0.25"
export VER_FTD2XX="v2.12.36.4"
export VER_LIBFTDI1="1.5_devkit_x86_x64_19July2020"
export LIBUSB1_CFLAGS="-I$OCDLIB_DIR/libusb-$VER_LIBUSB/include/libusb-1.0"
export LIBUSB1_LIBS=" -L$OCDLIB_DIR/libusb-$VER_LIBUSB/MinGW64/dll \
    -L$OCDLIB_DIR/libusb-$VER_LIBUSB/MinGW64/static \
    -L$OCDLIB_DIR/libusb-$VER_LIBUSB/MinGW32/dll \
    -L$OCDLIB_DIR/libusb-$VER_LIBUSB/MinGW32/static -lusb-1.0"
export libusb_CFLAGS="$LIBUSB1_CFLAGS"
export libusb_LIBS="$LIBUSB1_LIBS"
make distclean || true
rm -rf binary-win64
rm -rf build-win64
./bootstrap
make distclean -C src/jtag/drivers/libjaylink || true
export CFLAGS="$CFLAGS -I$OCDLIB_DIR/libyaml_0_2_2/include \
    -I$OCDLIB_DIR/ftd2xx_$VER_FTD2XX/ \
    -I$OCDLIB_DIR/libftdi1-$VER_LIBFTDI1/include"
export LDFLAGS="$LDFLAGS -L$OCDLIB_DIR/libyaml_0_2_2/bin  \
    -L$OCDLIB_DIR/libyaml_0_2_2/lib -L$OCDLIB_DIR/ftd2xx_$VER_FTD2XX/amd64 \
    -L$OCDLIB_DIR/libftdi1-$VER_LIBFTDI1/lib64"

mkdir build-win64
cd build-win64
../configure --prefix=${PWD}/../binary-win64 --host=x86_64-w64-mingw32 \
    --enable-ftdi --enable-ftdi-oscan1 --enable-cmsis-dap=no \
    --enable-kitprog=no --enable-nulink=no --enable-jtag_vpi \
    --enable-ftdi-cjtag
echo '#define HAVE_LIBUSB_GET_PORT_NUMBERS 1' >> config.h
make -j4
make install

