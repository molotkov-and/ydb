#include "oidc_session_create_yandex.h"

namespace NOIDC {

THandlerSessionCreateYandex::THandlerSessionCreateYandex(const NActors::TActorId& sender,
                          const NHttp::THttpIncomingRequestPtr& request,
                          const NActors::TActorId& httpProxyId,
                          const TOpenIdConnectSettings& settings,
                          const TYdbLocation& location)
    : THandlerSessionCreate(sender, request, httpProxyId, settings, location)
{}

void THandlerSessionCreateYandex::RequestSessionToken(const TString& code, const NActors::TActorContext& ctx) {
    NHttp::THttpOutgoingRequestPtr httpRequest = NHttp::THttpOutgoingRequest::CreateRequestPost(Settings.GetTokenEndpointURL());
    httpRequest->Set<&NHttp::THttpRequest::ContentType>("application/x-www-form-urlencoded");
    httpRequest->Set("Authorization", Settings.GetAuthorizationString());
    TStringBuilder body;
    body << "grant_type=authorization_code&code=" << code;
    httpRequest->Set<&NHttp::THttpRequest::Body>(body);
    ctx.Send(HttpProxyId, new NHttp::TEvHttpProxy::TEvHttpOutgoingRequest(httpRequest));
    Become(&THandlerSessionCreateYandex::StateWork);
}

void THandlerSessionCreateYandex::ProcessSessionToken(const TString& sessionToken, const NActors::TActorContext& ctx) {
    std::unique_ptr<NYdbGrpc::TServiceConnection<TSessionService>> connection = CreateGRpcServiceConnection<TSessionService>(Settings.SessionServiceEndpoint);

    yandex::cloud::priv::oauth::v1::CreateSessionRequest requestCreate;
    requestCreate.Setaccess_token(sessionToken);

    NMVP::TMvpTokenator* tokenator = MVPAppData()->Tokenator;
    TString token = "";
    if (tokenator) {
        token = tokenator->GetToken(Settings.SessionServiceTokenName);
    }
    NYdbGrpc::TCallMeta meta;
    SetHeader(meta, "authorization", token);
    meta.Timeout = TDuration::Seconds(10);

    NActors::TActorSystem* actorSystem = ctx.ActorSystem();
    NActors::TActorId actorId = ctx.SelfID;
    NYdbGrpc::TResponseCallback<yandex::cloud::priv::oauth::v1::CreateSessionResponse> responseCb =
        [actorId, actorSystem](NYdbGrpc::TGrpcStatus&& status, yandex::cloud::priv::oauth::v1::CreateSessionResponse&& response) -> void {
        if (status.Ok()) {
            actorSystem->Send(actorId, new TEvPrivate::TEvCreateSessionResponse(std::move(response)));
        } else {
            actorSystem->Send(actorId, new TEvPrivate::TEvErrorResponse(status));
        }
    };
    connection->DoRequest(requestCreate, std::move(responseCb), &yandex::cloud::priv::oauth::v1::SessionService::Stub::AsyncCreate, meta);
}

void THandlerSessionCreateYandex::HandleCreateSession(TEvPrivate::TEvCreateSessionResponse::TPtr event, const NActors::TActorContext& ctx) {
    LOG_DEBUG_S(ctx, NMVP::EService::MVP, "SessionService.Create(): OK");
    auto response = event->Get()->Response;
    NHttp::THeadersBuilder responseHeaders;
    for (const auto& cookie : response.Getset_cookie_header()) {
        responseHeaders.Set("Set-Cookie", ChangeSameSiteFieldInSessionCookie(cookie));
    }
    NHttp::THttpOutgoingResponsePtr httpResponse;
    responseHeaders.Set("Location", OidcSession.GetRedirectUrl());
    httpResponse = Request->CreateResponse("302", "Cookie set", responseHeaders);
    ctx.Send(Sender, new NHttp::TEvHttpProxy::TEvHttpOutgoingResponse(httpResponse));
    Die(ctx);
}

void THandlerSessionCreateYandex::HandleError(TEvPrivate::TEvErrorResponse::TPtr event, const NActors::TActorContext& ctx) {
    LOG_DEBUG_S(ctx, NMVP::EService::MVP, "SessionService.Create(): " << event->Get()->Status);
    NHttp::THttpOutgoingResponsePtr httpResponse;
    if (event->Get()->Status == "400") {
        httpResponse = GetHttpOutgoingResponsePtr({.OidcSession = OidcSession,
                                                .IncomingRequest = Request,
                                                .Settings = Settings,
                                                .NeedStoreSessionOnClientSide = true}/*Request, Settings, ResponseHeaders, IsAjaxRequest*/);
    } else {
        NHttp::THeadersBuilder responseHeaders;
        responseHeaders.Set("Content-Type", "text/plain");
        httpResponse = Request->CreateResponse( event->Get()->Status, event->Get()->Message, responseHeaders, event->Get()->Details);
    }
    ctx.Send(Sender, new NHttp::TEvHttpProxy::TEvHttpOutgoingResponse(httpResponse));
    Die(ctx);
}

TString THandlerSessionCreateYandex::ChangeSameSiteFieldInSessionCookie(const TString& cookie) {
    const static TStringBuf SameSiteParameter {"SameSite=Lax"};
    size_t n = cookie.find(SameSiteParameter);
    if (n == TString::npos) {
        return cookie;
    }
    TStringBuilder cookieBuilder;
    cookieBuilder << cookie.substr(0, n);
    cookieBuilder << "SameSite=None";
    cookieBuilder << cookie.substr(n + SameSiteParameter.size());
    return cookieBuilder;
}

} // NOIDC
