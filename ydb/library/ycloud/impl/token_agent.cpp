#include <ydb/public/api/client/yc_private/iam/token_agent.grpc.pb.h>
#include <ydb/library/ycloud/api/token_agent.h>
#include <ydb/library/grpc/actor_client/grpc_service_client.h>
#include <ydb/library/grpc/actor_client/grpc_service_settings.h>

#include "token_agent.h"

namespace NCloud {

class TTokenAgent : public NActors::TActor<TTokenAgent>, NGrpcActorClient::TGrpcServiceClient<yandex::cloud::priv::iam::v1::TokenAgent> {
    using TThis = TTokenAgent;
    using TBase = NActors::TActor<TTokenAgent>;

private:
    struct TGetTokenRequest : NGrpcActorClient::TGrpcServiceClient<yandex::cloud::priv::iam::v1::TokenAgent>::TGrpcRequest {
        static constexpr auto Request = &yandex::cloud::priv::iam::v1::TokenAgent::Stub::AsyncGetToken;
        using TRequestEventType = NEvTokenAgent::TEvGetTokenRequest;
        using TResponseEventType = NEvTokenAgent::TEvGetTokenResponse;

        static const yandex::cloud::priv::iam::v1::GetTokenRequest& Obfuscate(const yandex::cloud::priv::iam::v1::GetTokenRequest& request) {
            return request;
        }

        static yandex::cloud::priv::iam::v1::GetTokenResponse Obfuscate(const yandex::cloud::priv::iam::v1::GetTokenResponse& response) {
            yandex::cloud::priv::iam::v1::GetTokenResponse obfuscatedResponse(response);
            obfuscatedResponse.Setiam_token(MaskToken(obfuscatedResponse.Getiam_token()));
            return obfuscatedResponse;
        }
    };

public:
    TTokenAgent(const NGrpcActorClient::TGrpcClientSettings& settings);
    void StateWork(TAutoPtr<NActors::IEventHandle>& ev);

private:
    void Handle(NEvTokenAgent::TEvGetTokenRequest::TPtr& ev);
};

TTokenAgent::TTokenAgent(const NGrpcActorClient::TGrpcClientSettings& settings)
    : TBase(&TThis::StateWork)
    , TGrpcServiceClient(settings)
{}

void TTokenAgent::StateWork(TAutoPtr<NActors::IEventHandle>& ev) {
    switch (ev->GetTypeRewrite()) {
        hFunc(NEvTokenAgent::TEvGetTokenRequest, Handle);
        cFunc(TEvents::TSystem::PoisonPill, PassAway);
    }
}

void TTokenAgent::Handle(NEvTokenAgent::TEvGetTokenRequest::TPtr& ev) {
    MakeCall<TGetTokenRequest>(std::move(ev));
}

IActor* CreateTokenAgent(const NGrpcActorClient::TGrpcClientSettings& settings) {
    return new TTokenAgent(settings);
}

IActor* CreateTokenAgent(const TString& endpoint) {
    NGrpcActorClient::TGrpcClientSettings settings;
    settings.Endpoint = endpoint;
    return CreateTokenAgent(settings);
}

} // NCloud
