PROTO_LIBRARY()

PROTO_NAMESPACE(ydb/library/testlib/service_mocks/envoy_mock/proto)

PY_NAMESPACE(xds.core.v3)

EXCLUDE_TAGS(
    GO_PROTO
    JAVA_PROTO
)

PEERDIR(
    ydb/library/testlib/service_mocks/envoy_mock/proto/xds/annotations/v3
)

SRCDIR(
    ydb/library/testlib/service_mocks/envoy_mock/proto/xds/core/v3
)

SRCS(
    context_params.proto
)

END()
