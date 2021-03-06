/**
 * @file
 */

#pragma once

#include "core/Common.h"
#include "core/Assert.h"
#include "Timestamp.h"
#include "FieldType.h"
#include "BindParam.h"
#include "State.h"
#include "ConstraintType.h"
#include "ForwardDecl.h"
#include "Order.h"

#include <cstdio>
#include <cstdlib>
#include <cstddef>
#include <unordered_map>
#include <vector>
#include <set>

namespace persistence {

// don't change the order without changing the string mapping
enum class Operator {
	ADD,
	SUBTRACT,
	SET,

	MAX
};

struct Field {
	std::string name;
	FieldType type = FieldType::STRING;
	Operator updateOperator = Operator::SET;
	// bitmask from ConstraintType
	uint32_t contraintMask = 0u;
	std::string defaultVal = "";
	int length = 0;
	intptr_t offset = -1;
	intptr_t nulloffset = -1;

	inline bool isAutoincrement() const {
		return (contraintMask & std::enum_value(ConstraintType::AUTOINCREMENT)) != 0u;
	}

	inline bool isIndex() const {
		return (contraintMask & std::enum_value(ConstraintType::INDEX)) != 0u;
	}

	inline bool isNotNull() const {
		return (contraintMask & std::enum_value(ConstraintType::NOTNULL)) != 0u;
	}

	inline bool isPrimaryKey() const {
		return (contraintMask & std::enum_value(ConstraintType::PRIMARYKEY)) != 0u;
	}

	inline bool isUnique() const {
		return (contraintMask & std::enum_value(ConstraintType::UNIQUE)) != 0u;
	}
};
typedef std::vector<Field> Fields;

using FieldName = const char *;

struct Constraint {
	std::vector<std::string> fields;
	// bitmask from persistence::Model::ConstraintType
	uint32_t types;
};
struct ForeignKey {
	std::string table;
	std::string field;
};

typedef std::unordered_map<std::string, Constraint> Constraints;
typedef std::unordered_map<std::string, ForeignKey> ForeignKeys;
typedef std::vector<std::set<std::string>> UniqueKeys;

/**
 * @brief The base class for your database models.
 *
 * Contains metadata to build the needed sql statements in the @c DBHandler
 *
 * @ingroup Persistence
 */
class Model {
protected:
	friend class DBHandler;
	friend class PreparedStatement;
	Fields _fields;
	std::string _tableName;
	int _primaryKeys = 0;
	uint8_t* _membersPointer;
	Constraints _constraints;
	UniqueKeys _uniqueKeys;
	ForeignKeys _foreignKeys;

	const Field& getField(const std::string& name) const;
	bool fillModelValues(State& state);
public:
	Model(const std::string& tableName);
	virtual ~Model();

	const std::string& tableName() const;

	const Fields& fields() const;

	const Constraints& constraints() const;

	const UniqueKeys& uniqueKeys() const;

	const ForeignKeys& foreignKeys() const;

	int primaryKeys() const;

	bool isPrimaryKey(const std::string& fieldname) const;

	template<class T>
	T getValue(const Field& f) const {
		core_assert(f.nulloffset < 0);
		core_assert(f.offset >= 0);
		const uint8_t* target = (const uint8_t*)(_membersPointer + f.offset);
		const T* targetValue = (const T*)target;
		return *targetValue;
	}

	template<class T>
	const T* getValuePointer(const Field& f) const {
		core_assert(f.nulloffset >= 0);
		core_assert(f.offset >= 0);
		const uint8_t* target = (const uint8_t*)(_membersPointer + f.offset);
		const T* targetValue = (const T*)target;
		return targetValue;
	}

	void setValue(const Field& f, const std::string& value);
	void setValue(const Field& f, const Timestamp& value);

	template<class TYPE>
	void setValue(const Field& f, const TYPE& value) {
		core_assert(f.offset >= 0);
		uint8_t* target = (uint8_t*)(_membersPointer + f.offset);
		TYPE* targetValue = (TYPE*)target;
		*targetValue = value;
	}

	void setIsNull(const Field& f, bool isNull);
	bool isNull(const Field& f) const;

	bool exec(const std::string& query) const;
	bool exec(const std::string& query);

	bool exec(const char* query) const;
	bool exec(const char* query);
};

inline bool Model::exec(const std::string& query) const {
	return exec(query.c_str());
}

inline bool Model::exec(const std::string& query) {
	return exec(query.c_str());
}

inline const std::string& Model::tableName() const {
	return _tableName;
}

inline const Fields& Model::fields() const {
	return _fields;
}

inline const Constraints& Model::constraints() const {
	return _constraints;
}

inline const UniqueKeys& Model::uniqueKeys() const {
	return _uniqueKeys;
}

inline const ForeignKeys& Model::foreignKeys() const {
	return _foreignKeys;
}

inline int Model::primaryKeys() const {
	return _primaryKeys;
}

}
