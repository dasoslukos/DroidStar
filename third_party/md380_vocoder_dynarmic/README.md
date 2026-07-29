# md380_vocoder_dynarmic

MD380 vocoder library that works on x86_64 and arm64 natively. This uses dynarmic to run the MD380 firmware.  Boost libs are required for whichever platform this is built for.  See the header file for usage. 


## Building
```
cd md380_vocoder_dynarmic
mkdir build
cd build
cmake ..
make
../makelib.sh
```
manually copy the libmd380_vocoder.a and md380_vocoder.h files to your preffered lib and include locations

This is an example Android cmake command, once Android boost libs have been installed:
```
cmake -DANDROID_ABI=arm64-v8a -DCMAKE_CXX_STANDARD=17 -DBoost_NO_SYSTEM_PATHS=TRUE -DBoost_ADDITIONAL_VERSIONS="1.85" -DCMAKE_TOOLCHAIN_FILE=~/Android/Sdk/ndk/26.1.10909125/build/cmake/android.toolchain.cmake -DBOOST_ROOT="~/Android/boost/arm64-v8a" -DBoost_INCLUDE_DIR="~/Android/boost/arm64-v8a/include/boost-1_85" ..
```
