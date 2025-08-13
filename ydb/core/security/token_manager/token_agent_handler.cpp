#include <ydb/library/ycloud/api/token_agent.h>
#include <ydb/library/actors/core/hfunc.h>
#include <ydb/library/actors/core/log.h>
#include <ydb/library/security/util.h>

#include <memory>

// #include <ydb/library/services/services.pb.h>

#include "token_manager_log.h"
#include "private_events.h"
#include "token_agent_handler.h"

namespace NKikimrYndx {

TTokenAgentProviderHandler::TTokenAgentProviderHandler(const NActors::TActorId& sender,
                                    const NActors::TActorId& tokenAgentId,
                                    const NKikimrProto::TTokenManager::TTokenAgentProvider::TTokenAgentInfo& providerInfo,
                                    const NKikimr::NTokenManager::TTokenProviderSettings& settings)
    : Sender(sender)
    , TokenAgentId(tokenAgentId)
    , ProviderInfo(providerInfo)
    , Settings(settings)
{}

void TTokenAgentProviderHandler::Bootstrap() {
    std::unique_ptr<NCloud::NEvTokenAgent::TEvGetTokenRequest> request = std::make_unique<NCloud::NEvTokenAgent::TEvGetTokenRequest>();
    request->Request.Settag(ProviderInfo.GetTag());
    BLOG_D("Send request to token agent: tag# " << ProviderInfo.GetTag());
    Send(TokenAgentId, request.release());
    TBase::Become(&TTokenAgentProviderHandler::StateWork);
}

void TTokenAgentProviderHandler::StateWork(TAutoPtr<NActors::IEventHandle>& ev) {
    switch (ev->GetTypeRewrite()) {
        hFunc(NCloud::NEvTokenAgent::TEvGetTokenResponse, Handle);
    }
}

void TTokenAgentProviderHandler::Handle(NCloud::NEvTokenAgent::TEvGetTokenResponse::TPtr& ev) {
    NCloud::NEvTokenAgent::TEvGetTokenResponse* responseEv = ev->Get();
    NKikimr::TEvTokenManager::TStatus status {.Code = NKikimr::TEvTokenManager::TStatus::ECode::SUCCESS, .Message = TString(responseEv->Status.Msg)};
    TDuration refreshPeriod = Settings.SuccessRefreshPeriod;
    TString token;
    if (!responseEv->Status.Ok()) {
        status.Code = NKikimr::TEvTokenManager::TStatus::ECode::ERROR;
        refreshPeriod = Settings.MaxErrorRefreshPeriod;
    }
    token = responseEv->Response.Getiam_token();
    BLOG_D("Handle response from token agent: token# " << (!token.empty() ? NKikimr::MaskTicket(token) : "''") << ", status message# " << status.Message);
    Send(Sender, new NKikimr::NTokenManager::TEvPrivate::TEvUpdateToken(ProviderInfo.GetId(), status, token, refreshPeriod));
    PassAway();
}

} // NKikimrYndx
