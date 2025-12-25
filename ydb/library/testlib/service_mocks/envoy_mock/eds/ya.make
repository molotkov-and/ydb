PY3_PROGRAM()

PY_SRCS(
    MAIN mock.py
    # MAIN __main__.py
)

PEERDIR(
    ydb/library/testlib/service_mocks/envoy_mock/proto/envoy
    ydb/library/testlib/service_mocks/envoy_mock/proto/udpa
    ydb/library/testlib/service_mocks/envoy_mock/proto/validate
    ydb/library/testlib/service_mocks/envoy_mock/proto/xds
    contrib/python/grpcio
)

END()
