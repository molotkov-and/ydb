#include "xds_bootstrap_config_builder.h"

#include <library/cpp/protobuf/json/proto2json.h>
#include <library/cpp/json/json_writer.h>
#include <library/cpp/json/json_reader.h>

namespace NKikimr {

TXdsBootstrapConfigBuilder::TXdsBootstrapConfigBuilder(const NKikimrProto::TXdsBootstrap& config)
    : Config(config)
{}

TString TXdsBootstrapConfigBuilder::Build(const TConfigValues& configValues) const {
    NJson::TJsonValue xdsBootstrapConfigJson;
    NProtobufJson::Proto2Json(Config, xdsBootstrapConfigJson, {.FieldNameMode = NProtobufJson::TProto2JsonConfig::FldNameMode::FieldNameSnakeCaseDense});
    BuildFieldNode(&xdsBootstrapConfigJson, configValues);
    BuildFieldXdsServers(&xdsBootstrapConfigJson);
    return NJson::WriteJson(xdsBootstrapConfigJson, false);
}

void TXdsBootstrapConfigBuilder::BuildFieldNode(NJson::TJsonValue* const json, const TConfigValues& configValues) const {
    NJson::TJsonValue& nodeJson = (*json)["node"];
    if (Config.GetNode().HasMeta()) {
        // Message in protobuf can not contain field with name "metadata", so
        // Create field "meta" with string in JSON format
        // Convert string from field "meta" to JsonValue struct and write to field "metadata"
        ConvertStringToJsonValue(nodeJson["meta"].GetString(), &nodeJson["metadata"]);
        nodeJson.EraseValue("meta");
    }
    if (!Config.GetNode().HasId()) {
        nodeJson["id"] = configValues.NodeId;
    }
    if (!Config.GetNode().GetLocality().HasZone()) {
        nodeJson["locality"]["zone"] = configValues.DataCenterId;
    }
}

void TXdsBootstrapConfigBuilder::BuildFieldXdsServers(NJson::TJsonValue* const json) const {
    NJson::TJsonValue& xdsServersJson = *json;
    NJson::TJsonValue::TArray xdsServers;
    xdsServersJson["xds_servers"].GetArray(&xdsServers);
    xdsServersJson.EraseValue("xds_servers");
    for (auto& xdsServerJson : xdsServers) {
        NJson::TJsonValue::TArray channelCreds;
        xdsServerJson["channel_creds"].GetArray(&channelCreds);
        xdsServerJson.EraseValue("channel_creds");
        for (auto& channelCredJson : channelCreds) {
            if (channelCredJson.Has("config")) {
                ConvertStringToJsonValue(channelCredJson["config"].GetString(), &channelCredJson["config"]);
            }
            xdsServerJson["channel_creds"].AppendValue(channelCredJson);
        }
        xdsServersJson["xds_servers"].AppendValue(xdsServerJson);
    }
}

void TXdsBootstrapConfigBuilder::ConvertStringToJsonValue(const TString& jsonString, NJson::TJsonValue* const out) const {
    NJson::TJsonValue jsonValue;
    NJson::TJsonReaderConfig jsonConfig;
    if (NJson::ReadJsonTree(jsonString, &jsonConfig, &jsonValue)) {
        *out = jsonValue;
    }
}

} // NKikimr
