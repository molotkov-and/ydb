#include <ydb/library/actors/core/event.h>
#include <ydb/library/actors/core/event_local.h>
#include <ydb/core/testlib/test_client.h>
#include <ydb/library/testlib/service_mocks/token_agent_mock.h>
#include <ydb/library/grpc/server/grpc_server.h>
#include <ydb/library/grpc/actor_client/grpc_service_settings.h>
#include <ydb/public/lib/deprecated/kicli/kicli.h>
#include <library/cpp/testing/unittest/registar.h>
#include <library/cpp/testing/unittest/tests_data.h>
#include <util/string/builder.h>
#include "token_agent.h"

using namespace NKikimr;
using namespace Tests;

namespace {

struct TTestSetup {
    TPortManager PortManager;
    ui16 KikimrPort;
    ui16 ServicePort;

    // Kikimr
    THolder<TServer> Server;
    THolder<TClient> Client;
    THolder<NClient::TKikimr> Kikimr;
    TActorId EdgeActor;
    IActor* TokenAgentActor = nullptr;

    // Access service
    TTokenAgentMock TokenAgentMock;
    std::unique_ptr<grpc::Server> TokenAgentServer;

    TTestSetup()
        : KikimrPort(PortManager.GetPort(2134))
        , ServicePort(PortManager.GetPort(4286))
    {
        StartKikimr();
        StartAccessService();
    }

    TTestActorRuntime* GetRuntime() {
        return Server->GetRuntime();
    }

    void StartKikimr() {
        NKikimrProto::TAuthConfig authConfig;
        auto settings = TServerSettings(KikimrPort, authConfig);
        settings.SetDomainName("Root");
        Server = MakeHolder<TServer>(settings);
        Server->GetRuntime()->SetLogPriority(NKikimrServices::GRPC_CLIENT, NLog::PRI_DEBUG);
        Client = MakeHolder<TClient>(settings);
        Kikimr = MakeHolder<NClient::TKikimr>(Client->GetClientConfig());
        Client->InitRootScheme();
        EdgeActor = GetRuntime()->AllocateEdgeActor();

        //AccessServiceActor = NCloud::CreateAccessService("localhost:" + ToString(ServicePort));
        NGrpcActorClient::TGrpcClientSettings sets;
        sets.Endpoint = "localhost:" + ToString(ServicePort);
        TokenAgentActor = NCloud::CreateTokenAgent(sets);
        GetRuntime()->Register(TokenAgentActor);
    }

    void StartAccessService() {
        grpc::ServerBuilder builder;
        builder.AddListeningPort("[::]:" + ToString(ServicePort), grpc::InsecureServerCredentials()).RegisterService(&TokenAgentMock);
        TokenAgentServer = builder.BuildAndStart();
    }
};

} // namespace

Y_UNIT_TEST_SUITE(TTokenAgent) {

Y_UNIT_TEST(CanGetIamToken) {
    TTestSetup setup;
    TAutoPtr<IEventHandle> handle;

    // check for not found
    auto request = MakeHolder<NCloud::NEvTokenAgent::TEvGetTokenRequest>();
    request->Request.set_tag("other-service");
    setup.GetRuntime()->Send(new IEventHandle(setup.TokenAgentActor->SelfId(), setup.EdgeActor, request.Release()));
    auto result = setup.GetRuntime()->GrabEdgeEvent<NCloud::NEvTokenAgent::TEvGetTokenResponse>(handle);
    UNIT_ASSERT(result);
    UNIT_ASSERT_VALUES_EQUAL(result->Status.Msg, "Access Denied");

    // check for found
    request = MakeHolder<NCloud::NEvTokenAgent::TEvGetTokenRequest>();
    request->Request.set_tag("ydb-service");
    setup.GetRuntime()->Send(new IEventHandle(setup.TokenAgentActor->SelfId(), setup.EdgeActor, request.Release()));
    result = setup.GetRuntime()->GrabEdgeEvent<NCloud::NEvTokenAgent::TEvGetTokenResponse>(handle);
    UNIT_ASSERT(result);
    UNIT_ASSERT(result->Status.Ok());
    UNIT_ASSERT_VALUES_EQUAL(result->Response.iam_token(), "ydb.service.token");
}

}
