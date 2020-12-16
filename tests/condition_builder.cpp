#include <gtest/gtest.h>
#include "sqlitepp/database.h"
#include "sqlitepp/condition.h"

using namespace sqlitepp;
using namespace sqlitepp::literals;

enum class test_enum {
    hello
};

TEST(SQLITEPP_ConditionBuilder, EqualsEnum) {
    std::string s;
    auto q = "t"_c == test_enum::hello;
    auto p = q.as_partial();
    ASSERT_EQ(p.query, "`t` = ?");
    ASSERT_EQ(p.params.size(), 1);
    ASSERT_TRUE(std::holds_alternative<orm::db_integer_type>(p.params[0]));
}

TEST(SQLITEPP_ConditionBuilder, NotEqualsEnum) {
    std::string s;
    auto q = "t"_c != test_enum::hello;
    auto p = q.as_partial();
    ASSERT_EQ(p.query, "`t` <> ?");
    ASSERT_EQ(p.params.size(), 1);
    ASSERT_TRUE(std::holds_alternative<orm::db_integer_type>(p.params[0]));
}

TEST(SQLITEPP_ConditionBuilder, EqualsString) {
    std::string s;
    auto q = "t"_c == "hello";
    auto p = q.as_partial();
    ASSERT_EQ(p.query, "`t` = ?");
    ASSERT_EQ(p.params.size(), 1);
    ASSERT_TRUE(std::holds_alternative<orm::db_text_type>(p.params[0]));
}

TEST(SQLITEPP_ConditionBuilder, NotEqualsString) {
    std::string s;
    auto q = "t"_c != "hello";
    auto p = q.as_partial();
    ASSERT_EQ(p.query, "`t` <> ?");
    ASSERT_EQ(p.params.size(), 1);
    ASSERT_TRUE(std::holds_alternative<orm::db_text_type>(p.params[0]));
}

TEST(SQLITEPP_ConditionBuilder, NegateCondition) {
    std::string s;
    auto q = !("t"_c == "hello");
    auto p = q.as_partial();
    ASSERT_EQ(p.query, " NOT (`t` = ?)");
    ASSERT_EQ(p.params.size(), 1);
    ASSERT_TRUE(std::holds_alternative<orm::db_text_type>(p.params[0]));
}

TEST(SQLITEPP_ConditionBuilder, IsNull) {
    std::string s;
    auto q = "t"_c == nullptr;
    auto p = q.as_partial();
    ASSERT_EQ(p.query, "`t` IS NULL");
    ASSERT_EQ(p.params.size(), 0);
}

TEST(SQLITEPP_ConditionBuilder, IsNotNull) {
    std::string s;
    auto q = "t"_c != nullptr;
    auto p = q.as_partial();
    ASSERT_EQ(p.query, "`t` IS NOT NULL");
    ASSERT_EQ(p.params.size(), 0);
}

TEST(SQLITEPP_ConditionBuilder, AndConditions) {
    std::string s;
    auto q = "t"_c == "hello" && "t2"_c == "test";
    auto p = q.as_partial();
    ASSERT_EQ(p.query, "(`t` = ?) AND (`t2` = ?)");
    ASSERT_EQ(p.params.size(), 2);
    ASSERT_TRUE(std::holds_alternative<orm::db_text_type>(p.params[0]));
    ASSERT_TRUE(std::holds_alternative<orm::db_text_type>(p.params[1]));
}

TEST(SQLITEPP_ConditionBuilder, OrConditions) {
    std::string s;
    auto q = "t"_c == "hello" || "t2"_c == "test";
    auto p = q.as_partial();
    ASSERT_EQ(p.query, "(`t` = ?) OR (`t2` = ?)");
    ASSERT_EQ(p.params.size(), 2);
    ASSERT_TRUE(std::holds_alternative<orm::db_text_type>(p.params[0]));
    ASSERT_TRUE(std::holds_alternative<orm::db_text_type>(p.params[1]));
}

TEST(SQLITEPP_ConditionBuilder, Between) {
    std::string s;
    auto q = "t"_c.between(static_cast<int64_t>(10),static_cast<int64_t>(20));
    auto p = q.as_partial();
    ASSERT_EQ(p.query, "`t` BETWEEN ? AND ?");
    ASSERT_EQ(p.params.size(), 2);
    ASSERT_TRUE(std::holds_alternative<orm::db_integer_type>(p.params[0]));
    ASSERT_TRUE(std::holds_alternative<orm::db_integer_type>(p.params[1]));
}

TEST(SQLITEPP_ConditionBuilder, NotBetween) {
    std::string s;
    auto q = "t"_c.not_between(static_cast<int64_t>(10),static_cast<int64_t>(20));
    auto p = q.as_partial();
    ASSERT_EQ(p.query, "`t` NOT BETWEEN ? AND ?");
    ASSERT_EQ(p.params.size(), 2);
    ASSERT_TRUE(std::holds_alternative<orm::db_integer_type>(p.params[0]));
    ASSERT_TRUE(std::holds_alternative<orm::db_integer_type>(p.params[1]));
}

TEST(SQLITEPP_ConditionBuilder, Like) {
    std::string s;
    auto q = "t"_c.like("test%");
    auto p = q.as_partial();
    ASSERT_EQ(p.query, "`t` LIKE ?");
    ASSERT_EQ(p.params.size(), 1);
    ASSERT_TRUE(std::holds_alternative<orm::db_text_type>(p.params[0]));
}

TEST(SQLITEPP_ConditionBuilder, Glob) {
    std::string s;
    auto q = "t"_c.glob("test%");
    auto p = q.as_partial();
    ASSERT_EQ(p.query, "`t` GLOB ?");
    ASSERT_EQ(p.params.size(), 1);
    ASSERT_TRUE(std::holds_alternative<orm::db_text_type>(p.params[0]));
}

TEST(SQLITEPP_ConditionBuilder, GreaterThan) {
    std::string s;
    auto q = "t"_c > static_cast<int64_t>(10);
    auto p = q.as_partial();
    ASSERT_EQ(p.query, "`t` > ?");
    ASSERT_EQ(p.params.size(), 1);
    ASSERT_TRUE(std::holds_alternative<orm::db_integer_type>(p.params[0]));
}

TEST(SQLITEPP_ConditionBuilder, GreaterThanEqual) {
    std::string s;
    auto q = "t"_c >= static_cast<int64_t>(10);
    auto p = q.as_partial();
    ASSERT_EQ(p.query, "`t` >= ?");
    ASSERT_EQ(p.params.size(), 1);
    ASSERT_TRUE(std::holds_alternative<orm::db_integer_type>(p.params[0]));
}

TEST(SQLITEPP_ConditionBuilder, LessThan) {
    std::string s;
    auto q = "t"_c < static_cast<int64_t>(10);
    auto p = q.as_partial();
    ASSERT_EQ(p.query, "`t` < ?");
    ASSERT_EQ(p.params.size(), 1);
    ASSERT_TRUE(std::holds_alternative<orm::db_integer_type>(p.params[0]));
}

TEST(SQLITEPP_ConditionBuilder, LessThanEqual) {
    std::string s;
    auto q = "t"_c <= static_cast<int64_t>(10);
    auto p = q.as_partial();
    ASSERT_EQ(p.query, "`t` <= ?");
    ASSERT_EQ(p.params.size(), 1);
    ASSERT_TRUE(std::holds_alternative<orm::db_integer_type>(p.params[0]));
}