make distclean || true
make distclean -C src/jtag/drivers/libjaylink || true
./bootstrap
rm -rf binary-linux64
rm -rf build-linux64
mkdir build-linux64
cd build-linux64
../configure --prefix=${PWD}/../binary-linux64 --enable-ftdi \
    --enable-ftdi-oscan1 --enable-jtag_vpi --enable-ftdi-cjtag
make -j4
make install
mkdir -p ${PWD}/../binary-win64/share/openocd/loader/emmc_dr1x90
cp -f ../contrib/loaders/flash/emmc/dwcmshc/build_dr1x90/*.elf \
        ${PWD}/../binary-win64/share/openocd/loader/emmc_dr1x90/
mkdir -p ${PWD}/../binary-win64/share/openocd/loader/qspi_dr1x90
cp -f ../contrib/loaders/flash/qspi/dwcssi/build_dr1x90/*.elf \
        ${PWD}/../binary-win64/share/openocd/loader/qspi_dr1x90/
mkdir -p ${PWD}/../binary-win64/share/openocd/loader/emmc_dr1m35
cp -f ../contrib/loaders/flash/emmc/dwcmshc/build_dr1m35/*.elf \
        ${PWD}/../binary-win64/share/openocd/loader/emmc_dr1m35/
mkdir -p ${PWD}/../binary-win64/share/openocd/loader/qspi_dr1m35
cp -f ../contrib/loaders/flash/qspi/dwcssi/build_dr1m35/*.elf \
        ${PWD}/../binary-win64/share/openocd/loader/qspi_dr1m35/
mkdir -p ${PWD}/../binary-win64/share/openocd/loader/emmc_ph1a400
cp -f ../contrib/loaders/flash/emmc/dwcmshc/build_ph1a400/*.elf \
        ${PWD}/../binary-win64/share/openocd/loader/emmc_ph1a400/
mkdir -p ${PWD}/../binary-win64/share/openocd/loader/qspi_ph1a400
cp -f ../contrib/loaders/flash/qspi/dwcssi/build_ph1a400/*.elf \
        ${PWD}/../binary-win64/share/openocd/loader/qspi_ph1a400/
