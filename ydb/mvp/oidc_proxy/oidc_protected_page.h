#pragma once

#include <util/generic/algorithm.h>
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
#include <ydb/public/api/client/yc_private/oauth/session_service.grpc.pb.h>
#include <ydb/mvp/core/mvp_log.h>
#include <ydb/mvp/core/mvp_tokens.h>
#include <ydb/mvp/core/appdata.h>
#include <ydb/mvp/core/protos/mvp.pb.h>
#include <ydb/core/util/wildcard.h>
#include "openid_connect.h"

struct TYdbLocation;

namespace NOIDC {

class THandlerSessionServiceCheck : public NActors::TActorBootstrapped<THandlerSessionServiceCheck> {
protected:
    using TBase = NActors::TActorBootstrapped<THandlerSessionServiceCheck>;

    const NActors::TActorId Sender;
    const NHttp::THttpIncomingRequestPtr Request;
    NActors::TActorId HttpProxyId;
    const TOpenIdConnectSettings Settings;
    TString ProtectedPageUrl;
    TString RequestedPageScheme;
    bool IsAjaxRequest = false;
    const TYdbLocation& Location;

    const static inline TStringBuf NOT_FOUND_HTML_PAGE = "<html><head><title>404 Not Found</title></head><body bgcolor=\"white\"><center><h1>404 Not Found</h1></center></body></html>";
    const static inline TStringBuf IAM_TOKEN_SCHEME = "Bearer ";
    const static inline TStringBuf IAM_TOKEN_SCHEME_LOWER = "bearer ";
    const static inline TStringBuf AUTH_HEADER_NAME = "Authorization";

public:
    THandlerSessionServiceCheck(const NActors::TActorId& sender,
                                const NHttp::THttpIncomingRequestPtr& request,
                                const NActors::TActorId& httpProxyId,
                                const TOpenIdConnectSettings& settings,
                                const TYdbLocation& location);


    virtual void Bootstrap(const NActors::TActorContext& ctx);
    void HandleProxy(NHttp::TEvHttpProxy::TEvHttpIncomingResponse::TPtr event, const NActors::TActorContext& ctx);

protected:
    virtual void StartOidcProcess(const NActors::TActorContext& ctx) = 0;
    virtual void ForwardUserRequest(TStringBuf authHeader, const NActors::TActorContext& ctx, bool secure = false);
    virtual bool NeedSendSecureHttpRequest(const NHttp::THttpIncomingResponsePtr& response) const = 0;
    bool CheckRequestedHost();
    void ForwardRequestHeaders(NHttp::THttpOutgoingRequestPtr& request) const;
    static bool IsAuthorizedRequest(TStringBuf authHeader);
    static TString FixReferenceInHtml(TStringBuf html, TStringBuf host, TStringBuf findStr);
    static TString FixReferenceInHtml(TStringBuf html, TStringBuf host);

private:
    NHttp::THeadersBuilder GetResponseHeaders(const NHttp::THttpIncomingResponsePtr& response);
    void SendSecureHttpRequest(const NHttp::THttpIncomingResponsePtr& response, const NActors::TActorContext& ctx);
    TString GetFixedLocationHeader(TStringBuf location);
};

}  // NOIDC
