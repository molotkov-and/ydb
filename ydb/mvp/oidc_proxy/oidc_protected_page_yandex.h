#pragma once

#include "oidc_protected_page.h"
#include "oidc_session.h"
#include <util/generic/maybe.h>
#include <ydb/mvp/core/core_ydb_impl.h>
#include <ydb/public/sdk/cpp/client/ydb_table/table.h>

struct TYdbLocation;

namespace NOIDC {

class THandlerSessionServiceCheckYandex : public THandlerSessionServiceCheck {
private:
    using TBase = THandlerSessionServiceCheck;
    using TSessionService = yandex::cloud::priv::oauth::v1::SessionService;

    TMaybe<NYdb::NTable::TSession> DbSession;
    TOidcSession OidcSession;

public:
    THandlerSessionServiceCheckYandex(const NActors::TActorId& sender,
                                const NHttp::THttpIncomingRequestPtr& request,
                                const NActors::TActorId& httpProxyId,
                                const TOpenIdConnectSettings& settings,
                                const TYdbLocation& location);

    void Bootstrap(const NActors::TActorContext& ctx) override;
    void Handle(TEvPrivate::TEvCheckSessionResponse::TPtr event, const NActors::TActorContext& ctx);
    void Handle(TEvPrivate::TEvErrorResponse::TPtr event, const NActors::TActorContext& ctx);
    void Handle(NMVP::THandlerActorYdb::TEvPrivate::TEvCreateSessionResult::TPtr event, const NActors::TActorContext& ctx);
    void Handle(NMVP::THandlerActorYdb::TEvPrivate::TEvDataQueryResult::TPtr event, const NActors::TActorContext& ctx);

    STFUNC(StateWork) {
        switch (ev->GetTypeRewrite()) {
            HFunc(NHttp::TEvHttpProxy::TEvHttpIncomingResponse, HandleProxy);
            HFunc(TEvPrivate::TEvCheckSessionResponse, Handle);
            HFunc(TEvPrivate::TEvErrorResponse, Handle);
            HFunc(NMVP::THandlerActorYdb::TEvPrivate::TEvCreateSessionResult, Handle);
            HFunc(NMVP::THandlerActorYdb::TEvPrivate::TEvDataQueryResult, Handle);
        }
    }

private:
    void StartOidcProcess(const NActors::TActorContext& ctx) override;
    bool NeedSendSecureHttpRequest(const NHttp::THttpIncomingResponsePtr& response) const override;
};

}  // NOIDC
