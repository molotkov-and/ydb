PROTO_LIBRARY()

PROTO_NAMESPACE(ydb/library/testlib/service_mocks/envoy_mock/proto)

PY_NAMESPACE(validate)

EXCLUDE_TAGS(
    GO_PROTO
    JAVA_PROTO
)

SRCDIR(
    ydb/library/testlib/service_mocks/envoy_mock/proto/validate
)

SRCS(
    validate.proto
)

END()

# RECURSE(
#     __frameworks__
# )
