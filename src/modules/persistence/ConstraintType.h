/**
 * @file
 */

#pragma once

namespace persistence {

// don't change the order without changing the string mapping
enum class ConstraintType {
	UNIQUE = 1 << 0,
	PRIMARYKEY = 1 << 1,
	AUTOINCREMENT = 1 << 2,
	NOTNULL = 1 << 3,
	INDEX = 1 << 4,
	FOREIGNKEY = 1 << 5
};
static constexpr int MAX_CONSTRAINTTYPES = 6;

}
