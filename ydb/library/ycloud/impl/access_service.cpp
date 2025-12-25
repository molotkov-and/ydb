#include <ydb/library/actors/core/actorsystem.h>
#include <ydb/library/actors/core/actor.h>
#include <library/cpp/json/json_value.h>
#include <ydb/public/api/client/yc_private/accessservice/access_service.grpc.pb.h>
#include "access_service.h"
#include <ydb/library/grpc/actor_client/grpc_service_client.h>
#include <ydb/library/grpc/actor_client/grpc_service_cache.h>

namespace NCloud {

using namespace NKikimr;

class TAccessService : public NActors::TActor<TAccessService>, NGrpcActorClient::TGrpcServiceClient<yandex::cloud::priv::accessservice::v2::AccessService> {
    using TThis = TAccessService;
    using TBase = NActors::TActor<TAccessService>;

    struct TAuthenticateRequest : TGrpcRequest {
        static constexpr auto Request = &yandex::cloud::priv::accessservice::v2::AccessService::Stub::AsyncAuthenticate;
        using TRequestEventType = TEvAccessService::TEvAuthenticateRequest;
        using TResponseEventType = TEvAccessService::TEvAuthenticateResponse;

        static yandex::cloud::priv::accessservice::v2::AuthenticateRequest Obfuscate(const yandex::cloud::priv::accessservice::v2::AuthenticateRequest& p);

        static const yandex::cloud::priv::accessservice::v2::AuthenticateResponse& Obfuscate(const yandex::cloud::priv::accessservice::v2::AuthenticateResponse& p) {
            return p;
        }
    };

    struct TAuthorizeRequest : TGrpcRequest {
        static constexpr auto Request = &yandex::cloud::priv::accessservice::v2::AccessService::Stub::AsyncAuthorize;
        using TRequestEventType = TEvAccessService::TEvAuthorizeRequest;
        using TResponseEventType = TEvAccessService::TEvAuthorizeResponse;

        static yandex::cloud::priv::accessservice::v2::AuthorizeRequest Obfuscate(const yandex::cloud::priv::accessservice::v2::AuthorizeRequest& p);

        static const yandex::cloud::priv::accessservice::v2::AuthorizeResponse& Obfuscate(const yandex::cloud::priv::accessservice::v2::AuthorizeResponse& p) {
            return p;
        }
    };
    struct TBulkAuthorizeRequest : TGrpcRequest {
        static constexpr auto Request = &yandex::cloud::priv::accessservice::v2::AccessService::Stub::AsyncBulkAuthorize;
        using TRequestEventType = TEvAccessService::TEvBulkAuthorizeRequest;
        using TResponseEventType = TEvAccessService::TEvBulkAuthorizeResponse;

        static yandex::cloud::priv::accessservice::v2::BulkAuthorizeRequest Obfuscate(const yandex::cloud::priv::accessservice::v2::BulkAuthorizeRequest& p);

        static const yandex::cloud::priv::accessservice::v2::BulkAuthorizeResponse& Obfuscate(const yandex::cloud::priv::accessservice::v2::BulkAuthorizeResponse& p) {
            return p;
        }
    };

    void Handle(TEvAccessService::TEvAuthenticateRequest::TPtr& ev) {
        MakeCall<TAuthenticateRequest>(std::move(ev));
    }

    void Handle(TEvAccessService::TEvAuthorizeRequest::TPtr& ev) {
        MakeCall<TAuthorizeRequest>(std::move(ev));
    }

    void Handle(TEvAccessService::TEvBulkAuthorizeRequest::TPtr& ev) {
        MakeCall<TBulkAuthorizeRequest>(std::move(ev));
    }

public:
    static constexpr NKikimrServices::TActivity::EType ActorActivityType() { return NKikimrServices::TActivity::ACCESS_SERVICE_ACTOR; }

    TAccessService(const TAccessServiceSettings& settings)
        : TBase(&TThis::StateWork)
        , TGrpcServiceClient(settings)
    {}

    void StateWork(TAutoPtr<NActors::IEventHandle>& ev) {
        switch (ev->GetTypeRewrite()) {
            hFunc(TEvAccessService::TEvAuthenticateRequest, Handle);
            hFunc(TEvAccessService::TEvAuthorizeRequest, Handle);
            hFunc(TEvAccessService::TEvBulkAuthorizeRequest, Handle);
            cFunc(TEvents::TSystem::PoisonPill, PassAway);
        }
    }
};

IActor* CreateAccessService(const TAccessServiceSettings& settings) {
    return new TAccessService(settings);
}

IActor* CreateAccessServiceWithCache(const TAccessServiceSettings& settings) {
    IActor* accessService = CreateAccessService(settings);
    accessService = NGrpcActorClient::CreateGrpcServiceCache<TEvAccessService::TEvAuthenticateRequest, TEvAccessService::TEvAuthenticateResponse>(accessService);
    accessService = NGrpcActorClient::CreateGrpcServiceCache<TEvAccessService::TEvAuthorizeRequest, TEvAccessService::TEvAuthorizeResponse>(accessService);
    accessService = NGrpcActorClient::CreateGrpcServiceCache<TEvAccessService::TEvAuthorizeRequest, TEvAccessService::TEvBulkAuthorizeResponse>(accessService);
    return accessService;
}

yandex::cloud::priv::accessservice::v2::AuthenticateRequest TAccessService::TAuthenticateRequest::Obfuscate(const yandex::cloud::priv::accessservice::v2::AuthenticateRequest& p) {
    yandex::cloud::priv::accessservice::v2::AuthenticateRequest r(p);
    if (r.iam_token()) {
        r.set_iam_token(MaskToken(r.iam_token()));
    }
    if (r.api_key()) {
        r.set_api_key(MaskToken(r.api_key()));
    }
    r.clear_iam_cookie();
    return r;
}

yandex::cloud::priv::accessservice::v2::AuthorizeRequest TAccessService::TAuthorizeRequest::Obfuscate(const yandex::cloud::priv::accessservice::v2::AuthorizeRequest& p) {
    yandex::cloud::priv::accessservice::v2::AuthorizeRequest r(p);
    if (r.iam_token()) {
        r.set_iam_token(MaskToken(r.iam_token()));
    }
    if (r.api_key()) {
        r.set_api_key(MaskToken(r.api_key()));
    }
    return r;
}

yandex::cloud::priv::accessservice::v2::BulkAuthorizeRequest TAccessService::TBulkAuthorizeRequest::Obfuscate(const yandex::cloud::priv::accessservice::v2::BulkAuthorizeRequest& p) {
    yandex::cloud::priv::accessservice::v2::BulkAuthorizeRequest r(p);
    if (r.iam_token()) {
        r.set_iam_token(MaskToken(r.iam_token()));
    }
    if (r.api_key()) {
        r.set_api_key(MaskToken(r.api_key()));
    }
    return r;
}

}
