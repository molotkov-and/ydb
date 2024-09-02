#include <library/cpp/string_utils/base64/base64.h>
#include <util/random/random.h>
#include <util/string/hex.h>
#include <util/datetime/base.h>
#include <openssl/evp.h>
#include <openssl/hmac.h>
#include <openssl/sha.h>
#include <ydb/public/sdk/cpp/client/ydb_table/table.h>
#include <ydb/public/sdk/cpp/client/ydb_params/params.h>
#include <ydb/public/sdk/cpp/client/ydb_types/status/status.h>
#include "oidc_session.h"
#include "openid_connect.h"

namespace NOIDC {

const TDuration TOidcSession::STATE_LIFE_TIME = TDuration::Minutes(10);

TOidcSession::TOidcSession(const TString& state, const TString& redirectUrl, bool isAjaxRequest)
    : State(state)
    , IsAjaxRequest(isAjaxRequest)
    , RedirectUrl(redirectUrl)
{}

TOidcSession::TOidcSession(const NHttp::THttpIncomingRequestPtr& request)
    : State(GenerateState())
    , IsAjaxRequest(DetectAjaxRequest(request))
    , RedirectUrl(GetRedirectUrl(request, IsAjaxRequest))
{}

TOidcSession::TOidcSession(const TString& state, const NHttp::THttpIncomingRequestPtr& request, bool isAjaxRequest)
    : State(state)
    , IsAjaxRequest(isAjaxRequest)
    , RedirectUrl(GetRedirectUrl(request, IsAjaxRequest))
{}

TString TOidcSession::CreateOidcSessionCookie(const TString& secret) const {
    return TStringBuilder() << CreateNameYdbOidcCookie(secret, State)
                            << "=" << GenerateCookie(secret)
                            << "; Path=" << GetAuthCallbackUrl() << "; Max-Age=" << COOKIE_MAX_AGE_SEC <<"; SameSite=None; Secure";
}

void TOidcSession::SetState(const TString& state) {
    State = state;
}

TString TOidcSession::GetState() const {
    return State;
}

void TOidcSession::SetIsAjaxRequest(bool isAjaxRequest) {
    IsAjaxRequest = isAjaxRequest;
}

bool TOidcSession::GetIsAjaxRequest() const {
    return IsAjaxRequest;
}

void TOidcSession::SetRedirectUrl(const TString& redirectUrl) {
    RedirectUrl = redirectUrl;
}

TString TOidcSession::GetRedirectUrl() const {
    return RedirectUrl;
}

TString TOidcSession::CreateNameYdbOidcCookie(TStringBuf key, TStringBuf state) {
    static const TString YDB_OIDC_COOKIE = "ydb_oidc_cookie";
    return YDB_OIDC_COOKIE + "_" + HexEncode(HmacSHA256(key, state));
}

TString TOidcSession::GenerateCookie(const TString& secret) const {
    TInstant expirationTime = TInstant::Now() + STATE_LIFE_TIME;
    TStringBuilder stateStruct;
    stateStruct << "{\"state\":\"" << State
                << "\",\"redirect_url\":\"" << RedirectUrl
                << "\",\"expiration_time\":" << ToString(expirationTime.TimeT())
                << ",\"ajax_request\":" << (IsAjaxRequest ? "true" : "false") << "}";
    TString digest = HmacSHA256(secret, stateStruct);
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

bool TOidcSession::DetectAjaxRequest(const NHttp::THttpIncomingRequestPtr& request) {
    const static THashMap<TStringBuf, TStringBuf> expectedHeaders {
        {"Accept", "application/json"}
    };
    NHttp::THeaders headers(request->Headers);
    for (const auto& el : expectedHeaders) {
        TStringBuf headerValue = headers.Get(el.first);
        if (!headerValue || headerValue.find(el.second) == TStringBuf::npos) {
            return false;
        }
    }
    return true;
}

} // NOIDC
