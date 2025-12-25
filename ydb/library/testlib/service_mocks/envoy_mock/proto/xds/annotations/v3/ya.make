PROTO_LIBRARY()

PROTO_NAMESPACE(ydb/library/testlib/service_mocks/envoy_mock/proto)

PY_NAMESPACE(xds.annotations.v3)

EXCLUDE_TAGS(
    GO_PROTO
    JAVA_PROTO
)

SRCDIR(
    ydb/library/testlib/service_mocks/envoy_mock/proto/xds/annotations/v3
)

SRCS(
    status.proto
)

END()
