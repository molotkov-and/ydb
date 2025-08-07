#include "service_token_manager.h"
#include "service_token_manager_log.h"
#include "private_events.h"
#include "token_provider_settings.h"
#include "vm_metadata_token_provider_handler.h"

#include <util/datetime/base.h>
#include <util/generic/hash.h>
#include <util/generic/queue.h>

#include <memory>

#include <ydb/library/actors/core/actor_bootstrapped.h>
#include <ydb/library/actors/core/hfunc.h>
#include <ydb/core/protos/auth.pb.h>

namespace NKikimr {

class TServiceTokenManager : public NActors::TActorBootstrapped<TServiceTokenManager> {
    using TBase = NActors::TActorBootstrapped<TServiceTokenManager>;

public:
    struct TInitializer {
        NKikimrProto::TServiceTokenManager Config;
        NActors::TActorId HttpProxyId;
    };

protected:
    struct TTokenProvider;

private:
    struct TRefreshRecord {
        TInstant RefreshTime;
        std::shared_ptr<TTokenProvider> Provider;

        bool operator <(const TRefreshRecord& other) const {
            return RefreshTime > other.RefreshTime;
        }
    };

    struct TVmMetadataTokenProvider;

private:
    NKikimrProto::TServiceTokenManager Config;
    TDuration RefreshCheckPeriod = TDuration::Seconds(30);
    THashMap<TString, std::shared_ptr<TTokenProvider>> TokenProviders;
    NServiceTokenManager::TTokenProviderSettings VmMetadataProviderSettings;
    TPriorityQueue<TRefreshRecord> RefreshQueue;
    NActors::TActorId HttpProxyId;
    THashMap<TString, THashSet<NActors::TActorId>> Subscribers;

public:
    TServiceTokenManager(const TInitializer& initializer);
    TServiceTokenManager(const NKikimrProto::TServiceTokenManager& config);

    void Bootstrap();
    void StateWork(TAutoPtr<NActors::IEventHandle>& ev);
    void BootstrapVmMetadataProvider();

protected:
    void PassAway() override;

private:
    void HandleRefreshCheck();
    void RefreshAllTokens();
    void NotifySubscribers(const TString& id, const std::shared_ptr<TTokenProvider>& provider) const;

    void Handle(NServiceTokenManager::TEvPrivate::TEvUpdateToken::TPtr& ev);
    void Handle(TEvServiceTokenManager::TEvSubscribeUpdateToken::TPtr& ev);
};

struct TServiceTokenManager::TTokenProvider {
    TServiceTokenManager* Manager = nullptr;
    TString Token;
    TInstant RefreshTime;
    const NServiceTokenManager::TTokenProviderSettings& Settings;
    TEvServiceTokenManager::TStatus Status {
        .Code = TEvServiceTokenManager::TStatus::ECode::NOT_READY,
        .Message = "Token is not ready"
    };

    TTokenProvider(TServiceTokenManager* const manager, const NServiceTokenManager::TTokenProviderSettings& settings);
    virtual ~TTokenProvider() = default;
    virtual void CreateToken() = 0;
    TString GetToken() const;
    void UpdateToken(const TString& token, const TDuration& refreshPeriod);
    void SetError(const TEvServiceTokenManager::TStatus& status, const TDuration& refreshPeriod);
    TInstant GetRefreshTime() const;
    bool IsReady() const;
    TEvServiceTokenManager::TStatus GetStatus() const;
};

struct TServiceTokenManager::TVmMetadataTokenProvider : public TServiceTokenManager::TTokenProvider {
    using Base = TTokenProvider;

    const NActors::TActorId HttpProxyId;
    const NKikimrProto::TServiceTokenManager::TVmMetadataProvider::TVmMetadataInfo& ProviderInfo;

    TVmMetadataTokenProvider(TServiceTokenManager* const manager,
                                const NServiceTokenManager::TTokenProviderSettings& settings,
                                const NActors::TActorId httpProxyId,
                                const NKikimrProto::TServiceTokenManager::TVmMetadataProvider::TVmMetadataInfo& providerInfo);

    void CreateToken() override;
};

////////////////////////////////  IMPLEMENTATION  ////////////////////////////////

TServiceTokenManager::TServiceTokenManager(const TInitializer& initializer)
    : Config(initializer.Config)
    , HttpProxyId(initializer.HttpProxyId)
{}

TServiceTokenManager::TServiceTokenManager(const NKikimrProto::TServiceTokenManager& config)
    : Config(config)
{
    HttpProxyId = Register(NHttp::CreateHttpProxy());
}

void TServiceTokenManager::Bootstrap() {
    RefreshCheckPeriod = TDuration::Parse(Config.GetRefreshCheckPeriod());
    BootstrapVmMetadataProvider();
    RefreshAllTokens();
    Schedule(RefreshCheckPeriod, new NActors::TEvents::TEvWakeup());
    TBase::Become(&TServiceTokenManager::StateWork);
}

void TServiceTokenManager::BootstrapVmMetadataProvider() {
    if (Config.HasVmMetadataProvider()) {
        const auto& vmMetadataProvider = Config.GetVmMetadataProvider();
        const auto& tokenProviderSettings = vmMetadataProvider.GetTokenProviderSettings();
        VmMetadataProviderSettings =  {
            .SuccessRefreshPeriod = TDuration::Parse(tokenProviderSettings.GetSuccessRefreshPeriod()),
            .ErrorRefreshPeriod = TDuration::Parse(tokenProviderSettings.GetMaxErrorRefreshPeriod())
        };
        for (const auto& vmMetadataInfo : vmMetadataProvider.GetVmMetadataInfo()) {
            TokenProviders[vmMetadataInfo.GetId()] = std::make_shared<TVmMetadataTokenProvider>(this, VmMetadataProviderSettings, HttpProxyId, vmMetadataInfo);
        }
    }
}

void TServiceTokenManager::StateWork(TAutoPtr<NActors::IEventHandle>& ev) {
    switch (ev->GetTypeRewrite()) {
        hFunc(NServiceTokenManager::TEvPrivate::TEvUpdateToken, Handle);
        hFunc(TEvServiceTokenManager::TEvSubscribeUpdateToken, Handle);
        cFunc(NActors::TEvents::TSystem::Wakeup, HandleRefreshCheck);
        cFunc(NActors::TEvents::TSystem::PoisonPill, PassAway);
    }
}

void TServiceTokenManager::HandleRefreshCheck() {
    BLOG_TRACE("+++: Handle HandleRefreshCheck");
    while (!RefreshQueue.empty() && RefreshQueue.top().RefreshTime <= NActors::TlsActivationContext->Now()) {
        std::shared_ptr<TTokenProvider> provider = RefreshQueue.top().Provider;
        RefreshQueue.pop();
        BLOG_TRACE("+++: Handle HandleRefreshCheck.CreateToken");
        provider->CreateToken();
    }
    Schedule(RefreshCheckPeriod, new NActors::TEvents::TEvWakeup());
}

void TServiceTokenManager::PassAway() {
    TBase::PassAway();
}

void TServiceTokenManager::RefreshAllTokens() {
    for (const auto& tokenProvider : TokenProviders) {
        tokenProvider.second->CreateToken();
    }
}

void TServiceTokenManager::NotifySubscribers(const TString& id, const std::shared_ptr<TTokenProvider>& provider) const {
    BLOG_TRACE("+++: Handle NotifySubscribers");
    auto it = Subscribers.find(id);
    if (it != Subscribers.end()) {
        for (const auto& subscriber : it->second) {
            BLOG_TRACE("+++: Handle NotifySubscribers: Notify: Token# " << provider->GetToken());
            Send(subscriber, new TEvServiceTokenManager::TEvUpdateToken(id, provider->GetToken(), provider->GetStatus()));
        }
    } else {
        BLOG_TRACE("+++: Handle NotifySubscribers: Can not find subscribers");
    }
}

void TServiceTokenManager::Handle(NServiceTokenManager::TEvPrivate::TEvUpdateToken::TPtr& ev) {
    BLOG_TRACE("Handle TEvPrivate::TEvUpdateToken");
    const TString& tokenProviderId = ev->Get()->Id;
    auto it = TokenProviders.find(tokenProviderId);
    if (it != TokenProviders.end()) {
        if (ev->Get()->Status.Code == TEvServiceTokenManager::TStatus::ECode::SUCCESS) {
            it->second->UpdateToken(ev->Get()->Token, ev->Get()->RefreshPeriod);
        } else {
            it->second->SetError(ev->Get()->Status, ev->Get()->RefreshPeriod);
        }
        BLOG_TRACE("+++: Handle TEvPrivate::TEvUpdateToken: Token# " << ev->Get()->Token);
        RefreshQueue.push({.RefreshTime = it->second->GetRefreshTime(), .Provider = it->second});
        NotifySubscribers(tokenProviderId, it->second);
    } else {
        BLOG_TRACE("+++: Handle TEvPrivate::TEvUpdateToken: Can not find token provider");
    }
}

void TServiceTokenManager::Handle(TEvServiceTokenManager::TEvSubscribeUpdateToken::TPtr& ev) {
    BLOG_TRACE("+++: Handle TEvServiceTokenManager::TEvSubscribeUpdateToken");
    TString id = ev->Get()->Id;
    BLOG_TRACE("+++: Handle TEvServiceTokenManager::TEvSubscribeUpdateToken: Try response immediately");
    auto it = TokenProviders.find(id);
    if (it != TokenProviders.end()) {
        Subscribers[id].insert(ev->Sender);
        Send(ev->Sender, new TEvServiceTokenManager::TEvUpdateToken(id, it->second->GetToken(), it->second->GetStatus()));
    } else {
        // попытаться получить токен. Или отвечать с ошибкой, что такого провайдера не существует.
        BLOG_TRACE("+++: Handle TEvServiceTokenManager::TEvSubscribeUpdateToken: there is no token");
    }
}

TServiceTokenManager::TTokenProvider::TTokenProvider(TServiceTokenManager* const manager, const NServiceTokenManager::TTokenProviderSettings& settings)
    : Manager(manager)
    , Settings(settings)
{}

TString TServiceTokenManager::TTokenProvider::GetToken() const {
    return Token;
}

void TServiceTokenManager::TTokenProvider::UpdateToken(const TString& token, const TDuration& refreshPeriod) {
    Token = token;
    RefreshTime = NActors::TlsActivationContext->Now() + Min(refreshPeriod, Settings.SuccessRefreshPeriod);
    Status = {.Code = TEvServiceTokenManager::TStatus::ECode::SUCCESS, .Message = "OK"};
}

void TServiceTokenManager::TTokenProvider::SetError(const TEvServiceTokenManager::TStatus& status, const TDuration& refreshPeriod) {
    Status = status;
    // need exponential back off
    RefreshTime = NActors::TlsActivationContext->Now() + Min(refreshPeriod, Settings.ErrorRefreshPeriod);
}

TInstant TServiceTokenManager::TTokenProvider::GetRefreshTime() const {
    return RefreshTime;
}

bool TServiceTokenManager::TTokenProvider::IsReady() const {
    return Status.Code == TEvServiceTokenManager::TStatus::ECode::SUCCESS;
}

TEvServiceTokenManager::TStatus TServiceTokenManager::TTokenProvider::GetStatus() const {
    return Status;
}

TServiceTokenManager::TVmMetadataTokenProvider::TVmMetadataTokenProvider(TServiceTokenManager* const manager,
                                 const NServiceTokenManager::TTokenProviderSettings& settings,
                                 const NActors::TActorId httpProxyId,
                                 const NKikimrProto::TServiceTokenManager::TVmMetadataProvider::TVmMetadataInfo& providerInfo)
    : Base(manager, settings)
    , HttpProxyId(httpProxyId)
    , ProviderInfo(providerInfo)
{}

void TServiceTokenManager::TVmMetadataTokenProvider::CreateToken() {
    Manager->Register(new NServiceTokenManager::TVmMetadataTokenProviderHandler(Manager->SelfId(), HttpProxyId, ProviderInfo, Settings));
}

NActors::IActor* CreateServiceTokenManager(const NKikimrProto::TServiceTokenManager& config) {
    return new TServiceTokenManager(config);
}

NActors::IActor* CreateServiceTokenManager(const NKikimrProto::TServiceTokenManager& config, const NActors::TActorId& HttpProxyId) {
    return new TServiceTokenManager({.Config = config, .HttpProxyId = HttpProxyId});
}

} // NKikimr
