#include <gtest/gtest.h>
#include "../include/sqlitepp/database.h"
#include "../include/sqlitepp/orm.h"

using namespace sqlitepp;
using namespace sqlitepp::orm;

struct my_entity : orm::entity {
    enum class e_test {
        hello = 10,
        hello2 = 20
    };
    e_test t {e_test::hello};
    std::optional<int64_t> test_optional {};

    using entity::entity;

    static const orm::class_info _class_info;
    const orm::class_info& get_class_info() const noexcept override { return _class_info; }
};

const orm::class_info my_entity::_class_info = orm::builder<my_entity>("e")
    .field("t", &my_entity::t)
    .field("test_optional", &my_entity::test_optional)
    .build();

TEST(SQLITEPP_ORM, ReadWriteEnum) {
    database db;
    my_entity e(db);
    auto& info = e.get_class_info();
    auto* field = info.get_field_by_name("t");
    
    ASSERT_NE(field, nullptr);
    ASSERT_FALSE(field->nullable);

    auto val = field->getter(&e);
    ASSERT_TRUE(std::holds_alternative<db_integer_type>(val));
    ASSERT_EQ(std::get<db_integer_type>(val), 10);
    e.t = my_entity::e_test::hello2;
    val = field->getter(&e);
    ASSERT_TRUE(std::holds_alternative<db_integer_type>(val));
    ASSERT_EQ(std::get<db_integer_type>(val), 20);
    val = db_integer_type{10};
    field->setter(&e, val);
    ASSERT_EQ(e.t, my_entity::e_test::hello);
}

TEST(SQLITEPP_ORM, ReadWriteOptional) {
    database db;
    my_entity e(db);
    auto& info = e.get_class_info();
    auto* field = info.get_field_by_name("test_optional");
    
    ASSERT_NE(field, nullptr);
    ASSERT_TRUE(field->nullable);

    auto val = field->getter(&e);
    ASSERT_TRUE(std::holds_alternative<db_null_type>(val));

    e.test_optional = 100;
    val = field->getter(&e);
    ASSERT_TRUE(std::holds_alternative<db_integer_type>(val));
    ASSERT_EQ(std::get<db_integer_type>(val), 100);

    val = db_integer_type{10};
    field->setter(&e, val);
    ASSERT_TRUE(e.test_optional.has_value());
    ASSERT_EQ(e.test_optional, 10);

    val = db_null_type{};
    field->setter(&e, val);
    ASSERT_FALSE(e.test_optional.has_value());
}

TEST(SQLITEPP_ORM, IsModified) {
    database db;
    my_entity e(db);
    auto& info = e.get_class_info();
    db.exec(generate_create_table(info));

    ASSERT_TRUE(e.is_modified());
    e.save();
    ASSERT_FALSE(e.is_modified());
    e.test_optional = 1337;
    ASSERT_TRUE(e.is_modified());
    e.save();
    ASSERT_FALSE(e.is_modified());

    e.test_optional.reset();
    ASSERT_TRUE(e.is_modified());
    e.reset();
    ASSERT_FALSE(e.is_modified());
    ASSERT_TRUE(e.test_optional.has_value());
    ASSERT_EQ(e.test_optional.value(), 1337);
}
