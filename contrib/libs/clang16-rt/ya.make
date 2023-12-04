# Generated by devtools/yamaker from nixpkgs 23.05.

OWNER(g:cpp-contrib)

VERSION(16.0.6)

ORIGINAL_SOURCE(https://github.com/llvm/llvm-project/archive/llvmorg-16.0.6.tar.gz)

RECURSE(
    lib/asan
    lib/asan-preinit
    lib/asan_cxx
    lib/asan_static
    lib/lsan
    lib/profile
    lib/stats
    lib/stats_client
    lib/tsan
    lib/tsan_cxx
    lib/ubsan_minimal
    lib/ubsan_standalone
    lib/ubsan_standalone_cxx
)

IF (OS_LINUX)
    RECURSE(
        lib/cfi
        lib/cfi_diag
        lib/dd
        lib/dfsan
        lib/gwp_asan
        lib/hwasan
        lib/hwasan_aliases
        lib/hwasan_aliases_cxx
        lib/hwasan_cxx
        lib/memprof
        lib/memprof-preinit
        lib/memprof_cxx
        lib/msan
        lib/msan_cxx
        lib/safestack
        lib/scudo_standalone
        lib/scudo_standalone_cxx
    )
ENDIF()