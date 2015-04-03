1. Create a 'build' directory outside of the source
    cd ${SOURCE_DIR}/..; mkdir build; cd build; BUILD_DIR=$(pwd)
2. Download STM32CubeF0 from ST's website and put it into build directory
    cd ${BUILD_DIR}; wget ${URL_OF_STM32CubeF0}
3. Run the script/bootstrap.sh in the build directory
    cd ${BUILD_DIR}; ${SOURCE_DIR}/scripts/bootstrap.sh
4. Cmake to generate makefiles
    cd ${BUILD_DIR}; cmake ${SOURCE_DIR}
5. Build
    make
