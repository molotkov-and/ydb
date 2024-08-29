#include <library/cpp/string_utils/base64/base64.h>
#include <util/random/random.h>
#include <util/string/hex.h>
#include <util/datetime/base.h>
#include <openssl/evp.h>
#include <openssl/hmac.h>
#include <openssl/sha.h>
#include <ydb/public/sdk/cpp/client/ydb_table/table.h>
#include <ydb/public/sdk/cpp/client/ydb_params/params.h>
#include "oidc_session.h"
#include "openid_connect.h"

namespace NOIDC {

// #define MLOG_D(stream) LOG_DEBUG_S((NMVP::InstanceMVP->ActorSystem), NMVP::EService::MVP, stream)

namespace {

TString HmacSHA256(TStringBuf key, TStringBuf data) {
    unsigned char hash[SHA256_DIGEST_LENGTH];
    ui32 hl = SHA256_DIGEST_LENGTH;
    const auto* res = HMAC(EVP_sha256(), key.data(), key.size(), reinterpret_cast<const unsigned char*>(data.data()), data.size(), hash, &hl);
    Y_ENSURE(res);
    Y_ENSURE(hl == SHA256_DIGEST_LENGTH);
    return TString{reinterpret_cast<const char*>(res), hl};
}

} // namespace

const TDuration TOidcSession::STATE_LIFE_TIME = TDuration::Minutes(10);

TOidcSession::TOidcSession(const TOpenIdConnectSettings& settings)
    : OidcClientSecret(settings.ClientSecret)
    , MetaLocation("oidc-proxy", "oidc-proxy", {std::make_pair("cluster-api", settings.StoreSessionsOnServerSideSetting.Endpoint)}, settings.StoreSessionsOnServerSideSetting.Database)
    , MetaAccessTokenName(settings.StoreSessionsOnServerSideSetting.AccessTokenName)
{}

TOidcSession::TOidcSession(const NHttp::THttpIncomingRequestPtr& request, const TOpenIdConnectSettings& settings, bool isAjaxRequest)
    : State(GenerateState())
    , RedirectUrl(GetRedirectUrl(request, isAjaxRequest))
    , IsAjaxRequest(isAjaxRequest)
    , OidcClientSecret(settings.ClientSecret)
    , MetaLocation("oidc-proxy", "oidc-proxy", {std::make_pair("cluster-api", settings.StoreSessionsOnServerSideSetting.Endpoint)}, settings.StoreSessionsOnServerSideSetting.Database)
    , MetaAccessTokenName(settings.StoreSessionsOnServerSideSetting.AccessTokenName)
{}

TOidcSession::TOidcSession(const TString& state, const NHttp::THttpIncomingRequestPtr& request, const TOpenIdConnectSettings& settings, bool isAjaxRequest)
    : State(state)
    , RedirectUrl(GetRedirectUrl(request, isAjaxRequest))
    , IsAjaxRequest(isAjaxRequest)
    , OidcClientSecret(settings.ClientSecret)
    , MetaLocation("oidc-proxy", "oidc-proxy", {std::make_pair("cluster-api", settings.StoreSessionsOnServerSideSetting.Endpoint)}, settings.StoreSessionsOnServerSideSetting.Database)
    , MetaAccessTokenName(settings.StoreSessionsOnServerSideSetting.AccessTokenName)
{}

TString TOidcSession::CreateOidcSessionCookie() const {
    return TStringBuilder() << CreateNameYdbOidcCookie(OidcClientSecret, State)
                            << "=" << GenerateCookie()
                            << "; Path=" << GetAuthCallbackUrl() << "; Max-Age=" << COOKIE_MAX_AGE_SEC <<"; SameSite=None; Secure";
}

void TOidcSession::SaveSessionOnServerSide(std::function<void(const TString& error)> cb) const {
    NYdb::NTable::TClientSettings clientTableSettings;
    clientTableSettings.Database(MetaLocation.RootDomain)
                       .AuthToken(MVPAppData()->Tokenator->GetToken(MetaAccessTokenName));
    auto tableClient = MetaLocation.GetTableClient(clientTableSettings);
    tableClient.CreateSession().Subscribe([state = State, redirectUrl = RedirectUrl, isAjaxRequest = IsAjaxRequest, cb = std::move(cb)] (const NYdb::NTable::TAsyncCreateSessionResult& result) {
        auto resultCopy = result;
        auto res = resultCopy.ExtractValue();
        if (res.IsSuccess()) {
            auto session = res.GetSession();
            TStringBuilder query;
            query << "DECLARE $STATE AS Text;\n"
                     "DECLARE $REDIRECT_URL AS Text;\n"
                     "DECLARE $IS_AJAX_REQUEST AS Bool;\n";
            query << "INSERT INTO `ydb/OidcSessions` (state, redirect_url, expiration_time, is_ajax_request)\n";
            query << "VALUES ($STATE, $REDIRECT_URL, CurrentUtcDateTime() + Interval('PT" << STATE_LIFE_TIME.Seconds() << "S'), $IS_AJAX_REQUEST);\n";
            auto txControl = NYdb::NTable::TTxControl::BeginTx(NYdb::NTable::TTxSettings::SerializableRW()).CommitTx();
            NYdb::TParamsBuilder params;
            params.AddParam("$STATE", NYdb::TValueBuilder().Utf8(state).Build());
            params.AddParam("$REDIRECT_URL", NYdb::TValueBuilder().Utf8(redirectUrl).Build());
            params.AddParam("$IS_AJAX_REQUEST", NYdb::TValueBuilder().Bool(isAjaxRequest).Build());
            auto executeDataQueryResult = session.ExecuteDataQuery(query, txControl, params.Build());
            executeDataQueryResult.Subscribe([cb = std::move(cb), session] (const NYdb::NTable::TAsyncDataQueryResult& result) mutable {
                NYdb::NTable::TAsyncDataQueryResult resultCopy = result;
                auto res = resultCopy.ExtractValue();
                if (res.IsSuccess()) {
                    cb(TStringBuilder() << "SaveSessionOnServerSide - success:\n" << (NYdb::TStatus&)res);
                } else {
                    // no result
                    cb(TStringBuilder() << "SaveSessionOnServerSide - failed to get result:\n" << (NYdb::TStatus&)res);
                }
                session.Close();
            });
        } else {
            // no session
            cb(TStringBuilder() << "SaveSessionOnServerSide - failed to get session:\n" << (NYdb::TStatus&)res);
        }
    });
}

TString TOidcSession::GetState() const {
    return State;
}

bool TOidcSession::GetIsAjaxRequest() const {
    return IsAjaxRequest;
}

TString TOidcSession::GetRedirectUrl() const {
    return RedirectUrl;
}

TString TOidcSession::CreateNameYdbOidcCookie(TStringBuf key, TStringBuf state) {
    static const TString YDB_OIDC_COOKIE = "ydb_oidc_cookie";
    return YDB_OIDC_COOKIE + "_" + HexEncode(HmacSHA256(key, state));
}

TString TOidcSession::GenerateCookie() const {
    TInstant expirationTime = TInstant::Now() + STATE_LIFE_TIME;
    TStringBuilder stateStruct;
    stateStruct << "{\"state\":\"" << State
                << "\",\"redirect_url\":\"" << RedirectUrl
                << "\",\"expiration_time\":" << ToString(expirationTime.TimeT())
                << ",\"ajax_request\":" << (IsAjaxRequest ? "true" : "false") << "}";
    TString digest = HmacSHA256(OidcClientSecret, stateStruct);
    TString cookieStruct {"{\"state_struct\":\"" + Base64Encode(stateStruct) + "\",\"digest\":\"" + Base64Encode(digest) + "\"}"};
    return Base64Encode(cookieStruct);
}

TString TOidcSession::GenerateState() {
    TStringBuilder sb;
    const size_t CHAR_NUMBER = 15;
    for (size_t i{0}; i < CHAR_NUMBER; i++) {
        sb << RandomNumber<char>();
    }
    return Base64EncodeUrlNoPadding(sb);
}

TStringBuf TOidcSession::GetRedirectUrl(const NHttp::THttpIncomingRequestPtr& request, bool isAjaxRequest) {
    NHttp::THeaders headers(request->Headers);
    TStringBuf requestedUrl = headers.Get("Referer");
    if (!isAjaxRequest || requestedUrl.empty()) {
        return request->URL;
    }
    return requestedUrl;
}

TString TOidcSession::Check(const TString& state, const NHttp::TCookies& cookies) {
    const TString cookieName {CreateNameYdbOidcCookie(OidcClientSecret, state)};
    if (!cookies.Has(cookieName)) {
        return TStringBuilder() << "Check state: Cannot find cookie " << cookieName;
    }
    TString cookieStruct = Base64Decode(cookies.Get(cookieName));
    TString stateStruct;
    TString expectedDigest;
    NJson::TJsonValue jsonValue;
    NJson::TJsonReaderConfig jsonConfig;
    if (NJson::ReadJsonTree(cookieStruct, &jsonConfig, &jsonValue)) {
        const NJson::TJsonValue* jsonStateStruct = nullptr;
        if (jsonValue.GetValuePointer("state_struct", &jsonStateStruct)) {
            stateStruct = jsonStateStruct->GetStringRobust();
            stateStruct = Base64Decode(stateStruct);
        }
        const NJson::TJsonValue* jsonDigest = nullptr;
        if (jsonValue.GetValuePointer("digest", &jsonDigest)) {
            expectedDigest = jsonDigest->GetStringRobust();
            expectedDigest = Base64Decode(expectedDigest);
        }
    }
    if (stateStruct.Empty() || expectedDigest.Empty()) {
        return "Check state: Struct with state and expected digest are empty";
    }
    TString digest = HmacSHA256(OidcClientSecret, stateStruct);
    if (expectedDigest != digest) {
        return "Check state: Calculated digest is not equal expected digest";
    }
    if (NJson::ReadJsonTree(stateStruct, &jsonConfig, &jsonValue)) {
        const NJson::TJsonValue* jsonState = nullptr;
        if (jsonValue.GetValuePointer("state", &jsonState)) {
            State = jsonState->GetStringRobust();
        }
        const NJson::TJsonValue* jsonRedirectUrl = nullptr;
        if (jsonValue.GetValuePointer("redirect_url", &jsonRedirectUrl)) {
            RedirectUrl = jsonRedirectUrl->GetStringRobust();
        } else {
            return "Check state: Redirect url not found in json";
        }
        const NJson::TJsonValue* jsonExpirationTime = nullptr;
        if (jsonValue.GetValuePointer("expiration_time", &jsonExpirationTime)) {
            timeval timeVal {
                .tv_sec = jsonExpirationTime->GetIntegerRobust()
            };
            if (TInstant::Now() > TInstant(timeVal)) {
                return "Check state: State life time expired";
            }
        } else {
            return "Check state: Expiration time not found in json";
        }
        const NJson::TJsonValue* jsonAjaxRequest = nullptr;
        if (jsonValue.GetValuePointer("ajax_request", &jsonAjaxRequest)) {
            IsAjaxRequest = jsonAjaxRequest->GetBooleanRobust();
        } else {
            return "Check state: Can not detect ajax request";
        }
    }
    return (!State.Empty() && State == state ? "" : "Unexpected state");
}

} // NOIDC
