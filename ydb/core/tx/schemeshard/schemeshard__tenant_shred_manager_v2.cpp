#include "schemeshard__tenant_shred_manager.h"
#include <ydb/core/keyvalue/keyvalue_events.h>
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

TTenantShredManager::TStarter::TStarter(TTenantShredManager* const manager)
    : Manager(manager)
{}

NOperationQueue::EStartStatus TTenantShredManager::TStarter::StartOperation(const TShardIdx& shardIdx) {
    return Manager->StartShredOperation(shardIdx);
}

NOperationQueue::EStartStatus TTenantShredManager::StartShredOperation(const TShardIdx& shardIdx) {
    // UpdateMetrics();

    auto ctx = SchemeShard->ActorContext();
    auto it = SchemeShard->ShardInfos.find(shardIdx);
    if (it == SchemeShard->ShardInfos.end()) {
        LOG_WARN_S(ctx, NKikimrServices::FLAT_TX_SCHEMESHARD, "[TenantShredManager] [Start] Failed to resolve shard info"
            " for shred# " << shardIdx <<
            " at schemeshard# " << SchemeShard->TabletID());
        return NOperationQueue::EStartStatus::EOperationRemove;
    }

    const auto& tabletId = it->second.TabletID;
    const auto& pathId = it->second.PathId;
    LOG_INFO_S(ctx, NKikimrServices::FLAT_TX_SCHEMESHARD, "[TenantShredManager] [Start] Shred"
        " for pathId# " << pathId << ", tabletId# " << tabletId
        << " at schemeshard " << SchemeShard->TabletID());

    std::unique_ptr<IEventBase> request = nullptr;
    switch (it->second.TabletType) {
    case NKikimr::NSchemeShard::ETabletType::DataShard: {
        request.reset(new TEvDataShard::TEvVacuum(Generation));
        break;
    }
    case NKikimr::NSchemeShard::ETabletType::PersQueue: {
        request.reset(new TEvKeyValue::TEvVacuumRequest(Generation));
        break;
    }
    default:
        return NOperationQueue::EStartStatus::EOperationRemove;
    }

    ActivePipes[shardIdx] = SchemeShard->PipeClientCache->Send(ctx, ui64(tabletId), request.release());
    return NOperationQueue::EStartStatus::EOperationRunning;
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

void TTenantShredManager::StartShred(NIceDb::TNiceDb& db, ui64 newGen) {
    const auto& ctx = SchemeShard->ActorContext();
    SetGeneration(newGen);
    Queue->Clear();
    ActivePipes.clear();
    for (const auto& [shardIdx, status] : WaitingShredShards) {
        db.Table<Schema::WaitingShredShards>().Key(shardIdx.GetOwnerId(), shardIdx.GetLocalId()).Delete();
    }
    WaitingShredShards.clear();
    Status = EShredStatus::IN_PROGRESS;
    for (const auto& [shardIdx, shardInfo] : SchemeShard->ShardInfos) {
        switch (shardInfo.TabletType) {
        case NKikimr::NSchemeShard::ETabletType::DataShard:
        case NKikimr::NSchemeShard::ETabletType::PersQueue: {
            if (Queue->Enqueue(shardIdx)) {
                WaitingShredShards[shardIdx] = EShredStatus::IN_PROGRESS;
                db.Table<Schema::WaitingShredShards>().Key(shardIdx.GetOwnerId(), shardIdx.GetLocalId()).Update<Schema::WaitingShredShards::Status>(WaitingShredShards[shardIdx]);
                LOG_TRACE_S(ctx, NKikimrServices::FLAT_TX_SCHEMESHARD,
                    "[TenantShredManager] [Enqueue] Enqueued shard# " << shardIdx << " at schemeshard " << SchemeShard->TabletID());
            } else {
                LOG_TRACE_S(ctx, NKikimrServices::FLAT_TX_SCHEMESHARD,
                    "[TenantShredManager] [Enqueue] Skipped or already exists shard# " << shardIdx << " at schemeshard " << SchemeShard->TabletID());
            }
            break;
        }
        default:
            break;
        }
    }
    if (WaitingShredShards.empty()) {
        Status = EShredStatus::COMPLETED;
    }
    db.Table<Schema::TenantShredGenerations>().Key(Generation).Update<Schema::TenantShredGenerations::Status>(Status);

    LOG_NOTICE_S(ctx, NKikimrServices::FLAT_TX_SCHEMESHARD,
        "[TenantShredManager] StartShred: "
        "WaitingShredShards.size# " << WaitingShredShards.size() <<
        ", Status# " << PrintStatus(Status));
}

void TTenantShredManager::FinishShred(NIceDb::TNiceDb& db, const TTabletId& tabletId) {
    const TShardIdx shardIdx = SchemeShard->GetShardIdx(tabletId);
    auto duration = Queue->OnDone(shardIdx);
    auto ctx = SchemeShard->ActorContext();
    if (shardIdx == InvalidShardIdx) {
        LOG_WARN_S(ctx, NKikimrServices::FLAT_TX_SCHEMESHARD, "[TenantShredManager] [Finished] Failed to resolve shard info "
            "for tabletId# " << tabletId
            << " in# " << duration.MilliSeconds() << " ms"
            << ", next wakeup in# " << Queue->GetWakeupDelta()
            << ", rate# " << Queue->GetRate()
            << ", in queue# " << Queue->Size() << " shards"
            << ", running# " << Queue->RunningSize() << " shards"
            << " at schemeshard " << SchemeShard->TabletID());
    } else {
        LOG_INFO_S(ctx, NKikimrServices::FLAT_TX_SCHEMESHARD, "[TenantShredManager] [Finished] Shred is completed "
            "for tabletId# " << tabletId
            << ", shardIdx# " << shardIdx
            << " in# " << duration.MilliSeconds() << " ms"
            << ", next wakeup in# " << Queue->GetWakeupDelta()
            << ", rate# " << Queue->GetRate()
            << ", in queue# " << Queue->Size() << " shards"
            << ", running# " << Queue->RunningSize() << " shards"
            << " at schemeshard " << SchemeShard->TabletID());
    }

    ActivePipes.erase(shardIdx);
    auto waitingShardIt = WaitingShredShards.find(shardIdx);
    if (waitingShardIt != WaitingShredShards.end()) {
        WaitingShredShards.erase(waitingShardIt);
        db.Table<Schema::WaitingShredShards>().Key(shardIdx.GetOwnerId(), shardIdx.GetLocalId()).Delete();
    }
    if (WaitingShredShards.empty()) {
        LOG_NOTICE_S(ctx, NKikimrServices::FLAT_TX_SCHEMESHARD,
            "[TenantShredManager] Shred in shards is completed. Send response to domain schemeshard");
        Queue->Clear();
        ActivePipes.clear();
        Status = EShredStatus::COMPLETED;
        db.Table<Schema::TenantShredGenerations>().Key(Generation).Update<Schema::TenantShredGenerations::Status>(Status);
    }
}

void TTenantShredManager::HandleDisconnect(TTabletId tabletId, const TActorId& clientId, const TActorContext& ctx) {
    LOG_DEBUG_S(ctx, NKikimrServices::FLAT_TX_SCHEMESHARD, "3333333333333333333333333333333333333333333333333333333333");
    if (tabletId == TTabletId(SchemeShard->ParentDomainId.OwnerId)) {
        LOG_DEBUG_S(ctx, NKikimrServices::FLAT_TX_SCHEMESHARD,
            "[TenantShredManager] [HandleDisconnect] Retry send response to domain schemeshard form# " << SchemeShard->TabletID());
        std::unique_ptr<TEvSchemeShard::TEvTenantShredResponse> response = std::make_unique<TEvSchemeShard::TEvTenantShredResponse>(
            SchemeShard->ParentDomainId,
            SchemeShard->TenantShredManager->GetGeneration(),
            NKikimrScheme::TEvTenantShredResponse::COMPLETED
        );
        SchemeShard->PipeClientCache->Send(ctx, ui64(SchemeShard->ParentDomainId.OwnerId), response.release());
        return;
    }
    auto tabletIt = SchemeShard->TabletIdToShardIdx.find(tabletId);
    if (tabletIt == SchemeShard->TabletIdToShardIdx.end()) {
        return; // just sanity check
    }
    const auto& shardIdx = tabletIt->second;
    auto it = ActivePipes.find(shardIdx);
    if (it == ActivePipes.end() ||
        it->second != clientId) {
        return;
    }
    LOG_INFO_S(ctx, NKikimrServices::FLAT_TX_SCHEMESHARD, "[TenantShredManager] [Disconnect] Shred disconnect "
        "to tablet: " << tabletId
        << ", at schemeshard: " << SchemeShard->TabletID());

    ActivePipes.erase(it);
    StartShredOperation(shardIdx);
}

void TTenantShredManager::RetryShred(const TTabletId& tabletId) {
    auto tabletIt = SchemeShard->TabletIdToShardIdx.find(tabletId);
    if (tabletIt == SchemeShard->TabletIdToShardIdx.end()) {
        return; // just sanity check
    }
    const auto& shardIdx = tabletIt->second;
    StartShredOperation(shardIdx);
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
        if (record.GetGeneration() > shredManager->GetGeneration()) {
            NIceDb::TNiceDb db(txc.DB);
            shredManager->StartShred(db, record.GetGeneration());
        }
        if (record.GetGeneration() < shredManager->GetGeneration() ||
            record.GetGeneration() == shredManager->GetGeneration() && shredManager->GetStatus() == EShredStatus::COMPLETED) {
            LOG_DEBUG_S(ctx, NKikimrServices::FLAT_TX_SCHEMESHARD,
                "TTxRunTenantShred: Already complete" <<
                " for requested generation# " << record.GetGeneration() <<
                ", schemeshard# " << Self->TabletID() <<
                ", requested from# " << Ev->Sender
            );
            Response = std::make_unique<TEvSchemeShard::TEvTenantShredResponse>(Self->ParentDomainId, shredManager->GetGeneration(), NKikimrScheme::TEvTenantShredResponse::COMPLETED);
        }
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

template <typename TEvType>
struct TSchemeShard::TTxCompleteShredShard : public TSchemeShard::TRwTxBase {
    TEvType Ev;
    std::unique_ptr<TEvSchemeShard::TEvTenantShredResponse> Response = nullptr;

    TTxCompleteShredShard(TSelf *self, TEvType& ev)
        : TRwTxBase(self)
        , Ev(std::move(ev))
    {}

    TTxType GetTxType() const override { return TXTYPE_COMPLETE_SHRED_SHARD; }

    void DoExecute(TTransactionContext& txc, const TActorContext& ctx) override {
        LOG_DEBUG_S(ctx, NKikimrServices::FLAT_TX_SCHEMESHARD,
            "TTxCompleteShredShard Execute at schemestard: " << Self->TabletID());
        if (!IsSuccess(Ev)) {
            HandleBadStatus(Ev, ctx);
            return;
        }
        const ui64 complitedGeneration = GetComplitedGeneration(Ev);
        auto& shredManager = Self->TenantShredManager;
        if (complitedGeneration != shredManager->GetGeneration()) {
            LOG_DEBUG_S(ctx, NKikimrServices::FLAT_TX_SCHEMESHARD,
                "TTxCompleteShredShard: Unknown generation#" << complitedGeneration <<
                ", Expected gen# " << shredManager->GetGeneration() << " at schemestard: " << Self->TabletID());
            return;
        }
        NIceDb::TNiceDb db(txc.DB);
        shredManager->FinishShred(db, TTabletId(GetTabletId(Ev)));
        if (shredManager->GetStatus() == EShredStatus::COMPLETED) {
            Response = std::make_unique<TEvSchemeShard::TEvTenantShredResponse>(Self->ParentDomainId, shredManager->GetGeneration(), NKikimrScheme::TEvTenantShredResponse::COMPLETED);
        }
    }

    void DoComplete(const TActorContext& ctx) override {
        if (Response) {
            LOG_DEBUG_S(ctx, NKikimrServices::FLAT_TX_SCHEMESHARD, "++++++++++++++++++++Send to domain schemeshard");
            Self->PipeClientCache->Send(ctx, ui64(Self->ParentDomainId.OwnerId), Response.release());
        }
    }

private:
    bool IsSuccess(TEvDataShard::TEvVacuumResult::TPtr& ev) const {
        const auto& record = ev->Get()->Record;
        return record.GetStatus() == NKikimrTxDataShard::TEvVacuumResult::OK;
    }

    void HandleBadStatus(TEvDataShard::TEvVacuumResult::TPtr& ev, const TActorContext& ctx) const {
        const auto& record = ev->Get()->Record;
        LOG_DEBUG_S(ctx, NKikimrServices::FLAT_TX_SCHEMESHARD,
            "TTxCompleteShredShard: shred failed at DataShard#" << record.GetTabletId()
            << " with status: " << NKikimrTxDataShard::TEvVacuumResult::EStatus_Name(record.GetStatus())
            << ", schemestard: " << Self->TabletID());
        Self->TenantShredManager->RetryShred(TTabletId(record.GetTabletId()));
    }

    ui64 GetComplitedGeneration(TEvDataShard::TEvVacuumResult::TPtr& ev) const {
        const auto& record = ev->Get()->Record;
        return record.GetVacuumGeneration();
    }

    ui64 GetTabletId(TEvDataShard::TEvVacuumResult::TPtr& ev) const {
        const auto& record = ev->Get()->Record;
        return record.GetTabletId();
    }

    bool IsSuccess(TEvKeyValue::TEvVacuumResponse::TPtr& ev) {
        const auto& record = ev->Get()->Record;
        return record.status() == NKikimrKeyValue::VacuumResponse::STATUS_SUCCESS;
    }

    void HandleBadStatus(TEvKeyValue::TEvVacuumResponse::TPtr& ev, const TActorContext& ctx) const {
        const auto& record = ev->Get()->Record;
        LOG_DEBUG_S(ctx, NKikimrServices::FLAT_TX_SCHEMESHARD,
            "TTxCompleteShredShard: shred failed at KeyValue#" << record.tablet_id() <<
            " with status: " << NKikimrKeyValue::VacuumResponse::Status_Name(record.status()) <<
            ", schemestard: " << Self->TabletID());
        Self->TenantShredManager->RetryShred(TTabletId(record.tablet_id()));
    }

    ui64 GetComplitedGeneration(TEvKeyValue::TEvVacuumResponse::TPtr& ev) const {
        const auto& record = ev->Get()->Record;
        return record.actual_generation();
    }

    ui64 GetTabletId(TEvKeyValue::TEvVacuumResponse::TPtr& ev) const {
        const auto& record = ev->Get()->Record;
        return record.tablet_id();
    }
};

template <typename TEvType>
NTabletFlatExecutor::ITransaction* TSchemeShard::CreateTxCompleteShredShard(TEvType& ev) {
    return new TTxCompleteShredShard(this, ev);
}

template NTabletFlatExecutor::ITransaction* TSchemeShard::CreateTxCompleteShredShard<TEvDataShard::TEvVacuumResult::TPtr>(TEvDataShard::TEvVacuumResult::TPtr& ev);
template NTabletFlatExecutor::ITransaction* TSchemeShard::CreateTxCompleteShredShard<TEvKeyValue::TEvVacuumResponse::TPtr>(TEvKeyValue::TEvVacuumResponse::TPtr& ev);

} // NKikimr::NSchemeShard
