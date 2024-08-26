#pragma once

#include "oidc_session_create.h"

namespace NOIDC {

class THandlerSessionCreateNebius : public THandlerSessionCreate {
private:
    using TBase = THandlerSessionCreate;

public:
    THandlerSessionCreateNebius(const NActors::TActorId& sender,
                                const NHttp::THttpIncomingRequestPtr& request,
                                const NActors::TActorId& httpProxyId,
                                const TOpenIdConnectSettings& settings);

private:
    void RequestSessionToken(const TString& code, const NActors::TActorContext& ctx) override;
    virtual void ProcessSessionToken(const TString& sessionToken, const NActors::TActorContext& ctx) override;

    STFUNC(StateWork) {
        switch (ev->GetTypeRewrite()) {
            HFunc(NHttp::TEvHttpProxy::TEvHttpIncomingResponse, Handle);
        }
    }
};

}  // NOIDC
