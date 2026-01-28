#include <library/cpp/testing/unittest/tests_data.h>
#include <library/cpp/testing/unittest/registar.h>

#include <ydb/public/sdk/cpp/include/ydb-cpp-sdk/client/types/credentials/credentials.h>
#include <ydb/public/lib/ydb_cli/commands/ydb_sdk_core_access.h>

#include <ydb/core/testlib/test_client.h>
#include <ydb/library/testlib/service_mocks/ldap_mock/simple_server.h>
#include <ydb/library/testlib/service_mocks/ldap_mock/ldap_defines.h>

#include <util/system/tempfile.h>
#include "ydb_common_ut.h"

namespace NKikimr {

using namespace Tests;
using namespace NYdb;

namespace {

enum class ESecurityConnectionType {
    NON_SECURE,
    START_TLS,
    LDAPS_SCHEME,
};
struct TLdapClientOptions {
    TString CaCertFile;
    TString CertFile;
    TString KeyFile;
    ESecurityConnectionType Type = ESecurityConnectionType::NON_SECURE;
    bool IsLoginAuthenticationEnabled = true;
};

void InitLdapSettings(NKikimrProto::TLdapAuthentication* ldapSettings, ui16 ldapPort, const TLdapClientOptions& ldapClientOptions) {
    ldapSettings->SetHost("localhost");
    ldapSettings->SetPort(ldapPort);
    ldapSettings->SetBaseDn("dc=search,dc=yandex,dc=net");
    ldapSettings->SetBindDn("cn=robouser,dc=search,dc=yandex,dc=net");
    ldapSettings->SetBindPassword("robouserPassword");
    ldapSettings->SetSearchFilter("uid=$username");

    const auto setCertificate = [&ldapSettings] (bool useStartTls, const TLdapClientOptions& ldapClientOptions) {
        auto useTls = ldapSettings->MutableUseTls();
        useTls->SetEnable(useStartTls);
        useTls->SetCaCertFile(ldapClientOptions.CaCertFile);
        useTls->SetCertRequire(NKikimrProto::TLdapAuthentication::TUseTls::DEMAND);
    };

    switch (ldapClientOptions.Type) {
        case ESecurityConnectionType::NON_SECURE:
            break;
        case ESecurityConnectionType::START_TLS:
            setCertificate(true, ldapClientOptions);
            break;
        case ESecurityConnectionType::LDAPS_SCHEME:
            ldapSettings->SetScheme("ldaps");
            setCertificate(false, ldapClientOptions);
            break;
    }
}

void InitLdapSettingsWithInvalidRobotUserLogin(NKikimrProto::TLdapAuthentication* ldapSettings, ui16 ldapPort, const TLdapClientOptions& ldapClientOptions) {
    InitLdapSettings(ldapSettings, ldapPort, ldapClientOptions);
    ldapSettings->SetBindDn("cn=invalidRobouser,dc=search,dc=yandex,dc=net");
}

void InitLdapSettingsWithInvalidRobotUserPassword(NKikimrProto::TLdapAuthentication* ldapSettings, ui16 ldapPort, const TLdapClientOptions& ldapClientOptions) {
    InitLdapSettings(ldapSettings, ldapPort, ldapClientOptions);
    ldapSettings->SetBindPassword("invalidPassword");
}

void InitLdapSettingsWithInvalidFilter(NKikimrProto::TLdapAuthentication* ldapSettings, ui16 ldapPort, const TLdapClientOptions& ldapClientOptions) {
    InitLdapSettings(ldapSettings, ldapPort, ldapClientOptions);
    ldapSettings->SetSearchFilter("&(uid=$username)()");
}

void InitLdapSettingsWithUnavailableHost(NKikimrProto::TLdapAuthentication* ldapSettings, ui16 ldapPort, const TLdapClientOptions& ldapClientOptions) {
    InitLdapSettings(ldapSettings, ldapPort, ldapClientOptions);
    ldapSettings->SetHost("unavailablehost");
}

void InitLdapSettingsWithEmptyHosts(NKikimrProto::TLdapAuthentication* ldapSettings, ui16 ldapPort, const TLdapClientOptions& ldapClientOptions) {
    InitLdapSettings(ldapSettings, ldapPort, ldapClientOptions);
    ldapSettings->SetHost("");
}

void InitLdapSettingsWithEmptyBaseDn(NKikimrProto::TLdapAuthentication* ldapSettings, ui16 ldapPort, const TLdapClientOptions& ldapClientOptions) {
    InitLdapSettings(ldapSettings, ldapPort, ldapClientOptions);
    ldapSettings->SetBaseDn("");
}

void InitLdapSettingsWithEmptyBindDn(NKikimrProto::TLdapAuthentication* ldapSettings, ui16 ldapPort, const TLdapClientOptions& ldapClientOptions) {
    InitLdapSettings(ldapSettings, ldapPort, ldapClientOptions);
    ldapSettings->SetBindDn("");
}

void InitLdapSettingsWithEmptyBindPassword(NKikimrProto::TLdapAuthentication* ldapSettings, ui16 ldapPort, const TLdapClientOptions& ldapClientOptions) {
    InitLdapSettings(ldapSettings, ldapPort, ldapClientOptions);
    ldapSettings->SetBindPassword("");
}

class TLoginClientConnection {
public:
    TLoginClientConnection(std::function<void(NKikimrProto::TLdapAuthentication*, ui16, const TLdapClientOptions&)> initLdapSettings, const TLdapClientOptions& ldapClientOptions)
        : LdapClientOptions(ldapClientOptions)
        , Server(InitAuthSettings(std::move(initLdapSettings)))
        , Connection(GetDriverConfig(Server.GetPort()))
        , Client(Connection)
    {
        Server.GetRuntime()->SetLogPriority(NKikimrServices::LDAP_AUTH_PROVIDER, NLog::PRI_TRACE);
    }

    ui16 GetLdapPort() const {
        return LdapPort;
    }

    void Stop() {
        Connection.Stop(true);
    }

    std::shared_ptr<NYdb::ICoreFacility> GetCoreFacility() {
        return Client.GetCoreFacility();
    }

private:
    NKikimrConfig::TAppConfig InitAuthSettings(std::function<void(NKikimrProto::TLdapAuthentication*, ui16, const TLdapClientOptions&)>&& initLdapSettings) {
        TPortManager tp;
        LdapPort = tp.GetPort(389);

        NKikimrConfig::TAppConfig appConfig;
        auto authConfig = appConfig.MutableAuthConfig();

        authConfig->SetUseBlackBox(false);
        authConfig->SetUseLoginProvider(true);
        authConfig->SetEnableLoginAuthentication(LdapClientOptions.IsLoginAuthenticationEnabled);
        appConfig.MutableDomainsConfig()->MutableSecurityConfig()->SetEnforceUserTokenRequirement(true);
        appConfig.MutableFeatureFlags()->SetAllowYdbRequestsWithoutDatabase(false);

        initLdapSettings(authConfig->MutableLdapAuthentication(), LdapPort, LdapClientOptions);
        return appConfig;
    }

    TDriverConfig GetDriverConfig(ui16 grpcPort) {
        TDriverConfig config;
        config.SetEndpoint("localhost:" + ToString(grpcPort));
        return config;
    }

private:
    const TLdapClientOptions LdapClientOptions;
    ui16 LdapPort;
    TKikimrWithGrpcAndRootSchemaWithAuth Server;
    NYdb::TDriver Connection;
    NConsoleClient::TDummyClient Client;
};

class TCertStorage {
public:
    TCertStorage()
        : CaCertAndKey(GenerateCA(TProps::AsCA()))
        , ServerCertAndKey(GenerateSignedCert(CaCertAndKey, TProps::AsClientServer()))
    {
        CaCertFile.Write(CaCertAndKey.Certificate.c_str(), CaCertAndKey.Certificate.size());
        ServerCertFile.Write(ServerCertAndKey.Certificate.c_str(), ServerCertAndKey.Certificate.size());
        ServerKeyFile.Write(ServerCertAndKey.PrivateKey.c_str(), ServerCertAndKey.PrivateKey.size());
    }

    TString GetCaCertFileName() const {
        return CaCertFile.Name();
    }

    TString GetServerCertFileName() const {
        return ServerCertFile.Name();
    }

    TString GetServerKeyFileName() const {
        return ServerKeyFile.Name();
    }

private:
    TCertAndKey CaCertAndKey;
    TCertAndKey ServerCertAndKey;
    TTempFileHandle CaCertFile;
    TTempFileHandle ServerCertFile;
    TTempFileHandle ServerKeyFile;
};

TCertStorage CertStorage;

} // namespace


// These tests check to create token for ldap authenticated users
Y_UNIT_TEST_SUITE(TGRpcLdapAuthentication) {
    Y_UNIT_TEST(LdapAuthWithValidCredentials) {
        TString login = "ldapuser";
        TString password = "ldapUserPassword";

        LdapMock::TLdapMockResponses responses;
        responses.BindResponses.push_back({{{.Login = "cn=robouser,dc=search,dc=yandex,dc=net", .Password = "robouserPassword"}}, {.Status = LdapMock::EStatus::SUCCESS}});
        responses.BindResponses.push_back({{{.Login = "uid=" + login + ",dc=search,dc=yandex,dc=net", .Password = password}}, {.Status = LdapMock::EStatus::SUCCESS}});

        LdapMock::TSearchRequestInfo fetchUserSearchRequestInfo {
            {
                .BaseDn = "dc=search,dc=yandex,dc=net",
                .Scope = 2,
                .DerefAliases = 0,
                .Filter = {.Type = LdapMock::EFilterType::LDAP_FILTER_EQUALITY, .Attribute = "uid", .Value = login},
                .Attributes = {"1.1"}
            }
        };

        std::vector<LdapMock::TSearchEntry> fetchUserSearchResponseEntries {
            {
                .Dn = "uid=" + login + ",dc=search,dc=yandex,dc=net"
            }
        };

        LdapMock::TSearchResponseInfo fetchUserSearchResponseInfo {
            .ResponseEntries = fetchUserSearchResponseEntries,
            .ResponseDone = {.Status = LdapMock::EStatus::SUCCESS}
        };
        responses.SearchResponses.push_back({fetchUserSearchRequestInfo, fetchUserSearchResponseInfo});


        TLoginClientConnection loginConnection(InitLdapSettings, {
            .CaCertFile = CertStorage.GetCaCertFileName(),
            .Type = ESecurityConnectionType::NON_SECURE
        });
        LdapMock::TSimpleServer ldapServer({
            .Port = loginConnection.GetLdapPort(),
            .Cert = CertStorage.GetServerCertFileName(),
            .Key = CertStorage.GetServerKeyFileName(),
            .UseTls = false
        }, responses);

        ldapServer.Start();
        auto factory = CreateLoginCredentialsProviderFactory({.User = login + "@ldap", .Password = password});
        auto loginProvider = factory->CreateProvider(loginConnection.GetCoreFacility());
        TString token;
        UNIT_ASSERT_NO_EXCEPTION(token = loginProvider->GetAuthInfo());
        UNIT_ASSERT(!token.empty());

        loginConnection.Stop();
        ldapServer.Stop();
    }

    Y_UNIT_TEST(LdapAuthWithInvalidRobouserLogin) {
        TString login = "ldapuser";
        TString password = "ldapUserPassword";

        LdapMock::TLdapMockResponses responses;
        responses.BindResponses.push_back({{{.Login = "cn=invalidRobouser,dc=search,dc=yandex,dc=net", .Password = "robouserPassword"}}, {.Status = LdapMock::EStatus::INVALID_CREDENTIALS}});

        // TLoginClientConnection loginConnection(InitLdapSettingsWithInvalidRobotUserLogin);
        // LdapMock::TLdapSimpleServer ldapServer(loginConnection.GetLdapPort(), responses);
        TLoginClientConnection loginConnection(InitLdapSettingsWithInvalidRobotUserLogin, {
            .CaCertFile = CertStorage.GetCaCertFileName(),
            .Type = ESecurityConnectionType::NON_SECURE
        });
        LdapMock::TSimpleServer ldapServer({
            .Port = loginConnection.GetLdapPort(),
            .Cert = CertStorage.GetServerCertFileName(),
            .Key = CertStorage.GetServerKeyFileName(),
            .UseTls = false
        }, responses);

        ldapServer.Start();
        auto factory = CreateLoginCredentialsProviderFactory({.User = login + "@ldap", .Password = password});
        auto loginProvider = factory->CreateProvider(loginConnection.GetCoreFacility());
        UNIT_ASSERT_EXCEPTION_CONTAINS(loginProvider->GetAuthInfo(), yexception, "Could not login via LDAP");

        loginConnection.Stop();
        ldapServer.Stop();
    }

    Y_UNIT_TEST(LdapAuthWithInvalidRobouserPassword) {
        TString login = "ldapuser";
        TString password = "ldapUserPassword";

        LdapMock::TLdapMockResponses responses;
        responses.BindResponses.push_back({{{.Login = "cn=robouser,dc=search,dc=yandex,dc=net", .Password = "invalidPassword"}}, {.Status = LdapMock::EStatus::INVALID_CREDENTIALS}});

        // TLoginClientConnection loginConnection(InitLdapSettingsWithInvalidRobotUserPassword);
        // LdapMock::TLdapSimpleServer ldapServer(loginConnection.GetLdapPort(), responses);
        TLoginClientConnection loginConnection(InitLdapSettingsWithInvalidRobotUserPassword, {
            .CaCertFile = CertStorage.GetCaCertFileName(),
            .Type = ESecurityConnectionType::NON_SECURE
        });
        LdapMock::TSimpleServer ldapServer({
            .Port = loginConnection.GetLdapPort(),
            .Cert = CertStorage.GetServerCertFileName(),
            .Key = CertStorage.GetServerKeyFileName(),
            .UseTls = false
        }, responses);

        ldapServer.Start();
        auto factory = CreateLoginCredentialsProviderFactory({.User = login + "@ldap", .Password = password});
        auto loginProvider = factory->CreateProvider(loginConnection.GetCoreFacility());
        UNIT_ASSERT_EXCEPTION_CONTAINS(loginProvider->GetAuthInfo(), yexception, "Could not login via LDAP");

        loginConnection.Stop();
        ldapServer.Stop();
    }

    Y_UNIT_TEST(LdapAuthWithInvalidSearchFilter) {
        TString login = "ldapuser";
        TString password = "ldapUserPassword";

        LdapMock::TLdapMockResponses responses;
        responses.BindResponses.push_back({{{.Login = "cn=robouser,dc=search,dc=yandex,dc=net", .Password = "robouserPassword"}}, {.Status = LdapMock::EStatus::SUCCESS}});

        // TLoginClientConnection loginConnection(InitLdapSettingsWithInvalidFilter);
        // LdapMock::TLdapSimpleServer ldapServer(loginConnection.GetLdapPort(), responses);
        TLoginClientConnection loginConnection(InitLdapSettingsWithInvalidFilter, {
            .CaCertFile = CertStorage.GetCaCertFileName(),
            .Type = ESecurityConnectionType::NON_SECURE
        });
        LdapMock::TSimpleServer ldapServer({
            .Port = loginConnection.GetLdapPort(),
            .Cert = CertStorage.GetServerCertFileName(),
            .Key = CertStorage.GetServerKeyFileName(),
            .UseTls = false
        }, responses);

        ldapServer.Start();
        auto factory = CreateLoginCredentialsProviderFactory({.User = login + "@ldap", .Password = password});
        auto loginProvider = factory->CreateProvider(loginConnection.GetCoreFacility());
        UNIT_ASSERT_EXCEPTION_CONTAINS(loginProvider->GetAuthInfo(), yexception, "Could not login via LDAP");

        loginConnection.Stop();
        ldapServer.Stop();
    }

    void CheckRequiredLdapSettings(std::function<void(NKikimrProto::TLdapAuthentication*, ui16, const TLdapClientOptions&)> initLdapSettings, const TString& expectedErrorMessage) {
        TString login = "ldapuser";
        TString password = "ldapUserPassword";

        LdapMock::TLdapMockResponses responses;
        // TLoginClientConnection loginConnection(initLdapSettings);
        // LdapMock::TLdapSimpleServer ldapServer(loginConnection.GetLdapPort(), responses);
        TLoginClientConnection loginConnection(initLdapSettings, {
            .CaCertFile = CertStorage.GetCaCertFileName(),
            .Type = ESecurityConnectionType::NON_SECURE
        });
        LdapMock::TSimpleServer ldapServer({
            .Port = loginConnection.GetLdapPort(),
            .Cert = CertStorage.GetServerCertFileName(),
            .Key = CertStorage.GetServerKeyFileName(),
            .UseTls = false
        }, responses);

        ldapServer.Start();
        auto factory = CreateLoginCredentialsProviderFactory({.User = login + "@ldap", .Password = password});
        auto loginProvider = factory->CreateProvider(loginConnection.GetCoreFacility());
        UNIT_ASSERT_EXCEPTION_CONTAINS(loginProvider->GetAuthInfo(), yexception, expectedErrorMessage);

        loginConnection.Stop();
        ldapServer.Stop();
    }

    Y_UNIT_TEST(LdapAuthServerIsUnavailable) {
        CheckRequiredLdapSettings(InitLdapSettingsWithUnavailableHost, "Could not login via LDAP");
    }

    Y_UNIT_TEST(LdapAuthSettingsWithEmptyHosts) {
        CheckRequiredLdapSettings(InitLdapSettingsWithEmptyHosts, "Could not login via LDAP");
    }

    Y_UNIT_TEST(LdapAuthSettingsWithEmptyBaseDn) {
        CheckRequiredLdapSettings(InitLdapSettingsWithEmptyBaseDn, "Could not login via LDAP");
    }

    Y_UNIT_TEST(LdapAuthSettingsWithEmptyBindDn) {
        CheckRequiredLdapSettings(InitLdapSettingsWithEmptyBindDn, "Could not login via LDAP");
    }

    Y_UNIT_TEST(LdapAuthSettingsWithEmptyBindPassword) {
        CheckRequiredLdapSettings(InitLdapSettingsWithEmptyBindPassword, "Could not login via LDAP");
    }

    Y_UNIT_TEST(LdapAuthWithInvalidLogin) {
        TString nonExistentUser = "nonexistentldapuser";
        TString password = "ldapUserPassword";

        LdapMock::TLdapMockResponses responses;
        responses.BindResponses.push_back({{{.Login = "cn=robouser,dc=search,dc=yandex,dc=net", .Password = "robouserPassword"}}, {.Status = LdapMock::EStatus::SUCCESS}});

        LdapMock::TSearchRequestInfo fetchUserSearchRequestInfo {
            {
                .BaseDn = "dc=search,dc=yandex,dc=net",
                .Scope = 2,
                .DerefAliases = 0,
                .Filter = {.Type = LdapMock::EFilterType::LDAP_FILTER_EQUALITY, .Attribute = "uid", .Value = nonExistentUser},
                .Attributes = {"1.1"}
            }
        };

        LdapMock::TSearchResponseInfo fetchUserSearchResponseInfo {
            .ResponseEntries = {}, // User does not exist. Return empty entries list
            .ResponseDone = {.Status = LdapMock::EStatus::SUCCESS}
        };
        responses.SearchResponses.push_back({fetchUserSearchRequestInfo, fetchUserSearchResponseInfo});

        // TLoginClientConnection loginConnection(InitLdapSettings);
        // LdapMock::TLdapSimpleServer ldapServer(loginConnection.GetLdapPort(), responses);
        TLoginClientConnection loginConnection(InitLdapSettings, {
            .CaCertFile = CertStorage.GetCaCertFileName(),
            .Type = ESecurityConnectionType::NON_SECURE
        });
        LdapMock::TSimpleServer ldapServer({
            .Port = loginConnection.GetLdapPort(),
            .Cert = CertStorage.GetServerCertFileName(),
            .Key = CertStorage.GetServerKeyFileName(),
            .UseTls = false
        }, responses);

        ldapServer.Start();
        auto factory = CreateLoginCredentialsProviderFactory({.User = nonExistentUser + "@ldap", .Password = password});
        auto loginProvider = factory->CreateProvider(loginConnection.GetCoreFacility());
        UNIT_ASSERT_EXCEPTION_CONTAINS(loginProvider->GetAuthInfo(), yexception, "Could not login via LDAP");

        loginConnection.Stop();
        ldapServer.Stop();
    }

    Y_UNIT_TEST(LdapAuthWithInvalidPassword) {
        TString login = "ldapUser";
        TString password = "wrongLdapUserPassword";

        LdapMock::TLdapMockResponses responses;
        responses.BindResponses.push_back({{{.Login = "cn=robouser,dc=search,dc=yandex,dc=net", .Password = "robouserPassword"}}, {.Status = LdapMock::EStatus::SUCCESS}});
        responses.BindResponses.push_back({{{.Login = "uid=" + login + ",dc=search,dc=yandex,dc=net", .Password = password}}, {.Status = LdapMock::EStatus::INVALID_CREDENTIALS}});

        LdapMock::TSearchRequestInfo fetchUserSearchRequestInfo {
            {
                .BaseDn = "dc=search,dc=yandex,dc=net",
                .Scope = 2,
                .DerefAliases = 0,
                .Filter = {.Type = LdapMock::EFilterType::LDAP_FILTER_EQUALITY, .Attribute = "uid", .Value = login},
                .Attributes = {"1.1"}
            }
        };

        std::vector<LdapMock::TSearchEntry> fetchUserSearchResponseEntries {
            {
                .Dn = "uid=" + login + ",dc=search,dc=yandex,dc=net"
            }
        };

        LdapMock::TSearchResponseInfo fetchUserSearchResponseInfo {
            .ResponseEntries = fetchUserSearchResponseEntries,
            .ResponseDone = {.Status = LdapMock::EStatus::SUCCESS}
        };
        responses.SearchResponses.push_back({fetchUserSearchRequestInfo, fetchUserSearchResponseInfo});

        // TLoginClientConnection loginConnection(InitLdapSettings);
        // LdapMock::TLdapSimpleServer ldapServer(loginConnection.GetLdapPort(), responses);
        TLoginClientConnection loginConnection(InitLdapSettings, {
            .CaCertFile = CertStorage.GetCaCertFileName(),
            .Type = ESecurityConnectionType::NON_SECURE
        });
        LdapMock::TSimpleServer ldapServer({
            .Port = loginConnection.GetLdapPort(),
            .Cert = CertStorage.GetServerCertFileName(),
            .Key = CertStorage.GetServerKeyFileName(),
            .UseTls = false
        }, responses);

        ldapServer.Start();
        auto factory = CreateLoginCredentialsProviderFactory({.User = login + "@ldap", .Password = password});
        auto loginProvider = factory->CreateProvider(loginConnection.GetCoreFacility());
        UNIT_ASSERT_EXCEPTION_CONTAINS(loginProvider->GetAuthInfo(), yexception, "Could not login via LDAP");

        loginConnection.Stop();
        ldapServer.Stop();
    }

    Y_UNIT_TEST(LdapAuthWithEmptyPassword) {
        TString login = "ldapUser";
        TString password = "";

        LdapMock::TLdapMockResponses responses;
        responses.BindResponses.push_back({{{.Login = "cn=robouser,dc=search,dc=yandex,dc=net", .Password = "robouserPassword"}}, {.Status = LdapMock::EStatus::SUCCESS}});

        LdapMock::TSearchRequestInfo fetchUserSearchRequestInfo {
            {
                .BaseDn = "dc=search,dc=yandex,dc=net",
                .Scope = 2,
                .DerefAliases = 0,
                .Filter = {.Type = LdapMock::EFilterType::LDAP_FILTER_EQUALITY, .Attribute = "uid", .Value = login},
                .Attributes = {"1.1"}
            }
        };

        std::vector<LdapMock::TSearchEntry> fetchUserSearchResponseEntries {
            {
                .Dn = "uid=" + login + ",dc=search,dc=yandex,dc=net"
            }
        };

        LdapMock::TSearchResponseInfo fetchUserSearchResponseInfo {
            .ResponseEntries = fetchUserSearchResponseEntries,
            .ResponseDone = {.Status = LdapMock::EStatus::SUCCESS}
        };
        responses.SearchResponses.push_back({fetchUserSearchRequestInfo, fetchUserSearchResponseInfo});

        // TLoginClientConnection loginConnection(InitLdapSettings);
        // LdapMock::TLdapSimpleServer ldapServer(loginConnection.GetLdapPort(), responses);
        TLoginClientConnection loginConnection(InitLdapSettings, {
            .CaCertFile = CertStorage.GetCaCertFileName(),
            .Type = ESecurityConnectionType::NON_SECURE
        });
        LdapMock::TSimpleServer ldapServer({
            .Port = loginConnection.GetLdapPort(),
            .Cert = CertStorage.GetServerCertFileName(),
            .Key = CertStorage.GetServerKeyFileName(),
            .UseTls = false
        }, responses);

        ldapServer.Start();
        auto factory = CreateLoginCredentialsProviderFactory({.User = login + "@ldap", .Password = password});
        auto loginProvider = factory->CreateProvider(loginConnection.GetCoreFacility());
        UNIT_ASSERT_EXCEPTION_CONTAINS(loginProvider->GetAuthInfo(), yexception, "Could not login via LDAP");

        loginConnection.Stop();
        ldapServer.Stop();
    }

    Y_UNIT_TEST(LdapAuthSetIncorrectDomain) {
        TString login = "ldapuser";
        TString password = "ldapUserPassword";
        const TString incorrectLdapDomain = "@ldap.domain"; // Correct domain is AuthConfig.LdapAuthenticationDomain: "ldap"

        auto factory = CreateLoginCredentialsProviderFactory({.User = login + incorrectLdapDomain, .Password = password});
        // TLoginClientConnection loginConnection(InitLdapSettings);
        TLoginClientConnection loginConnection(InitLdapSettings, {
            .CaCertFile = CertStorage.GetCaCertFileName(),
            .Type = ESecurityConnectionType::NON_SECURE
        });
        auto loginProvider = factory->CreateProvider(loginConnection.GetCoreFacility());
        UNIT_ASSERT_EXCEPTION_CONTAINS(loginProvider->GetAuthInfo(), yexception, "Cannot find user 'ldapuser@ldap.domain'");

        loginConnection.Stop();
    }

    Y_UNIT_TEST(DisableBuiltinAuthMechanism) {
        TString login = "builtinUser";
        TString password = "builtinUserPassword";

        // TLoginClientConnection loginConnection(InitLdapSettings, false);
        TLoginClientConnection loginConnection(InitLdapSettings, {
            .CaCertFile = CertStorage.GetCaCertFileName(),
            .Type = ESecurityConnectionType::NON_SECURE,
            .IsLoginAuthenticationEnabled = false
        });

        auto factory = CreateLoginCredentialsProviderFactory({.User = login, .Password = password});
        auto loginProvider = factory->CreateProvider(loginConnection.GetCoreFacility());
        TStringBuilder expectedErrorMessage;
        UNIT_ASSERT_EXCEPTION_CONTAINS(loginProvider->GetAuthInfo(), yexception, "Login authentication is disabled");

        loginConnection.Stop();
    }
}
} //namespace NKikimr
