PROTO_LIBRARY()

PROTO_NAMESPACE(ydb/library/testlib/service_mocks/envoy_mock/proto)

PY_NAMESPACE(envoy.config.accesslog.v3)

EXCLUDE_TAGS(
    GO_PROTO
    JAVA_PROTO
)

PEERDIR(
    ydb/library/testlib/service_mocks/envoy_mock/proto/envoy/config/core/v3
    ydb/library/testlib/service_mocks/envoy_mock/proto/envoy/config/route/v3
    ydb/library/testlib/service_mocks/envoy_mock/proto/envoy/type/matcher/v3
    ydb/library/testlib/service_mocks/envoy_mock/proto/envoy/type/v3
    # taxi/schemas/schemas/proto/google
    # taxi/schemas/schemas/proto/opencensus
    # taxi/schemas/schemas/proto/prometheus
    ydb/library/testlib/service_mocks/envoy_mock/proto/udpa
    ydb/library/testlib/service_mocks/envoy_mock/proto/udpa/annotations
    ydb/library/testlib/service_mocks/envoy_mock/proto/validate
    ydb/library/testlib/service_mocks/envoy_mock/proto/xds
)

SRCDIR(
    ydb/library/testlib/service_mocks/envoy_mock/proto/envoy/config/accesslog/v3
)

SRCS(
    accesslog.proto
)

END()
