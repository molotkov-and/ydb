PROTO_LIBRARY()

PROTO_NAMESPACE(ydb/library/testlib/service_mocks/envoy_mock/proto)

PY_NAMESPACE(udpa.annotations)

EXCLUDE_TAGS(
    GO_PROTO
    JAVA_PROTO
)

PEERDIR(
    ydb/library/testlib/service_mocks/envoy_mock/proto/validate
)

SRCDIR(
    ydb/library/testlib/service_mocks/envoy_mock/proto/udpa/annotations
)

SRCS(
    migrate.proto
    security.proto
    sensitive.proto
    status.proto
    versioning.proto
)

END()
