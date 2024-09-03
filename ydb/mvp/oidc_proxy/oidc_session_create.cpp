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
    , OidcSession()
{}

void THandlerSessionCreate::Bootstrap(const NActors::TActorContext& ctx) {
    NHttp::TUrlParameters urlParameters(Request->URL);
    Code = urlParameters["code"];
    TString state = urlParameters["state"];

    OidcSession.SetState(state);
    if (Settings.StoreSessionsOnServerSideSetting.Enable) {
        CreateDbSession(Location, Settings.StoreSessionsOnServerSideSetting.AccessTokenName, ctx);
    } else {
        TryRestoreOidcSessionFromCookie(ctx);
    }
}

void THandlerSessionCreate::Handle(NMVP::THandlerActorYdb::TEvPrivate::TEvCreateSessionResult::TPtr event, const NActors::TActorContext& ctx) {
    const NYdb::NTable::TCreateSessionResult& result(event->Get()->Result);
    if (result.IsSuccess()) {
        DbSession = result.GetSession();
        ReadOidcSessionFromDb(ctx);
    } else {
        LOG_DEBUG_S(ctx, NMVP::EService::MVP, "Can not create session to read oidc session\n" << (NYdb::TStatus&)result);
        if (DbSession) {
            DbSession->Close();
        }
        if (CurrentNumberAttemptsCreateDbSession < MAX_ATTEMPTS_CREATE_DB_SESSION) {
            CreateDbSession(Location, Settings.StoreSessionsOnServerSideSetting.AccessTokenName, ctx); // Сделать небольшую задержку?
            ++CurrentNumberAttemptsCreateDbSession;
        } else {
            TryRestoreOidcSessionFromCookie(ctx);
            Die(ctx);
        }
    }
}

void THandlerSessionCreate::Handle(NMVP::THandlerActorYdb::TEvPrivate::TEvDataQueryResult::TPtr event, const NActors::TActorContext& ctx) {
    NYdb::NTable::TDataQueryResult& result(event->Get()->Result);
    TStringBuilder errorMessage;
    errorMessage << "Restore oidc session from DB failed: ";
    if (result.IsSuccess()) {
        DbSession->Close();
        try {
            auto resultSet = result.GetResultSet(0);
            NYdb::TResultSetParser rsParser(resultSet);
            const auto& columnsMeta = resultSet.GetColumnsMeta();
            if (rsParser.TryNextRow()) {
                TInstant expirationTime;
                for (size_t columnNum = 0; columnNum < columnsMeta.size(); ++columnNum) {
                    const NYdb::TColumn& columnMeta = columnsMeta[columnNum];
                    if (columnMeta.Name == "redirect_url") {
                        OidcSession.SetRedirectUrl(rsParser.ColumnParser(columnNum).GetOptionalUtf8().GetRef());
                    }
                    if (columnMeta.Name == "is_ajax_request") {
                        OidcSession.SetIsAjaxRequest(rsParser.ColumnParser(columnNum).GetOptionalBool().GetRef());
                    }
                    if (columnMeta.Name == "expiration_time") {
                        expirationTime = rsParser.ColumnParser(columnNum).GetDatetime();
                    }
                }
                if (TInstant::Now() < expirationTime) {
                    RequestSessionToken(Code, ctx);
                } else {
                    LOG_DEBUG_S(ctx, NMVP::EService::MVP, errorMessage << "State life time expired");
                    TryRetryRequestToProtectedResource(!OidcSession.GetRedirectUrl().Empty(), ctx);
                    Die(ctx);
                }
            } else {
                LOG_DEBUG_S(ctx, NMVP::EService::MVP, errorMessage << "Not found oidc session in DB, try restore session from cookie");
                TryRestoreOidcSessionFromCookie(ctx);
            }
        } catch (const std::exception& e) {
            LOG_DEBUG_S(ctx, NMVP::EService::MVP, errorMessage << "Failed to parse result:\n" << e.what());
            TryRetryRequestToProtectedResource(!OidcSession.GetRedirectUrl().Empty(), ctx);
            Die(ctx);
        }
    } else {
        LOG_DEBUG_S(ctx, NMVP::EService::MVP, errorMessage << "Failed to get result\n" << (NYdb::TStatus&)result);
        DbSession->Close();
        TryRestoreOidcSessionFromCookie(ctx);
        // Обработать ошибку в чтении бд
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

void THandlerSessionCreate::TryRetryRequestToProtectedResource(bool ssErrorRetryable,const NActors::TActorContext& ctx) const {
    if (ssErrorRetryable) {
        RetryRequestToProtectedResource("Cannot restore oidc session", ctx);
    } else {
        const static TStringBuf BAD_REQUEST_HTML_PAGE = "<html><head><title>400 Bad Request</title></head><body bgcolor=\"white\"><center><h1>go back to the page</h1></center></body></html>";
        ctx.Send(Sender, new NHttp::TEvHttpProxy::TEvHttpOutgoingResponse(Request->CreateResponseBadRequest(BAD_REQUEST_HTML_PAGE, "text/html")));
    }
}

void THandlerSessionCreate::RetryRequestToProtectedResource(const TString& responseMessage, const NActors::TActorContext& ctx) const {
    NHttp::THeadersBuilder responseHeaders;
    RetryRequestToProtectedResource(&responseHeaders, responseMessage, ctx);
}

void THandlerSessionCreate::RetryRequestToProtectedResource(NHttp::THeadersBuilder* responseHeaders, const TString& responseMessage, const NActors::TActorContext& ctx) const {
    responseHeaders->Set("Location", OidcSession.GetRedirectUrl());
    ctx.Send(Sender, new NHttp::TEvHttpProxy::TEvHttpOutgoingResponse(Request->CreateResponse("302", responseMessage, *responseHeaders)));
}

void THandlerSessionCreate::ReadOidcSessionFromDb(const NActors::TActorContext& ctx) {
    static const TString STATE_DECLARATION = "$STATE";
    static const TString QUERY = TStringBuilder() << "DECLARE $STATE AS Text;\n"
                                                  << "SELECT * FROM `ydb/OidcSessions`\n"
                                                  << "WHERE state=" << STATE_DECLARATION << ";\n";
    auto txControl = NYdb::NTable::TTxControl::BeginTx(NYdb::NTable::TTxSettings::OnlineRO(NYdb::NTable::TTxOnlineSettings().AllowInconsistentReads(true))).CommitTx();
    NYdb::TParamsBuilder params;
    params.AddParam(STATE_DECLARATION, NYdb::TValueBuilder().Utf8(OidcSession.GetState()).Build());
    auto executeDataQueryResult = DbSession->ExecuteDataQuery(QUERY, txControl, params.Build());
    NActors::TActorSystem* actorSystem = ctx.ExecutorThread.ActorSystem;
    NActors::TActorId actorId = ctx.SelfID;
    executeDataQueryResult.Subscribe([actorId, actorSystem] (const NYdb::NTable::TAsyncDataQueryResult& result) mutable {
        NYdb::NTable::TAsyncDataQueryResult res(result);
        actorSystem->Send(actorId, new NMVP::THandlerActorYdb::TEvPrivate::TEvDataQueryResult(res.ExtractValue()));
    });
}

void THandlerSessionCreate::TryRestoreOidcSessionFromCookie(const NActors::TActorContext& ctx) {
    NHttp::THeaders headers(Request->Headers);
    NHttp::TCookies cookies(headers.Get("cookie"));
    TRestoreOidcSessionResult restoreSessionResult = RestoreSessionStoredOnClientSide(OidcSession.GetState(), cookies, Settings.ClientSecret);
    OidcSession = restoreSessionResult.Session;
    if (restoreSessionResult.IsSuccess()) {
        if (Code.Empty()) {
            LOG_DEBUG_S(ctx, NMVP::EService::MVP, "Restore oidc session failed: receive empty 'code' parameter");
            RetryRequestToProtectedResource("Empty code", ctx);
            Die(ctx);
        } else {
            RequestSessionToken(Code, ctx);
        }
    } else {
        const auto& restoreSessionStatus = restoreSessionResult.Status;
        LOG_DEBUG_S(ctx, NMVP::EService::MVP, restoreSessionStatus.ErrorMessage);
        TryRetryRequestToProtectedResource(restoreSessionStatus.IsErrorRetryable, ctx);
        Die(ctx);
    }
}

} // NOIDC
