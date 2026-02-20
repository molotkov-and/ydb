#include "schemeshard__tenant_shred_manager.h"
#include <ydb/core/tx/schemeshard/schemeshard_impl.h>

namespace NKikimr::NSchemeShard {

namespace {

TString PrintStatus(const EShredStatus& status) {
    switch (status) {
    case EShredStatus::UNSPECIFIED:
        return "UNSPECIFIED";
    case EShredStatus::COMPLETED:
        return "COMPLITED";
    case EShredStatus::IN_PROGRESS:
        return "IN_PROGRESS";
    case EShredStatus::IN_PROGRESS_BSC:
        return "IN_PROGRESS_BSC";
    }
}

// void SendTenantShredResponse(TSchemeShard* const schemeShard, const TActorContext& ctx) {
//     std::unique_ptr<TEvSchemeShard::TEvTenantShredResponse> response(
//         new TEvSchemeShard::TEvTenantShredResponse(schemeShard->ParentDomainId, schemeShard->TenantShredManager->GetGeneration(), NKikimrScheme::TEvTenantShredResponse::COMPLETED));

//     const ui64 rootSchemeshard = schemeShard->ParentDomainId.OwnerId;
//     schemeShard->PipeClientCache->Send(
//         ctx,
//         ui64(rootSchemeshard),
//         response.release());
// }

} // namespace

TTenantShredManager::TStarter::TStarter(TTenantShredManager* const /*manager*/)
    // : Manager(manager)
{}

NOperationQueue::EStartStatus TTenantShredManager::TStarter::StartOperation(const TShardIdx& /*shardIdx*/) {
    return NOperationQueue::EStartStatus::EOperationRunning;//Manager->StartShred(shardIdx);
}

void TTenantShredManager::TStarter::OnTimeout(const TShardIdx&) {
    // Do not use
}


TTenantShredManager::TTenantShredManager(TSchemeShard* const schemeShard, const NKikimrConfig::TDataErasureConfig& config)
    : SchemeShard(schemeShard)
    , Starter(this)
    , Queue(new TQueue(ConvertConfig(config), Starter))
{
    const auto ctx = SchemeShard->ActorContext();
    ctx.RegisterWithSameMailbox(Queue);

    const auto& tenantShredConfig = config.GetTenantDataErasureConfig();
    LOG_NOTICE_S(ctx, NKikimrServices::FLAT_TX_SCHEMESHARD,
        "[TenantShredManager] Created: Rate# " << Queue->GetRate()
        << ", InflightLimit# " << tenantShredConfig.GetInflightLimit());
}

TTenantShredManager::TQueue::TConfig TTenantShredManager::ConvertConfig(const NKikimrConfig::TDataErasureConfig& config) {
    TQueue::TConfig queueConfig;
    const auto& tenantShredConfig = config.GetTenantDataErasureConfig();
    queueConfig.IsCircular = false;
    queueConfig.MaxRate = tenantShredConfig.GetMaxRate();
    queueConfig.InflightLimit = tenantShredConfig.GetInflightLimit();
    queueConfig.Timeout = TDuration::Zero(); // unlimited

    return queueConfig;
}

void TTenantShredManager::Start() {
    const auto ctx = SchemeShard->ActorContext();
    LOG_NOTICE_S(ctx, NKikimrServices::FLAT_TX_SCHEMESHARD,
        "[TenantShredManager] Start: Status# " << PrintStatus(Status));

    Queue->Start();
    // if (Status == EShredStatus::COMPLETED) {
    //     SendResponseToRootSchemeShard();
    // } else if (Status == EShredStatus::IN_PROGRESS) {
    //     ClearOperationQueue();
    //     Continue();
    // }
}

struct TSchemeShard::TTxRunTenantShred : public TSchemeShard::TRwTxBase {
    TEvSchemeShard::TEvTenantShredRequest::TPtr Ev;
    std::unique_ptr<TEvSchemeShard::TEvTenantShredResponse> Response = nullptr;

    TTxRunTenantShred(TSelf *self, TEvSchemeShard::TEvTenantShredRequest::TPtr& ev)
        : TRwTxBase(self)
        , Ev(std::move(ev))
    {}

    TTxType GetTxType() const override { return TXTYPE_RUN_SHRED_TENANT; }

    void DoExecute(TTransactionContext& txc, const TActorContext& ctx) override {
        const auto& record = Ev->Get()->Record;
        LOG_DEBUG_S(ctx, NKikimrServices::FLAT_TX_SCHEMESHARD,
            "TTxRunTenantShred: Execute at schemestard: " << Self->TabletID()
        );
        auto& shredManager = Self->TenantShredManager;
        NIceDb::TNiceDb db(txc.DB);
        if (record.GetGeneration() < shredManager->GetGeneration() ||
            record.GetGeneration() == shredManager->GetGeneration() && shredManager->GetStatus() == EShredStatus::COMPLETED) {
            LOG_DEBUG_S(ctx, NKikimrServices::FLAT_TX_SCHEMESHARD,
                "TTxRunTenantShred: Already complete" <<
                " for requested generation# " << record.GetGeneration() <<
                ", schemeshard# " << Self->TabletID() <<
                ", requested from# " << Ev->Sender
            );
            Response = std::make_unique<TEvSchemeShard::TEvTenantShredResponse>(Self->ParentDomainId, shredManager->GetGeneration(), NKikimrScheme::TEvTenantShredResponse::COMPLETED);
            return;
        }
        // Run shred
        shredManager->SetGeneration(record.GetGeneration());
        db.Table<Schema::TenantShredGenerations>().Key(shredManager->GetGeneration()).Update<Schema::TenantShredGenerations::Status>(EShredStatus::IN_PROGRESS);

        // if (Self->IsDomainSchemeShard) {
        //     LOG_WARN_S(ctx, NKikimrServices::FLAT_TX_SCHEMESHARD, "[TenantShred] [Request] Cannot run shred on root schemeshard");
        //     return;
        // }

        // NIceDb::TNiceDb db(txc.DB);
        // auto& shredManager = Self->ShredManager;
        // if (shredManager->GetGeneration() < record.GetGeneration()) {
        //     shredManager->SetGeneration(record.GetGeneration());
        //     shredManager->ClearOperationQueue();
        //     shredManager->ClearWaitingShredRequests(db);
        //     shredManager->Run(db);
        // }
        // if (Self->ShredManager->GetGeneration() == record.GetGeneration() && Self->ShredManager->GetStatus() == EShredStatus::COMPLETED) {
        //     NeedResponseComplete = true;
        // }
    }

    void DoComplete(const TActorContext& ctx) override {
        if (Response) {
            ctx.Send(Ev->Sender, std::move(Response));
        }
    }
};

NTabletFlatExecutor::ITransaction* TSchemeShard::CreateTxRunTenantShred(TEvSchemeShard::TEvTenantShredRequest::TPtr& ev) {
    return new TTxRunTenantShred(this, ev);
}

} // NKikimr::NSchemeShard
