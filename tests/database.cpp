#include "sqlitepp/database.h"
#include "sqlitepp/extension.h"
#include "sqlitepp/statement.h"
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


TEST(SQLITEPP_Database, CustomFunction) {
	database db;
	db.create_function("noop", 1, encoding::utf8, [](inplace_context* ctx, int argc, inplace_value** argv) {
        if(argc != 1) {
            return ctx->error("missing argument to noop");
        }
        return ctx->result_value(argv[0]);
    });

    statement stmt{db, "SELECT noop(42);"};
    for(auto& e : stmt.iterate<int64_t>()) {
        ASSERT_EQ(std::get<int64_t>(e), 42);
        return;
    }
    FAIL();
}
