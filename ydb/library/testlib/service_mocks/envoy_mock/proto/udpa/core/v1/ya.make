PROTO_LIBRARY()

PROTO_NAMESPACE(ydb/library/testlib/service_mocks/envoy_mock/proto)

PY_NAMESPACE(udpa.core.v1)

EXCLUDE_TAGS(
    GO_PROTO
    JAVA_PROTO
)

PEERDIR(
    ydb/library/testlib/service_mocks/envoy_mock/proto/udpa/annotations
    ydb/library/testlib/service_mocks/envoy_mock/proto/validate
)

SRCDIR(
    ydb/library/testlib/service_mocks/envoy_mock/proto/udpa/core/v1
)

SRCS(
    authority.proto
    collection_entry.proto
    context_params.proto
    # resource.proto
    resource_locator.proto
    resource_name.proto
)

END()
