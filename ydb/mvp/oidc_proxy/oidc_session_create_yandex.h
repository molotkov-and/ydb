#pragma once

#include <ydb/mvp/core/core_ydb_impl.h>
#include "oidc_session_create.h"
#include "oidc_session.h"

struct TYdbLocation;

namespace NOIDC {

class THandlerSessionCreateYandex : public THandlerSessionCreate {
private:
    using TBase = THandlerSessionCreate;

public:
    THandlerSessionCreateYandex(const NActors::TActorId& sender,
                          const NHttp::THttpIncomingRequestPtr& request,
                          const NActors::TActorId& httpProxyId,
                          const TOpenIdConnectSettings& settings,
                          const TYdbLocation& location);

private:
    virtual void ProcessSessionToken(const TString& sessionToken, const NActors::TActorContext& ctx) override;
    void RequestSessionToken(const TString& code, const NActors::TActorContext& ctx) override;
    void HandleCreateSession(TEvPrivate::TEvCreateSessionResponse::TPtr event, const NActors::TActorContext& ctx);
    void HandleError(TEvPrivate::TEvErrorResponse::TPtr event, const NActors::TActorContext& ctx);

    static TString ChangeSameSiteFieldInSessionCookie(const TString& cookie);

    STFUNC(StateWork) {
        switch (ev->GetTypeRewrite()) {
            HFunc(NHttp::TEvHttpProxy::TEvHttpIncomingResponse, Handle);
            HFunc(TEvPrivate::TEvCreateSessionResponse, HandleCreateSession);
            HFunc(TEvPrivate::TEvErrorResponse, HandleError);
            HFunc(NMVP::THandlerActorYdb::TEvPrivate::TEvCreateSessionResult, Handle);
            HFunc(NMVP::THandlerActorYdb::TEvPrivate::TEvDataQueryResult, Handle);
        }
    }
};

}  // NOIDC
