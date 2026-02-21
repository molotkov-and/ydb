#pragma once

#include <ydb/core/tx/schemeshard/operation_queue_timer.h>
#include <ydb/core/tx/schemeshard/schemeshard_identificators.h>
#include <ydb/core/tx/schemeshard/schemeshard_private.h>
#include <ydb/core/tx/schemeshard/schemeshard_types.h>
#include <ydb/core/util/circular_queue.h>

#include <util/generic/ptr.h>

namespace NKikimrConfig {

class TDataErasureConfig;

} // NKikimrConfig

namespace NKikimr::NSchemeShard {

class TSchemeShard;

class TTenantShredManager {
private:
using TQueue = NOperationQueue::TOperationQueueWithTimer<
    TShardIdx,
    TFifoQueue<TShardIdx>,
    TEvPrivate::EvRunTenantShred,
    NKikimrServices::FLAT_TX_SCHEMESHARD,
    NKikimrServices::TActivity::SCHEMESHARD_TENANT_SHRED>;

    class TStarter : public TQueue::IStarter {
    public:
        TStarter(TTenantShredManager* const manager);

        NOperationQueue::EStartStatus StartOperation(const TShardIdx& shardIdx) override;
        void OnTimeout(const TShardIdx& shardIdx) override;

    private:
        TTenantShredManager* const Manager;
    };

private:
    TSchemeShard* const SchemeShard = nullptr;
    EShredStatus Status = EShredStatus::COMPLETED;
    ui64 Generation = 0;
    TStarter Starter;
    TQueue* Queue = nullptr;
    THashMap<TShardIdx, EShredStatus> WaitingShredShards;
    THashMap<TShardIdx, TActorId> ActivePipes;

public:
    TTenantShredManager(TSchemeShard* const schemeShard, const NKikimrConfig::TDataErasureConfig& config);

    // void UpdateConfig(const NKikimrConfig::TDataErasureConfig& config) override;
    void Start();
    // void Stop() override;
    // void ClearOperationQueue() override;
    // void ClearWaitingShredRequests(NIceDb::TNiceDb& db) override;
    // void ClearWaitingShredRequests() override;
    // void WakeupToRunShred(TEvSchemeShard::TEvWakeupToRunShred::TPtr& ev, const NActors::TActorContext& ctx) override;
    void StartShred(NIceDb::TNiceDb& db, ui64 newGen);
    // void Continue() override;
    void HandleDisconnect(TTabletId tabletId, const TActorId& clientId, const TActorContext& ctx);
    void FinishShred(NIceDb::TNiceDb& db, const TTabletId& tabletId);
    void RetryShred(const TTabletId& tabletId);
    // void ScheduleRequestToBSC() override;
    // void SendRequestToBSC() override;
    // void Complete() override;
    // bool Restore(NIceDb::TNiceDb& db) override;
    // bool Remove(const TPathId& pathId) override;
    // bool Remove(const TShardIdx& shardIdx) override;
    // void HandleNewPartitioning(const std::vector<TShardIdx>& shredShards, NIceDb::TNiceDb& db) override;
    // void SyncBscGeneration(NIceDb::TNiceDb& db, ui64 currentBscGeneration) override;

    ui64 GetGeneration() const {return Generation;}
    void SetGeneration(ui64 newGeneration) {Generation = newGeneration;}
    EShredStatus GetStatus() const {return Status;}
    void SetStatus(const EShredStatus& status) {Status = status;}

private:
    static TQueue::TConfig ConvertConfig(const NKikimrConfig::TDataErasureConfig& config);

    NOperationQueue::EStartStatus StartShredOperation(const TShardIdx& shardIdx);
    // void OnTimeout(const TShardIdx& shardIdx);
    // bool Enqueue(const TShardIdx& shardIdx);
    // void UpdateMetrics();
    // void SendResponseToRootSchemeShard();
};

} // NKikimr::NSchemeShard
