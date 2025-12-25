PROTO_LIBRARY()

PROTO_NAMESPACE(ydb/library/testlib/service_mocks/envoy_mock/proto)

PY_NAMESPACE(envoy.config.core.v3)

EXCLUDE_TAGS(
    GO_PROTO
    JAVA_PROTO
)

PEERDIR(
    ydb/library/testlib/service_mocks/envoy_mock/proto/envoy/annotations
    ydb/library/testlib/service_mocks/envoy_mock/proto/envoy/type/matcher/v3
    ydb/library/testlib/service_mocks/envoy_mock/proto/envoy/type/v3
    # taxi/schemas/schemas/proto/google
    # taxi/schemas/schemas/proto/opencensus
    # taxi/schemas/schemas/proto/prometheus
    ydb/library/testlib/service_mocks/envoy_mock/proto/udpa
    ydb/library/testlib/service_mocks/envoy_mock/proto/udpa/annotations
    ydb/library/testlib/service_mocks/envoy_mock/proto/udpa/core/v1
    ydb/library/testlib/service_mocks/envoy_mock/proto/validate
    ydb/library/testlib/service_mocks/envoy_mock/proto/xds
    ydb/library/testlib/service_mocks/envoy_mock/proto/xds/core/v3
)

SRCDIR(
    ydb/library/testlib/service_mocks/envoy_mock/proto/envoy/config/core/v3
)

SRCS(
    address.proto
    backoff.proto
    base.proto
    config_source.proto
    event_service_config.proto
    extension.proto
    # grpc_method_list.proto
    grpc_service.proto
    health_check.proto
    http_uri.proto
    protocol.proto
    proxy_protocol.proto
    socket_option.proto
    substitution_format_string.proto
)

# USE_COMMON_GOOGLE_APIS(
#     api/annotations
# )

END()
