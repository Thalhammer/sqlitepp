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

TEST(SQLITEPP_Database, GetTables) {
	database db;
	ASSERT_TRUE(db.get_tables().empty());
	db.exec("CREATE TABLE test (test1 INTEGER, test2 TEXT);");
	auto tbls = db.get_tables();
	ASSERT_EQ(tbls.size(), 1);
	ASSERT_EQ(*tbls.begin(), "test");
}

TEST(SQLITEPP_Database, GetSchemas) {
	database db;
	auto schemas = db.get_schemas();
	ASSERT_EQ(schemas.size(), 1);
	ASSERT_EQ(schemas.begin()->first, "main");
	ASSERT_EQ(schemas.begin()->second, "");

	db.exec("ATTACH DATABASE ':memory:' AS aux1;");

	schemas = db.get_schemas();
	ASSERT_EQ(schemas.size(), 2);
	auto it = schemas.begin();
	if (it->first == "main") {
		ASSERT_EQ(it->first, "main");
		ASSERT_EQ(it->second, "");
		it++;
		ASSERT_EQ(it->first, "aux1");
		ASSERT_EQ(it->second, "");
	} else {
		ASSERT_EQ(it->first, "aux1");
		ASSERT_EQ(it->second, "");
		it++;
		ASSERT_EQ(it->first, "main");
		ASSERT_EQ(it->second, "");
	}
}

TEST(SQLITEPP_Database, ApplicationID) {
	database db;
	ASSERT_EQ(db.get_application_id(), 0);
	auto id = rand();
	db.set_application_id(id);
	ASSERT_EQ(db.get_application_id(), id);
}

TEST(SQLITEPP_Database, UserVersion) {
	database db;
	ASSERT_EQ(db.get_user_version(), 0);
	auto id = rand();
	db.set_user_version(id);
	ASSERT_EQ(db.get_user_version(), id);
}

// TODO: We need a compiled extension somewhere
TEST(DISABLED_SQLITEPP_Database, LoadExtension) {
	database db;
	db.load_extension("myextension");
}
