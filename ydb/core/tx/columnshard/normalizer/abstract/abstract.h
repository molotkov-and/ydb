#pragma once

#include <ydb/core/tablet_flat/tablet_flat_executor.h>
#include <ydb/core/tx/columnshard/blobs_action/abstract/storages_manager.h>
#include <ydb/core/tx/columnshard/data_accessor/manager.h>
#include <ydb/core/tx/columnshard/resource_subscriber/task.h>

#include <ydb/library/accessor/accessor.h>
#include <ydb/library/conclusion/result.h>

#include <library/cpp/object_factory/object_factory.h>

namespace NKikimr::NIceDb {
class TNiceDb;
}

namespace NKikimr::NOlap {

class TNormalizerCounters: public NColumnShard::TCommonCountersOwner {
    using TBase = NColumnShard::TCommonCountersOwner;

    NMonitoring::TDynamicCounters::TCounterPtr ObjectsCount;
    NMonitoring::TDynamicCounters::TCounterPtr StartedCount;
    NMonitoring::TDynamicCounters::TCounterPtr FinishedCount;
    NMonitoring::TDynamicCounters::TCounterPtr FailedCount;

public:
    TNormalizerCounters(const TString& normalizerName)
        : TBase("Normalizer") {
        DeepSubGroup("normalizer", normalizerName);

        ObjectsCount = TBase::GetDeriviative("Objects/Count");
        StartedCount = TBase::GetDeriviative("Started");
        FinishedCount = TBase::GetDeriviative("Finished");
        FailedCount = TBase::GetDeriviative("Failed");
    }

    void CountObjects(const ui64 objectsCount) const {
        ObjectsCount->Add(objectsCount);
    }

    void OnNormalizerStart() const {
        StartedCount->Add(1);
    }

    void OnNormalizerFinish() const {
        FinishedCount->Add(1);
    }

    void OnNormalizerFails() const {
        FailedCount->Add(1);
    }
};

// DONT REMOVE AND DONT CHANGE PLACES! PERSISTENT! ADD Deprecated PREFIX FOR REMOVED NORMALIZER
enum class ENormalizerSequentialId : ui32 {
    Granules = 1,
    Chunks,
    DeprecatedPortionsCleaner,
    TablesCleaner,
    DeprecatedPortionsMetadata,
    CleanGranuleId,
    DeprecatedEmptyPortionsCleaner,
    DeprecatedCleanInsertionDedup,
    GCCountersNormalizer,
    DeprecatedRestorePortionFromChunks,
    SyncPortionFromChunks,
    DeprecatedRestoreV1Chunks,
    DeprecatedSyncMinSnapshotFromChunks,
    DeprecatedRestoreV1Chunks_V1,
    RestoreV1Chunks_V2,
    RestoreV2Chunks,
    CleanDeprecatedSnapshot,
    RestoreV0ChunksMeta,
    CopyBlobIdsToV2,

    MAX
};

class TNormalizationContext {
    YDB_ACCESSOR_DEF(TActorId, ResourceSubscribeActor);
    YDB_ACCESSOR_DEF(TActorId, ShardActor);
    std::shared_ptr<NOlap::NResourceBroker::NSubscribe::TResourcesGuard> ResourcesGuard;

public:
    void SetResourcesGuard(std::shared_ptr<NOlap::NResourceBroker::NSubscribe::TResourcesGuard> rg) {
        ResourcesGuard = rg;
    }
};

class TNormalizationController;

class INormalizerTask {
public:
    using TPtr = std::shared_ptr<INormalizerTask>;
    virtual ~INormalizerTask() {
    }

    virtual void Start(const TNormalizationController& controller, const TNormalizationContext& nCtx) = 0;
};

class INormalizerChanges {
public:
    using TPtr = std::shared_ptr<INormalizerChanges>;
    virtual ~INormalizerChanges() {
    }

    virtual bool ApplyOnExecute(NTabletFlatExecutor::TTransactionContext& txc, const TNormalizationController& normalizationContext) const = 0;
    virtual void ApplyOnComplete(const TNormalizationController& normalizationContext) const {
        Y_UNUSED(normalizationContext);
    }

    virtual ui64 GetSize() const = 0;
    virtual TString DebugString() const {
        return TStringBuilder() << "size=" << GetSize();
    }
};

class TTrivialNormalizerTask: public INormalizerTask {
    INormalizerChanges::TPtr Changes;

public:
    TTrivialNormalizerTask(const INormalizerChanges::TPtr& changes)
        : Changes(changes) {
        AFL_VERIFY(Changes);
    }

    void Start(const TNormalizationController& /* controller */, const TNormalizationContext& /*nCtx*/) override;
};

class TNormalizationController {
public:
    class TInitContext {
    private:
        TIntrusiveConstPtr<TTabletStorageInfo> StorageInfo;
        const ui64 TabletId;
        const NActors::TActorId TabletActorId;

    public:
        TInitContext(TTabletStorageInfo* info, const ui64 tabletId, const NActors::TActorId& actorId)
            : StorageInfo(info)
            , TabletId(tabletId)
            , TabletActorId(actorId) {
        }

        ui64 GetTabletId() const {
            return TabletId;
        }

        const NActors::TActorId& GetTabletActorId() const {
            return TabletActorId;
        }

        TIntrusiveConstPtr<TTabletStorageInfo> GetStorageInfo() const {
            return StorageInfo;
        }
    };

    class TNormalizerFullId {
    private:
        YDB_READONLY_DEF(TString, ClassName);
        YDB_READONLY_DEF(TString, Description);

    public:
        bool operator<(const TNormalizerFullId& item) const {
            if (ClassName == item.ClassName) {
                return Description < item.Description;
            }
            return ClassName < item.ClassName;
        }

        TNormalizerFullId(const TString& className, const TString& description)
            : ClassName(className)
            , Description(description) {
        }
    };

    class INormalizerComponent {
    private:
        YDB_ACCESSOR(bool, IsRepair, false);
        YDB_ACCESSOR(bool, IsDryRun, false);
        YDB_ACCESSOR_DEF(TString, UniqueDescription);
        YDB_ACCESSOR(TString, UniqueId, TGUID::CreateTimebased().AsUuidString());

        virtual TString DoDebugString() const {
            return "";
        }

        virtual std::optional<ENormalizerSequentialId> DoGetEnumSequentialId() const = 0;

    protected:
        const ui64 TabletId;
        const NActors::TActorId TabletActorId;

    public:
        using TPtr = std::shared_ptr<INormalizerComponent>;
        using TFactory = NObjectFactory::TParametrizedObjectFactory<INormalizerComponent, TString, TInitContext>;

        virtual ~INormalizerComponent() = default;
        INormalizerComponent(const TInitContext& context)
            : TabletId(context.GetTabletId())
            , TabletActorId(context.GetTabletActorId()) {
        }

        TNormalizerFullId GetNormalizerFullId() const {
            return TNormalizerFullId(GetClassName(), UniqueDescription);
        }

        bool HasActiveTasks() const {
            return AtomicGet(ActiveTasksCount) > 0;
        }

        [[nodiscard]] ui64 DecActiveCounters() {
            const i64 result = AtomicDecrement(ActiveTasksCount);
            AFL_VERIFY(result >= 0);
            return result;
        }

        std::optional<ENormalizerSequentialId> GetEnumSequentialId() const {
            if (IsRepair) {
                return {};
            }
            return DoGetEnumSequentialId();
        }

        ENormalizerSequentialId GetEnumSequentialIdVerified() const {
            auto type = GetEnumSequentialId();
            AFL_VERIFY(!!type);
            return *type;
        }

        std::optional<ui32> GetSequentialId() const {
            auto type = GetEnumSequentialId();
            if (!type) {
                return {};
            } else {
                return (ui32)*type;
            }
        }

        ui32 GetSequentialIdVerified() const {
            auto result = GetSequentialId();
            AFL_VERIFY(!!result);
            return *result;
        }

        virtual TString GetClassName() const = 0;

        TString DebugString() const {
            TStringBuilder result;
            result << "CLASS_NAME=" << GetClassName();
            const TString details = DoDebugString();
            if (!details) {
                return result;
            } else {
                return result + ";details=" + details;
            }
        }

        TConclusion<std::vector<INormalizerTask::TPtr>> Init(
            const TNormalizationController& controller, NTabletFlatExecutor::TTransactionContext& txc);

    private:
        virtual TConclusion<std::vector<INormalizerTask::TPtr>> DoInit(
            const TNormalizationController& controller, NTabletFlatExecutor::TTransactionContext& txc) = 0;

        TAtomic ActiveTasksCount = 0;
    };
    using TPtr = std::shared_ptr<INormalizerComponent>;

private:
    std::shared_ptr<IStoragesManager> StoragesManager;
    NDataAccessorControl::TDataAccessorsManagerContainer DataAccessorsManager;
    NOlap::NResourceBroker::NSubscribe::TTaskContext TaskSubscription;

    std::deque<INormalizerComponent::TPtr> Normalizers;
    std::deque<TNormalizerCounters> Counters;
    std::set<TNormalizerFullId> FinishedNormalizers;
    std::map<TNormalizerFullId, TString> StartedNormalizers;
    YDB_READONLY_DEF(std::optional<ui32>, LastSavedNormalizerId);

private:
    INormalizerComponent::TPtr RegisterNormalizer(INormalizerComponent::TPtr normalizer);

public:
    TNormalizationController(std::shared_ptr<IStoragesManager> storagesManager,
        const std::shared_ptr<NOlap::NResourceBroker::NSubscribe::TSubscriberCounters>& counters)
        : StoragesManager(storagesManager)
        , TaskSubscription("CS::NORMALIZER", counters) {
        AFL_VERIFY(StoragesManager);
    }

    void SetDataAccessorsManager(const NDataAccessorControl::TDataAccessorsManagerContainer& dataAccessorsManager) {
        AFL_VERIFY(!DataAccessorsManager);
        AFL_VERIFY(!!dataAccessorsManager);
        DataAccessorsManager = dataAccessorsManager;
    }

    std::shared_ptr<NDataAccessorControl::IDataAccessorsManager> GetDataAccessorsManager() const {
        return DataAccessorsManager.GetObjectPtrVerified();
    }

    const NOlap::NResourceBroker::NSubscribe::TTaskContext& GetTaskSubscription() const {
        return TaskSubscription;
    }

    void InitNormalizers(const TInitContext& ctx);
    void OnNormalizerFinished(NIceDb::TNiceDb& db) const;
    void AddNormalizerEvent(NIceDb::TNiceDb& db, const TString& eventType, const TString& eventDescription) const;
    bool InitControllerState(NIceDb::TNiceDb& db);

    std::shared_ptr<IStoragesManager> GetStoragesManager() const {
        AFL_VERIFY(!!StoragesManager);
        return StoragesManager;
    }

    TString DebugString() const {
        return TStringBuilder() << "normalizers_count=" << Normalizers.size()
                                << ";current_normalizer=" << (Normalizers.size() ? Normalizers.front()->DebugString() : "NO_DATA");
    }

    const INormalizerComponent::TPtr& GetNormalizer() const;
    bool IsNormalizationFinished() const;
    bool SwitchNormalizer();
    const TNormalizerCounters& GetCounters() const;
};
}   // namespace NKikimr::NOlap
