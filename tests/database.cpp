#include "sqlitepp/database.h"
#include <gtest/gtest.h>

using namespace sqlitepp;

TEST(SQLITEPP_Database, OpenMemory) {
	database db;
}

TEST(SQLITEPP_Database, HasTable) {
	database db;
	ASSERT_FALSE(db.has_table("test"));
	db.exec("CREATE TABLE test (test1 INTEGER, test2 TEXT);");
	ASSERT_TRUE(db.has_table("test"));
}
