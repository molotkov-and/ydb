PROTO_LIBRARY()

PROTO_NAMESPACE(ydb/library/testlib/service_mocks/envoy_mock/proto)

PY_NAMESPACE(envoy)

EXCLUDE_TAGS(
    GO_PROTO
    JAVA_PROTO
)

PEERDIR(
    ydb/library/testlib/service_mocks/envoy_mock/proto/envoy/annotations
    ydb/library/testlib/service_mocks/envoy_mock/proto/envoy/config/accesslog/v3
    ydb/library/testlib/service_mocks/envoy_mock/proto/envoy/config/core/v3
    ydb/library/testlib/service_mocks/envoy_mock/proto/envoy/config/endpoint/v3
    ydb/library/testlib/service_mocks/envoy_mock/proto/envoy/config/listener/v3
    ydb/library/testlib/service_mocks/envoy_mock/proto/envoy/config/route/v3
    ydb/library/testlib/service_mocks/envoy_mock/proto/envoy/extensions/filters/network/http_connection_manager/v3
    ydb/library/testlib/service_mocks/envoy_mock/proto/envoy/service/discovery/v3
    ydb/library/testlib/service_mocks/envoy_mock/proto/envoy/type/matcher/v3
    ydb/library/testlib/service_mocks/envoy_mock/proto/envoy/type/metadata/v3
    ydb/library/testlib/service_mocks/envoy_mock/proto/envoy/type/tracing/v3
    ydb/library/testlib/service_mocks/envoy_mock/proto/envoy/type/v3
)

SRCDIR(
    ydb/library/testlib/service_mocks/envoy_mock/proto/envoy
)

END()
