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

    NHttp::THeaders headers(Request->Headers);
    NHttp::TCookies cookies(headers.Get("cookie"));

    if (Settings.StoreSessionsOnServerSideSetting.Enable) {
        OidcSession.SetState(state);
        NActors::TActorSystem* actorSystem = ctx.ActorSystem();
        NActors::TActorId actorId = ctx.SelfID;
        NYdb::NTable::TClientSettings clientTableSettings;
        clientTableSettings.Database(Location.RootDomain)
                            .AuthToken(MVPAppData()->Tokenator->GetToken(Settings.StoreSessionsOnServerSideSetting.AccessTokenName));
        auto tableClient = Location.GetTableClient(clientTableSettings);
        tableClient.CreateSession().Subscribe([actorId, actorSystem] (const NYdb::NTable::TAsyncCreateSessionResult& result) {
            NYdb::NTable::TAsyncCreateSessionResult res(result);
            actorSystem->Send(actorId, new NMVP::THandlerActorYdb::TEvPrivate::TEvCreateSessionResult(res.ExtractValue()));
        });
    } else {
        TRestoreOidcSessionResult restoreSessionResult = RestoreSessionStoredOnClientSide(state, cookies, Settings.ClientSecret);
        OidcSession = restoreSessionResult.Session;
        if (restoreSessionResult.IsSuccess()) {
            if (Code.Empty()) {
                LOG_DEBUG_S(ctx, NMVP::EService::MVP, "Restore oidc session failed: receive empty 'code' parameter");
                NHttp::THeadersBuilder responseHeaders;
                responseHeaders.Set("Location", OidcSession.GetRedirectUrl());
                ctx.Send(Sender, new NHttp::TEvHttpProxy::TEvHttpOutgoingResponse(Request->CreateResponse("302", "Empty code", responseHeaders)));
                Die(ctx);
            } else {
                RequestSessionToken(Code, ctx);
            }
        } else {
            const auto& restoreSessionStatus = restoreSessionResult.Status;
            LOG_DEBUG_S(ctx, NMVP::EService::MVP, restoreSessionStatus.ErrorMessage);
            if (restoreSessionStatus.IsErrorRetryable) {
                NHttp::THeadersBuilder responseHeaders;
                responseHeaders.Set("Location", OidcSession.GetRedirectUrl());
                ctx.Send(Sender, new NHttp::TEvHttpProxy::TEvHttpOutgoingResponse(Request->CreateResponse("302", "Cannot restore oidc session", responseHeaders)));
                Die(ctx);
            } else {
                const static TStringBuf BAD_REQUEST_HTML_PAGE = "<html><head><title>400 Bad Request</title></head><body bgcolor=\"white\"><center><h1>go back to the page</h1></center></body></html>";
                ctx.Send(Sender, new NHttp::TEvHttpProxy::TEvHttpOutgoingResponse(Request->CreateResponseBadRequest(BAD_REQUEST_HTML_PAGE, "text/html")));
                Die(ctx);
            }
        }
    }
}

void THandlerSessionCreate::Handle(NMVP::THandlerActorYdb::TEvPrivate::TEvCreateSessionResult::TPtr event, const NActors::TActorContext& ctx) {
    const NYdb::NTable::TCreateSessionResult& result(event->Get()->Result);
    if (result.IsSuccess()) {
        DbSession = result.GetSession();
        TStringBuilder query;
        query << "DECLARE $STATE AS Text;\n";
        query << "SELECT * FROM `ydb/OidcSessions` WHERE state=$STATE AND CurrentUtcDatetime() < expire_time;\n";
        auto txControl = NYdb::NTable::TTxControl::BeginTx(NYdb::NTable::TTxSettings::OnlineRO(NYdb::NTable::TTxOnlineSettings().AllowInconsistentReads(true))).CommitTx();
        NYdb::TParamsBuilder params;
        params.AddParam("$STATE", NYdb::TValueBuilder().Utf8(OidcSession.GetState()).Build());
        auto executeDataQueryResult = DbSession->ExecuteDataQuery(query, txControl, params.Build());
        NActors::TActorSystem* actorSystem = ctx.ExecutorThread.ActorSystem;
        NActors::TActorId actorId = ctx.SelfID;
        executeDataQueryResult.Subscribe([actorId, actorSystem] (const NYdb::NTable::TAsyncDataQueryResult& result) mutable {
            NYdb::NTable::TAsyncDataQueryResult res(result);
            actorSystem->Send(actorId, new NMVP::THandlerActorYdb::TEvPrivate::TEvDataQueryResult(res.ExtractValue()));
        });
    } else {
        LOG_DEBUG_S(ctx, NMVP::EService::MVP, "Can not create session to read oidc session");
        // Обработать ошибку создания сессии
    }
}

void THandlerSessionCreate::Handle(NMVP::THandlerActorYdb::TEvPrivate::TEvDataQueryResult::TPtr event, const NActors::TActorContext& ctx) {
    NYdb::NTable::TDataQueryResult& result(event->Get()->Result);
    if (result.IsSuccess()) {
        try {
            auto resultSet = result.GetResultSet(0);
            NYdb::TResultSetParser rsParser(resultSet);
            const auto& columnsMeta = resultSet.GetColumnsMeta();
            if (rsParser.TryNextRow()) {
                for (size_t columnNum = 0; columnNum < columnsMeta.size(); ++columnNum) {
                    const NYdb::TColumn& columnMeta = columnsMeta[columnNum];
                    if (columnMeta.Name == "redirect_url") {
                        OidcSession.SetRedirectUrl(rsParser.ColumnParser(columnNum).GetOptionalUtf8().GetRef());
                    }
                    if (columnMeta.Name == "is_ajax_request") {
                        OidcSession.SetIsAjaxRequest(rsParser.ColumnParser(columnNum).GetOptionalBool().GetRef());
                    }
                }
                RequestSessionToken(Code, ctx);
            } else {
                LOG_DEBUG_S(ctx, NMVP::EService::MVP, "Read session: no data for " << OidcSession.GetState());
            }
        } catch (const std::exception& e) {
            LOG_DEBUG_S(ctx, NMVP::EService::MVP, "Read session: failed to get result:\n" << e.what());
        }
    } else {
        LOG_DEBUG_S(ctx, NMVP::EService::MVP, "Read session: failed to get result");
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

} // NOIDC
