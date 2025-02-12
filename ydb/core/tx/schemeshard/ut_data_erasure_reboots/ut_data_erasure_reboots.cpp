#include <ydb/core/tx/schemeshard/ut_helpers/helpers.h>
#include <ydb/core/mind/bscontroller/bsc.h>

using namespace NKikimr;
using namespace NSchemeShard;
using namespace NSchemeShardUT_Private;

namespace {

ui64 CreateTestSubdomain(TTestActorRuntime& runtime,
    TTestEnv& env,
    ui64* txId,
    const TString& name) {
    TestCreateExtSubDomain(runtime, ++(*txId), "/MyRoot", Sprintf(R"(
        Name: "%s"
    )", name.c_str()));
    env.TestWaitNotification(runtime, *txId);

    TestAlterExtSubDomain(runtime, ++(*txId), "/MyRoot", Sprintf(R"(
        PlanResolution: 50
        Coordinators: 1
        Mediators: 1
        TimeCastBucketsPerMediator: 2
        ExternalSchemeShard: true
        ExternalHive: false
        Name: "%s"
        StoragePools {
            Name: "name_%s_kind_hdd-1"
            Kind: "common"
        }
        StoragePools {
            Name: "name_%s_kind_hdd-2"
            Kind: "external"
        }
    )", name.c_str(), name.c_str(), name.c_str()));
    env.TestWaitNotification(runtime, *txId);

    ui64 schemeshardId;
    TestDescribeResult(DescribePath(runtime, TStringBuilder() << "/MyRoot/" << name), {
        NLs::PathExist,
        NLs::ExtractTenantSchemeshard(&schemeshardId)
    });

    TestCreateTable(runtime, schemeshardId, ++(*txId), TStringBuilder() << "/MyRoot/" << name,
        R"____(
            Name: "Simple"
            Columns { Name: "key1"  Type: "Uint32"}
            Columns { Name: "Value" Type: "Utf8"}
            KeyColumnNames: ["key1"]
            UniformPartitionsCount: 2
        )____");
    env.TestWaitNotification(runtime, *txId, schemeshardId);

    return schemeshardId;
}
} // namespace

Y_UNIT_TEST_SUITE(TSchemeShardTestDataErasureReboots) {
    Y_UNIT_TEST(Fake) {
    }

    Y_UNIT_TEST(Test1) {
        TTestWithReboots t;
        t.Run([&](TTestActorRuntime& runtime, bool& activeZone) {
            runtime.SetLogPriority(NKikimrServices::TX_PROXY, NLog::PRI_DEBUG);
            runtime.SetLogPriority(NKikimrServices::FLAT_TX_SCHEMESHARD, NActors::NLog::PRI_TRACE);

            CreateTestBootstrapper(runtime, CreateTestTabletInfo(MakeBSControllerID(), TTabletTypes::BSController),
                        &CreateFlatBsController);

            runtime.GetAppData().FeatureFlags.SetEnableDataErasure(true);
            auto& dataErasureConfig = runtime.GetAppData().DataErasureConfig;
            dataErasureConfig.SetDataErasureIntervalSeconds(2);
            dataErasureConfig.SetBlobStorageControllerRequestIntervalSeconds(1);

            auto sender = runtime.AllocateEdgeActor();
            RebootTablet(runtime, TTestTxConfig::SchemeShard, sender);

            ui64 txId = 100;

            CreateTestSubdomain(runtime, *(t.TestEnv), &txId, "Database1");
            // CreateTestSubdomain(runtime, *(t.TestEnv), &txId, "Database2");

            {
                TInactiveZone inactive(activeZone);
                t.TestEnv->SimulateSleep(runtime, TDuration::Seconds(3));

                auto request = MakeHolder<TEvSchemeShard::TEvDataErasureInfoRequest>();
                runtime.SendToPipe(TTestTxConfig::SchemeShard, sender, request.Release(), 0, GetPipeConfigWithRetries());

                TAutoPtr<IEventHandle> handle;
                auto response = runtime.GrabEdgeEventRethrow<TEvSchemeShard::TEvDataErasureInfoResponse>(handle);

                UNIT_ASSERT_EQUAL(response->Record.GetGeneration(), 1);
                UNIT_ASSERT_EQUAL(response->Record.GetStatus(), NKikimrScheme::TEvDataErasureInfoResponse::COMPLETED);
            }
        });
    }
}
