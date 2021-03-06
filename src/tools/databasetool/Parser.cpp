#include "Parser.h"
#include "Util.h"
#include "Mapping.h"
#include "core/Log.h"

namespace databasetool {

static const char *Keywords[] {
	"new", "delete", "const", "volatile",
	nullptr
};

static bool checkFieldname(const std::string& in) {
	const char **token = Keywords;
	while (*token) {
		if (in == *token) {
			return false;
		}
		++token;
	}
	return true;
}

bool parseField(core::Tokenizer& tok, Table& table) {
	if (!tok.hasNext()) {
		Log::error("Expected field name");
		return false;
	}
	const std::string& fieldname = tok.next();
	if (!tok.hasNext()) {
		Log::error("Expected { after field name %s", fieldname.c_str());
		return false;
	}
	if (!checkFieldname(fieldname)) {
		Log::error("Field %s uses a reserved keyword", fieldname.c_str());
		return false;
	}
	std::string token = tok.next();
	if (token != "{") {
		Log::error("Expected {, found %s", token.c_str());
		return false;
	}
	persistence::Field field;
	field.name = fieldname;
	while (tok.hasNext()) {
		token = tok.next();
		if (token == "}") {
			break;
		}
		if (token == "type") {
			if (!tok.hasNext()) {
				Log::error("missing type for field %s", fieldname.c_str());
				return false;
			}
			const std::string& type = tok.next();
			persistence::FieldType typeMapping = persistence::FieldType::STRING;
			bool foundType = false;
			for (int i = 0; i < persistence::MAX_FIELDTYPES; ++i) {
				if (core::string::iequals(type, FieldTypeNames[i])) {
					typeMapping = (persistence::FieldType)i;
					foundType = true;
					break;
				}
			}
			if (!foundType) {
				Log::error("invalid field type for field %s: %s", fieldname.c_str(), type.c_str());
				return false;
			}
			field.type = typeMapping;
		} else if (token == "notnull") {
			auto i = table.constraints.find(field.name);
			if (i != table.constraints.end()) {
				persistence::Constraint& c = i->second;
				c.types |= std::enum_value(persistence::ConstraintType::NOTNULL);
			} else {
				table.constraints.insert(std::make_pair(field.name, persistence::Constraint{{field.name}, (uint32_t)std::enum_value(persistence::ConstraintType::NOTNULL)}));
			}
		} else if (token == "default") {
			if (!tok.hasNext()) {
				Log::error("Missing value for default of %s", fieldname.c_str());
				return false;
			}
			if (!field.defaultVal.empty()) {
				Log::error("There is already a default value (%s) defined for field '%s'", field.defaultVal.c_str(), field.name.c_str());
				return false;
			}
			field.defaultVal = tok.next();
		} else if (token == "operator") {
			if (!tok.hasNext()) {
				Log::error("missing operator for field %s", fieldname.c_str());
				return false;
			}
			const std::string& opStr = tok.next();
			persistence::Operator op = persistence::Operator::SET;
			bool foundOperator = false;
			for (int i = 0; i < (int)persistence::Operator::MAX; ++i) {
				if (core::string::iequals(opStr, OperatorNames[i])) {
					op = (persistence::Operator)i;
					foundOperator = true;
					break;
				}
			}
			if (!foundOperator) {
				Log::error("invalid operator for field %s: %s", fieldname.c_str(), opStr.c_str());
				return false;
			}
			field.updateOperator = op;
		} else if (token == "length") {
			if (!tok.hasNext()) {
				Log::error("Missing value for length of '%s'", fieldname.c_str());
				return false;
			}
			if (field.type != persistence::FieldType::STRING && field.type != persistence::FieldType::PASSWORD) {
				Log::error("Field '%s' of type %i doesn't support length parameter", fieldname.c_str(), std::enum_value(field.type));
				return false;
			}
			field.length = core::string::toInt(tok.next());
		} else {
			Log::error("Unknown token found in table definition: %s", token.c_str());
			return false;
		}
	}
	table.fields.insert(std::make_pair(field.name, field));
	return true;
}

bool parseConstraints(core::Tokenizer& tok, Table& table) {
	if (!tok.hasNext()) {
		Log::error("Expected { after constraints");
		return false;
	}
	std::string token = tok.next();
	Log::trace("token: '%s'", token.c_str());
	if (token != "{") {
		Log::error("Expected {, found %s", token.c_str());
		return false;
	}
	while (tok.hasNext()) {
		std::set<std::string> fieldNames;
		token = tok.next();
		Log::trace("token: '%s'", token.c_str());
		if (token == "}") {
			break;
		}
		if (token == "(") {
			// parse token list
			while (tok.hasNext()) {
				token = tok.next();
				Log::trace("list token: '%s'", token.c_str());
				if (token == "," || token.empty()) {
					continue;
				}
				if (token == ")") {
					// this might happen because the separator and split char might follow each other
					if (!tok.hasNext()) {
						return false;
					}
					token = tok.next();
					if (!token.empty()) {
						token = tok.prev();
					}
					break;
				}
				fieldNames.insert(token);
			}
		} else {
			fieldNames.insert(token);
		}
		if (!tok.hasNext()) {
			Log::error("invalid constraint syntax for table %s", table.name.c_str());
			return false;
		}
		token = tok.next();
		Log::trace("type: '%s', table: %s", token.c_str(), table.name.c_str());
		uint32_t typeMapping = 0u;
		for (uint32_t i = 0; i < persistence::MAX_CONSTRAINTTYPES; ++i) {
			if (core::string::iequals(token, ConstraintTypeNames[i])) {
				typeMapping = 1 << i;
				break;
			}
		}
		if (typeMapping == 0u) {
			Log::error("invalid constraint syntax for table '%s': '%s' - there is no type mapping found", table.name.c_str(), token.c_str());
			return false;
		}

		if ((typeMapping & std::enum_value(persistence::ConstraintType::FOREIGNKEY)) != 0) {
			if (fieldNames.size() != 1) {
				Log::error("invalid foreign key constraint for table %s - expected to have exactly one field given",
						table.name.c_str());
				return false;
			}
			if (!tok.hasNext()) {
				Log::error("invalid foreign key constraint for table %s - expected foreign table", table.name.c_str());
				return false;
			}
			token = tok.next();
			if (!tok.hasNext()) {
				Log::error("invalid foreign key constraint for table %s - expected foreign field in table %s",
						table.name.c_str(), token.c_str());
				return false;
			}
			const persistence::ForeignKey fk{token, tok.next()};
			// there is only one entry
			const std::string& fieldName = *fieldNames.begin();
			table.foreignKeys.insert(std::make_pair(fieldName, fk));
		}

		if (fieldNames.size() == 1) {
			const std::string& name = *fieldNames.begin();
			auto i = table.constraints.find(name);
			if (i != table.constraints.end()) {
				persistence::Constraint& c = i->second;
				c.types |= typeMapping;
			} else {
				Log::trace("fieldnames: %i", (int)fieldNames.size());
				std::vector<std::string> fieldNamesVec;
				std::copy(fieldNames.begin(), fieldNames.end(), std::back_inserter(fieldNamesVec));
				table.constraints.insert(std::make_pair(name, persistence::Constraint{fieldNamesVec, typeMapping}));
			}
		} else {
			if (typeMapping != std::enum_value(persistence::ConstraintType::UNIQUE)) {
				Log::error("Unsupported type mapping for table '%s'", table.name.c_str());
				return false;
			}
			table.uniqueKeys.emplace_back(std::move(fieldNames));
		}
	}
	return true;
}

bool parseTable(core::Tokenizer& tok, Table& table) {
	if (!tok.hasNext()) {
		Log::error("Expected {");
		return false;
	}
	std::string token = tok.next();
	if (token != "{") {
		Log::error("Expected {, found %s", token.c_str());
		return false;
	}
	while (tok.hasNext()) {
		token = tok.next();
		if (token == "}") {
			break;
		}
		if (token == "field") {
			if (!parseField(tok, table)) {
				return false;
			}
		} else if (token == "constraints") {
			if (!parseConstraints(tok, table)) {
				return false;
			}
		} else if (token == "namespace") {
			if (!tok.hasNext()) {
				Log::error("missing namespace name for table %s", table.name.c_str());
				return false;
			}
			table.namespaceSrc = tok.next();
		} else if (token == "classname") {
			if (!tok.hasNext()) {
				Log::error("missing clasname name for table %s", table.name.c_str());
				return false;
			}
			table.classname = tok.next();
		} else {
			Log::error("Unknown token in table %s: %s", table.name.c_str(), token.c_str());
			return false;
		}
	}

	for (auto entry : table.constraints) {
		const persistence::Constraint& c = entry.second;
		for (const std::string& fieldName: c.fields) {
			if (table.fields.find(fieldName) == table.fields.end()) {
				Log::error("constraint referenced field wasn't found: '%s'", fieldName.c_str());
				return false;
			}
			Log::debug("transfer constraint to field for faster lookup for %s", fieldName.c_str());
			table.fields[fieldName].contraintMask |= c.types;
		}
		if ((c.types & std::enum_value(persistence::ConstraintType::PRIMARYKEY)) != 0) {
			table.primaryKeys += c.fields.size();
		}
	}

	sort(table.fields);

	return !table.fields.empty();
}

}
