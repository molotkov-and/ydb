LIBRARY()

SRCS(
    token_manager.cpp
    vm_metadata_token_provider_handler.cpp
    token_provider.cpp
    kikimr_token_manager.cpp
    token_agent_provider.cpp
    token_agent_handler.cpp
)


PEERDIR(
    ydb/core/base
    ydb/library/actors/core
    ydb/library/actors/http
    ydb/core/protos
    ydb/core/util
    library/cpp/json
    ydb/public/api/client/yc_private/iam
    ydb/library/security
)

END()

RECURSE_FOR_TESTS(
    ut
)
