make distclean || true
make distclean -C src/jtag/drivers/libjaylink || true
./bootstrap
rm -rf binary-linux64
rm -rf build-linux64
mkdir build-linux64
cd build-linux64
export CFLAGS="$CFLAGS -g3"
../configure --prefix=${PWD}/../binary-linux64 --enable-ftdi --enable-ftdi-oscan1 \
            --enable-jtag_vpi --enable-ftdi-cjtag
make -j4
make install

mkdir -p ${PWD}/../binary-win64/share/openocd/loader/emmc_90
cp -f ../contrib/loaders/flash/emmc/dwcmshc/build_90/*.elf \
        ${PWD}/../binary-win64/share/openocd/loader/emmc_90/
mkdir -p ${PWD}/../binary-win64/share/openocd/loader/qspi_90
cp -f ../contrib/loaders/flash/qspi/dwcssi/build_90/*.elf \
        ${PWD}/../binary-win64/share/openocd/loader/qspi_90/
mkdir -p ${PWD}/../binary-win64/share/openocd/loader/emmc_35
cp -f ../contrib/loaders/flash/emmc/dwcmshc/build_35/*.elf \
        ${PWD}/../binary-win64/share/openocd/loader/emmc_35/
mkdir -p ${PWD}/../binary-win64/share/openocd/loader/qspi_35
cp -f ../contrib/loaders/flash/qspi/dwcssi/build_35/*.elf \
        ${PWD}/../binary-win64/share/openocd/loader/qspi_35/
