#include <ydb/core/testlib/test_client.h>
#include <ydb/library/testlib/service_mocks/token_agent_mock.h>
#include <ydb/library/testlib/service_mocks/access_service_mock.h>
#include <ydb/library/grpc/actor_client/grpc_service_settings.h>
#include <ydb/library/ycloud/impl/token_agent.h>  //////////////////
#include <library/cpp/testing/unittest/registar.h>
#include <library/cpp/testing/unittest/tests_data.h>
#include <ydb/core/security/token_manager/kikimr_token_manager.h>

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
    // THolder<NClient::TKikimr> Kikimr;
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
        // Kikimr = MakeHolder<NClient::TKikimr>(Client->GetClientConfig());
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

struct TAuthConfigSettings {
    struct TProviderSettings {
        TString SuccessRefreshPeriod = "1h";
        TString MinErrorRefreshPeriod = "1s";
        TString MaxErrorRefreshPeriod = "1m";
        TString RequestTimeout = "1m";
    };


    struct TTokenAgentProviderInitializer {
        struct TTokenAgentInfoSettings {
            TString Id;
            TString Tag;
        };

        struct TSettings {
            TProviderSettings CommonSettings;
            TString Endpoint;
        };

        std::vector<TTokenAgentInfoSettings> TokenAgentInfo;
        TSettings Settings;
    };

    bool UseBlackBox = false;
    bool UseStaff = false;
    bool UseAccessServiceTLS = false;
    TString AccessServiceEndpoint;
    TString AccessServiceTokenName;
    bool EnableTokenManager = true;
    TTokenAgentProviderInitializer TokenAgentProviderInitializer;
};

NKikimrProto::TAuthConfig CreateAuthConfig(const TAuthConfigSettings& authConfigSettings) {
    NKikimrProto::TAuthConfig authConfig;
    authConfig.SetUseBlackBox(authConfigSettings.UseBlackBox);
    authConfig.SetUseAccessService(true);
    authConfig.SetAccessServiceType("Yandex_v2");
    authConfig.SetUseAccessServiceApiKey(false);
    authConfig.SetUseAccessServiceTLS(authConfigSettings.UseAccessServiceTLS);
    authConfig.SetAccessServiceEndpoint(authConfigSettings.AccessServiceEndpoint);
    authConfig.SetUseStaff(authConfigSettings.UseStaff);
    authConfig.SetAccessServiceTokenName(authConfigSettings.AccessServiceTokenName);
    auto tokenManagerConfig = authConfig.MutableTokenManager();
    tokenManagerConfig->SetEnable(authConfigSettings.EnableTokenManager);
    auto tokenAgentProviderConfig = tokenManagerConfig->MutableTokenAgentProvider();
    auto tokenAgentInfos = tokenAgentProviderConfig->MutableProvidersInfo();
    const auto& tokenAgentInitializer = authConfigSettings.TokenAgentProviderInitializer;
    for (const auto& infoSettings : tokenAgentInitializer.TokenAgentInfo) {
        auto tokenAgentInfo = tokenAgentInfos->Add();
        tokenAgentInfo->SetId(infoSettings.Id);
        tokenAgentInfo->SetTag(infoSettings.Tag);
    }
    auto tokenAgentProviderSettings = tokenAgentProviderConfig->MutableSettings();
    tokenAgentProviderSettings->SetEndpoint(tokenAgentInitializer.Settings.Endpoint);
    auto commonSettings = tokenAgentProviderSettings->MutableCommonSettings();
    const auto& commonSettingsInitializer = tokenAgentInitializer.Settings.CommonSettings;
    commonSettings->SetSuccessRefreshPeriod(commonSettingsInitializer.SuccessRefreshPeriod);
    commonSettings->SetMinErrorRefreshPeriod(commonSettingsInitializer.MinErrorRefreshPeriod);
    commonSettings->SetMaxErrorRefreshPeriod(commonSettingsInitializer.MaxErrorRefreshPeriod);
    commonSettings->SetRequestTimeout(commonSettingsInitializer.RequestTimeout);
    return authConfig;
}

} // namespace

Y_UNIT_TEST_SUITE(TokenAgentProvider) {

Y_UNIT_TEST(CanGetTokenFromTokenAgent) {
    TPortManager tp;
    const ui16 port = tp.GetPort(2134);
    // const ui16 accessServicePort = tp.GetPort(4284);
    // const TString accessServiceEndpoint = "localhost:" + ToString(accessServicePort);
    const ui16 tokenAgentPort = tp.GetPort(6543);
    const TString tokenAgentEndpoint = "localhost:" + ToString(tokenAgentPort);

    // // Create Access Service mock
    // TTicketParserAccessServiceMockV2 accessServiceMock;
    // grpc::ServerBuilder accessServiceServerBuilder;
    // accessServiceServerBuilder.AddListeningPort(accessServiceEndpoint, grpc::InsecureServerCredentials()).RegisterService(&accessServiceMock);
    // std::unique_ptr<grpc::Server> accessServer(accessServiceServerBuilder.BuildAndStart());
    // accessServiceMock.AllowedServiceAuthTokens = {"Bearer ydb.service.token"};

    // Create Token Agent mock
    TTokenAgentMock tokenAgentMock;
    grpc::ServerBuilder tokenAgentServerBuilder;
    tokenAgentServerBuilder.AddListeningPort(tokenAgentEndpoint, grpc::InsecureServerCredentials()).RegisterService(&tokenAgentMock);
    std::unique_ptr<grpc::Server> tokenAgentServer(tokenAgentServerBuilder.BuildAndStart());

    const TString accessServiceTokenName = "token-for-access-service";

    NKikimrProto::TAuthConfig authConfig = CreateAuthConfig({
        .UseBlackBox = false,
        .UseStaff = false,
        .UseAccessServiceTLS = false,
        // .AccessServiceEndpoint = accessServiceEndpoint,
        .AccessServiceTokenName = accessServiceTokenName,
        .EnableTokenManager = true,
        .TokenAgentProviderInitializer = {
            .TokenAgentInfo = {
                {
                    .Id = accessServiceTokenName,
                    .Tag = "ydb-service"
                }
            },
            .Settings = {
                .CommonSettings = {},
                .Endpoint = tokenAgentEndpoint
            }
        }
    });

    auto settings = TServerSettings(port, authConfig);
    settings.SetEnableAccessServiceBulkAuthorization(true);
    settings.SetDomainName("Root");
    // settings.CreateTicketParser = NKikimr::CreateTicketParser;
    settings.CreateTokenManager = NKikimrYndx::CreateTokenManager;

    TServer server(settings);
    TTestActorRuntime* runtime = server.GetRuntime();
    // runtime->SetLogPriority(NKikimrServices::TICKET_PARSER, NLog::PRI_TRACE);
    runtime->SetLogPriority(NKikimrServices::GRPC_CLIENT, NLog::PRI_TRACE);
    runtime->SetLogPriority(NKikimrServices::TOKEN_MANAGER, NLog::PRI_TRACE);

    TActorId fakeTicketParser = runtime->AllocateEdgeActor();
    runtime->Send(new IEventHandle(MakeTokenManagerID(), fakeTicketParser, new TEvTokenManager::TEvSubscribeUpdateToken(accessServiceTokenName)));
    TAutoPtr<IEventHandle> handle;
    TEvTokenManager::TEvUpdateToken* updateTokenEv = runtime->GrabEdgeEvent<TEvTokenManager::TEvUpdateToken>(handle);

    if (updateTokenEv->Status.Code == TEvTokenManager::TStatus::ECode::NOT_READY) {
        updateTokenEv = runtime->GrabEdgeEvent<TEvTokenManager::TEvUpdateToken>(handle);
    }
    UNIT_ASSERT_STRINGS_EQUAL_C(updateTokenEv->Id, accessServiceTokenName, updateTokenEv->Id);
    UNIT_ASSERT_EQUAL(updateTokenEv->Status.Code, TEvTokenManager::TStatus::ECode::SUCCESS);
    UNIT_ASSERT(!updateTokenEv->Token.empty());
    UNIT_ASSERT_STRINGS_EQUAL(updateTokenEv->Token, "ydb.service.token");
}

Y_UNIT_TEST(CanRefreshTokenFromTokenAgent) {
    TPortManager tp;
    const ui16 port = tp.GetPort(2134);
    const ui16 tokenAgentPort = tp.GetPort(6543);
    const TString tokenAgentEndpoint = "localhost:" + ToString(tokenAgentPort);

    // Create Token Agent mock
    TTokenAgentMock tokenAgentMock;
    grpc::ServerBuilder tokenAgentServerBuilder;
    tokenAgentServerBuilder.AddListeningPort(tokenAgentEndpoint, grpc::InsecureServerCredentials()).RegisterService(&tokenAgentMock);
    std::unique_ptr<grpc::Server> tokenAgentServer(tokenAgentServerBuilder.BuildAndStart());

    const TString accessServiceTokenName = "token-for-access-service";

    NKikimrProto::TAuthConfig authConfig = CreateAuthConfig({
        .UseBlackBox = false,
        .UseStaff = false,
        .UseAccessServiceTLS = false,
        .AccessServiceTokenName = accessServiceTokenName,
        .EnableTokenManager = true,
        .TokenAgentProviderInitializer = {
            .TokenAgentInfo = {
                {
                    .Id = accessServiceTokenName,
                    .Tag = "ydb-service"
                }
            },
            .Settings = {
                .CommonSettings = {
                    .SuccessRefreshPeriod = "1s"
                },
                .Endpoint = tokenAgentEndpoint
            }
        }
    });

    auto settings = TServerSettings(port, authConfig);
    settings.SetEnableAccessServiceBulkAuthorization(true);
    settings.SetDomainName("Root");
    settings.CreateTokenManager = NKikimrYndx::CreateTokenManager;

    TServer server(settings);
    TTestActorRuntime* runtime = server.GetRuntime();
    runtime->SetLogPriority(NKikimrServices::GRPC_CLIENT, NLog::PRI_TRACE);
    runtime->SetLogPriority(NKikimrServices::TOKEN_MANAGER, NLog::PRI_TRACE);

    TActorId fakeTicketParser = runtime->AllocateEdgeActor();
    runtime->Send(new IEventHandle(MakeTokenManagerID(), fakeTicketParser, new TEvTokenManager::TEvSubscribeUpdateToken(accessServiceTokenName)));
    TAutoPtr<IEventHandle> handle;
    TEvTokenManager::TEvUpdateToken* updateTokenEv = runtime->GrabEdgeEvent<TEvTokenManager::TEvUpdateToken>(handle);

    if (updateTokenEv->Status.Code == TEvTokenManager::TStatus::ECode::NOT_READY) {
        updateTokenEv = runtime->GrabEdgeEvent<TEvTokenManager::TEvUpdateToken>(handle);
    }
    UNIT_ASSERT_STRINGS_EQUAL_C(updateTokenEv->Id, accessServiceTokenName, updateTokenEv->Id);
    UNIT_ASSERT_EQUAL(updateTokenEv->Status.Code, TEvTokenManager::TStatus::ECode::SUCCESS);
    UNIT_ASSERT(!updateTokenEv->Token.empty());
    UNIT_ASSERT_STRINGS_EQUAL(updateTokenEv->Token, "ydb.service.token");

    // refresh token in token agent for 'ydb-service'
    tokenAgentMock.Tokens = {{"ydb-service", "ydb.service.token-refreshed"}};
    updateTokenEv = runtime->GrabEdgeEvent<TEvTokenManager::TEvUpdateToken>(handle);
    UNIT_ASSERT_STRINGS_EQUAL_C(updateTokenEv->Id, accessServiceTokenName, updateTokenEv->Id);
    UNIT_ASSERT_EQUAL(updateTokenEv->Status.Code, TEvTokenManager::TStatus::ECode::SUCCESS);
    UNIT_ASSERT(!updateTokenEv->Token.empty());
    UNIT_ASSERT_STRINGS_EQUAL(updateTokenEv->Token, "ydb.service.token-refreshed");
}

Y_UNIT_TEST(CanHandleErrorsFromTokenAgent) {
    TPortManager tp;
    const ui16 port = tp.GetPort(2134);
    const ui16 tokenAgentPort = tp.GetPort(6543);
    const TString tokenAgentEndpoint = "localhost:" + ToString(tokenAgentPort);

    // Create Token Agent mock
    TTokenAgentMock tokenAgentMock;
    grpc::ServerBuilder tokenAgentServerBuilder;
    tokenAgentServerBuilder.AddListeningPort(tokenAgentEndpoint, grpc::InsecureServerCredentials()).RegisterService(&tokenAgentMock);
    std::unique_ptr<grpc::Server> tokenAgentServer(tokenAgentServerBuilder.BuildAndStart());

    const TString accessServiceTokenName = "token-for-access-service";

    NKikimrProto::TAuthConfig authConfig = CreateAuthConfig({
        .UseBlackBox = false,
        .UseStaff = false,
        .UseAccessServiceTLS = false,
        .AccessServiceTokenName = accessServiceTokenName,
        .EnableTokenManager = true,
        .TokenAgentProviderInitializer = {
            .TokenAgentInfo = {
                {
                    .Id = accessServiceTokenName,
                    .Tag = "ydb-service"
                }
            },
            .Settings = {
                .CommonSettings = {
                    .SuccessRefreshPeriod = "1s"
                },
                .Endpoint = tokenAgentEndpoint
            }
        }
    });

    auto settings = TServerSettings(port, authConfig);
    settings.SetEnableAccessServiceBulkAuthorization(true);
    settings.SetDomainName("Root");
    settings.CreateTokenManager = NKikimrYndx::CreateTokenManager;

    TServer server(settings);
    TTestActorRuntime* runtime = server.GetRuntime();
    runtime->SetLogPriority(NKikimrServices::GRPC_CLIENT, NLog::PRI_TRACE);
    runtime->SetLogPriority(NKikimrServices::TOKEN_MANAGER, NLog::PRI_TRACE);

    TActorId fakeTicketParser = runtime->AllocateEdgeActor();
    runtime->Send(new IEventHandle(MakeTokenManagerID(), fakeTicketParser, new TEvTokenManager::TEvSubscribeUpdateToken(accessServiceTokenName)));
    TAutoPtr<IEventHandle> handle;
    TEvTokenManager::TEvUpdateToken* updateTokenEv = runtime->GrabEdgeEvent<TEvTokenManager::TEvUpdateToken>(handle);

    if (updateTokenEv->Status.Code == TEvTokenManager::TStatus::ECode::NOT_READY) {
        updateTokenEv = runtime->GrabEdgeEvent<TEvTokenManager::TEvUpdateToken>(handle);
    }
    updateTokenEv = runtime->GrabEdgeEvent<TEvTokenManager::TEvUpdateToken>(handle);
    UNIT_ASSERT_STRINGS_EQUAL_C(updateTokenEv->Id, accessServiceTokenName, updateTokenEv->Id);
    UNIT_ASSERT_EQUAL(updateTokenEv->Status.Code, TEvTokenManager::TStatus::ECode::SUCCESS);
    UNIT_ASSERT(!updateTokenEv->Token.empty());
    UNIT_ASSERT_STRINGS_EQUAL(updateTokenEv->Token, "ydb.service.token");


    // retry error for token agent
    tokenAgentMock.ReturnUnavailable = true;
    updateTokenEv = runtime->GrabEdgeEvent<TEvTokenManager::TEvUpdateToken>(handle);
    UNIT_ASSERT_STRINGS_EQUAL_C(updateTokenEv->Id, accessServiceTokenName, updateTokenEv->Id);
    UNIT_ASSERT_EQUAL(updateTokenEv->Status.Code, TEvTokenManager::TStatus::ECode::ERROR);
    // return old token
    UNIT_ASSERT(!updateTokenEv->Token.empty());
    UNIT_ASSERT_STRINGS_EQUAL(updateTokenEv->Token, "ydb.service.token");

    // refresh token in token agent for 'ydb-service'
    tokenAgentMock.ReturnUnavailable = false;
    tokenAgentMock.Tokens = {{"ydb-service", "ydb.service.token-refreshed"}};
    updateTokenEv = runtime->GrabEdgeEvent<TEvTokenManager::TEvUpdateToken>(handle);
    UNIT_ASSERT_STRINGS_EQUAL_C(updateTokenEv->Id, accessServiceTokenName, updateTokenEv->Id);
    UNIT_ASSERT_EQUAL(updateTokenEv->Status.Code, TEvTokenManager::TStatus::ECode::SUCCESS);
    UNIT_ASSERT(!updateTokenEv->Token.empty());
    UNIT_ASSERT_STRINGS_EQUAL(updateTokenEv->Token, "ydb.service.token-refreshed");
}

}
