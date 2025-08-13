UNITTEST_FOR(ydb/library/ycloud/impl)

FORK_SUBTESTS()

SIZE(MEDIUM)

PEERDIR(
    library/cpp/testing/unittest
    ydb/core/util/actorsys_test
    ydb/library/testlib/service_mocks
)

YQL_LAST_ABI_VERSION()

SRCS(
    token_agent_ut.cpp
)

END()
