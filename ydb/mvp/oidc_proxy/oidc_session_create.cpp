#include "oidc_session_create.h"

namespace NOIDC {

THandlerSessionCreate::THandlerSessionCreate(const NActors::TActorId& sender,
                          const NHttp::THttpIncomingRequestPtr& request,
                          const NActors::TActorId& httpProxyId,
                          const TOpenIdConnectSettings& settings,
                          const TYdbLocation& location)
    : Sender(sender)
    , Request(request)
    , HttpProxyId(httpProxyId)
    , Settings(settings)
    , Location(location)
    , OidcSession(settings)
{}

void THandlerSessionCreate::Bootstrap(const NActors::TActorContext& ctx) {
    NHttp::TUrlParameters urlParameters(Request->URL);
    TString code = urlParameters["code"];
    TString state = urlParameters["state"];

    NHttp::THeaders headers(Request->Headers);
    NHttp::TCookies cookies(headers.Get("cookie"));

    // TString error;
    // if (Settings.StoreSessionsOnServerSideSetting.Enable) {
    //     error = OidcSession.CheckSessionStoredOnServerSide(state);
    // } else {
    //     RestoreSessionStoredOnClientSide(state, cookies, Settings.ClientSecret);
    // }
    TRestoreOidcSessionResult restoreSessionResult = RestoreSessionStoredOnClientSide(state, cookies, Settings.ClientSecret);
    if (restoreSessionResult.IsSuccess()/*IsStateValid(state, cookies, ctx)*/ && !code.Empty()) {
        RequestSessionToken(code, ctx);
    } else {
        NHttp::THttpOutgoingResponsePtr response = GetHttpOutgoingResponsePtr({.OidcSession = NOIDC::TOidcSession(Request, Settings),
                                                                                .IncomingRequest = Request,
                                                                                .Settings = Settings,
                                                                                .NeedStoreSessionOnClientSide = true}/*Request, Settings, ResponseHeaders, IsAjaxRequest*/);
        ctx.Send(Sender, new NHttp::TEvHttpProxy::TEvHttpOutgoingResponse(response));
        TBase::Die(ctx);
        return;
    }
}

void THandlerSessionCreate::Handle(NHttp::TEvHttpProxy::TEvHttpIncomingResponse::TPtr event, const NActors::TActorContext& ctx) {
    NHttp::THttpOutgoingResponsePtr httpResponse;
    NHttp::THeadersBuilder responseHeaders;
    if (event->Get()->Error.empty() && event->Get()->Response) {
        NHttp::THttpIncomingResponsePtr response = event->Get()->Response;
        LOG_DEBUG_S(ctx, NMVP::EService::MVP, "Incoming response from authorization server: " << response->Status);
        if (response->Status == "200") {
            TStringBuf jsonError;
            NJson::TJsonValue jsonValue;
            NJson::TJsonReaderConfig jsonConfig;
            if (NJson::ReadJsonTree(response->Body, &jsonConfig, &jsonValue)) {
                const NJson::TJsonValue* jsonAccessToken;
                if (jsonValue.GetValuePointer("access_token", &jsonAccessToken)) {
                    TString sessionToken = jsonAccessToken->GetStringRobust();
                    ProcessSessionToken(sessionToken, ctx);
                    return;
                } else {
                    jsonError = "Wrong OIDC provider response: access_token not found";
                }
            } else {
                jsonError =  "Wrong OIDC response";
            }
            responseHeaders.Set("Content-Type", "text/plain");
            httpResponse = Request->CreateResponse("400", "Bad Request", responseHeaders, jsonError);
        } else {
            responseHeaders.Parse(response->Headers);
            httpResponse = Request->CreateResponse(response->Status, response->Message, responseHeaders, response->Body);
        }
    } else {
        responseHeaders.Set("Content-Type", "text/plain");
        httpResponse = Request->CreateResponse("400", "Bad Request", responseHeaders, event->Get()->Error);
    }
    ctx.Send(Sender, new NHttp::TEvHttpProxy::TEvHttpOutgoingResponse(httpResponse));
    Die(ctx);
}

} // NOIDC
