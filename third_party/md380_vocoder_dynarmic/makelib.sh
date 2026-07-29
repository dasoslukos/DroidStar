#!/bin/sh -x
ar -x _deps/dynarmic-build/src/dynarmic/libdynarmic.a
ar -x _deps/dynarmic-build/externals/zydis/zycore/libZycore.a
ar -x _deps/dynarmic-build/externals/zydis/libZydis.a
ar -x _deps/dynarmic-build/externals/fmt/libfmt.a
ar -x _deps/dynarmic-build/externals/mcl/src/libmcl.a
ar -x libmd380_vocoder.a
rm libmd380_vocoder.a
ar -qc libmd380_vocoder.a *.o
rm *.o
#sudo cp libmd380_vocoder.a /usr/local/lib64

