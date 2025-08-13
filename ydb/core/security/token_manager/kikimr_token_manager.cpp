#include <ydb/library/actors/core/log.h>
#include <ydb/library/ycloud/impl/token_agent.h>

#include "kikimr_token_manager.h"
#include "token_manager_log.h"
#include "token_agent_provider.h"

namespace NKikimrYndx {

TTokenManager::TTokenManager(const NKikimr::TTokenManagerSettings& settings)
    : TBase(settings)
{}

void TTokenManager::BootstrapTokenProviders() {
    TBase::BootstrapTokenProviders();
    if (Config.HasTokenAgentProvider()) {
        const auto& tokenAgentProvider = Config.GetTokenAgentProvider();
        const auto& tokenProviderSettings = tokenAgentProvider.GetSettings();
        const auto& commonSettings = tokenProviderSettings.GetCommonSettings();
        TokenAgentProviderSettings =  {
            .SuccessRefreshPeriod = TDuration::Parse(commonSettings.GetSuccessRefreshPeriod()),
            .MinErrorRefreshPeriod = TDuration::Parse(commonSettings.GetMinErrorRefreshPeriod()),
            .MaxErrorRefreshPeriod = TDuration::Parse(commonSettings.GetMaxErrorRefreshPeriod()),
            .RequestTimeout = TDuration::Parse(commonSettings.GetRequestTimeout())
        };
        const NActors::TActorId tokenAgentId = Register(NCloud::CreateTokenAgent({.Endpoint = tokenProviderSettings.GetEndpoint(),
                                                                                        .RequestTimeoutMs = TokenAgentProviderSettings.RequestTimeout.MilliSeconds()}));
        for (const auto& tokenAgentInfo : tokenAgentProvider.GetProvidersInfo()) {
            BLOG_TRACE("Initialize token provider# " << tokenAgentInfo.GetId());
            TokenProviders[tokenAgentInfo.GetId()] = std::make_shared<TTokenAgentProvider>(this->SelfId(), TokenAgentProviderSettings, tokenAgentId, tokenAgentInfo);
        }
    }
}

NActors::IActor* CreateTokenManager(const NKikimr::TTokenManagerSettings& settings) {
    return new TTokenManager(settings);
}

} // NKikimrYndx
