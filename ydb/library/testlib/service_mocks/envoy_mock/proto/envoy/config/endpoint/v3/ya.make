PROTO_LIBRARY()

PROTO_NAMESPACE(ydb/library/testlib/service_mocks/envoy_mock/proto)

PY_NAMESPACE(envoy.config.endpoint.v3)

EXCLUDE_TAGS(
    GO_PROTO
    JAVA_PROTO
)

PEERDIR(
    ydb/library/testlib/service_mocks/envoy_mock/proto/envoy/config/core/v3
    ydb/library/testlib/service_mocks/envoy_mock/proto/envoy/type/v3
    # taxi/schemas/schemas/proto/google
    # taxi/schemas/schemas/proto/opencensus
    # taxi/schemas/schemas/proto/prometheus
    ydb/library/testlib/service_mocks/envoy_mock/proto/udpa
    ydb/library/testlib/service_mocks/envoy_mock/proto/udpa/annotations
    ydb/library/testlib/service_mocks/envoy_mock/proto/validate
    ydb/library/testlib/service_mocks/envoy_mock/proto/xds
)

USE_COMMON_GOOGLE_APIS()

SRCDIR(
    ydb/library/testlib/service_mocks/envoy_mock/proto/envoy/config/endpoint/v3
)

SRCS(
    endpoint.proto
    endpoint_components.proto
    # load_report.proto
)

END()
