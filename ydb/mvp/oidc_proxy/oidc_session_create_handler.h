#pragma once

#include <ydb/library/actors/core/actor.h>
#include "openid_connect.h"

struct TYdbLocation;

namespace NOIDC {

class TSessionCreateHandler : public NActors::TActor<TSessionCreateHandler> {
    using TBase = NActors::TActor<TSessionCreateHandler>;

    const NActors::TActorId HttpProxyId;
    const TOpenIdConnectSettings Settings;
    const TYdbLocation& Location;

public:
    TSessionCreateHandler(const NActors::TActorId& httpProxyId, const TOpenIdConnectSettings& settings, const TYdbLocation& location);
    void Handle(NHttp::TEvHttpProxy::TEvHttpIncomingRequest::TPtr event, const NActors::TActorContext& ctx);

    STFUNC(StateWork) {
        switch (ev->GetTypeRewrite()) {
            HFunc(NHttp::TEvHttpProxy::TEvHttpIncomingRequest, Handle);
        }
    }
};

}  // NOIDC
