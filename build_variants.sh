cmake -DCMAKE_INSTALL_PREFIX="~/libraries/hdf5_no_staging" -DHDF5_BUILD_CPP_LIB=1 -DHDF5_BUILD_DOC=0 -DHDF5_BUILD_EXAMPLES=0 -DHDF5_BUILD_TOOLS=0 -DHDF5_BUILD_UTILS=0 -BUILD_STATIC_LIBS=0 -S . -B build_no_staging
cmake --build ./build_no_staging --target install

cmake -DSTAGING=1 -DSTAGING_MEMORY_OPTIMIZED=1 -DCMAKE_INSTALL_PREFIX="~/libraries/hdf5_staging_memory_optimized" -DHDF5_BUILD_CPP_LIB=1 -DHDF5_BUILD_DOC=0 -DHDF5_BUILD_EXAMPLES=0 -DHDF5_BUILD_TOOLS=0 -DHDF5_BUILD_UTILS=0 -BUILD_STATIC_LIBS=0 -S . -B build_staging_memory_optimized
cmake --build ./build_staging_memory_optimized --target install

cmake -DSTAGING=1 -DSTAGING_DISK_OPTIMIZED=1 -DCMAKE_INSTALL_PREFIX="~/libraries/hdf5_staging_disk_optimized" -DHDF5_BUILD_CPP_LIB=1 -DHDF5_BUILD_DOC=0 -DHDF5_BUILD_EXAMPLES=0 -DHDF5_BUILD_TOOLS=0 -DHDF5_BUILD_UTILS=0 -BUILD_STATIC_LIBS=0 -S . -B build_staging_disk_optimized
cmake --build ./build_staging_disk_optimized --target install