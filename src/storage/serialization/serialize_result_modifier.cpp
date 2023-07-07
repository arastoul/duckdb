//===----------------------------------------------------------------------===//
// This file is automatically generated by scripts/generate_serialization.py
// Do not edit this file manually, your changes will be overwritten
//===----------------------------------------------------------------------===//

#include "duckdb/common/serializer/format_serializer.hpp"
#include "duckdb/common/serializer/format_deserializer.hpp"
#include "duckdb/parser/result_modifier.hpp"

namespace duckdb {

void ResultModifier::FormatSerialize(FormatSerializer &serializer) const {
	serializer.WriteProperty("type", type);
}

unique_ptr<ResultModifier> ResultModifier::FormatDeserialize(FormatDeserializer &deserializer) {
	auto type = deserializer.ReadProperty<ResultModifierType>("type");
	unique_ptr<ResultModifier> result;
	switch (type) {
	case ResultModifierType::DISTINCT_MODIFIER:
		result = DistinctModifier::FormatDeserialize(deserializer);
		break;
	case ResultModifierType::LIMIT_MODIFIER:
		result = LimitModifier::FormatDeserialize(deserializer);
		break;
	case ResultModifierType::LIMIT_PERCENT_MODIFIER:
		result = LimitPercentModifier::FormatDeserialize(deserializer);
		break;
	case ResultModifierType::ORDER_MODIFIER:
		result = OrderModifier::FormatDeserialize(deserializer);
		break;
	default:
		throw SerializationException("Unsupported type for deserialization of ResultModifier!");
	}
	return result;
}

void LimitModifier::FormatSerialize(FormatSerializer &serializer) const {
	ResultModifier::FormatSerialize(serializer);
	serializer.WriteOptionalProperty("limit", limit);
	serializer.WriteOptionalProperty("offset", offset);
}

unique_ptr<ResultModifier> LimitModifier::FormatDeserialize(FormatDeserializer &deserializer) {
	auto result = duckdb::unique_ptr<LimitModifier>(new LimitModifier());
	deserializer.ReadOptionalProperty("limit", result->limit);
	deserializer.ReadOptionalProperty("offset", result->offset);
	return std::move(result);
}

void DistinctModifier::FormatSerialize(FormatSerializer &serializer) const {
	ResultModifier::FormatSerialize(serializer);
	serializer.WriteProperty("distinct_on_targets", distinct_on_targets);
}

unique_ptr<ResultModifier> DistinctModifier::FormatDeserialize(FormatDeserializer &deserializer) {
	auto result = duckdb::unique_ptr<DistinctModifier>(new DistinctModifier());
	deserializer.ReadProperty("distinct_on_targets", result->distinct_on_targets);
	return std::move(result);
}

void OrderModifier::FormatSerialize(FormatSerializer &serializer) const {
	ResultModifier::FormatSerialize(serializer);
	serializer.WriteProperty("orders", orders);
}

unique_ptr<ResultModifier> OrderModifier::FormatDeserialize(FormatDeserializer &deserializer) {
	auto result = duckdb::unique_ptr<OrderModifier>(new OrderModifier());
	deserializer.ReadProperty("orders", result->orders);
	return std::move(result);
}

void LimitPercentModifier::FormatSerialize(FormatSerializer &serializer) const {
	ResultModifier::FormatSerialize(serializer);
	serializer.WriteOptionalProperty("limit", limit);
	serializer.WriteOptionalProperty("offset", offset);
}

unique_ptr<ResultModifier> LimitPercentModifier::FormatDeserialize(FormatDeserializer &deserializer) {
	auto result = duckdb::unique_ptr<LimitPercentModifier>(new LimitPercentModifier());
	deserializer.ReadOptionalProperty("limit", result->limit);
	deserializer.ReadOptionalProperty("offset", result->offset);
	return std::move(result);
}

} // namespace duckdb
