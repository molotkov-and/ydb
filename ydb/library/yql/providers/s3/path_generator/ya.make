LIBRARY()

SRCS(
    yql_s3_path_generator.cpp
)

PEERDIR(
    library/cpp/scheme
    yql/essentials/minikql/datetime
    yql/essentials/public/udf
)

GENERATE_ENUM_SERIALIZATION(yql_s3_path_generator.h)

YQL_LAST_ABI_VERSION()

END()

RECURSE_FOR_TESTS(
    ut
)
