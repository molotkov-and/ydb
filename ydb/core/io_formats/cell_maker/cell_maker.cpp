#include "cell_maker.h"

#include <ydb/library/yverify_stream/yverify_stream.h>
#include <yql/essentials/types/binary_json/write.h>
#include <yql/essentials/types/dynumber/dynumber.h>
#include <yql/essentials/types/uuid/uuid.h>

#include <yql/essentials/minikql/dom/yson.h>
#include <yql/essentials/minikql/dom/json.h>
#include <yql/essentials/public/decimal/yql_decimal.h>
#include <yql/essentials/public/udf/udf_types.h>
#include <yql/essentials/utils/utf8.h>

#include <contrib/libs/double-conversion/double-conversion/double-conversion.h>
#include <library/cpp/json/json_writer.h>
#include <library/cpp/json/yson/json2yson.h>
#include <library/cpp/string_utils/base64/base64.h>
#include <library/cpp/string_utils/quote/quote.h>

#include <util/datetime/base.h>
#include <util/string/cast.h>

#include <typeinfo>

namespace NKikimr::NFormats {

namespace {

    bool CheckedUnescape(TStringBuf value, TString& result) {
        if (value.size() < 2 || value.front() != '"' || value.back() != '"') {
            return false;
        }

        result = CGIUnescapeRet(value.Skip(1).Chop(1));
        return true;
    }

    template <typename T>
    TString MakeError() {
        return TStringBuilder() << "" << typeid(T).name() << " is expected.";
    }

    template <typename T>
    bool TryParse(TStringBuf value, T& result) {
        return TryFromString(value, result);
    }

    template <>
    bool TryParse(TStringBuf value, double& result) {
        struct TCvt: public double_conversion::StringToDoubleConverter {
            inline TCvt()
                : StringToDoubleConverter(ALLOW_TRAILING_JUNK | ALLOW_HEX | ALLOW_LEADING_SPACES, 0.0, NAN, "inf", "nan")
            {
            }
        };

        int processed = 0;
        result = Singleton<TCvt>()->StringToDouble(value.data(), value.size(), &processed);

        return static_cast<size_t>(processed) == value.size();
    }

    template <>
    bool TryParse(TStringBuf value, float& result) {
        double tmp;
        if (TryParse(value, tmp)) {
            result = static_cast<float>(tmp);
            return true;
        }

        return false;
    }

    template <>
    bool TryParse(TStringBuf value, TInstant& result) {
        return TInstant::TryParseIso8601(value, result);
    }

    template <>
    bool TryParse(TStringBuf value, TString& result) {
        return CheckedUnescape(value, result);
    }

    template <>
    bool TryParse(TStringBuf value, TMaybe<NBinaryJson::TBinaryJson>& result) {
        TString unescaped;
        if (!CheckedUnescape(value, unescaped)) {
            return false;
        }

        auto serializedJson = NBinaryJson::SerializeToBinaryJson(unescaped);
        if (std::holds_alternative<TString>(serializedJson)) {
            return false;
        }

        result = std::get<NBinaryJson::TBinaryJson>(std::move(serializedJson));
        return true;
    }

    template <>
    bool TryParse(TStringBuf value, TMaybe<TString>& result) {
        result = NDyNumber::ParseDyNumberString(value);
        return result.Defined();
    }

    template <typename T>
    bool TryParse(TStringBuf value, T& result, TString& err, const NScheme::TTypeInfo& typeInfo) {
        Y_UNUSED(value);
        Y_UNUSED(result);
        Y_UNUSED(err);
        Y_UNUSED(typeInfo);
        Y_ABORT("TryParse with typeInfo is unimplemented");
    }

    template <>
    bool TryParse(TStringBuf value, NYql::NDecimal::TInt128& result, TString& err, const NScheme::TTypeInfo& typeInfo) {
        Y_UNUSED(err);
        
        if (!NYql::NDecimal::IsValid(value)) {
            return false;
        }

        result = NYql::NDecimal::FromString(value, typeInfo.GetDecimalType().GetPrecision(), typeInfo.GetDecimalType().GetScale());
        return true;
    }

    template<>
    bool TryParse<NPg::TConvertResult>(TStringBuf value, NPg::TConvertResult& result, TString& err, const NScheme::TTypeInfo& typeInfo) {
        TString unescaped;
        if (!CheckedUnescape(value, unescaped)) {
            err = MakeError<NPg::TConvertResult>();
            return false;
        }

        result = NPg::PgNativeBinaryFromNativeText(unescaped, typeInfo.GetPgTypeDesc());
        if (result.Error) {
            err = *result.Error;
            return false;
        }

        return true;
    }

    struct TUuidHolder {
        union {
            ui16 Array[8];
            char Str[16];
        } Buf;
    };

    template <>
    bool TryParse(TStringBuf value, TUuidHolder& result) {
        if (!NUuid::ParseUuidToArray(value, result.Buf.Array, false)) {
            return false;
        }
        return true;
    }

    template <typename T, typename U>
    using TConverter = std::function<U(const T&)>;

    template <typename T, typename U>
    U Implicit(const T& v) {
        return v;
    }

    ui16 Days(const TInstant& v) {
        return v.Days();
    }

    ui32 Seconds(const TInstant& v) {
        return v.Seconds();
    }

    ui64 MicroSeconds(const TInstant& v) {
        return v.MicroSeconds();
    }

    std::pair<ui64, ui64> Int128ToPair(const NYql::NDecimal::TInt128& v) {
        return NYql::NDecimal::MakePair(v);
    }

    TStringBuf BinaryJsonToStringBuf(const TMaybe<NBinaryJson::TBinaryJson>& v) {
        Y_ABORT_UNLESS(v.Defined());
        return TStringBuf(v->Data(), v->Size());
    }

    TStringBuf DyNumberToStringBuf(const TMaybe<TString>& v) {
        Y_ABORT_UNLESS(v.Defined());
        return TStringBuf(*v);
    }

    TStringBuf PgToStringBuf(const NPg::TConvertResult& v) {
        Y_ABORT_UNLESS(!v.Error);
        return v.Str;
    }

    TStringBuf UuidToStringBuf(const TUuidHolder& uuid) {
        return TStringBuf(uuid.Buf.Str, 16);
    }

    template <typename T, typename U = T>
    struct TCellMaker {
        static bool Make(TCell& c, TStringBuf v, TMemoryPool& pool, TString& err, TConverter<T, U> conv = &Implicit<T, U>) {
            T t;
            if (!TryParse<T>(v, t)) {
                err = MakeError<T>();
                return false;
            }

            return Conv(c, t, pool, conv);
        }

        static bool Make(TCell& c, TStringBuf v, TMemoryPool& pool, TString& err, TConverter<T, U> conv, const NScheme::TTypeInfo& typeInfo) {
            T t;
            if (!TryParse(v, t, err, typeInfo)) {
                return false;
            }

            return Conv(c, t, pool, conv);
        }        

        static bool MakeDirect(TCell& c, const T& v, TMemoryPool& pool, TString&, TConverter<T, U> conv = &Implicit<T, U>) {
            return Conv(c, v, pool, conv);
        }

    private:
        static bool Conv(TCell& c, const T& t, TMemoryPool& pool, TConverter<T, U> conv) {
            auto& u = *pool.Allocate<U>();
            u = conv(t);
            c = TCell(reinterpret_cast<const char*>(&u), sizeof(u));

            return true;
        }
    };

    template <typename T>
    struct TCellMaker<T, TStringBuf> {
        static bool Make(TCell& c, TStringBuf v, TMemoryPool& pool, TString& err, TConverter<T, TStringBuf> conv = &Implicit<T, TStringBuf>) {
            T t;
            if (!TryParse<T>(v, t)) {
                err = MakeError<T>();
                return false;
            }

            return Conv(c, t, pool, conv);
        }

        static bool MakeDirect(TCell& c, const T& v, TMemoryPool& pool, TString&, TConverter<T, TStringBuf> conv = &Implicit<T, TStringBuf>) {
            return Conv(c, v, pool, conv);
        }

        static bool Make(TCell& c, TStringBuf v, TMemoryPool& pool, TString& err, TConverter<T, TStringBuf> conv, const NScheme::TTypeInfo& typeInfo) {
            T t;
            if (!TryParse(v, t, err, typeInfo)) {
                return false;
            }

            return Conv(c, t, pool, conv);
        }

    private:
        static bool Conv(TCell& c, const T& t, TMemoryPool& pool, TConverter<T, TStringBuf> conv) {
            const auto u = pool.AppendString(conv(t));
            c = TCell(u.data(), u.size());

            return true;
        }
    };

    NJson::TJsonWriterConfig DefaultJsonConfig() {
        NJson::TJsonWriterConfig jsonConfig;
        jsonConfig.ValidateUtf8 = false;
        jsonConfig.WriteNanAsString = true;
        return jsonConfig;
    }

    TString WriteJson(const NJson::TJsonValue& json) {
        TStringStream str;
        NJson::WriteJson(&str, &json, DefaultJsonConfig());
        return str.Str();
    }

} // anonymous

void AddTwoCells(TCell& result, const TCell& cell1, const TCell& cell2, const NScheme::TTypeId& typeId) {

    Y_ENSURE(cell1.Size() == NScheme::GetFixedSize(typeId));
    Y_ENSURE(cell2.Size() == NScheme::GetFixedSize(typeId));

    switch (typeId) {
    case NScheme::NTypeIds::Int8:
        result = TCell::Make(i8(cell1.AsValue<i8>() + cell2.AsValue<i8>()));
        break;
    case NScheme::NTypeIds::Uint8:
        result = TCell::Make(ui8(cell1.AsValue<ui8>() + cell2.AsValue<ui8>()));
        break;
    case NScheme::NTypeIds::Int16:
        result = TCell::Make(i16(cell1.AsValue<i16>() + cell2.AsValue<i16>()));
        break;
    case NScheme::NTypeIds::Uint16:
        result = TCell::Make(ui16(cell1.AsValue<ui16>() + cell2.AsValue<ui16>()));
        break;
    case NScheme::NTypeIds::Int32:
        result = TCell::Make(i32(cell1.AsValue<i32>() + cell2.AsValue<i32>()));
        break;
    case NScheme::NTypeIds::Uint32:
        result = TCell::Make(ui32(cell1.AsValue<ui32>() + cell2.AsValue<ui32>()));
        break;
    case NScheme::NTypeIds::Int64:
        result = TCell::Make(i64(cell1.AsValue<i64>() + cell2.AsValue<i64>()));
        break;
    case NScheme::NTypeIds::Uint64:
        result = TCell::Make(ui64(cell1.AsValue<ui64>() + cell2.AsValue<ui64>()));
        break;
    case NScheme::NTypeIds::Float:
    case NScheme::NTypeIds::Double:
    case NScheme::NTypeIds::Date:
    case NScheme::NTypeIds::Datetime:
    case NScheme::NTypeIds::Timestamp:
    case NScheme::NTypeIds::Interval:
    case NScheme::NTypeIds::Date32:
    case NScheme::NTypeIds::Datetime64:
    case NScheme::NTypeIds::Timestamp64:
    case NScheme::NTypeIds::Interval64:
    case NScheme::NTypeIds::String:
    case NScheme::NTypeIds::String4k:
    case NScheme::NTypeIds::String2m:
    case NScheme::NTypeIds::Utf8:
    case NScheme::NTypeIds::Yson:
    case NScheme::NTypeIds::Json:
    case NScheme::NTypeIds::JsonDocument:
    case NScheme::NTypeIds::DyNumber:
    case NScheme::NTypeIds::Decimal:
    case NScheme::NTypeIds::Pg:
    case NScheme::NTypeIds::Uuid:
        Y_ENSURE(false);
    default:
        Y_ENSURE(false);
    }
}

bool MakeCell(TCell& cell, TStringBuf value, const NScheme::TTypeInfo& typeInfo, TMemoryPool& pool, TString& err) {
    if (value == "null") {
        return true;
    }

    switch (typeInfo.GetTypeId()) {
    case NScheme::NTypeIds::Bool:
        return TCellMaker<bool>::Make(cell, value, pool, err);
    case NScheme::NTypeIds::Int8:
        return TCellMaker<i8>::Make(cell, value, pool, err);
    case NScheme::NTypeIds::Uint8:
        return TCellMaker<ui8>::Make(cell, value, pool, err);
    case NScheme::NTypeIds::Int16:
        return TCellMaker<i16>::Make(cell, value, pool, err);
    case NScheme::NTypeIds::Uint16:
        return TCellMaker<ui16>::Make(cell, value, pool, err);
    case NScheme::NTypeIds::Int32:
        return TCellMaker<i32>::Make(cell, value, pool, err);
    case NScheme::NTypeIds::Uint32:
        return TCellMaker<ui32>::Make(cell, value, pool, err);
    case NScheme::NTypeIds::Int64:
        return TCellMaker<i64>::Make(cell, value, pool, err);
    case NScheme::NTypeIds::Uint64:
        return TCellMaker<ui64>::Make(cell, value, pool, err);
    case NScheme::NTypeIds::Float:
        return TCellMaker<float>::Make(cell, value, pool, err);
    case NScheme::NTypeIds::Double:
        return TCellMaker<double>::Make(cell, value, pool, err);
    case NScheme::NTypeIds::Date:
        return TCellMaker<TInstant, ui16>::Make(cell, value, pool, err, &Days);
    case NScheme::NTypeIds::Datetime:
        return TCellMaker<TInstant, ui32>::Make(cell, value, pool, err, &Seconds);
    case NScheme::NTypeIds::Timestamp:
        return TCellMaker<TInstant, ui64>::Make(cell, value, pool, err, &MicroSeconds);
    case NScheme::NTypeIds::Interval:
        return TCellMaker<i64>::Make(cell, value, pool, err);
    case NScheme::NTypeIds::Date32:
        return TCellMaker<i32>::Make(cell, value, pool, err);
    case NScheme::NTypeIds::Datetime64:
    case NScheme::NTypeIds::Timestamp64:
    case NScheme::NTypeIds::Interval64:
        return TCellMaker<i64>::Make(cell, value, pool, err);
    case NScheme::NTypeIds::String:
    case NScheme::NTypeIds::String4k:
    case NScheme::NTypeIds::String2m:
    case NScheme::NTypeIds::Utf8:
    case NScheme::NTypeIds::Yson:
    case NScheme::NTypeIds::Json:
        return TCellMaker<TString, TStringBuf>::Make(cell, value, pool, err);
    case NScheme::NTypeIds::JsonDocument:
        return TCellMaker<TMaybe<NBinaryJson::TBinaryJson>, TStringBuf>::Make(cell, value, pool, err, &BinaryJsonToStringBuf);
    case NScheme::NTypeIds::DyNumber:
        return TCellMaker<TMaybe<TString>, TStringBuf>::Make(cell, value, pool, err, &DyNumberToStringBuf);
    case NScheme::NTypeIds::Decimal:
        return TCellMaker<NYql::NDecimal::TInt128, std::pair<ui64, ui64>>::Make(cell, value, pool, err, &Int128ToPair, typeInfo);
    case NScheme::NTypeIds::Pg:
        return TCellMaker<NPg::TConvertResult, TStringBuf>::Make(cell, value, pool, err, &PgToStringBuf, typeInfo);
    case NScheme::NTypeIds::Uuid:
        return TCellMaker<TUuidHolder, TStringBuf>::Make(cell, value, pool, err, &UuidToStringBuf);
    default:
        return false;
    }
}

bool MakeCell(TCell& cell, const NJson::TJsonValue& value, const NScheme::TTypeInfo& typeInfo, TMemoryPool& pool, TString& err) {
    if (value.IsNull()) {
        return true;
    }

    try {
        switch (typeInfo.GetTypeId()) {
        case NScheme::NTypeIds::Bool:
            return TCellMaker<bool>::MakeDirect(cell, value.GetBooleanSafe(), pool, err);
        case NScheme::NTypeIds::Int8:
            return TCellMaker<i8>::MakeDirect(cell, value.GetIntegerSafe(), pool, err);
        case NScheme::NTypeIds::Uint8:
            return TCellMaker<ui8>::MakeDirect(cell, value.GetUIntegerSafe(), pool, err);
        case NScheme::NTypeIds::Int16:
            return TCellMaker<i16>::MakeDirect(cell, value.GetIntegerSafe(), pool, err);
        case NScheme::NTypeIds::Uint16:
            return TCellMaker<ui16>::MakeDirect(cell, value.GetUIntegerSafe(), pool, err);
        case NScheme::NTypeIds::Int32:
            return TCellMaker<i32>::MakeDirect(cell, value.GetIntegerSafe(), pool, err);
        case NScheme::NTypeIds::Uint32:
            return TCellMaker<ui32>::MakeDirect(cell, value.GetUIntegerSafe(), pool, err);
        case NScheme::NTypeIds::Int64:
            return TCellMaker<i64>::MakeDirect(cell, value.GetIntegerSafe(), pool, err);
        case NScheme::NTypeIds::Uint64:
            return TCellMaker<ui64>::MakeDirect(cell, value.GetUIntegerSafe(), pool, err);
        case NScheme::NTypeIds::Float:
            return TCellMaker<float>::MakeDirect(cell, value.GetDoubleSafe(), pool, err);
        case NScheme::NTypeIds::Double:
            return TCellMaker<double>::MakeDirect(cell, value.GetDoubleSafe(), pool, err);
        case NScheme::NTypeIds::Date:
            return TCellMaker<TInstant, ui16>::Make(cell, value.GetStringSafe(), pool, err, &Days);
        case NScheme::NTypeIds::Datetime:
            return TCellMaker<TInstant, ui32>::Make(cell, value.GetStringSafe(), pool, err, &Seconds);
        case NScheme::NTypeIds::Timestamp:
            return TCellMaker<TInstant, ui64>::Make(cell, value.GetStringSafe(), pool, err, &MicroSeconds);
        case NScheme::NTypeIds::Interval:
            return TCellMaker<i64>::MakeDirect(cell, value.GetIntegerSafe(), pool, err);
        case NScheme::NTypeIds::Date32:
            return TCellMaker<i32>::MakeDirect(cell, value.GetIntegerSafe(), pool, err);
        case NScheme::NTypeIds::Datetime64:
        case NScheme::NTypeIds::Timestamp64:
        case NScheme::NTypeIds::Interval64:
            return TCellMaker<i64>::MakeDirect(cell, value.GetIntegerSafe(), pool, err);
        case NScheme::NTypeIds::String:
        case NScheme::NTypeIds::String4k:
        case NScheme::NTypeIds::String2m:
            return TCellMaker<TString, TStringBuf>::MakeDirect(cell, Base64Decode(value.GetStringSafe()), pool, err);
        case NScheme::NTypeIds::Utf8:
            return TCellMaker<TString, TStringBuf>::MakeDirect(cell, value.GetStringSafe(), pool, err);
        case NScheme::NTypeIds::Yson:
            return TCellMaker<TString, TStringBuf>::MakeDirect(cell, NJson2Yson::SerializeJsonValueAsYson(value), pool, err);
        case NScheme::NTypeIds::Json:
            return TCellMaker<TString, TStringBuf>::MakeDirect(cell, NFormats::WriteJson(value), pool, err);
        case NScheme::NTypeIds::JsonDocument:
            if (auto result = NBinaryJson::SerializeToBinaryJson(NFormats::WriteJson(value)); std::holds_alternative<NBinaryJson::TBinaryJson>(result)) {
                return TCellMaker<TMaybe<NBinaryJson::TBinaryJson>, TStringBuf>::MakeDirect(cell, std::get<NBinaryJson::TBinaryJson>(std::move(result)), pool, err, &BinaryJsonToStringBuf);
            } else {
                return false;
            }
        case NScheme::NTypeIds::DyNumber:
            return TCellMaker<TMaybe<TString>, TStringBuf>::Make(cell, value.GetStringSafe(), pool, err, &DyNumberToStringBuf);
        case NScheme::NTypeIds::Decimal:
            return TCellMaker<NYql::NDecimal::TInt128, std::pair<ui64, ui64>>::Make(cell, value.GetStringSafe(), pool, err, &Int128ToPair, typeInfo);
        case NScheme::NTypeIds::Pg:
            if (auto result = NPg::PgNativeBinaryFromNativeText(value.GetStringSafe(), typeInfo.GetPgTypeDesc()); result.Error) {
                err = *result.Error;
                return false;
            } else {
                return TCellMaker<NPg::TConvertResult, TStringBuf>::MakeDirect(cell, result, pool, err, &PgToStringBuf);
            }
        case NScheme::NTypeIds::Uuid:
            return TCellMaker<TUuidHolder, TStringBuf>::Make(cell, value.GetStringSafe(), pool, err, &UuidToStringBuf);
        default:
            return false;
        }
    } catch (const yexception&) {
        return false;
    }
}

bool CheckCellValue(const TCell& cell, const NScheme::TTypeInfo& typeInfo) {
    if (cell.IsNull()) {
        return true;
    }

    switch (typeInfo.GetTypeId()) {
    case NScheme::NTypeIds::Bool:
    case NScheme::NTypeIds::Int8:
    case NScheme::NTypeIds::Uint8:
    case NScheme::NTypeIds::Int16:
    case NScheme::NTypeIds::Uint16:
    case NScheme::NTypeIds::Int32:
    case NScheme::NTypeIds::Uint32:
    case NScheme::NTypeIds::Int64:
    case NScheme::NTypeIds::Uint64:
    case NScheme::NTypeIds::Float:
    case NScheme::NTypeIds::Double:
    case NScheme::NTypeIds::String:
    case NScheme::NTypeIds::String4k:
    case NScheme::NTypeIds::String2m:
    case NScheme::NTypeIds::JsonDocument: // checked at parsing time
    case NScheme::NTypeIds::DyNumber: // checked at parsing time
    case NScheme::NTypeIds::Pg:       // checked at parsing time
    case NScheme::NTypeIds::Uuid:       // checked at parsing time
        return true;
    case NScheme::NTypeIds::Date:
        return cell.AsValue<ui16>() < NUdf::MAX_DATE;
    case NScheme::NTypeIds::Datetime:
        return cell.AsValue<ui32>() < NUdf::MAX_DATETIME;
    case NScheme::NTypeIds::Timestamp:
        return cell.AsValue<ui64>() < NUdf::MAX_TIMESTAMP;
    case NScheme::NTypeIds::Interval:
        return (ui64)std::abs(cell.AsValue<i64>()) < NUdf::MAX_TIMESTAMP;
    case NScheme::NTypeIds::Date32:
        return cell.AsValue<i32>() < NUdf::MAX_DATE32;
    case NScheme::NTypeIds::Datetime64:
        return cell.AsValue<i64>() < NUdf::MAX_DATETIME64;
    case NScheme::NTypeIds::Timestamp64:
        return cell.AsValue<i64>() < NUdf::MAX_TIMESTAMP64;
    case NScheme::NTypeIds::Interval64:
        return std::abs(cell.AsValue<i64>()) < NUdf::MAX_INTERVAL64;
    case NScheme::NTypeIds::Utf8:
        return NYql::IsUtf8(cell.AsBuf());
    case NScheme::NTypeIds::Yson:
        return NYql::NDom::IsValidYson(cell.AsBuf());
    case NScheme::NTypeIds::Json:
        return NYql::NDom::IsValidJson(cell.AsBuf());
    case NScheme::NTypeIds::Decimal:
        return !NYql::NDecimal::IsError(cell.AsValue<NYql::NDecimal::TInt128>());
    default:
        return false;
    }
}

}
