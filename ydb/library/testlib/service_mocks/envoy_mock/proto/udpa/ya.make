PROTO_LIBRARY()

PROTO_NAMESPACE(ydb/library/testlib/service_mocks/envoy_mock/proto)

PY_NAMESPACE(udpa)

EXCLUDE_TAGS(
    GO_PROTO
    JAVA_PROTO
)

PEERDIR(
    ydb/library/testlib/service_mocks/envoy_mock/proto/udpa/annotations
    ydb/library/testlib/service_mocks/envoy_mock/proto/udpa/core/v1
    # taxi/schemas/schemas/proto/udpa/data/orca/v1
    # taxi/schemas/schemas/proto/udpa/service/orca/v1
    # taxi/schemas/schemas/proto/udpa/type/v1
    ydb/library/testlib/service_mocks/envoy_mock/proto/validate
)

SRCDIR(
    ydb/library/testlib/service_mocks/envoy_mock/proto/udpa
)

END()
