PY3_PROGRAM()

PY_SRCS(
    MAIN mock.py
)

PEERDIR(
    # ydb/library/testlib/service_mocks/envoy_mock/proto
    contrib/python/grpcio
)

END()
