#include "data_erasure_manager.h"

#include <ydb/core/tx/schemeshard/schemeshard_impl.h>

namespace NKikimr::NSchemeShard {

TTenantDataErasureManager::TStarter::TStarter(TTenantDataErasureManager* const manager)
    : Manager(manager)
{}

NOperationQueue::EStartStatus TTenantDataErasureManager::TStarter::StartOperation(const TShardIdx& shardIdx) {
    return Manager->StartDataErasure(shardIdx);
}

void TTenantDataErasureManager::TStarter::OnTimeout(const TShardIdx& shardIdx) {
    Manager->OnTimeout(shardIdx);
}

TTenantDataErasureManager::TTenantDataErasureManager(TSchemeShard* const schemeShard, const NKikimrConfig::TDataErasureConfig& config)
    : TDataErasureManager(schemeShard)
    , Starter(this)
    , Queue(new TQueue(ConvertConfig(config), Starter))
{
    const auto ctx = SchemeShard->ActorContext();
    ctx.RegisterWithSameMailbox(Queue);

    const auto& tenantDataErasureConfig = config.GetTenantDataErasureConfig();
    LOG_NOTICE_S(ctx, NKikimrServices::FLAT_TX_SCHEMESHARD,
        "TenantDataErasureManager created: Timeout# " << tenantDataErasureConfig.GetTimeoutSeconds()
        << ", Rate# " << Queue->GetRate()
        << ", InflightLimit# " << tenantDataErasureConfig.GetInflightLimit());
}

void TTenantDataErasureManager::UpdateConfig(const NKikimrConfig::TDataErasureConfig& config) {
    TTenantDataErasureManager::TQueue::TConfig queueConfig = ConvertConfig(config);
    Queue->UpdateConfig(queueConfig);

    const auto ctx = SchemeShard->ActorContext();
    LOG_NOTICE_S(ctx, NKikimrServices::FLAT_TX_SCHEMESHARD,
        "TenantDataErasureManager config updated: Timeout# " << queueConfig.Timeout
        << ", Rate# " << Queue->GetRate()
        << ", InflightLimit# " << queueConfig.InflightLimit);
}

void TTenantDataErasureManager::Start() {
    Queue->Start();
    if (Status == EStatus::COMPLETED) {
        SendResponseToRootSchemeShard();
    } else if (Status == EStatus::IN_PROGRESS) {
        ClearOperationQueue();
        Continue();
    }
}

void TTenantDataErasureManager::Stop() {
    Queue->Stop();
}

void TTenantDataErasureManager::ClearOperationQueue() {
    Queue->Clear();
}

void TTenantDataErasureManager::WakeupToRunDataErasure(TEvSchemeShard::TEvWakeupToRunDataErasure::TPtr& ev, const NActors::TActorContext& ctx) {
    Y_UNUSED(ev, ctx);
}

void TTenantDataErasureManager::ClearWaitingDataErasureRequests(NIceDb::TNiceDb& db) {
    for (const auto& [shardIdx, status] : WaitingDataErasureShards) {
        db.Table<Schema::ActiveDataErasureShards>().Key(shardIdx.GetOwnerId(), shardIdx.GetLocalId()).Delete();
    }
    WaitingDataErasureShards.clear();
}

void TTenantDataErasureManager::Run(NIceDb::TNiceDb& db) {
    Status = EStatus::IN_PROGRESS;
    for (const auto& [shardIdx, shardInfo] : SchemeShard->ShardInfos) {
        if (shardInfo.TabletType == ETabletType::DataShard) {
            Enqueue(shardIdx); // forward generation
            WaitingDataErasureShards[shardIdx] = EStatus::IN_PROGRESS;
            db.Table<Schema::ActiveDataErasureShards>().Key(shardIdx.GetOwnerId(), shardIdx.GetLocalId()).Update<Schema::ActiveDataErasureTenants::Status>(static_cast<ui32>(WaitingDataErasureShards[shardIdx]));
        }
    }
    if (WaitingDataErasureShards.empty()) {
        Status = EStatus::COMPLETED;
    }
    db.Table<Schema::TenantDataErasureStarts>().Key(Generation).Update<Schema::TenantDataErasureStarts::Status>(static_cast<ui32>(Status));
}

void TTenantDataErasureManager::Continue() {
    for (const auto& [shardIdx, status] : WaitingDataErasureShards) {
        if (status == EStatus::IN_PROGRESS) {
            Enqueue(shardIdx); // forward generation
        }
    }
}

NOperationQueue::EStartStatus TTenantDataErasureManager::StartDataErasure(const TShardIdx& shardIdx) {
    UpdateMetrics();

    auto ctx = SchemeShard->ActorContext();

    auto it = SchemeShard->ShardInfos.find(shardIdx);
    if (it == SchemeShard->ShardInfos.end()) {
        LOG_WARN_S(ctx, NKikimrServices::FLAT_TX_SCHEMESHARD, "[TenantDataErasure] [Start] Failed to resolve shard info "
            "for data erasure# " << shardIdx
            << " at schemeshard# " << SchemeShard->TabletID());

        return NOperationQueue::EStartStatus::EOperationRemove;
    }

    const auto& datashardId = it->second.TabletID;
    const auto& pathId = it->second.PathId;

    LOG_INFO_S(ctx, NKikimrServices::FLAT_TX_SCHEMESHARD, "[TenantDataErasure] [Start] Data erasure "
        "for pathId# " << pathId << ", datashard# " << datashardId
        << ", next wakeup# " << Queue->GetWakeupDelta()
        << ", rate# " << Queue->GetRate()
        << ", in queue# " << Queue->Size() << " shards"
        << ", running# " << Queue->RunningSize() << " shards"
        << " at schemeshard " << SchemeShard->TabletID());

    std::unique_ptr<TEvDataShard::TEvForceDataCleanup> request(
        new TEvDataShard::TEvForceDataCleanup(Generation));

    ActivePipes[shardIdx] = SchemeShard->PipeClientCache->Send(
        ctx,
        ui64(datashardId),
        request.release());

    return NOperationQueue::EStartStatus::EOperationRunning;
}

void TTenantDataErasureManager::OnTimeout(const TShardIdx& shardIdx) {
    UpdateMetrics();
    SchemeShard->TabletCounters->Cumulative()[COUNTER_TENANT_DATA_ERASURE_TIMEOUT].Increment(1);

    ActivePipes.erase(shardIdx);

    auto ctx = SchemeShard->ActorContext();

    auto it = SchemeShard->ShardInfos.find(shardIdx);
    if (it == SchemeShard->ShardInfos.end()) {
        LOG_WARN_S(ctx, NKikimrServices::FLAT_TX_SCHEMESHARD, "[TenantDataErasure] [Timeout] Failed to resolve shard info "
            "for timeout data erasure# " << shardIdx
            << " at schemeshard# " << SchemeShard->TabletID());
        return;
    }

    const auto& datashardId = it->second.TabletID;
    const auto& pathId = it->second.PathId;

    LOG_INFO_S(ctx, NKikimrServices::FLAT_TX_SCHEMESHARD, "[TenantDataErasure] [Timeout] Data erasure timeout "
        "for pathId# " << pathId << ", datashard# " << datashardId
        << ", next wakeup# " << Queue->GetWakeupDelta()
        << ", in queue# " << Queue->Size() << " shards"
        << ", running# " << Queue->RunningSize() << " shards"
        << " at schemeshard " << SchemeShard->TabletID());

    // retry
    Enqueue(shardIdx);
}

void TTenantDataErasureManager::Enqueue(const TShardIdx& shardIdx) {
    auto ctx = SchemeShard->ActorContext();

    if (Queue->Enqueue(shardIdx)) {
        LOG_TRACE_S(ctx, NKikimrServices::FLAT_TX_SCHEMESHARD,
            "[TenantDataErasure] [Enqueue] Enqueued shard# " << shardIdx << " at schemeshard " << SchemeShard->TabletID());
        UpdateMetrics();
    }  else {
        LOG_TRACE_S(ctx, NKikimrServices::FLAT_TX_SCHEMESHARD,
            "[TenantDataErasure] [Enqueue] Skipped or already exists shard# " << shardIdx << " at schemeshard " << SchemeShard->TabletID());
    }
}

void TTenantDataErasureManager::HandleDisconnect(TTabletId tabletId, const TActorId& clientId, const TActorContext& ctx) {
    if (tabletId == TTabletId(SchemeShard->ParentDomainId.OwnerId)) {
        SendResponseToRootSchemeShard();
        return;
    }
    auto tabletIt = SchemeShard->TabletIdToShardIdx.find(tabletId);
    if (tabletIt == SchemeShard->TabletIdToShardIdx.end())
        return; // just sanity check
    const auto& shardIdx = tabletIt->second;

    auto it = ActivePipes.find(shardIdx);
    if (it == ActivePipes.end())
        return;

    if (it->second != clientId)
        return;

    ActivePipes.erase(it);

    LOG_INFO_S(ctx, NKikimrServices::FLAT_TX_SCHEMESHARD, "[TenantDataErasure] [Disconnect] Data erasure disconnect "
        "to tablet: " << tabletId
        << ", at schemeshard: " << SchemeShard->TabletID());

    StartDataErasure(shardIdx);
}

void TTenantDataErasureManager::OnDone(const TPathId&, NIceDb::TNiceDb&) {
    auto ctx = SchemeShard->ActorContext();
    LOG_INFO_S(ctx, NKikimrServices::FLAT_TX_SCHEMESHARD, "Cannot execute in tenant schemeshard: " << SchemeShard->TabletID());
}

void TTenantDataErasureManager::OnDone(const TTabletId& tabletId, NIceDb::TNiceDb& db) {
    const TShardIdx shardIdx = SchemeShard->GetShardIdx(tabletId);
    const auto it = SchemeShard->ShardInfos.find(shardIdx);

    auto duration = Queue->OnDone(shardIdx);

    auto ctx = SchemeShard->ActorContext();
    if (shardIdx == InvalidShardIdx) {
        LOG_WARN_S(ctx, NKikimrServices::FLAT_TX_SCHEMESHARD, "[TenantDataErasure] [Finished] Failed to resolve shard info "
            "for pathId# " << (it != SchemeShard->ShardInfos.end() ? it->second.PathId.ToString() : "") << ", datashard# " << tabletId
            << " in# " << duration.MilliSeconds() << " ms, with status# "// << (int)record.GetStatus()
            << ", next wakeup in# " << Queue->GetWakeupDelta()
            << ", rate# " << Queue->GetRate()
            << ", in queue# " << Queue->Size() << " shards"
            << ", running# " << Queue->RunningSize() << " shards"
            << " at schemeshard " << SchemeShard->TabletID());
    } else {
        LOG_INFO_S(ctx, NKikimrServices::FLAT_TX_SCHEMESHARD, "[TenantDataErasure] [Finished] Data erasure is completed "
            "for pathId# " << (it != SchemeShard->ShardInfos.end() ? it->second.PathId.ToString() : "") << ", datashard# " << tabletId
            << ", shardIdx# " << shardIdx
            << " in# " << duration.MilliSeconds() << " ms, with status# "// << (int)record.GetStatus()
            << ", next wakeup in# " << Queue->GetWakeupDelta()
            << ", rate# " << Queue->GetRate()
            << ", in queue# " << Queue->Size() << " shards"
            << ", running# " << Queue->RunningSize() << " shards"
            << " at schemeshard " << SchemeShard->TabletID());
    }

    ActivePipes.erase(shardIdx);
    {
        auto it = WaitingDataErasureShards.find(shardIdx);
        if (it != WaitingDataErasureShards.end()) {
            it->second = EStatus::COMPLETED;
        }
        db.Table<Schema::ActiveDataErasureShards>().Key(shardIdx.GetOwnerId(), shardIdx.GetLocalId()).Update<Schema::ActiveDataErasureShards::Status>(static_cast<ui32>(it->second));
    }

    SchemeShard->TabletCounters->Cumulative()[COUNTER_TENANT_DATA_ERASURE_OK].Increment(1);
    UpdateMetrics();

    bool isTenantDataErasureCompleted = true;
    for (const auto& [shardIdx, status] : WaitingDataErasureShards) {
        if (status == EStatus::IN_PROGRESS) {
            isTenantDataErasureCompleted = false;
        }
    }
    if (isTenantDataErasureCompleted) {
        Complete();
        db.Table<Schema::TenantDataErasureStarts>().Key(Generation).Update<Schema::TenantDataErasureStarts::Status>(static_cast<ui32>(Status));
    }
}

void TTenantDataErasureManager::ScheduleRequestToBSC() {
    auto ctx = SchemeShard->ActorContext();
    LOG_INFO_S(ctx, NKikimrServices::FLAT_TX_SCHEMESHARD, "Cannot ScheduleRequestToBSC in tenant schemeshard: " << SchemeShard->TabletID());
}

void TTenantDataErasureManager::SendRequestToBSC() {
    auto ctx = SchemeShard->ActorContext();
    LOG_INFO_S(ctx, NKikimrServices::FLAT_TX_SCHEMESHARD, "Cannot SendRequestToBSC in tenant schemeshard: " << SchemeShard->TabletID());
}

void TTenantDataErasureManager::Complete() {
    Status = EStatus::COMPLETED;
}

bool TTenantDataErasureManager::Restore(NIceDb::TNiceDb& db) {
    {
        auto rowset = db.Table<Schema::TenantDataErasureStarts>().Range().Select();
        if (!rowset.IsReady()) {
            return false;
        }
        while (!rowset.EndOfSet()) {
            ui64 generation = rowset.GetValue<Schema::TenantDataErasureStarts::Generation>();
            if (generation >= Generation) {
                Generation = generation;
                ui32 statusValue = rowset.GetValue<Schema::TenantDataErasureStarts::Status>();
                Status = EStatus::UNSPECIFIED;
                if (statusValue >= static_cast<ui32>(EStatus::UNSPECIFIED) &&
                    statusValue <= static_cast<ui32>(EStatus::IN_PROGRESS_BSC)) {
                        Status = static_cast<EStatus>(statusValue);
                }
            }

            if (!rowset.Next()) {
                return false;
            }
        }
    }

    {
        auto rowset = db.Table<Schema::ActiveDataErasureShards>().Range().Select();
        if (!rowset.IsReady()) {
            return false;
        }
        while (!rowset.EndOfSet()) {
            TOwnerId ownerId = rowset.GetValue<Schema::ActiveDataErasureShards::OwnerShardIdx>();
            TLocalShardIdx localShardId = rowset.GetValue<Schema::ActiveDataErasureShards::LocalShardIdx>();
            TShardIdx shardId(ownerId, localShardId);

            ui32 statusValue = rowset.GetValue<Schema::ActiveDataErasureShards::Status>();
            EStatus status = EStatus::COMPLETED;
            if (statusValue >= static_cast<ui32>(EStatus::UNSPECIFIED) &&
                statusValue <= static_cast<ui32>(EStatus::IN_PROGRESS_BSC)) {
                    status = static_cast<EStatus>(statusValue);
            }
            WaitingDataErasureShards[shardId] = status;

            if (!rowset.Next()) {
                return false;
            }
        }
        if (Status == EStatus::IN_PROGRESS && WaitingDataErasureShards.empty()) {
            Status = EStatus::COMPLETED;
        }
    }
    return true;
}

void TTenantDataErasureManager::Remove(const TPathId&) {
    auto ctx = SchemeShard->ActorContext();
    LOG_INFO_S(ctx, NKikimrServices::FLAT_TX_SCHEMESHARD, "Remove path id do not implemented for tenant schemeshard: " << SchemeShard->TabletID());
}

void TTenantDataErasureManager::UpdateMetrics() {
    SchemeShard->TabletCounters->Simple()[COUNTER_TENANT_DATA_ERASURE_QUEUE_SIZE].Set(Queue->Size());
    SchemeShard->TabletCounters->Simple()[COUNTER_TENANT_DATA_ERASURE_QUEUE_RUNNING].Set(Queue->RunningSize());
}

void TTenantDataErasureManager::SendResponseToRootSchemeShard() {
    std::unique_ptr<TEvSchemeShard::TEvTenantDataErasureResponse> response(
        new TEvSchemeShard::TEvTenantDataErasureResponse(SchemeShard->ParentDomainId, SchemeShard->DataErasureManager->GetGeneration(), TEvSchemeShard::TEvTenantDataErasureResponse::EStatus::COMPLETED));

    const ui64 rootSchemeshard = SchemeShard->ParentDomainId.OwnerId;

    auto ctx = SchemeShard->ActorContext();
    SchemeShard->PipeClientCache->Send(
        ctx,
        ui64(rootSchemeshard),
        response.release());
}


TTenantDataErasureManager::TQueue::TConfig TTenantDataErasureManager::ConvertConfig(const NKikimrConfig::TDataErasureConfig& config) {
    TQueue::TConfig queueConfig;
    const auto& tenantDataErasureConfig = config.GetTenantDataErasureConfig();
    queueConfig.IsCircular = false;
    queueConfig.MaxRate = tenantDataErasureConfig.GetMaxRate();
    queueConfig.InflightLimit = tenantDataErasureConfig.GetInflightLimit();
    queueConfig.Timeout = TDuration::Seconds(tenantDataErasureConfig.GetTimeoutSeconds());

    return queueConfig;
}

struct TSchemeShard::TTxRunTenantDataErasure : public TSchemeShard::TRwTxBase {
    TEvSchemeShard::TEvTenantDataErasureRequest::TPtr Ev;
    bool NeedResponseComplete = false;

    TTxRunTenantDataErasure(TSelf *self, TEvSchemeShard::TEvTenantDataErasureRequest::TPtr& ev)
        : TRwTxBase(self)
        , Ev(std::move(ev))
    {}

    TTxType GetTxType() const override { return TXTYPE_RUN_DATA_ERASURE_TENANT; }

    void DoExecute(TTransactionContext& txc, const TActorContext& ctx) override {
        LOG_DEBUG_S(ctx, NKikimrServices::FLAT_TX_SCHEMESHARD,
            "+++TTxRunTenantDataErasure Execute at schemestard: " << Self->TabletID());

        if (Self->IsDomainSchemeShard) {
            LOG_WARN_S(ctx, NKikimrServices::FLAT_TX_SCHEMESHARD, "[TenantDataErasure] [Request] Cannot run data erasure on root schemeshard");
            return;
        }

        NIceDb::TNiceDb db(txc.DB);
        const auto& record = Ev->Get()->Record;
        auto& dataErasureManager = Self->DataErasureManager;
        if (dataErasureManager->GetGeneration() < record.GetGeneration()) {
            dataErasureManager->SetGeneration(record.GetGeneration());
            dataErasureManager->ClearOperationQueue();
            dataErasureManager->ClearWaitingDataErasureRequests(db);
            dataErasureManager->Run(db);
        }
    }

    void DoComplete(const TActorContext& ctx) override {
        LOG_DEBUG_S(ctx, NKikimrServices::FLAT_TX_SCHEMESHARD,
            "+++TTxRunTenantDataErasure Execute at schemestard: " << Self->TabletID());
            const auto& record = Ev->Get()->Record;
            if (Self->DataErasureManager->GetGeneration() == record.GetGeneration() && Self->DataErasureManager->GetStatus() == TDataErasureManager::EStatus::COMPLETED) {
            std::unique_ptr<TEvSchemeShard::TEvTenantDataErasureResponse> response(
                new TEvSchemeShard::TEvTenantDataErasureResponse(Self->ParentDomainId, Self->DataErasureManager->GetGeneration(), TEvSchemeShard::TEvTenantDataErasureResponse::EStatus::COMPLETED));

            const ui64 rootSchemeshard = Self->ParentDomainId.OwnerId;

            Self->PipeClientCache->Send(
                ctx,
                ui64(rootSchemeshard),
                response.release());
        }
    }
};

NTabletFlatExecutor::ITransaction* TSchemeShard::CreateTxRunTenantDataErasure(TEvSchemeShard::TEvTenantDataErasureRequest::TPtr& ev) {
    return new TTxRunTenantDataErasure(this, ev);
}

struct TSchemeShard::TTxCompleteDataErasureShard : public TSchemeShard::TRwTxBase {
    TEvDataShard::TEvForceDataCleanupResult::TPtr Ev;

    TTxCompleteDataErasureShard(TSelf *self, TEvDataShard::TEvForceDataCleanupResult::TPtr& ev)
        : TRwTxBase(self)
        , Ev(std::move(ev))
    {}

    TTxType GetTxType() const override { return TXTYPE_RUN_DATA_ERASURE_TENANT ; }

    void DoExecute(TTransactionContext& txc, const TActorContext& ctx) override {
        const auto& record = Ev->Get()->Record;

        auto& manager = Self->DataErasureManager;
        const ui64 completedGeneration = record.GetDataCleanupGeneration();
        if (completedGeneration != manager->GetGeneration()) {
            LOG_DEBUG_S(ctx, NKikimrServices::FLAT_TX_SCHEMESHARD,
                "Handle TEvForceDataCleanupResult: Unknown generation#" << completedGeneration
                                             << ", Expected gen# " << manager->GetGeneration() << " at schemestard: " << Self->TabletID());
            return;
        }
        NIceDb::TNiceDb db(txc.DB);
        manager->OnDone(TTabletId(record.GetTabletId()), db);
    }

    void DoComplete(const TActorContext& ctx) override {
        LOG_DEBUG_S(ctx, NKikimrServices::FLAT_TX_SCHEMESHARD,
            "TTxCompleteDataErasureShard Complete at schemestard: " << Self->TabletID());
        if (Self->DataErasureManager->GetStatus() == TDataErasureManager::EStatus::COMPLETED) {
            std::unique_ptr<TEvSchemeShard::TEvTenantDataErasureResponse> response(
                new TEvSchemeShard::TEvTenantDataErasureResponse(Self->ParentDomainId, Self->DataErasureManager->GetGeneration(), TEvSchemeShard::TEvTenantDataErasureResponse::EStatus::COMPLETED));

            const ui64 rootSchemeshard = Self->ParentDomainId.OwnerId;

            Self->PipeClientCache->Send(
                ctx,
                ui64(rootSchemeshard),
                response.release());
        }
    }
};

NTabletFlatExecutor::ITransaction* TSchemeShard::CreateTxCompleteDataErasureShard(TEvDataShard::TEvForceDataCleanupResult::TPtr& ev) {
    return new TTxCompleteDataErasureShard(this, ev);
}

} // NKikimr::NSchemeShard
