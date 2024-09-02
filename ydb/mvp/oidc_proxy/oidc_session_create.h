#pragma once

#include <util/generic/hash_set.h>
#include <ydb/library/actors/core/actorsystem.h>
#include <ydb/library/actors/core/actor.h>
#include <ydb/library/actors/core/hfunc.h>
#include <ydb/library/actors/core/events.h>
#include <ydb/library/actors/core/event_local.h>
#include <ydb/library/actors/core/actor_bootstrapped.h>
#include <ydb/library/actors/http/http.h>
#include <ydb/public/lib/deprecated/client/grpc_client.h>
#include <ydb/library/grpc/client/grpc_client_low.h>
#include <ydb/library/actors/core/log.h>
#include <library/cpp/http/io/stream.h>
#include <util/network/sock.h>
#include <library/cpp/json/json_reader.h>
#include <ydb/public/api/client/yc_private/oauth/session_service.grpc.pb.h>
#include <ydb/mvp/core/protos/mvp.pb.h>
#include <ydb/mvp/core/mvp_log.h>
#include <ydb/mvp/core/mvp_tokens.h>
#include <ydb/mvp/core/appdata.h>
#include "openid_connect.h"

struct TYdbLocation;

namespace NOIDC {

class THandlerSessionCreate : public NActors::TActorBootstrapped<THandlerSessionCreate> {
private:
    using TBase = NActors::TActorBootstrapped<THandlerSessionCreate>;

protected:
    using TSessionService = yandex::cloud::priv::oauth::v1::SessionService;

    const NActors::TActorId Sender;
    const NHttp::THttpIncomingRequestPtr Request;
    NActors::TActorId HttpProxyId;
    const TOpenIdConnectSettings Settings;
    const TYdbLocation& Location;
    NOIDC::TOidcSession OidcSession;

public:
    THandlerSessionCreate(const NActors::TActorId& sender,
                          const NHttp::THttpIncomingRequestPtr& request,
                          const NActors::TActorId& httpProxyId,
                          const TOpenIdConnectSettings& settings,
                          const TYdbLocation& location);

    void Bootstrap(const NActors::TActorContext& ctx);

protected:
    virtual void RequestSessionToken(const TString&, const NActors::TActorContext&) = 0;
    virtual void ProcessSessionToken(const TString& accessToken, const NActors::TActorContext&) = 0;
    void Handle(NHttp::TEvHttpProxy::TEvHttpIncomingResponse::TPtr event, const NActors::TActorContext& ctx);
};

}  // NOIDC
