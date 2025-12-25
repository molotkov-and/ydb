PROTO_LIBRARY()

PROTO_NAMESPACE(ydb/library/testlib/service_mocks/envoy_mock/proto)

PY_NAMESPACE(envoy.type.v3)

EXCLUDE_TAGS(
    GO_PROTO
    JAVA_PROTO
)

PEERDIR(
    # taxi/schemas/schemas/proto/google
    # taxi/schemas/schemas/proto/opencensus
    # taxi/schemas/schemas/proto/prometheus
    ydb/library/testlib/service_mocks/envoy_mock/proto/udpa
    ydb/library/testlib/service_mocks/envoy_mock/proto/udpa/annotations
    ydb/library/testlib/service_mocks/envoy_mock/proto/validate
    ydb/library/testlib/service_mocks/envoy_mock/proto/xds
)

SRCDIR(
    ydb/library/testlib/service_mocks/envoy_mock/proto/envoy/type/v3
)

SRCS(
    # hash_policy.proto
    http.proto
    # http_status.proto
    percent.proto
    range.proto
    # ratelimit_unit.proto
    semantic_version.proto
    # token_bucket.proto
)

END()
