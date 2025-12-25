PROTO_LIBRARY()

PROTO_NAMESPACE(ydb/library/testlib/service_mocks/envoy_mock/proto)

PY_NAMESPACE(envoy.config.listener.v3)

EXCLUDE_TAGS(
    GO_PROTO
    JAVA_PROTO
)

PEERDIR(
    ydb/library/testlib/service_mocks/envoy_mock/proto/envoy/config/accesslog/v3
    ydb/library/testlib/service_mocks/envoy_mock/proto/envoy/config/core/v3
    ydb/library/testlib/service_mocks/envoy_mock/proto/envoy/type/v3
    # taxi/schemas/schemas/proto/google
    # taxi/schemas/schemas/proto/opencensus
    # taxi/schemas/schemas/proto/prometheus
    ydb/library/testlib/service_mocks/envoy_mock/proto/udpa
    ydb/library/testlib/service_mocks/envoy_mock/proto/udpa/annotations
    ydb/library/testlib/service_mocks/envoy_mock/proto/udpa/core/v1
    ydb/library/testlib/service_mocks/envoy_mock/proto/validate
    ydb/library/testlib/service_mocks/envoy_mock/proto/xds
)

USE_COMMON_GOOGLE_APIS()

SRCDIR(
    ydb/library/testlib/service_mocks/envoy_mock/proto/envoy/config/listener/v3
)

SRCS(
    api_listener.proto
    listener.proto
    listener_components.proto
    # quic_config.proto
    # udp_default_writer_config.proto
    # udp_gso_batch_writer_config.proto
    udp_listener_config.proto
)

END()
