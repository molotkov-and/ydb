#include "data_erasure_manager.h"

#include <ydb/core/tx/schemeshard/schemeshard_impl.h>

namespace NKikimr::NSchemeShard {

TRootDataErasureManager::TStarter::TStarter(TRootDataErasureManager* const manager)
    : Manager(manager)
{}

NOperationQueue::EStartStatus TRootDataErasureManager::TStarter::StartOperation(const TPathId& pathId) {
    return Manager->StartDataErasure(pathId);
}

void TRootDataErasureManager::TStarter::OnTimeout(const TPathId& pathId) {
    Manager->OnTimeout(pathId);
}

TRootDataErasureManager::TRootDataErasureManager(TSchemeShard* const schemeShard, const NKikimrConfig::TDataErasureConfig& config)
    : TDataErasureManager(schemeShard)
    , Starter(this)
    , Queue(new TQueue(ConvertConfig(config), Starter))
    , DataErasureInterval(TDuration::Seconds(config.GetDataErasureIntervalSeconds()))
    , DataErasureBSCInterval(TDuration::Seconds(config.GetBlobStorageControllerRequestIntervalSeconds()))
    , CurrentWakeupInterval(DataErasureInterval)
    , BSC(MakeBSControllerID())
{
    const auto ctx = SchemeShard->ActorContext();
    ctx.RegisterWithSameMailbox(Queue);

    LOG_NOTICE_S(ctx, NKikimrServices::FLAT_TX_SCHEMESHARD,
        "RootDataErasureManager created: Timeout# " << config.GetTimeoutSeconds()
        << ", Rate# " << Queue->GetRate()
        << ", InflightLimit# " << config.GetInflightLimit());
}

void TRootDataErasureManager::UpdateConfig(const NKikimrConfig::TDataErasureConfig& config) {
    TRootDataErasureManager::TQueue::TConfig queueConfig = ConvertConfig(config);
    Queue->UpdateConfig(queueConfig);

    const auto ctx = SchemeShard->ActorContext();
    LOG_NOTICE_S(ctx, NKikimrServices::FLAT_TX_SCHEMESHARD,
        "RootDataErasureManager config updated: Timeout# " << queueConfig.Timeout
        << ", Rate# " << Queue->GetRate()
        << ", InflightLimit# " << queueConfig.InflightLimit);
}

void TRootDataErasureManager::Start() {
    const auto ctx = SchemeShard->ActorContext();
    LOG_NOTICE_S(ctx, NKikimrServices::FLAT_TX_SCHEMESHARD,
        "+++ Start root config updated:");
    Queue->Start();
    if (Status == EStatus::UNSPECIFIED) {
        LOG_NOTICE_S(ctx, NKikimrServices::FLAT_TX_SCHEMESHARD,
            "+++ Start MarkFirstRunRootDataErasureManager");
        SchemeShard->MarkFirstRunRootDataErasureManager();
        ScheduleDataErasureWakeup();
    } else if (Status == EStatus::COMPLETED) {
        LOG_NOTICE_S(ctx, NKikimrServices::FLAT_TX_SCHEMESHARD,
            "+++ Start ScheduleDataErasureWakeup");
        ScheduleDataErasureWakeup();
    } else {
        LOG_NOTICE_S(ctx, NKikimrServices::FLAT_TX_SCHEMESHARD,
            "+++ Start Continue");
        ClearOperationQueue();
        Continue();
    }
}

void TRootDataErasureManager::Stop() {
    Queue->Stop();
}

void TRootDataErasureManager::ClearOperationQueue() {
    Queue->Clear();
    ActivePipes.clear();
}

void TRootDataErasureManager::ClearWaitingDataErasureRequests(NIceDb::TNiceDb& db) {
    for (const auto& [pathId, status] : WaitingDataErasureTenants) {
        db.Table<Schema::ActiveDataErasureTenants>().Key(pathId.OwnerId, pathId.LocalPathId).Delete();
    }
    WaitingDataErasureTenants.clear();
}

void TRootDataErasureManager::Run(NIceDb::TNiceDb& db) {
    Status = EStatus::IN_PROGRESS;
    StartTime =AppData(SchemeShard->ActorContext())->TimeProvider->Now();
    for (auto& [pathId, subdomain] : SchemeShard->SubDomains) {
        auto path = TPath::Init(pathId, SchemeShard);
        if (path->IsRoot()) {
            continue;
        }
        if (subdomain->GetTenantSchemeShardID() == InvalidTabletId) { // no tenant schemeshard
            continue;
        }
        Enqueue(pathId);
        WaitingDataErasureTenants[pathId] = EStatus::IN_PROGRESS;
        db.Table<Schema::ActiveDataErasureTenants>().Key(pathId.OwnerId, pathId.LocalPathId).Update<Schema::ActiveDataErasureTenants::Status>(static_cast<ui32>(WaitingDataErasureTenants[pathId]));
    }
    if (WaitingDataErasureTenants.empty()) {
        Status = EStatus::IN_PROGRESS_BSC;
    }
    db.Table<Schema::DataErasureStarts>().Key(Generation).Update<Schema::DataErasureStarts::Status,
                                                                     Schema::DataErasureStarts::StartTime>(static_cast<ui32>(Status), StartTime.MicroSeconds());
}

void TRootDataErasureManager::Continue() {
    if (Status == EStatus::IN_PROGRESS) {
        for (const auto& [pathId, status] : WaitingDataErasureTenants) {
            if (status == EStatus::IN_PROGRESS) {
                Enqueue(pathId);
            }
        }
    }
}

void TRootDataErasureManager::ScheduleDataErasureWakeup() {
    if (IsDataErasureWakeupScheduled) {
        return;
    }

    const auto ctx = SchemeShard->ActorContext();
    LOG_NOTICE_S(ctx, NKikimrServices::FLAT_TX_SCHEMESHARD,
        "+++ ScheduleDataErasureWakeup: interval: " << CurrentWakeupInterval);
    ctx.Schedule(CurrentWakeupInterval, new TEvSchemeShard::TEvWakeupToRunDataErasure);
    IsDataErasureWakeupScheduled = true;
}

void TRootDataErasureManager::WakeupToRunDataErasure(TEvSchemeShard::TEvWakeupToRunDataErasure::TPtr& ev, const NActors::TActorContext& ctx) {
    Y_UNUSED(ev);
    IsDataErasureWakeupScheduled = false;
    LOG_NOTICE_S(ctx, NKikimrServices::FLAT_TX_SCHEMESHARD,
        "+++ WakeupToRunDataErasure");
    SchemeShard->RunDataErasure(true);
}

NOperationQueue::EStartStatus TRootDataErasureManager::StartDataErasure(const TPathId& pathId) {
    UpdateMetrics();

    auto ctx = SchemeShard->ActorContext();

    auto it = SchemeShard->SubDomains.find(pathId);
    if (it == SchemeShard->SubDomains.end()) {
        LOG_WARN_S(ctx, NKikimrServices::FLAT_TX_SCHEMESHARD, "[DataErasure] [Start] Failed to resolve subdomain info "
            "for pathId# " << pathId
            << " at schemeshard# " << SchemeShard->TabletID());

        return NOperationQueue::EStartStatus::EOperationRemove;
    }

    const auto& tenantSchemeShardId = it->second->GetTenantSchemeShardID();

    LOG_INFO_S(ctx, NKikimrServices::FLAT_TX_SCHEMESHARD, "+++[DataErasure] [Start] Data erasure "
        "for pathId# " << pathId
        << ", tenant schemeshard# " << tenantSchemeShardId
        << ", next wakeup# " << Queue->GetWakeupDelta()
        << ", rate# " << Queue->GetRate()
        << ", in queue# " << Queue->Size() << " tenants"
        << ", running# " << Queue->RunningSize() << " tenants"
        << " at schemeshard " << SchemeShard->TabletID());

    std::unique_ptr<TEvSchemeShard::TEvTenantDataErasureRequest> request(
        new TEvSchemeShard::TEvTenantDataErasureRequest(Generation));

    ActivePipes[pathId] = SchemeShard->PipeClientCache->Send(
        ctx,
        ui64(tenantSchemeShardId),
        request.release());

    return NOperationQueue::EStartStatus::EOperationRunning;
}

void TRootDataErasureManager::OnTimeout(const TPathId& pathId) {
    UpdateMetrics();
    SchemeShard->TabletCounters->Cumulative()[COUNTER_DATA_ERASURE_TIMEOUT].Increment(1);

    ActivePipes.erase(pathId);

    auto ctx = SchemeShard->ActorContext();

    if (!SchemeShard->SubDomains.contains(pathId)) {
        LOG_WARN_S(ctx, NKikimrServices::FLAT_TX_SCHEMESHARD, "[DataErasure] [Timeout] Failed to resolve subdomain info "
            "for path# " << pathId
            << " at schemeshard# " << SchemeShard->TabletID());
        return;
    }

    LOG_INFO_S(ctx, NKikimrServices::FLAT_TX_SCHEMESHARD, "[DataErasure] [Timeout] Data erasure timeouted "
        "for pathId# " << pathId
        << ", next wakeup in# " << Queue->GetWakeupDelta()
        << ", rate# " << Queue->GetRate()
        << ", in queue# " << Queue->Size() << " tenants"
        << ", running# " << Queue->RunningSize() << " tenants"
        << " at schemeshard " << SchemeShard->TabletID());

    // retry
    Enqueue(pathId);
}

void TRootDataErasureManager::Enqueue(const TPathId& pathId) {
    auto ctx = SchemeShard->ActorContext();

    if (Queue->Enqueue(pathId)) {
        LOG_TRACE_S(ctx, NKikimrServices::FLAT_TX_SCHEMESHARD,
            "[DataErasure] [Enqueue] Enqueued pathId# " << pathId << " at schemeshard " << SchemeShard->TabletID());
        UpdateMetrics();
    } else {
        LOG_TRACE_S(ctx, NKikimrServices::FLAT_TX_SCHEMESHARD,
            "[DataErasure] [Enqueue] Skipped or already exists pathId# " << pathId << " at schemeshard " << SchemeShard->TabletID());
    }
}

void TRootDataErasureManager::HandleDisconnect(TTabletId tabletId, const TActorId& clientId, const TActorContext& ctx) {
    if (tabletId == BSC) {
        SendRequestToBSC();
        return;
    }

    const auto shardIdx = SchemeShard->GetShardIdx(tabletId);
    if (!SchemeShard->ShardInfos.contains(shardIdx)) {
        return;
    }

    const auto& pathId = SchemeShard->ShardInfos.at(shardIdx).PathId;
    if (!SchemeShard->TTLEnabledTables.contains(pathId)) {
        return;
    }

    const auto it = ActivePipes.find(pathId);
    if (it == ActivePipes.end()) {
        return;
    }

    if (it->second != clientId) {
        return;
    }

    ActivePipes.erase(pathId);

    LOG_INFO_S(ctx, NKikimrServices::FLAT_TX_SCHEMESHARD, "[DataErasure] [Disconnect] Data erasure disconnect "
        "to tablet: " << tabletId
        << ", at schemeshard: " << SchemeShard->TabletID());

    StartDataErasure(pathId);
}

void TRootDataErasureManager::OnDone(const TPathId& pathId, NIceDb::TNiceDb& db) {
    auto duration = Queue->OnDone(pathId);

    auto ctx = SchemeShard->ActorContext();
    if (!SchemeShard->SubDomains.contains(pathId)) {
        LOG_WARN_S(ctx, NKikimrServices::FLAT_TX_SCHEMESHARD, "[DataErasure] [Finished] Failed to resolve subdomain info "
            "for pathId# " << pathId
            << " in# " << duration.MilliSeconds() << " ms"
            << ", next wakeup in# " << Queue->GetWakeupDelta()
            << ", rate# " << Queue->GetRate()
            << ", in queue# " << Queue->Size() << " tenants"
            << ", running# " << Queue->RunningSize() << " tenants"
            << " at schemeshard " << SchemeShard->TabletID());
    } else {
        LOG_INFO_S(ctx, NKikimrServices::FLAT_TX_SCHEMESHARD, "[DataErasure] [Finished] Data erasure completed "
            "for pathId# " << pathId
            << " in# " << duration.MilliSeconds() << " ms"
            << ", next wakeup# " << Queue->GetWakeupDelta()
            << ", rate# " << Queue->GetRate()
            << ", in queue# " << Queue->Size() << " tenants"
            << ", running# " << Queue->RunningSize() << " tenants"
            << " at schemeshard " << SchemeShard->TabletID());
    }

    ActivePipes.erase(pathId);
    auto it = WaitingDataErasureTenants.find(pathId);
    if (it != WaitingDataErasureTenants.end()) {
        it->second = EStatus::COMPLETED;
        db.Table<Schema::ActiveDataErasureTenants>().Key(pathId.OwnerId, pathId.LocalPathId).Update<Schema::ActiveDataErasureTenants::Status>(static_cast<ui32>(it->second));
    }

    SchemeShard->TabletCounters->Cumulative()[COUNTER_DATA_ERASURE_OK].Increment(1);
    UpdateMetrics();

    bool isDataErasureCompleted = true;
    for (const auto& [pathId, status] : WaitingDataErasureTenants) {
        if (status == EStatus::IN_PROGRESS) {
            isDataErasureCompleted = false;
            break;
        }
    }

    if (isDataErasureCompleted) {
        LOG_INFO_S(ctx, NKikimrServices::FLAT_TX_SCHEMESHARD, "Data erasure in tenants is completed. Send request to BS controller");
        Status = EStatus::IN_PROGRESS_BSC;
        db.Table<Schema::DataErasureStarts>().Key(Generation).Update<Schema::DataErasureStarts::Status>(static_cast<ui32>(Status));
    }
}

void TRootDataErasureManager::OnDone(const TTabletId&, NIceDb::TNiceDb&) {
    auto ctx = SchemeShard->ActorContext();
    LOG_INFO_S(ctx, NKikimrServices::FLAT_TX_SCHEMESHARD, "Cannot execute in root schemeshard: " << SchemeShard->TabletID());
}

void TRootDataErasureManager::ScheduleRequestToBSC() {
    auto ctx = SchemeShard->ActorContext();
    ctx.Schedule(DataErasureBSCInterval, new TEvSchemeShard::TEvWakeupToRunDataErasureBSC);
}

void TRootDataErasureManager::SendRequestToBSC() {
    std::unique_ptr<TEvBlobStorage::TEvControllerShredRequest> request(
        new TEvBlobStorage::TEvControllerShredRequest(Generation));

    auto ctx = SchemeShard->ActorContext();
    SchemeShard->PipeClientCache->Send(ctx, MakeBSControllerID(), request.release());
}

void TRootDataErasureManager::Complete() {
    Status = EStatus::COMPLETED;
    auto ctx = SchemeShard->ActorContext();
    FinishTime = AppData(ctx)->TimeProvider->Now();
    TDuration dataErasureDuration = FinishTime - StartTime;
    if (dataErasureDuration > DataErasureInterval) {
        SchemeShard->RunDataErasure(true);
    } else {
        CurrentWakeupInterval = DataErasureInterval - dataErasureDuration;
        ScheduleDataErasureWakeup();
    }
}

bool TRootDataErasureManager::Restore(NIceDb::TNiceDb& db) {
    {
        auto rowset = db.Table<Schema::DataErasureStarts>().Range().Select();
        if (!rowset.IsReady()) {
            return false;
        }
        if (rowset.EndOfSet()) {
            Status = EStatus::UNSPECIFIED;
        } else {
            Generation = 0;
            Status = EStatus::UNSPECIFIED;
            while (!rowset.EndOfSet()) {
                ui64 generation = rowset.GetValue<Schema::DataErasureStarts::Generation>();
                if (generation >= Generation) {
                    Generation = generation;
                    StartTime = TInstant::FromValue(rowset.GetValue<Schema::DataErasureStarts::StartTime>());
                    ui32 statusValue = rowset.GetValue<Schema::DataErasureStarts::Status>();
                    if (statusValue >= static_cast<ui32>(EStatus::UNSPECIFIED) &&
                        statusValue <= static_cast<ui32>(EStatus::IN_PROGRESS_BSC)) {
                            Status = static_cast<EStatus>(statusValue);
                    }
                }

                if (!rowset.Next()) {
                    return false;
                }
            }
            if (Status == EStatus::UNSPECIFIED || Status == EStatus::COMPLETED) {
                auto ctx = SchemeShard->ActorContext();
                TDuration interval = AppData(ctx)->TimeProvider->Now() - StartTime;
                if (interval > DataErasureInterval) {
                    CurrentWakeupInterval = TDuration::Zero();
                } else {
                    CurrentWakeupInterval = DataErasureInterval - interval;
                }
            }
        }
    }

    {
        auto rowset = db.Table<Schema::ActiveDataErasureTenants>().Range().Select();
        if (!rowset.IsReady()) {
            return false;
        }
        while (!rowset.EndOfSet()) {
            TOwnerId ownerPathId = rowset.GetValue<Schema::ActiveDataErasureTenants::OwnerPathId>();
            TLocalPathId localPathId = rowset.GetValue<Schema::ActiveDataErasureTenants::LocalPathId>();
            TPathId pathId(ownerPathId, localPathId);
            Y_VERIFY_S(SchemeShard->PathsById.contains(pathId), "Path doesn't exist, pathId: " << pathId);
            TPathElement::TPtr path = SchemeShard->PathsById.at(pathId);
            Y_VERIFY_S(path->IsDomainRoot(), "Path is not a subdomain, pathId: " << pathId);

            Y_ABORT_UNLESS(SchemeShard->SubDomains.contains(pathId));

            ui32 statusValue = rowset.GetValue<Schema::ActiveDataErasureTenants::Status>();
            EStatus status = EStatus::COMPLETED;
            if (statusValue >= static_cast<ui32>(EStatus::UNSPECIFIED) &&
                statusValue <= static_cast<ui32>(EStatus::IN_PROGRESS_BSC)) {
                    status = static_cast<EStatus>(statusValue);
            }

            WaitingDataErasureTenants[pathId] = status;

            if (!rowset.Next()) {
                return false;
            }
        }
        if (Status == EStatus::IN_PROGRESS && WaitingDataErasureTenants.empty()) {
            Status = EStatus::IN_PROGRESS_BSC;
        }
    }
    return true;
}

void TRootDataErasureManager::Remove(const TPathId& pathId) {
    WaitingDataErasureTenants.erase(pathId);
}

void TRootDataErasureManager::UpdateMetrics() {
    SchemeShard->TabletCounters->Simple()[COUNTER_DATA_ERASURE_QUEUE_SIZE].Set(Queue->Size());
    SchemeShard->TabletCounters->Simple()[COUNTER_DATA_ERASURE_QUEUE_RUNNING].Set(Queue->RunningSize());
}

TRootDataErasureManager::TQueue::TConfig TRootDataErasureManager::ConvertConfig(const NKikimrConfig::TDataErasureConfig& config) {
    TQueue::TConfig queueConfig;
    queueConfig.IsCircular = false;
    queueConfig.MaxRate = config.GetMaxRate();
    queueConfig.InflightLimit = config.GetInflightLimit();
    queueConfig.Timeout = TDuration::Seconds(config.GetTimeoutSeconds());

    return queueConfig;
}

struct TSchemeShard::TTxDataErasureManagerInit : public TSchemeShard::TRwTxBase {
    TTxDataErasureManagerInit(TSelf* self)
        : TRwTxBase(self)
    {}

    TTxType GetTxType() const override { return TXTYPE_DATA_ERASURE_INIT; }

    void DoExecute(TTransactionContext& txc, const TActorContext& ctx) override {
        LOG_DEBUG_S(ctx, NKikimrServices::FLAT_TX_SCHEMESHARD,
            "TTxDataErasureManagerInit Execute at schemeshard: " << Self->TabletID());
        NIceDb::TNiceDb db(txc.DB);
        Self->DataErasureManager->SetStatus(TDataErasureManager::EStatus::COMPLETED);
        db.Table<Schema::DataErasureStarts>().Key(0).Update<Schema::DataErasureStarts::Status,
                                                               Schema::DataErasureStarts::StartTime>(static_cast<ui32>(Self->DataErasureManager->GetStatus()), AppData(ctx)->TimeProvider->Now().MicroSeconds());
    }

    void DoComplete(const TActorContext& ctx) override {
        LOG_DEBUG_S(ctx, NKikimrServices::FLAT_TX_SCHEMESHARD,
            "TTxDataErasureManagerInit Complete at schemeshard: " << Self->TabletID());
    }
};

NTabletFlatExecutor::ITransaction* TSchemeShard::CreateTxDataErasureManagerInit() {
    return new TTxDataErasureManagerInit(this);
}

struct TSchemeShard::TTxRunDataErasure : public TSchemeShard::TRwTxBase {
    bool IsNewDataErasure;

    TTxRunDataErasure(TSelf *self, bool isNewDataErasure)
        : TRwTxBase(self)
        , IsNewDataErasure(isNewDataErasure)
    {}

    TTxType GetTxType() const override { return TXTYPE_RUN_DATA_ERASURE; }

    void DoExecute(TTransactionContext& txc, const TActorContext& ctx) override {
        LOG_DEBUG_S(ctx, NKikimrServices::FLAT_TX_SCHEMESHARD,
                    "+++TTxRunDataErasure Execute at schemeshard: " << Self->TabletID());
        NIceDb::TNiceDb db(txc.DB);

        auto& dataErasureManager = Self->DataErasureManager;
        if (IsNewDataErasure) {
            dataErasureManager->ClearOperationQueue();
            dataErasureManager->ClearWaitingDataErasureRequests(db);
            dataErasureManager->IncGeneration();
            dataErasureManager->Run(db);
        }
    }
    void DoComplete(const TActorContext& ctx) override {
        LOG_DEBUG_S(ctx, NKikimrServices::FLAT_TX_SCHEMESHARD,
            "+++TTxRunDataErasure Complete at schemeshard: " << Self->TabletID());
        if (Self->DataErasureManager->GetStatus() == TDataErasureManager::EStatus::IN_PROGRESS_BSC) {
            Self->DataErasureManager->SendRequestToBSC();
        }
    }
};

NTabletFlatExecutor::ITransaction* TSchemeShard::CreateTxRunDataErasure(bool isNewDataErasure) {
    return new TTxRunDataErasure(this, isNewDataErasure);
}

struct TSchemeShard::TTxCompleteDataErasureTenant : public TSchemeShard::TRwTxBase {
    const TEvSchemeShard::TEvTenantDataErasureResponse::TPtr Ev;

    TTxCompleteDataErasureTenant(TSelf* self, const TEvSchemeShard::TEvTenantDataErasureResponse::TPtr& ev)
        : TRwTxBase(self)
        , Ev(std::move(ev))
    {}

    TTxType GetTxType() const override { return TXTYPE_COMPLETE_DATA_ERASURE_TENANT; }

    void DoExecute(TTransactionContext& txc, const TActorContext& ctx) override {
        LOG_DEBUG_S(ctx, NKikimrServices::FLAT_TX_SCHEMESHARD,
                    "TTxCompleteDataErasureTenant Execute at schemeshard: " << Self->TabletID());

        const auto& record = Ev->Get()->Record;

        auto& manager = Self->DataErasureManager;
        const ui64 completedGeneration = record.GetGeneration();
        if (completedGeneration != manager->GetGeneration()) {
            LOG_DEBUG_S(ctx, NKikimrServices::FLAT_TX_SCHEMESHARD,
                "[TTxCompleteDataErasureTenant] Unknown generation#" << completedGeneration << ", Expected gen# " << manager->GetGeneration() << " at schemestard: " << Self->TabletID());
            return;
        }

        NIceDb::TNiceDb db(txc.DB);
        auto pathId = TPathId(
            record.GetPathId().GetOwnerId(),
            record.GetPathId().GetLocalId());
        Self->DataErasureManager->OnDone(pathId, db);
    }

    void DoComplete(const TActorContext& ctx) override {
        LOG_DEBUG_S(ctx, NKikimrServices::FLAT_TX_SCHEMESHARD,
            "TTxCompleteDataErasure Complete at schemeshard: " << Self->TabletID());
        if (Self->DataErasureManager->GetStatus() == TDataErasureManager::EStatus::IN_PROGRESS_BSC) {
            std::unique_ptr<TEvBlobStorage::TEvControllerShredRequest> request(
                new TEvBlobStorage::TEvControllerShredRequest(Self->DataErasureManager->GetGeneration()));

            Self->PipeClientCache->Send(ctx, MakeBSControllerID(), request.release());
        }
    }
};

NTabletFlatExecutor::ITransaction* TSchemeShard::CreateTxCompleteDataErasureTenant(TEvSchemeShard::TEvTenantDataErasureResponse::TPtr& ev) {
    return new TTxCompleteDataErasureTenant(this, ev);
}

struct TSchemeShard::TTxCompleteDataErasureBSC : public TSchemeShard::TRwTxBase {
    const TEvBlobStorage::TEvControllerShredResponse::TPtr Ev;

    TTxCompleteDataErasureBSC(TSelf* self, const TEvBlobStorage::TEvControllerShredResponse::TPtr& ev)
        : TRwTxBase(self)
        , Ev(std::move(ev))
    {}

    TTxType GetTxType() const override { return TXTYPE_COMPLETE_DATA_ERASURE_BSC; }

    void DoExecute(TTransactionContext& txc, const TActorContext& ctx) override {
        LOG_DEBUG_S(ctx, NKikimrServices::FLAT_TX_SCHEMESHARD,
            "TTxCompleteDataErasureBSC Execute at schemeshard: " << Self->TabletID());

        const auto& record = Ev->Get()->Record;
        if (record.GetCurrentGeneration() != Self->DataErasureManager->GetGeneration()) {
            LOG_INFO_S(ctx, NKikimrServices::FLAT_TX_SCHEMESHARD, "Handle TEvControllerShredResponse: Get unexpected generation " << record.GetCurrentGeneration());
            return;
        }

        NIceDb::TNiceDb db(txc.DB);
        if (record.GetCompleted()) {
            LOG_INFO_S(ctx, NKikimrServices::FLAT_TX_SCHEMESHARD, "Handle TEvControllerShredResponse: Data shred in BSC is completed");
            Self->DataErasureManager->Complete();
            db.Table<Schema::DataErasureStarts>().Key(Self->DataErasureManager->GetGeneration()).Update<Schema::DataErasureStarts::Status>(static_cast<ui32>(Self->DataErasureManager->GetStatus()));
        } else {
            LOG_INFO_S(ctx, NKikimrServices::FLAT_TX_SCHEMESHARD, "Handle TEvControllerShredResponse: Progress data shred in BSC " << record.GetProgress10k());
        }
    }

    void DoComplete(const TActorContext& ctx) override {
        LOG_DEBUG_S(ctx, NKikimrServices::FLAT_TX_SCHEMESHARD,
            "TTxCompleteDataErasureBSC Complete at schemeshard: " << Self->TabletID());

        if (Self->DataErasureManager->GetStatus() == TDataErasureManager::EStatus::IN_PROGRESS_BSC) {
            Self->DataErasureManager->ScheduleRequestToBSC();
        }
    }
};

NTabletFlatExecutor::ITransaction* TSchemeShard::CreateTxCompleteDataErasureBSC(TEvBlobStorage::TEvControllerShredResponse::TPtr& ev) {
    return new TTxCompleteDataErasureBSC(this, ev);
}

} // NKikimr::NSchemeShard {
