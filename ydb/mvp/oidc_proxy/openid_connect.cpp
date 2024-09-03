#include <util/random/random.h>
#include <util/string/builder.h>
#include <util/string/hex.h>
#include <openssl/evp.h>
#include <openssl/hmac.h>
#include <openssl/sha.h>
#include "openid_connect.h"

namespace NOIDC {
namespace {

void SetCORS(const NHttp::THttpIncomingRequestPtr& request, NHttp::THeadersBuilder* const headers) {
    TString origin = TString(NHttp::THeaders(request->Headers)["Origin"]);
    if (origin.empty()) {
        origin = "*";
    }
    headers->Set("Access-Control-Allow-Origin", origin);
    headers->Set("Access-Control-Allow-Credentials", "true");
    headers->Set("Access-Control-Allow-Headers", "Content-Type,Authorization,Origin,Accept");
    headers->Set("Access-Control-Allow-Methods", "OPTIONS, GET, POST");
}

NHttp::THttpOutgoingResponsePtr CreateResponseForAjaxRequest(const NHttp::THttpIncomingRequestPtr& request, NHttp::THeadersBuilder& headers, const TString& redirectUrl) {
    headers.Set("Content-Type", "application/json; charset=utf-8");
    SetCORS(request, &headers);
    TString body {"{\"error\":\"Authorization Required\",\"authUrl\":\"" + redirectUrl + "\"}"};
    return request->CreateResponse("401", "Unauthorized", headers, body);
}

} // namespace

TRestoreOidcSessionResult::TRestoreOidcSessionResult(const TStatus& status, const TOidcSession& session)
    : Session(session)
    , Status(status)
{}


bool TRestoreOidcSessionResult::IsSuccess() const {
    return Status.IsSuccess;
}

TString HmacSHA256(TStringBuf key, TStringBuf data) {
    unsigned char hash[SHA256_DIGEST_LENGTH];
    ui32 hl = SHA256_DIGEST_LENGTH;
    const auto* res = HMAC(EVP_sha256(), key.data(), key.size(), reinterpret_cast<const unsigned char*>(data.data()), data.size(), hash, &hl);
    Y_ENSURE(res);
    Y_ENSURE(hl == SHA256_DIGEST_LENGTH);
    return TString{reinterpret_cast<const char*>(res), hl};
}

void SetHeader(NYdbGrpc::TCallMeta& meta, const TString& name, const TString& value) {
    for (auto& [exname, exvalue] : meta.Aux) {
        if (exname == name) {
            exvalue = value;
            return;
        }
    }
    meta.Aux.emplace_back(name, value);
}

NHttp::THttpOutgoingResponsePtr GetHttpOutgoingResponsePtr(const TRequestAuthorizationCodeInitializer& initializer) {
    const auto& settings = initializer.Settings;
    const auto& request = initializer.IncomingRequest;
    TStringBuilder redirectUrl;
    redirectUrl << settings.GetAuthEndpointURL()
                << "?response_type=code"
                << "&scope=openid"
                << "&state=" << initializer.OidcSession.GetState()
                << "&client_id=" << settings.ClientId
                << "&redirect_uri=" << (request->Endpoint->Secure ? "https://" : "http://") << request->Host << GetAuthCallbackUrl();

    NHttp::THeadersBuilder responseHeaders;
    if (initializer.NeedStoreSessionOnClientSide) {
        responseHeaders.Set("Set-Cookie", initializer.OidcSession.CreateOidcSessionCookie(settings.ClientSecret));
    }
    if (initializer.OidcSession.GetIsAjaxRequest()) {
        return CreateResponseForAjaxRequest(request, responseHeaders, redirectUrl);
    }
    responseHeaders.Set("Location", redirectUrl);
    return request->CreateResponse("302", "Authorization required", responseHeaders);
}

TString CreateNameSessionCookie(TStringBuf key) {
    return "__Host_" + TOpenIdConnectSettings::SESSION_COOKIE + "_" + HexEncode(key);
}

const TString& GetAuthCallbackUrl() {
    static const TString callbackUrl = "/auth/callback";
    return callbackUrl;
}

TString CreateSecureCookie(const TString& key, const TString& value) {
    TStringBuilder cookieBuilder;
    cookieBuilder << CreateNameSessionCookie(key) << "=" << Base64Encode(value)
            << "; Path=/; Secure; HttpOnly; SameSite=None; Partitioned";
    return cookieBuilder;
}

TRestoreOidcSessionResult RestoreSessionStoredOnClientSide(const TString& state, const NHttp::TCookies& cookies, const TString& secret) {
    TStringBuilder errorMessage;
    errorMessage << "Restore oidc session failed: ";
    const TString cookieName {TOidcSession::CreateNameYdbOidcCookie(secret, state)};
    if (!cookies.Has(cookieName)) {
        return TRestoreOidcSessionResult({.IsSuccess = false,
                                         .IsErrorRetryable = false,
                                         .ErrorMessage = errorMessage << "Cannot find cookie " << cookieName});
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
        return TRestoreOidcSessionResult({.IsSuccess = false,
                                         .IsErrorRetryable = false,
                                         .ErrorMessage = errorMessage << "Struct with state and expected digest are empty"});
    }
    TString digest = HmacSHA256(secret, stateStruct);
    if (expectedDigest != digest) {
        return TRestoreOidcSessionResult({.IsSuccess = false,
                                         .IsErrorRetryable = false,
                                         .ErrorMessage = errorMessage << "Calculated digest is not equal expected digest"});
    }
    TString expectedState;
    TString redirectUrl;
    bool isAjaxRequest = false;
    if (NJson::ReadJsonTree(stateStruct, &jsonConfig, &jsonValue)) {
        const NJson::TJsonValue* jsonState = nullptr;
        if (jsonValue.GetValuePointer("state", &jsonState)) {
            expectedState = jsonState->GetStringRobust();
        }
        const NJson::TJsonValue* jsonRedirectUrl = nullptr;
        if (jsonValue.GetValuePointer("redirect_url", &jsonRedirectUrl)) {
            redirectUrl = jsonRedirectUrl->GetStringRobust();
        } else {
            return TRestoreOidcSessionResult({.IsSuccess = false,
                                             .IsErrorRetryable = false,
                                             .ErrorMessage = errorMessage << "Redirect url not found in cookie"});
        }
        const NJson::TJsonValue* jsonExpirationTime = nullptr;
        if (jsonValue.GetValuePointer("expiration_time", &jsonExpirationTime)) {
            timeval timeVal {
                .tv_sec = jsonExpirationTime->GetIntegerRobust(),
                .tv_usec = 0
            };
            if (TInstant::Now() > TInstant(timeVal)) {
                return TRestoreOidcSessionResult({.IsSuccess = false,
                                                 .IsErrorRetryable = true,
                                                 .ErrorMessage = errorMessage << "State life time expired"}, TOidcSession(state, redirectUrl));
            }
        } else {
            return TRestoreOidcSessionResult({.IsSuccess = false,
                                             .IsErrorRetryable = true,
                                             .ErrorMessage = errorMessage << "Expiration time not found in json"}, TOidcSession(state, redirectUrl));
        }
        const NJson::TJsonValue* jsonAjaxRequest = nullptr;
        if (jsonValue.GetValuePointer("ajax_request", &jsonAjaxRequest)) {
            isAjaxRequest = jsonAjaxRequest->GetBooleanRobust();
        } else {
            return TRestoreOidcSessionResult({.IsSuccess = false,
                                             .IsErrorRetryable = true,
                                             .ErrorMessage = errorMessage << "Can not detect ajax request"}, TOidcSession(state, redirectUrl));
        }
    }
    if (expectedState.Empty() || expectedState != state) {
        return TRestoreOidcSessionResult({.IsSuccess = false,
                                         .IsErrorRetryable = true,
                                         .ErrorMessage = errorMessage << "Unknown state"}, TOidcSession(state, redirectUrl, isAjaxRequest));
    }
    return TRestoreOidcSessionResult({.IsSuccess = true,
                                     .IsErrorRetryable = true,
                                     .ErrorMessage = ""}, TOidcSession(state, redirectUrl, isAjaxRequest));
}

} // NOIDC
