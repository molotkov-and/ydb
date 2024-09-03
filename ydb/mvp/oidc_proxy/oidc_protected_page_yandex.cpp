#include <ydb/public/sdk/cpp/client/ydb_table/table.h>
#include <ydb/public/sdk/cpp/client/ydb_params/params.h>
#include <ydb/public/sdk/cpp/client/ydb_types/status/status.h>
#include <ydb/mvp/core/core_ydb_impl.h>
#include "oidc_protected_page_yandex.h"
#include "oidc_session.h"

namespace NYdb {
class TStatus;
}

namespace NOIDC {

THandlerSessionServiceCheckYandex::THandlerSessionServiceCheckYandex(const NActors::TActorId& sender,
                                const NHttp::THttpIncomingRequestPtr& request,
                                const NActors::TActorId& httpProxyId,
                                const TOpenIdConnectSettings& settings,
                                const TYdbLocation& location)
    : THandlerSessionServiceCheck(sender, request, httpProxyId, settings, location)
    // , DbSession()
    , OidcSession(request)
{}

void THandlerSessionServiceCheckYandex::Bootstrap(const NActors::TActorContext& ctx) {
    THandlerSessionServiceCheck::Bootstrap(ctx);
    Become(&THandlerSessionServiceCheckYandex::StateWork);
}

void THandlerSessionServiceCheckYandex::Handle(TEvPrivate::TEvCheckSessionResponse::TPtr event, const NActors::TActorContext& ctx) {
    LOG_DEBUG_S(ctx, NMVP::EService::MVP, "SessionService.Check(): OK");
    auto response = event->Get()->Response;
    const auto& iamToken = response.iam_token();
    const TString authHeader = IAM_TOKEN_SCHEME + iamToken.iam_token();
    ForwardUserRequest(authHeader, ctx);
}

void THandlerSessionServiceCheckYandex::Handle(TEvPrivate::TEvErrorResponse::TPtr event, const NActors::TActorContext& ctx) {
    LOG_DEBUG_S(ctx, NMVP::EService::MVP, "SessionService.Check(): " << event->Get()->Status);
    if (event->Get()->Status == "400") {
        if (Settings.StoreSessionsOnServerSideSetting.Enable) {
            CreateDbSession(ctx);
        } else {
            SaveOidcSessionOnClientSide(ctx);
            Die(ctx);
        }
    } else {
        NHttp::THttpOutgoingResponsePtr httpResponse = Request->CreateResponse( event->Get()->Status, event->Get()->Message, "text/plain", event->Get()->Details);
        ctx.Send(Sender, new NHttp::TEvHttpProxy::TEvHttpOutgoingResponse(httpResponse));
        Die(ctx);
    }
}

void THandlerSessionServiceCheckYandex::Handle(NMVP::THandlerActorYdb::TEvPrivate::TEvCreateSessionResult::TPtr event, const NActors::TActorContext& ctx) {
    const NYdb::NTable::TCreateSessionResult& result(event->Get()->Result);
    if (result.IsSuccess()) {
        DbSession = result.GetSession();
        SendRequestToWriteOidcSessionInDB(ctx);
    } else {
        LOG_DEBUG_S(ctx, NMVP::EService::MVP, "Can not create db session to write oidc session\n" << (NYdb::TStatus&)result);
        if (DbSession) {
            DbSession->Close();
        }
        if (CurrentNumberAttemptsCreateDbSession < MAX_ATTEMPTS_CREATE_DB_SESSION) {
            CreateDbSession(ctx); // Сделать небольшую задержку?
            ++CurrentNumberAttemptsCreateDbSession;
        } else {
            SaveOidcSessionOnClientSide(ctx);
            Die(ctx);
        }
    }
}

void THandlerSessionServiceCheckYandex::Handle(NMVP::THandlerActorYdb::TEvPrivate::TEvDataQueryResult::TPtr event, const NActors::TActorContext& ctx) {
    NYdb::NTable::TDataQueryResult& result(event->Get()->Result);
    if (result.IsSuccess()) {
        DbSession->Close();
        NHttp::THttpOutgoingResponsePtr httpResponse = GetHttpOutgoingResponsePtr({.OidcSession = OidcSession,
                                                                                .IncomingRequest = Request,
                                                                                .Settings = Settings,
                                                                                .NeedStoreSessionOnClientSide = false});
        ctx.Send(Sender, new NHttp::TEvHttpProxy::TEvHttpOutgoingResponse(httpResponse));
        Die(ctx);
    } else {
        LOG_DEBUG_S(ctx, NMVP::EService::MVP, "Write session to db is failed\n" << (NYdb::TStatus&)result);
        if (result.GetStatus() == NYdb::EStatus::PRECONDITION_FAILED && CurrentNumberAttemptsWriteOidcSessionToDb < MAX_ATTEMPTS_WRITE_OIDC_SESSION_TO_DB) {
            OidcSession = TOidcSession(Request);
            SendRequestToWriteOidcSessionInDB(ctx);
            ++CurrentNumberAttemptsWriteOidcSessionToDb;
        } else {
            if (DbSession) {
                DbSession->Close();
            }
            SaveOidcSessionOnClientSide(ctx);
            Die(ctx);
        }
        // Обработать ошибку записи в базу. Сделать повторную попытку
    }
}

void THandlerSessionServiceCheckYandex::StartOidcProcess(const NActors::TActorContext& ctx) {
    NHttp::THeaders headers(Request->Headers);
    TStringBuf cookie = headers.Get("cookie");
    yandex::cloud::priv::oauth::v1::CheckSessionRequest request;
    request.Setcookie_header(TString(cookie));

    std::unique_ptr<NYdbGrpc::TServiceConnection<TSessionService>> connection = CreateGRpcServiceConnection<TSessionService>(Settings.SessionServiceEndpoint);

    NActors::TActorSystem* actorSystem = ctx.ActorSystem();
    NActors::TActorId actorId = ctx.SelfID;
    NYdbGrpc::TResponseCallback<yandex::cloud::priv::oauth::v1::CheckSessionResponse> responseCb =
        [actorId, actorSystem](NYdbGrpc::TGrpcStatus&& status, yandex::cloud::priv::oauth::v1::CheckSessionResponse&& response) -> void {
        if (status.Ok()) {
            actorSystem->Send(actorId, new TEvPrivate::TEvCheckSessionResponse(std::move(response)));
        } else {
            actorSystem->Send(actorId, new TEvPrivate::TEvErrorResponse(status));
        }
    };

    NMVP::TMvpTokenator* tokenator = MVPAppData()->Tokenator;
    TString token = "";
    if (tokenator) {
        token = tokenator->GetToken(Settings.SessionServiceTokenName);
    }
    NYdbGrpc::TCallMeta meta;
    SetHeader(meta, "authorization", token);
    meta.Timeout = TDuration::Seconds(10);
    connection->DoRequest(request, std::move(responseCb), &yandex::cloud::priv::oauth::v1::SessionService::Stub::AsyncCheck, meta);
}

bool THandlerSessionServiceCheckYandex::NeedSendSecureHttpRequest(const NHttp::THttpIncomingResponsePtr& response) const {
    if ((response->Status == "400" || response->Status.empty()) && RequestedPageScheme.empty()) {
        NHttp::THttpOutgoingRequestPtr request = response->GetRequest();
        if (!request->Secure) {
            static const TStringBuf bodyContent = "The plain HTTP request was sent to HTTPS port";
            NHttp::THeadersBuilder headers(response->Headers);
            TStringBuf contentType = headers.Get("Content-Type").NextTok(';');
            TStringBuf body = response->Body;
            return contentType == "text/html" && body.find(bodyContent) != TStringBuf::npos;
        }
    }
    return false;
}

void THandlerSessionServiceCheckYandex::SaveOidcSessionOnClientSide(const NActors::TActorContext& ctx) const {
    NHttp::THttpOutgoingResponsePtr httpResponse = GetHttpOutgoingResponsePtr({.OidcSession = OidcSession,
                                                                                .IncomingRequest = Request,
                                                                                .Settings = Settings,
                                                                                .NeedStoreSessionOnClientSide = true});
    ctx.Send(Sender, new NHttp::TEvHttpProxy::TEvHttpOutgoingResponse(httpResponse));
}

void THandlerSessionServiceCheckYandex::CreateDbSession(const NActors::TActorContext& ctx) const {
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
}

void THandlerSessionServiceCheckYandex::SendRequestToWriteOidcSessionInDB(const NActors::TActorContext& ctx) {
    static const TString STATE_DECLARATION = "$STATE";
    static const TString REDIRECT_URL_DECLARATION = "$REDIRECT_URL";
    static const TString IS_AJAX_REQUEST_DECLARATION = "$IS_AJAX_REQUEST";
    static const TString QUERY = TStringBuilder() << "DECLARE $STATE AS Text;\n"
                                    << "DECLARE $REDIRECT_URL AS Text;\n"
                                    << "DECLARE $IS_AJAX_REQUEST AS Bool;\n"
                                    << "INSERT INTO `ydb/OidcSessions` (state, redirect_url, expiration_time, is_ajax_request)\n"
                                    << "VALUES ("
                                            << STATE_DECLARATION << ", "
                                            << REDIRECT_URL_DECLARATION << ", "
                                            << "CurrentUtcDateTime() + Interval('PT" << TOidcSession::STATE_LIFE_TIME.Seconds() << "S'), "
                                            << IS_AJAX_REQUEST_DECLARATION
                                    << ");\n";
    auto txControl = NYdb::NTable::TTxControl::BeginTx(NYdb::NTable::TTxSettings::SerializableRW()).CommitTx();
    NYdb::TParamsBuilder params;
    params.AddParam(STATE_DECLARATION, NYdb::TValueBuilder().Utf8(OidcSession.GetState()).Build());
    params.AddParam(REDIRECT_URL_DECLARATION, NYdb::TValueBuilder().Utf8(OidcSession.GetRedirectUrl()).Build());
    params.AddParam(IS_AJAX_REQUEST_DECLARATION, NYdb::TValueBuilder().Bool(OidcSession.GetIsAjaxRequest()).Build());
    auto executeDataQueryResult = DbSession->ExecuteDataQuery(QUERY, txControl, params.Build());
    NActors::TActorSystem* actorSystem = ctx.ExecutorThread.ActorSystem;
    NActors::TActorId actorId = ctx.SelfID;
    executeDataQueryResult.Subscribe([actorId, actorSystem] (const NYdb::NTable::TAsyncDataQueryResult& result) mutable {
        NYdb::NTable::TAsyncDataQueryResult res(result);
        actorSystem->Send(actorId, new NMVP::THandlerActorYdb::TEvPrivate::TEvDataQueryResult(res.ExtractValue()));
    });
}

} // NOIDC
