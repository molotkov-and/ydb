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
    , DbSession()
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
            NHttp::THttpOutgoingResponsePtr httpResponse = GetHttpOutgoingResponsePtr({.OidcSession = OidcSession,
                                                                                .IncomingRequest = Request,
                                                                                .Settings = Settings,
                                                                                .NeedStoreSessionOnClientSide = true});
            ctx.Send(Sender, new NHttp::TEvHttpProxy::TEvHttpOutgoingResponse(httpResponse));
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
        TStringBuilder query;
        query << "DECLARE $STATE AS Text;\n"
                    "DECLARE $REDIRECT_URL AS Text;\n"
                    "DECLARE $IS_AJAX_REQUEST AS Bool;\n";
        query << "INSERT INTO `ydb/OidcSessions` (state, redirect_url, expiration_time, is_ajax_request)\n";
        query << "VALUES ($STATE, $REDIRECT_URL, CurrentUtcDateTime() + Interval('PT" << TOidcSession::STATE_LIFE_TIME.Seconds() << "S'), $IS_AJAX_REQUEST);\n";
        auto txControl = NYdb::NTable::TTxControl::BeginTx(NYdb::NTable::TTxSettings::SerializableRW()).CommitTx();
        NYdb::TParamsBuilder params;
        params.AddParam("$STATE", NYdb::TValueBuilder().Utf8(OidcSession.GetState()).Build());
        params.AddParam("$REDIRECT_URL", NYdb::TValueBuilder().Utf8(OidcSession.GetRedirectUrl()).Build());
        params.AddParam("$IS_AJAX_REQUEST", NYdb::TValueBuilder().Bool(OidcSession.GetIsAjaxRequest()).Build());
        auto executeDataQueryResult = DbSession->ExecuteDataQuery(query, txControl, params.Build());
        NActors::TActorSystem* actorSystem = ctx.ExecutorThread.ActorSystem;
        NActors::TActorId actorId = ctx.SelfID;
        executeDataQueryResult.Subscribe([actorId, actorSystem] (const NYdb::NTable::TAsyncDataQueryResult& result) mutable {
            NYdb::NTable::TAsyncDataQueryResult res(result);
            actorSystem->Send(actorId, new NMVP::THandlerActorYdb::TEvPrivate::TEvDataQueryResult(res.ExtractValue()));
        });
    } else {
        // Обработать не создавшуюся сессию. Возможно попробовать снова создать
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

} // NOIDC
