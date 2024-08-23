#include <library/cpp/string_utils/base64/base64.h>
#include <util/random/random.h>
#include <util/string/hex.h>
#include <openssl/evp.h>
#include <openssl/hmac.h>
#include <openssl/sha.h>
#include "oidc_session.h"
#include "openid_connect.h"

namespace NOIDC {

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

TOidcSession::TOidcSession(const TOpenIdConnectSettings& settings)
    : OidcClientSecret(settings.ClientSecret)
{}

TOidcSession::TOidcSession(const NHttp::THttpIncomingRequestPtr& request, const TOpenIdConnectSettings& settings, bool isAjaxRequest)
    : State(GenerateState())
    , RedirectUrl(GetRedirectUrl(request, isAjaxRequest))
    , IsAjaxRequest(isAjaxRequest)
    , OidcClientSecret(settings.ClientSecret)
{}

TString TOidcSession::CreateOidcSessionCookie() const {
    return TStringBuilder() << CreateNameYdbOidcCookie(OidcClientSecret, State)
                            << "=" << GenerateCookie()
                            << "; Path=" << GetAuthCallbackUrl() << "; Max-Age=" << COOKIE_MAX_AGE_SEC <<"; SameSite=None; Secure";
}

void TOidcSession::SaveSessionOnServerSide(std::function<void()>) const {

}

TString TOidcSession::GetState() const {
    return State;
}

bool TOidcSession::GetIsAjaxRequest() const {
    return IsAjaxRequest;
}

TString TOidcSession::CreateNameYdbOidcCookie(TStringBuf key, TStringBuf state) {
    static const TString YDB_OIDC_COOKIE = "ydb_oidc_cookie";
    return YDB_OIDC_COOKIE + "_" + HexEncode(HmacSHA256(key, state));
}

TString TOidcSession::GenerateCookie() const {
    static const TDuration StateLifeTime = TDuration::Minutes(10);
    TInstant expirationTime = TInstant::Now() + StateLifeTime;
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
    // RemoveAppliedCookie(cookieName);
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
    // TString expectedState;
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
