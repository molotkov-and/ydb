#pragma once

#include <ydb/public/api/client/yc_private/iam/token_agent.grpc.pb.h>
#include <util/generic/hash.h>
#include <util/generic/string.h>

class TTokenAgentMock : public yandex::cloud::priv::iam::v1::TokenAgent::Service {
public:
    THashMap<TString, TString> Tokens {{"ydb-service", "ydb.service.token"}};
    bool ReturnUnavailable = false;

    grpc::Status GetToken(grpc::ServerContext*,
            const yandex::cloud::priv::iam::v1::GetTokenRequest* request,
            yandex::cloud::priv::iam::v1::GetTokenResponse* response) override;
};

grpc::Status TTokenAgentMock::GetToken(grpc::ServerContext*,
            const yandex::cloud::priv::iam::v1::GetTokenRequest* request,
            yandex::cloud::priv::iam::v1::GetTokenResponse* response) {
    Cerr << "++++++++++++" << Endl;
    if (ReturnUnavailable) {
        return grpc::Status(grpc::StatusCode::UNAVAILABLE, "Service Unavailable");
    }
    auto it = Tokens.find(request->Gettag());
    if (it != Tokens.end()) {
        response->Setiam_token(it->second);
        auto expiresAt = response->Mutableexpires_at();
        expiresAt->set_seconds(234537372);
        Cerr << "++++++++++++" << Endl;
        return grpc::Status(grpc::StatusCode::OK, "OK");
    }
    return grpc::Status(grpc::StatusCode::UNAUTHENTICATED, "Access Denied");
}
