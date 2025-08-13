#include <library/cpp/testing/unittest/registar.h>
#include <library/cpp/testing/unittest/tests_data.h>
#include <ydb/core/util/actorsys_test/testactorsys.h>
#include <ydb/library/testlib/service_mocks/token_agent_mock.h>
#include "token_agent.h"


Y_UNIT_TEST_SUITE(TTokenAgent) {

Y_UNIT_TEST(CanGetIamToken) {
    using namespace NKikimr;

    TTestActorSystem runtime(1, NLog::PRI_ERROR, MakeIntrusive<TDomainsInfo>());
    runtime.Start();
    ui32 nodeId = 1;
    auto &appData = runtime.GetNode(1)->AppData;
    appData->DomainsInfo->AddDomain(TDomainsInfo::TDomain::ConstructEmptyDomain("dom", 1).Release());

    runtime.SetLogPriority(NKikimrServices::GRPC_CLIENT, NLog::PRI_TRACE);

    TPortManager tp;
    const ui16 tokenAgentPort = tp.GetPort(4284);
    const TString tokenAgentEndpoint = "localhost:" + ToString(tokenAgentPort);

    // Create Token Agent mock
    TTokenAgentMock tokenAgentMock;
    grpc::ServerBuilder builder;
    builder.AddListeningPort(tokenAgentEndpoint, grpc::InsecureServerCredentials()).RegisterService(&tokenAgentMock);
    std::unique_ptr<grpc::Server> accessServer(builder.BuildAndStart());

    IActor* tokenAgentActor = NCloud::CreateTokenAgent(tokenAgentEndpoint);
    TActorId tokenAgentId = runtime.Register(tokenAgentActor, nodeId);

    NActors::TActorId sender = runtime.AllocateEdgeActor(nodeId);
    std::unique_ptr<NCloud::NEvTokenAgent::TEvGetTokenRequest> request = std::make_unique<NCloud::NEvTokenAgent::TEvGetTokenRequest>();
    request->Request.Settag("ydb-service");
    runtime.Send(new IEventHandle(tokenAgentId, sender, request.release()), nodeId);

    auto getTokenResponseEvPtr = runtime.WaitForEdgeActorEvent<NCloud::NEvTokenAgent::TEvGetTokenResponse>(sender, false);
    auto getTokenResponseEv = getTokenResponseEvPtr->Get();
    UNIT_ASSERT(getTokenResponseEv->Status.Ok());
    UNIT_ASSERT_STRINGS_EQUAL_C(getTokenResponseEv->Response.Getiam_token(), "ydb.service.token", getTokenResponseEv->Response.Getiam_token());
}

}
