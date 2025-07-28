#pragma once

#include <util/generic/string.h>
#include <ydb/core/protos/auth.pb.h>

namespace NJson {

class TJsonValue;

} // NJson

namespace NKikimr {

class TXdsBootstrapConfigBuilder {
    struct TConfigValues {
        TString DataCenterId;
        TString NodeId;
    };

private:
    NKikimrProto::TXdsBootstrap Config;

public:
    TXdsBootstrapConfigBuilder(const NKikimrProto::TXdsBootstrap& config);

    TString Build(const TConfigValues& configValues) const;

private:
    void BuildFieldNode(NJson::TJsonValue* const json, const TConfigValues& configValues) const;
    void BuildFieldXdsServers(NJson::TJsonValue* const json) const;
    void ConvertStringToJsonValue(const TString& jsonString, NJson::TJsonValue* const out) const;
};

} // NKikimr
