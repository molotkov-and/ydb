UNITTEST_FOR(ydb/core/security/token_manager)

FORK_SUBTESTS()

SIZE(MEDIUM)

PEERDIR(
    ydb/core/testlib/default
    ydb/library/testlib/service_mocks
)

YQL_LAST_ABI_VERSION()

SRCS(
    token_manager_ut.cpp
)

END()
