PROTO_LIBRARY()

PROTO_NAMESPACE(ydb/library/testlib/service_mocks/envoy_mock/proto)

PY_NAMESPACE(envoy.service.discovery.v3)

EXCLUDE_TAGS(
    GO_PROTO
    JAVA_PROTO
)

GRPC()

PEERDIR(
    ydb/library/testlib/service_mocks/envoy_mock/proto/envoy/config/core/v3
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
    ydb/library/testlib/service_mocks/envoy_mock/proto/envoy/service/discovery/v3
)

SRCS(
    ads.proto
    discovery.proto
)

END()
