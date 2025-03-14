make distclean || true
make distclean -C src/jtag/drivers/libjaylink || true
./bootstrap
rm -rf binary-linux64
rm -rf build-linux64
mkdir build-linux64
cd build-linux64
export CFLAGS="$CFLAGS -g3"
../configure --prefix=${PWD}/../binary-linux64 --enable-ftdi --enable-ftdi-oscan1 --enable-jtag_vpi --enable-ftdi-cjtag
make -j4
make install
