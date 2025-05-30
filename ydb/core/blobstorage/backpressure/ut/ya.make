UNITTEST_FOR(ydb/core/blobstorage/backpressure)

FORK_SUBTESTS()

IF (WITH_VALGRIND)
    SIZE(LARGE)
    TAG(ya:fat)
ELSE()
    SIZE(MEDIUM)
ENDIF()

PEERDIR(
    library/cpp/getopt
    library/cpp/svnversion
    ydb/core/base
    ydb/core/blobstorage/dsproxy/mock
)

SRCS(
    queue_backpressure_client_ut.cpp
    queue_backpressure_server_ut.cpp
)

END()
