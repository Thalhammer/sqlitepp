#pragma once
#include <array>
#include <type_traits>

#include <sqlitepp/fwd.h>

namespace sqlitepp {
namespace orm {
    template<size_t Size>
    struct partial {
        std::string query {};
        std::array<db_value,Size> params {};
    };

    template<size_t LhsSize, size_t RhsSize>
    struct condition;

    struct col {
        std::string name {};

        condition<0,1> operator==(const db_value& rhs);
        condition<0,1> operator!=(const db_value& rhs);

        condition<0,0> operator==(std::nullptr_t);
        condition<0,0> operator!=(std::nullptr_t);

        condition<0,2> between(const db_value& min, const db_value& max);
        condition<0,2> not_between(const db_value& min, const db_value& max);
        condition<0,1> like(const std::string& str);
        condition<0,1> glob(const std::string& str);

        condition<0,1> operator>(const db_value& rhs);
        condition<0,1> operator>=(const db_value& rhs);
        condition<0,1> operator<(const db_value& rhs);
        condition<0,1> operator<=(const db_value& rhs);
    };

    template<size_t LhsSize, size_t RhsSize>
    struct condition {
        partial<LhsSize> lhs {};
        std::string op {};
        partial<RhsSize> rhs {};

        std::string str() const {
            return lhs.query + " " + op + " " + rhs.query;
        }
        partial<LhsSize+RhsSize> as_partial() const {
            partial<LhsSize+RhsSize> p;
            p.query = lhs.query + " " + op + " " + rhs.query;
            std::copy(lhs.params.begin(), lhs.params.end(), p.params.begin());
            std::copy(rhs.params.begin(), rhs.params.end(), p.params.begin() + LhsSize);
            return p;
        }
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Weffc++"
        template<size_t OLhsSize, size_t ORhsSize>
        condition<LhsSize+RhsSize,OLhsSize+ORhsSize> operator&&(const condition<OLhsSize,ORhsSize>& other);
        template<size_t OLhsSize, size_t ORhsSize>
        condition<LhsSize+RhsSize,OLhsSize+ORhsSize> operator||(const condition<OLhsSize,ORhsSize>& other);
#pragma GCC diagnostic pop
        condition<0, LhsSize + RhsSize> operator!();
    };

    inline condition<0,1> col::operator==(const db_value& rhs) {
        condition<0,1> c;
        c.lhs = { "`" + name + "`", {} };
        c.op = "=";
        c.rhs = { "?", { rhs } };
        return c;
    }

    inline condition<0,1> col::operator!=(const db_value& rhs) {
        condition<0,1> c;
        c.lhs = { "`" + name + "`", {} };
        c.op = "<>";
        c.rhs = { "?", { rhs } };
        return c;
    }

    inline condition<0,0> col::operator==(std::nullptr_t) {
        condition<0,0> c;
        c.lhs = { "`" + name + "`", {} };
        c.op = "IS";
        c.rhs = { "NULL", {} };
        return c;
    }

    inline condition<0,0> col::operator!=(std::nullptr_t) {
        condition<0,0> c;
        c.lhs = { "`" + name + "`", {} };
        c.op = "IS NOT";
        c.rhs = { "NULL", {} };
        return c;
    }

    inline condition<0,2> col::between(const db_value& min, const db_value& max) {
        condition<0,2> c;
        c.lhs = { "`" + name + "`", {} };
        c.op = "BETWEEN";
        c.rhs = { "? AND ?", { min, max } };
        return c;
    }

    inline condition<0,2> col::not_between(const db_value& min, const db_value& max) {
        condition<0,2> c;
        c.lhs = { "`" + name + "`", {} };
        c.op = "NOT BETWEEN";
        c.rhs = { "? AND ?", { min, max } };
        return c;
    }

    inline condition<0,1> col::like(const std::string& str) {
        condition<0,1> c;
        c.lhs = { "`" + name + "`", {} };
        c.op = "LIKE";
        c.rhs = { "?", { str } };
        return c;
    }

    inline condition<0,1> col::glob(const std::string& str) {
        condition<0,1> c;
        c.lhs = { "`" + name + "`", {} };
        c.op = "GLOB";
        c.rhs = { "?", { str } };
        return c;
    }

    inline condition<0,1> col::operator>(const db_value& rhs) {
        condition<0,1> c;
        c.lhs = { "`" + name + "`", {} };
        c.op = ">";
        c.rhs = { "?", { rhs } };
        return c;
    }

    inline condition<0,1> col::operator>=(const db_value& rhs) {
        condition<0,1> c;
        c.lhs = { "`" + name + "`", {} };
        c.op = ">=";
        c.rhs = { "?", { rhs } };
        return c;
    }

    inline condition<0,1> col::operator<(const db_value& rhs) {
        condition<0,1> c;
        c.lhs = { "`" + name + "`", {} };
        c.op = "<";
        c.rhs = { "?", { rhs } };
        return c;
    }

    inline condition<0,1> col::operator<=(const db_value& rhs) {
        condition<0,1> c;
        c.lhs = { "`" + name + "`", {} };
        c.op = "<=";
        c.rhs = { "?", { rhs } };
        return c;
    }

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Weffc++"
    template<size_t LhsSize, size_t RhsSize>
    template<size_t OLhsSize, size_t ORhsSize>
    inline condition<LhsSize+RhsSize,OLhsSize+ORhsSize> condition<LhsSize,RhsSize>::operator&&(const condition<OLhsSize,ORhsSize>& other) {
        condition<LhsSize+RhsSize,OLhsSize+ORhsSize> c;
        c.lhs = as_partial();
        c.lhs.query = "(" + c.lhs.query + ")";
        c.op = "AND";
        c.rhs = other.as_partial();
        c.rhs.query = "(" + c.rhs.query + ")";
        return c;
    }

    template<size_t LhsSize, size_t RhsSize>
    template<size_t OLhsSize, size_t ORhsSize>
    inline condition<LhsSize+RhsSize,OLhsSize+ORhsSize> condition<LhsSize,RhsSize>::operator||(const condition<OLhsSize,ORhsSize>& other) {
        condition<LhsSize+RhsSize,OLhsSize+ORhsSize> c;
        c.lhs = as_partial();
        c.lhs.query = "(" + c.lhs.query + ")";
        c.op = "OR";
        c.rhs = other.as_partial();
        c.rhs.query = "(" + c.rhs.query + ")";
        return c;
    }
#pragma GCC diagnostic pop

    template<size_t LhsSize, size_t RhsSize>
    inline condition<0, LhsSize + RhsSize> condition<LhsSize, RhsSize>::operator!() {
        condition<0, LhsSize + RhsSize> res = {};
        res.op = "NOT";
        res.rhs = as_partial();
        res.rhs.query = "(" + res.rhs.query + ")";
        return res;
    }
}
namespace literals {
    inline orm::col operator ""_c(const char* str, std::size_t) {
        return orm::col{str};
    }
}
}