#include <library/cpp/testing/unittest/tests_data.h>
#include <library/cpp/testing/unittest/registar.h>

#include <ydb/public/sdk/cpp/include/ydb-cpp-sdk/client/types/credentials/credentials.h>
#include <ydb/public/lib/ydb_cli/commands/ydb_sdk_core_access.h>

#include <ydb/core/testlib/test_client.h>
#include <ydb/library/testlib/service_mocks/ldap_mock/ldap_simple_server.h>

#include <util/system/tempfile.h>
#include "ydb_common_ut.h"

namespace NKikimr {

using namespace Tests;
using namespace NYdb;

namespace {

TString certificateContent = R"___(-----BEGIN CERTIFICATE-----
MIIDjTCCAnWgAwIBAgIURt5IBx0J3xgEaQvmyrFH2A+NkpMwDQYJKoZIhvcNAQEL
BQAwVjELMAkGA1UEBhMCUlUxDzANBgNVBAgMBk1vc2NvdzEPMA0GA1UEBwwGTW9z
Y293MQ8wDQYDVQQKDAZZYW5kZXgxFDASBgNVBAMMC3Rlc3Qtc2VydmVyMB4XDTE5
MDkyMDE3MTQ0MVoXDTQ3MDIwNDE3MTQ0MVowVjELMAkGA1UEBhMCUlUxDzANBgNV
BAgMBk1vc2NvdzEPMA0GA1UEBwwGTW9zY293MQ8wDQYDVQQKDAZZYW5kZXgxFDAS
BgNVBAMMC3Rlc3Qtc2VydmVyMIIBIjANBgkqhkiG9w0BAQEFAAOCAQ8AMIIBCgKC
AQEAs0WY6HTuwKntcEcjo+pBuoNp5/GRgMX2qOJi09Iw021ZLK4Vf4drN7pXS5Ba
OVqzUPFmXvoiG13hS7PLTuobJc63qPbIodiB6EXB+Sp0v+mE6lYUUyW9YxNnTPDc
GG8E4vk9j3tBawT4yJIFTudIALWJfQvn3O9ebmYkilvq0ZT+TqBU8Mazo4lNu0T2
YxWMlivcEyNRLPbka5W2Wy5eXGOnStidQFYka2mmCgljtulWzj1i7GODg93vmVyH
NzjAs+mG9MJkT3ietG225BnyPDtu5A3b+vTAFhyJtMmDMyhJ6JtXXHu6zUDQxKiX
6HLGCLIPhL2sk9ckPSkwXoMOywIDAQABo1MwUTAdBgNVHQ4EFgQUDv/xuJ4CvCgG
fPrZP3hRAt2+/LwwHwYDVR0jBBgwFoAUDv/xuJ4CvCgGfPrZP3hRAt2+/LwwDwYD
VR0TAQH/BAUwAwEB/zANBgkqhkiG9w0BAQsFAAOCAQEAinKpMYaA2tjLpAnPVbjy
/ZxSBhhB26RiQp3Re8XOKyhTWqgYE6kldYT0aXgK9x9mPC5obQannDDYxDc7lX+/
qP/u1X81ZcDRo/f+qQ3iHfT6Ftt/4O3qLnt45MFM6Q7WabRm82x3KjZTqpF3QUdy
tumWiuAP5DMd1IRDtnKjFHO721OsEsf6NLcqdX89bGeqXDvrkwg3/PNwTyW5E7cj
feY8L2eWtg6AJUnIBu11wvfzkLiH3QKzHvO/SIZTGf5ihDsJ3aKEE9UNauTL3bVc
CRA/5XcX13GJwHHj6LCoc3sL7mt8qV9HKY2AOZ88mpObzISZxgPpdKCfjsrdm63V
6g==
-----END CERTIFICATE-----)___";

const TString serverCert {R"___(-----BEGIN CERTIFICATE-----
MIIDjTCCAnWgAwIBAgIURt5IBx0J3xgEaQvmyrFH2A+NkpMwDQYJKoZIhvcNAQEL
BQAwVjELMAkGA1UEBhMCUlUxDzANBgNVBAgMBk1vc2NvdzEPMA0GA1UEBwwGTW9z
Y293MQ8wDQYDVQQKDAZZYW5kZXgxFDASBgNVBAMMC3Rlc3Qtc2VydmVyMB4XDTE5
MDkyMDE3MTQ0MVoXDTQ3MDIwNDE3MTQ0MVowVjELMAkGA1UEBhMCUlUxDzANBgNV
BAgMBk1vc2NvdzEPMA0GA1UEBwwGTW9zY293MQ8wDQYDVQQKDAZZYW5kZXgxFDAS
BgNVBAMMC3Rlc3Qtc2VydmVyMIIBIjANBgkqhkiG9w0BAQEFAAOCAQ8AMIIBCgKC
AQEAs0WY6HTuwKntcEcjo+pBuoNp5/GRgMX2qOJi09Iw021ZLK4Vf4drN7pXS5Ba
OVqzUPFmXvoiG13hS7PLTuobJc63qPbIodiB6EXB+Sp0v+mE6lYUUyW9YxNnTPDc
GG8E4vk9j3tBawT4yJIFTudIALWJfQvn3O9ebmYkilvq0ZT+TqBU8Mazo4lNu0T2
YxWMlivcEyNRLPbka5W2Wy5eXGOnStidQFYka2mmCgljtulWzj1i7GODg93vmVyH
NzjAs+mG9MJkT3ietG225BnyPDtu5A3b+vTAFhyJtMmDMyhJ6JtXXHu6zUDQxKiX
6HLGCLIPhL2sk9ckPSkwXoMOywIDAQABo1MwUTAdBgNVHQ4EFgQUDv/xuJ4CvCgG
fPrZP3hRAt2+/LwwHwYDVR0jBBgwFoAUDv/xuJ4CvCgGfPrZP3hRAt2+/LwwDwYD
VR0TAQH/BAUwAwEB/zANBgkqhkiG9w0BAQsFAAOCAQEAinKpMYaA2tjLpAnPVbjy
/ZxSBhhB26RiQp3Re8XOKyhTWqgYE6kldYT0aXgK9x9mPC5obQannDDYxDc7lX+/
qP/u1X81ZcDRo/f+qQ3iHfT6Ftt/4O3qLnt45MFM6Q7WabRm82x3KjZTqpF3QUdy
tumWiuAP5DMd1IRDtnKjFHO721OsEsf6NLcqdX89bGeqXDvrkwg3/PNwTyW5E7cj
feY8L2eWtg6AJUnIBu11wvfzkLiH3QKzHvO/SIZTGf5ihDsJ3aKEE9UNauTL3bVc
CRA/5XcX13GJwHHj6LCoc3sL7mt8qV9HKY2AOZ88mpObzISZxgPpdKCfjsrdm63V
6g==
-----END CERTIFICATE-----)___"
};

const TString serverPrivateKey {R"___(-----BEGIN PRIVATE KEY-----
MIIEvwIBADANBgkqhkiG9w0BAQEFAASCBKkwggSlAgEAAoIBAQCzRZjodO7Aqe1w
RyOj6kG6g2nn8ZGAxfao4mLT0jDTbVksrhV/h2s3uldLkFo5WrNQ8WZe+iIbXeFL
s8tO6hslzreo9sih2IHoRcH5KnS/6YTqVhRTJb1jE2dM8NwYbwTi+T2Pe0FrBPjI
kgVO50gAtYl9C+fc715uZiSKW+rRlP5OoFTwxrOjiU27RPZjFYyWK9wTI1Es9uRr
lbZbLl5cY6dK2J1AViRraaYKCWO26VbOPWLsY4OD3e+ZXIc3OMCz6Yb0wmRPeJ60
bbbkGfI8O27kDdv69MAWHIm0yYMzKEnom1dce7rNQNDEqJfocsYIsg+EvayT1yQ9
KTBegw7LAgMBAAECggEBAKaOCrotqYQmXArsjRhFFDwMy+BKdzyEr93INrlFl0dX
WHpCYobRcbOc1G3H94tB0UdqgAnNqtJyLlb+++ydZAuEOu4oGc8EL+10ofq0jzOd
6Xct8kQt0/6wkFDTlii9PHUDy0X65ZRgUiNGRtg/2I2QG+SpowmI+trm2xwQueFs
VaWrjc3cVvXx0b8Lu7hqZUv08kgC38stzuRk/n2T5VWSAr7Z4ZWQbO918Dv35HUw
Wy/0jNUFP9CBCvFJ4l0OoH9nYhWFG+HXWzNdw6/Hca4jciRKo6esCiOZ9uWYv/ec
/NvX9rgFg8G8/SrTisX10+Bbeq+R1RKwq/IG409TH4ECgYEA14L+3QsgNIUMeYAx
jSCyk22R/tOHI1BM+GtKPUhnwHlAssrcPcxXMJovl6WL93VauYjym0wpCz9urSpA
I2CqTsG8GYciA6Dr3mHgD6cK0jj9UPAU6EnZ5S0mjhPqKZqutu9QegzD2uESvuN8
36xezwQthzAf0nI/P3sJGjVXjikCgYEA1POm5xcV6SmM6HnIdadEebhzZIJ9TXQz
ry3Jj3a7CKyD5C7fAdkHUTCjgT/2ElxPi9ABkZnC+d/cW9GtJFa0II5qO/agm3KQ
ZXYiutu9A7xACHYFXRiJEjVUdGG9dKMVOHUEa8IHEgrrcUVM/suy/GgutywIfaXs
y58IFP24K9MCgYEAk6zjz7wL+XEiNy+sxLQfKf7vB9sSwxQHakK6wHuY/L8Zomp3
uLEJHfjJm/SIkK0N2g0JkXkCtv5kbKyC/rsCeK0wo52BpVLjzaLr0k34kE0U6B1b
dkEE2pGx1bG3x4KDLj+Wuct9ecK5Aa0IqIyI+vo16GkFpUM8K9e3SQo8UOECgYEA
sCZYAkILYtJ293p9giz5rIISGasDAUXE1vxWBXEeJ3+kneTTnZCrx9Im/ewtnWR0
fF90XL9HFDDD88POqAd8eo2zfKR2l/89SGBfPBg2EtfuU9FkgGyiPciVcqvC7q9U
B15saMKX3KnhtdGwbfeLt9RqCCTJZT4SUSDcq5hwdvcCgYAxY4Be8mNipj8Cgg22
mVWSolA0TEzbtUcNk6iGodpi+Z0LKpsPC0YRqPRyh1K+rIltG1BVdmUBHcMlOYxl
lWWvbJH6PkJWy4n2MF7PO45kjN3pPZg4hgH63JjZeAineBwEArUGb9zHnvzcdRvF
wuQ2pZHL/HJ0laUSieHDJ5917w==
-----END PRIVATE KEY-----)___"
};

TTempFileHandle certificateFile;
TTempFileHandle certFile;
TTempFileHandle keyFile;

void InitLdapSettings(NKikimrProto::TLdapAuthentication* ldapSettings, ui16 ldapPort, TTempFileHandle& certificateFile) {
    ldapSettings->SetHost("localhost");
    ldapSettings->SetPort(ldapPort);
    ldapSettings->SetBaseDn("dc=search,dc=yandex,dc=net");
    ldapSettings->SetBindDn("cn=robouser,dc=search,dc=yandex,dc=net");
    ldapSettings->SetBindPassword("robouserPassword");
    ldapSettings->SetSearchFilter("uid=$username");

    auto useTls = ldapSettings->MutableUseTls();
    useTls->SetEnable(true);
    certificateFile.Write(certificateContent.data(), certificateContent.size());
    useTls->SetCaCertFile(certificateFile.Name());
    useTls->SetCertRequire(NKikimrProto::TLdapAuthentication::TUseTls::ALLOW); // Enable TLS connection if server certificate is untrusted

    certFile.Write(serverCert.data(), serverCert.size());
    useTls->SetCertFile(certFile.Name());

    keyFile.Write(serverPrivateKey.data(), serverPrivateKey.size());
    useTls->SetKeyFile(keyFile.Name());
}

void InitLdapSettingsWithInvalidRobotUserLogin(NKikimrProto::TLdapAuthentication* ldapSettings, ui16 ldapPort, TTempFileHandle& certificateFile) {
    InitLdapSettings(ldapSettings, ldapPort, certificateFile);
    ldapSettings->SetBindDn("cn=invalidRobouser,dc=search,dc=yandex,dc=net");
}

void InitLdapSettingsWithInvalidRobotUserPassword(NKikimrProto::TLdapAuthentication* ldapSettings, ui16 ldapPort, TTempFileHandle& certificateFile) {
    InitLdapSettings(ldapSettings, ldapPort, certificateFile);
    ldapSettings->SetBindPassword("invalidPassword");
}

void InitLdapSettingsWithInvalidFilter(NKikimrProto::TLdapAuthentication* ldapSettings, ui16 ldapPort, TTempFileHandle& certificateFile) {
    InitLdapSettings(ldapSettings, ldapPort, certificateFile);
    ldapSettings->SetSearchFilter("&(uid=$username)()");
}

void InitLdapSettingsWithUnavailableHost(NKikimrProto::TLdapAuthentication* ldapSettings, ui16 ldapPort, TTempFileHandle& certificateFile) {
    InitLdapSettings(ldapSettings, ldapPort, certificateFile);
    ldapSettings->SetHost("unavailablehost");
}

void InitLdapSettingsWithEmptyHosts(NKikimrProto::TLdapAuthentication* ldapSettings, ui16 ldapPort, TTempFileHandle& certificateFile) {
    InitLdapSettings(ldapSettings, ldapPort, certificateFile);
    ldapSettings->SetHost("");
}

void InitLdapSettingsWithEmptyBaseDn(NKikimrProto::TLdapAuthentication* ldapSettings, ui16 ldapPort, TTempFileHandle& certificateFile) {
    InitLdapSettings(ldapSettings, ldapPort, certificateFile);
    ldapSettings->SetBaseDn("");
}

void InitLdapSettingsWithEmptyBindDn(NKikimrProto::TLdapAuthentication* ldapSettings, ui16 ldapPort, TTempFileHandle& certificateFile) {
    InitLdapSettings(ldapSettings, ldapPort, certificateFile);
    ldapSettings->SetBindDn("");
}

void InitLdapSettingsWithEmptyBindPassword(NKikimrProto::TLdapAuthentication* ldapSettings, ui16 ldapPort, TTempFileHandle& certificateFile) {
    InitLdapSettings(ldapSettings, ldapPort, certificateFile);
    ldapSettings->SetBindPassword("");
}

void InitLdapSettingsWithMtlsAuth(NKikimrProto::TLdapAuthentication* ldapSettings, ui16 ldapPort, TTempFileHandle& certificateFile) {
    InitLdapSettings(ldapSettings, ldapPort, certificateFile);
    ldapSettings->SetBindPassword("");
    ldapSettings->MutableExtendedSettings()->SetEnableMtlsAuth(true);
}

class TLoginClientConnection {
public:
    TLoginClientConnection(std::function<void(NKikimrProto::TLdapAuthentication*, ui16, TTempFileHandle&)> initLdapSettings, bool isLoginAuthenticationEnabled = true)
        : CaCertificateFile()
        , Server(InitAuthSettings(std::move(initLdapSettings), isLoginAuthenticationEnabled))
        , Connection(GetDriverConfig(Server.GetPort()))
        , Client(Connection)
    {
        Server.GetRuntime()->SetLogPriority(NKikimrServices::GRPC_CLIENT, NLog::PRI_TRACE);
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
    NKikimrConfig::TAppConfig InitAuthSettings(std::function<void(NKikimrProto::TLdapAuthentication*, ui16, TTempFileHandle&)>&& initLdapSettings, bool isLoginAuthenticationEnabled = true) {
        TPortManager tp;
        LdapPort = tp.GetPort(389);

        NKikimrConfig::TAppConfig appConfig;
        auto authConfig = appConfig.MutableAuthConfig();

        authConfig->SetUseBlackBox(false);
        authConfig->SetUseLoginProvider(true);
        authConfig->SetEnableLoginAuthentication(isLoginAuthenticationEnabled);
        appConfig.MutableDomainsConfig()->MutableSecurityConfig()->SetEnforceUserTokenRequirement(true);
        appConfig.MutableFeatureFlags()->SetAllowYdbRequestsWithoutDatabase(false);

        initLdapSettings(authConfig->MutableLdapAuthentication(), LdapPort, CaCertificateFile);
        return appConfig;
    }

    TDriverConfig GetDriverConfig(ui16 grpcPort) {
        TDriverConfig config;
        config.SetEndpoint("localhost:" + ToString(grpcPort));
        return config;
    }

private:
    TTempFileHandle CaCertificateFile;
    ui16 LdapPort;
    TKikimrWithGrpcAndRootSchemaWithAuth Server;
    NYdb::TDriver Connection;
    NConsoleClient::TDummyClient Client;
};

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


        TLoginClientConnection loginConnection(InitLdapSettings);
        LdapMock::TLdapSimpleServer ldapServer(loginConnection.GetLdapPort(), responses);

        auto factory = CreateLoginCredentialsProviderFactory({.User = login + "@ldap", .Password = password});
        auto loginProvider = factory->CreateProvider(loginConnection.GetCoreFacility());
        TString token;
        UNIT_ASSERT_NO_EXCEPTION(token = loginProvider->GetAuthInfo());
        UNIT_ASSERT(!token.empty());

        loginConnection.Stop();
        ldapServer.Stop();
    }

    Y_UNIT_TEST(LdapAuthWithValidCredentialsSaslExternal) {
        TString login = "ldapuser";
        TString password = "ldapUserPassword";

        LdapMock::TLdapMockResponses responses;
        responses.BindResponses.push_back({{{.Login = "cn=robouser,dc=search,dc=yandex,dc=net", .Password = "", .Mechanism = LdapMock::ESaslMechanism::EXTERNAL}}, {.Status = LdapMock::EStatus::SUCCESS}});
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


        TLoginClientConnection loginConnection(InitLdapSettingsWithMtlsAuth);
        LdapMock::TLdapSimpleServer ldapServer(loginConnection.GetLdapPort(), responses);

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

        TLoginClientConnection loginConnection(InitLdapSettingsWithInvalidRobotUserLogin);
        LdapMock::TLdapSimpleServer ldapServer(loginConnection.GetLdapPort(), responses);

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

        TLoginClientConnection loginConnection(InitLdapSettingsWithInvalidRobotUserPassword);
        LdapMock::TLdapSimpleServer ldapServer(loginConnection.GetLdapPort(), responses);

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

        TLoginClientConnection loginConnection(InitLdapSettingsWithInvalidFilter);
        LdapMock::TLdapSimpleServer ldapServer(loginConnection.GetLdapPort(), responses);

        auto factory = CreateLoginCredentialsProviderFactory({.User = login + "@ldap", .Password = password});
        auto loginProvider = factory->CreateProvider(loginConnection.GetCoreFacility());
        UNIT_ASSERT_EXCEPTION_CONTAINS(loginProvider->GetAuthInfo(), yexception, "Could not login via LDAP");

        loginConnection.Stop();
        ldapServer.Stop();
    }

    void CheckRequiredLdapSettings(std::function<void(NKikimrProto::TLdapAuthentication*, ui16, TTempFileHandle&)> initLdapSettings, const TString& expectedErrorMessage) {
        TString login = "ldapuser";
        TString password = "ldapUserPassword";

        TLoginClientConnection loginConnection(initLdapSettings);
        LdapMock::TLdapMockResponses responses;
        LdapMock::TLdapSimpleServer ldapServer(loginConnection.GetLdapPort(), responses);

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

        TLoginClientConnection loginConnection(InitLdapSettings);
        LdapMock::TLdapSimpleServer ldapServer(loginConnection.GetLdapPort(), responses);

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

        TLoginClientConnection loginConnection(InitLdapSettings);
        LdapMock::TLdapSimpleServer ldapServer(loginConnection.GetLdapPort(), responses);

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

        TLoginClientConnection loginConnection(InitLdapSettings);
        LdapMock::TLdapSimpleServer ldapServer(loginConnection.GetLdapPort(), responses);

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
        TLoginClientConnection loginConnection(InitLdapSettings);
        auto loginProvider = factory->CreateProvider(loginConnection.GetCoreFacility());
        UNIT_ASSERT_EXCEPTION_CONTAINS(loginProvider->GetAuthInfo(), yexception, "Cannot find user 'ldapuser@ldap.domain'");

        loginConnection.Stop();
    }

    Y_UNIT_TEST(DisableBuiltinAuthMechanism) {
        TString login = "builtinUser";
        TString password = "builtinUserPassword";

        TLoginClientConnection loginConnection(InitLdapSettings, false);

        auto factory = CreateLoginCredentialsProviderFactory({.User = login, .Password = password});
        auto loginProvider = factory->CreateProvider(loginConnection.GetCoreFacility());
        TStringBuilder expectedErrorMessage;
        UNIT_ASSERT_EXCEPTION_CONTAINS(loginProvider->GetAuthInfo(), yexception, "Login authentication is disabled");

        loginConnection.Stop();
    }
}
} //namespace NKikimr
