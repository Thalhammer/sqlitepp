#include "sqlitepp/orm.h"
#include <set>

namespace sqlitepp {
    namespace orm {

        static void bind_db_val(statement& s, size_t i, const db_value& val) {
            if(std::holds_alternative<db_null_type>(val))
                return;
            if(std::holds_alternative<db_blob_type>(val))
                return s.bind(i, std::get<db_blob_type>(val));
            if(std::holds_alternative<db_text_type>(val))
                return s.bind(i, std::get<db_text_type>(val));
            if(std::holds_alternative<db_integer_type>(val))
                return s.bind(i, std::get<db_integer_type>(val));
            if(std::holds_alternative<db_real_type>(val))
                return s.bind(i, std::get<db_real_type>(val));
            throw std::logic_error("unreachable");
        }

        void entity::from_result(const sqlitepp::result_iterator& it) {
            auto& info = this->get_class_info();
            this->_rowid_ = it.column_int64("_rowid_");
            this->m_db_vals.resize(info.fields.size());
            for(size_t i=0; i<info.fields.size(); i++) {
                auto& e = info.fields[i];
                db_value val{db_null_type{}};
                switch(e.type) {
                case db_type::blob: {
                    auto data = it.column_blob(e.name);
                    val.emplace<db_blob_type>(data.second);
                    std::copy(static_cast<const uint8_t*>(data.first),
                        static_cast<const uint8_t*>(data.first) + data.second,
                        std::get<db_blob_type>(val).data());
                    break;
                }
                case db_type::text: val = it.column_string(e.name); break;
                case db_type::real: val = it.column_double(e.name); break;
                case db_type::integer: val = it.column_int64(e.name); break;
                }
                e.setter(this, val);
                this->m_db_vals[i] = val;
            }
        }

        bool entity::is_modified() const {
            auto& info = this->get_class_info();
            if(m_db_vals.size() != info.fields.size()) return true;
            for(size_t i = 0; i < m_db_vals.size(); i++) {
                auto dbval = info.fields[i].getter(this);
                if(dbval != m_db_vals[i]) return true;
            }
            return false;
        }

        void entity::reset() {
            auto& info = this->get_class_info();
            for(size_t i = 0; i < std::min(m_db_vals.size(), info.fields.size()); i++) {
                info.fields[i].setter(this, m_db_vals[i]);
            }
        }

        void entity::remove() {
            if(this->_rowid_ < 0) return;
            auto& info = this->get_class_info();
            std::string query = "DELETE FROM ";
            if(!info.schema.empty()) query += "`" + info.schema + "`";
            query += "`" + info.table + "` WHERE _rowid_ = ?;";

            statement stmt(this->m_db, query);
            stmt.bind(1, this->_rowid_);
            stmt.execute();
            this->_rowid_ = -1;
            for(auto& f : info.fields) {
                if(f.row_id) f.setter(this, this->_rowid_);
            }
        }

        void entity::insert() {
            auto& info = this->get_class_info();
            std::string query = "INSERT INTO ";
            if(!info.schema.empty()) query += "`" + info.schema + "`";
            query += "`" + info.table + "` (";
            for(size_t i = 0; i < info.fields.size(); i++) {
                if(i != 0) query += ", ";
                query += "`" + info.fields[i].name + "`";
            }
            query += ") VALUES (";
            for(size_t i = 0; i < info.fields.size(); i++) {
                if(i != 0) query += ", ";
                query += "?";
            }
            query += ");";
            statement stmt(this->m_db, query);
            std::vector<db_value> vals = this->m_db_vals;
            vals.resize(info.fields.size());
            for(size_t i = 0; i < info.fields.size(); i++) {
                if(info.fields[i].row_id) continue;
                auto val = info.fields[i].getter(this);
                bind_db_val(stmt, i + 1, val);
                vals[i] = val;
            }
            stmt.execute();
            this->_rowid_ = this->m_db.last_insert_rowid();
            for(size_t i = 0; i < info.fields.size(); i++) {
                auto& f = info.fields[i];
                if(!f.row_id) continue;
                f.setter(this, this->_rowid_);
                vals[i] = this->_rowid_;
            }
            this->m_db_vals = std::move(vals);
        }

        // TODO: Only update changed columns
        void entity::update() {
            auto& info = this->get_class_info();
            std::string query = "UPDATE ";
            if(!info.schema.empty()) query += "`" + info.schema + "`";
            query += "`" + info.table + "` SET ";
            for(size_t i = 0; i < info.fields.size(); i++) {
                if(i != 0) query += ", ";
                query += "`" + info.fields[i].name + "` = ?";
            }
            query += " WHERE _rowid_ = ?;";
            statement stmt(this->m_db, query);
            std::vector<db_value> vals = this->m_db_vals;
            vals.resize(info.fields.size());
            for(size_t i = 0; i < info.fields.size(); i++) {
                auto val = info.fields[i].getter(this);
                bind_db_val(stmt, i + 1, val);
                vals[i] = val;
            }
            this->m_db_vals = std::move(vals);
            stmt.bind(info.fields.size() + 1, this->_rowid_);
            stmt.execute();
        }

        void entity::save() {
            if(this->_rowid_ >= 0) this->update();
            else this->insert();
        }

        std::function<void(class_info&, field_info&)> primary_key(bool v) {
            return [v](class_info&, field_info& f){
                f.primary_key = v;
            };
        }
        std::function<void(class_info&, field_info&)> row_id(bool v) {
            return [v](class_info&, field_info& f){
                f.primary_key = v;
                f.row_id = v;
            };
        }
        std::function<void(class_info&, field_info&)> nullable(bool v) {
            return [v](class_info&, field_info& f){
                f.nullable = v;
            };
        }
        std::function<void(class_info&, field_info&)> unique_id(int id) {
            return [id](class_info&, field_info& f) {
                f.unique_id = id;
            };
        }
        std::function<void(class_info&, field_info&)> fk(const std::string& table, const std::string& field, fk_action del_action, fk_action update_action) {
            return [table, field, del_action, update_action](class_info&, field_info& f) {
                f.fk_table = table;
                f.fk_field = field;
                f.fk_del_action = del_action;
                f.fk_update_action = update_action;
            };
        }
        std::function<void(class_info&, field_info&)> default_value(db_value val) {
            return [val](class_info&, field_info& f) {
                f.default_value = val;
            };
        }
        std::function<void(class_info&)> schema(std::string schema) {
            return [schema](class_info& c) {
                c.schema = schema;
            };
        }
        std::function<void(class_info&)> temporary(bool v) {
            return [v](class_info& c){
                c.is_temporary = v;
            };
        }

        std::string generate_create_table(const class_info& info) {
            std::string res;
            if(info.is_temporary) res += "CREATE TEMPORARY TABLE ";
            else res += "CREATE TABLE ";
            if(!info.schema.empty()) res += "`" + info.schema + "`.";
            res += "`" + info.table + "` (\n";
            bool first = true;
            std::vector<std::string> pk_fields;
            std::map<int, std::vector<std::string>> unique_fields;
            for(auto& e : info.fields) {
                if(e.primary_key) pk_fields.push_back(e.name);
                if(e.unique_id > 0 || e.unique_id == field_info::UNIQUE_ID_DEFAULT) {
                    unique_fields[e.unique_id].push_back(e.name);
                }
                if(!first) res += ",\n";
                first = false;
                res += "\t`" + e.name + "`";
                switch(e.type) {
                case db_type::text: res += " TEXT"; break;
                case db_type::integer: res += " INTEGER"; break;
                case db_type::real: res += " REAL"; break;
                case db_type::blob: res += " BLOB"; break;
                }
                if(!e.nullable) res += " NOT NULL";
                if(e.unique_id == field_info::UNIQUE_ID_SINGLE_FIELD) res+= " UNIQUE";
            }
            if(!pk_fields.empty()) {
                res += ",\n\tPRIMARY KEY(";
                for(size_t i = 0; i < pk_fields.size(); i++) {
                    if(i != 0) res += ", ";
                    res += "`" + pk_fields[i] + "`";
                }
                res += ")";
            }
            for(auto& e : unique_fields) {
                res += ",\n\tUNIQUE(";
                for(size_t i = 0; i<e.second.size(); i++) {
                    res += "`" + e.second[i] + "`";
                    if(i != e.second.size()-1) res += ", ";
                }
                res += ")";
            }
            for(auto& e : info.fields) {
                if(e.fk_table.empty()) continue;
                res += ",\n\tFOREIGN KEY (`" + e.name + "`) REFERENCES ";
                res += "`" + e.fk_table + "` (`" + e.fk_field + "`) ";
                switch(e.fk_del_action) {
                case fk_action::no_action: res += "ON DELETE NO ACTION"; break;
                case fk_action::restrict: res += "ON DELETE RESTRICT"; break;
                case fk_action::set_null: res += "ON DELETE SET NULL"; break;
                case fk_action::set_default: res += "ON DELETE SET DEFAULT"; break;
                case fk_action::cascade: res += "ON DELETE CASCADE"; break;
                }
                res += " ";
                switch(e.fk_update_action) {
                case fk_action::no_action: res += "ON UPDATE NO ACTION"; break;
                case fk_action::restrict: res += "ON UPDATE RESTRICT"; break;
                case fk_action::set_null: res += "ON UPDATE SET NULL"; break;
                case fk_action::set_default: res += "ON UPDATE SET DEFAULT"; break;
                case fk_action::cascade: res += "ON UPDATE CASCADE"; break;
                }
            }
            res += "\n);";
            return res;
        }

        int64_t remove(database& db, const class_info& info, const std::string& where, std::vector<db_value> vals) {
            std::string query = "DELETE FROM ";
            if(!info.schema.empty()) query += "`" + info.schema + "`";
            query += "`" + info.table + "`";
            if(!where.empty()) query += " WHERE " + where;
            query += ";";

            auto nchanges = db.total_changes();
            sqlitepp::statement stmt(db, query);
            for(size_t i = 0; i<vals.size(); i++)
                bind_db_val(stmt, i+1, vals[i]);
            stmt.execute();
            return db.total_changes() - nchanges;
        }

        int64_t count(database& db, const class_info& info, const std::string& where, std::vector<db_value> vals) {
            std::string query = "SELECT COUNT(*) FROM ";
            if(!info.schema.empty()) query += "`" + info.schema + "`";
            query += "`" + info.table + "`";
            if(!where.empty()) query += " WHERE " + where;
            query += ";";

            sqlitepp::statement stmt(db, query);
            for(size_t i = 0; i<vals.size(); i++)
                bind_db_val(stmt, i+1, vals[i]);
            auto it = stmt.iterator();
            if(!it.next()) return -1; // TODO: This should never happen :(
            return it.column_int64(0);
        }

        std::vector<std::unique_ptr<entity>> select_multiple(database& db, const class_info& info, const std::string& where, std::vector<db_value> vals) {
            std::string query = "SELECT _rowid_ as _rowid_";
            for(auto& e: info.fields) {
                query +=", `" + e.name + "`";
            }
            query += " FROM ";
            if(!info.schema.empty()) query += "`" + info.schema + "`";
            query += "`" + info.table + "`";
            if(!where.empty()) {
                query += " WHERE " + where;
            }
            query += ";";

            sqlitepp::statement stmt(db, query);
            for(size_t i=0; i<vals.size(); i++)
                bind_db_val(stmt, i + 1, vals[i]);
            auto it = stmt.iterator();
            std::vector<std::unique_ptr<entity>> res;
            while(it.next()) {
                auto e = info.create(db);
                e->from_result(it);
                res.emplace_back(std::move(e));
            }
            return res;
        }

        std::unique_ptr<entity> select_one(database& db, const class_info& info, const std::string& where, std::vector<db_value> vals) {
            std::string query = "SELECT _rowid_ as _rowid_";
            for(auto& e: info.fields) {
                query +=", `" + e.name + "`";
            }
            query += " FROM ";
            if(!info.schema.empty()) query += "`" + info.schema + "`";
            query += "`" + info.table + "`";
            if(!where.empty()) {
                query += " WHERE " + where;
            }
            query += ";";

            sqlitepp::statement stmt(db, query);
            for(size_t i=0; i<vals.size(); i++)
                bind_db_val(stmt, i + 1, vals[i]);
            auto it = stmt.iterator();
            if(!it.next()) return nullptr;
            
            auto e = info.create(db);
            e->from_result(it);
            return e;
        }

        namespace {
        struct table_column
        {
            db_integer_type cid;
            db_text_type name;
            db_text_type type;
            db_integer_type non_null;
            db_text_type dflt_value;
            db_integer_type pk;
        };
        static std::vector<table_column> get_table_columns(database &db, const std::string &schema, const std::string &table)
        {
            std::string query = "PRAGMA ";
            if (!schema.empty())
                query += schema + ".";
            query += "table_info(" + table + ");";
            statement stmt{db, query};
            auto it = stmt.iterator();
            std::vector<table_column> res;
            while (it.next())
            {
                table_column col;
                it.get_all(col.cid, col.name, col.type, col.non_null, col.dflt_value, col.pk);
                res.push_back(col);
            }
            return res;
        }

        struct table_fk
        {
            db_integer_type id;
            db_integer_type seq;
            db_text_type table;
            db_text_type from;
            db_text_type to;
            db_text_type on_update;
            db_text_type on_delete;
            db_text_type match;
        };
        static std::vector<table_fk> get_table_fks(database &db, const std::string &schema, const std::string &table)
        {
            std::string query = "PRAGMA ";
            if (!schema.empty())
                query += schema + ".";
            query += "foreign_key_list(" + table + ");";
            statement stmt{db, query};
            auto it = stmt.iterator();
            std::vector<table_fk> res;
            while (it.next())
            {
                table_fk col;
                it.get_all(col.id, col.seq, col.table, col.from, col.to, col.on_update, col.on_delete, col.match);
                res.push_back(col);
            }
            return res;
        }

        struct table_uk
        {
            db_integer_type seq;
            db_text_type name;
            db_integer_type unique;
            db_text_type origin;
            db_integer_type partial;
            struct field
            {
                db_integer_type seqno;
                db_integer_type cid;
                db_text_type name;
            };
            std::vector<field> fields;

            field *get_field_by_name(const std::string &name)
            {
                for (auto &e : fields)
                {
                    if (e.name == name)
                        return &e;
                }
                return nullptr;
            }
        };
        static std::vector<table_uk> get_table_uks(database &db, const std::string &schema, const std::string &table)
        {
            std::string query = "PRAGMA ";
            if (!schema.empty())
                query += schema + ".";
            query += "index_list(" + table + ");";
            statement stmt{db, query};
            auto it = stmt.iterator();
            std::vector<table_uk> res;
            while (it.next())
            {
                table_uk col;
                it.get_all(col.seq, col.name, col.unique, col.origin, col.partial);
                if (col.origin != "u")
                    continue;
                res.push_back(col);
            }
            for (auto &e : res)
            {
                query = "PRAGMA ";
                if (!schema.empty())
                    query += schema + ".";
                query += "index_info(" + e.name + ");";
                statement stmt{db, query};
                auto it = stmt.iterator();
                while (it.next())
                {
                    table_uk::field col;
                    it.get_all(col.seqno, col.cid, col.name);
                    e.fields.push_back(col);
                }
            }
            return res;
        }

        static fk_action convert_fk_action(const std::string &str)
        {
            if (str == "RESTRICT")
                return fk_action::restrict;
            if (str == "SET NULL")
                return fk_action::set_null;
            if (str == "SET DEFAULT")
                return fk_action::set_default;
            if (str == "CASCADE")
                return fk_action::cascade;
            return fk_action::no_action;
        }

        static std::vector<std::set<std::string>> group_uks(const class_info &info)
        {
            std::vector<std::set<std::string>> res;
            std::set<std::string> default_uk;
            std::vector<std::set<std::string>> single_fields;
            for (auto &e : info.fields)
            {
                if (e.unique_id == 0)
                    continue;
                else if (e.unique_id == field_info::UNIQUE_ID_SINGLE_FIELD)
                {
                    single_fields.push_back({e.name});
                }
                else if (e.unique_id == field_info::UNIQUE_ID_DEFAULT)
                {
                    default_uk.insert(e.name);
                }
                else if (e.unique_id > 0)
                {
                    if (res.size() < e.unique_id)
                        res.resize(e.unique_id);
                    res.at(e.unique_id - 1).insert(e.name);
                }
            }
            if (!default_uk.empty())
                res.push_back(default_uk);
            for (auto &e : single_fields)
                res.push_back(std::move(e));
            return res;
        }

        static bool uk_match(const std::set<std::string> &uk1, const std::vector<table_uk::field> &uk2)
        {
            if (uk1.size() != uk2.size())
                return false;
            for (auto &f : uk2)
            {
                if (uk1.count(f.name) == 0)
                    return false;
            }
            return true;
        }
        }

        verify_result verify_table_schema(database &db, const class_info &info)
        {
            verify_result res;
            // Inconsistent
            if (info.is_temporary && info.schema != "temp")
            {
                res.errors.push_back("class is marked as temporary, but schema name is not 'temp'");
                return res;
            }
            // We dont have the table, no need to proceed
            if (!db.has_table(info.schema, info.table))
            {
                res.errors.push_back("missing table");
                return res;
            }

            auto cols = get_table_columns(db, info.schema, info.table);
            if (cols.size() == 0)
                return {{"table has no columns"}};

            // Verify that each column in the database is in the class
            for (auto &e : cols)
            {
                auto f = info.get_field_by_name(e.name);
                // Missing column
                if (f == nullptr)
                {
                    res.errors.push_back("table has extra field " + e.name);
                    continue;
                }
                switch (f->type)
                {
                case db_type::text:
                    if (e.type != "TEXT")
                        res.errors.push_back("field " + e.name + " has wrong type (expected TEXT, got " + e.type + ")");
                    break;
                case db_type::integer:
                    if (e.type != "INTEGER")
                        res.errors.push_back("field " + e.name + " has wrong type (expected INTEGER, got " + e.type + ")");
                    break;
                case db_type::real:
                    if (e.type != "REAL")
                        res.errors.push_back("field " + e.name + " has wrong type (expected REAL, got " + e.type + ")");
                    break;
                case db_type::blob:
                    if (e.type != "BLOB")
                        res.errors.push_back("field " + e.name + " has wrong type (expected BLOB, got " + e.type + ")");
                    break;
                default:
                    res.errors.push_back("field " + e.name + " has an unknown field type");
                }
                // Not null missmatch
                if (e.non_null != (f->nullable ? 0 : 1))
                {
                    if (f->nullable)
                        res.errors.push_back("field " + e.name + " should be nullable but isn't");
                    else
                        res.errors.push_back("field " + e.name + " shouldn't be nullable but is");
                }
                // Default value
                // TODO: if(f->default_value != e.dflt_value) return false;
                // Primary key missmatch
                if (e.pk != (f->primary_key ? 1 : 0))
                {
                    if (f->primary_key)
                        res.errors.push_back("field " + e.name + " should be a pk field but isn't");
                    else
                        res.errors.push_back("field " + e.name + " shouldn't be a pk field but is");
                }
            }
            // Verify that each field in the class is in the database
            for (auto &e : info.fields)
            {
                bool found = false;
                for (auto &c : cols)
                {
                    if (c.name == e.name)
                    {
                        found = true;
                        break;
                    }
                }
                if (!found)
                    res.errors.push_back("field " + e.name + " is missing in table");
            }

            auto fks = get_table_fks(db, info.schema, info.table);
            for (auto &key : fks)
            {
                auto f = info.get_field_by_name(key.from);
                if (f == nullptr)
                {
                    res.errors.push_back("field " + key.from + " is referenced in fk but not found in class");
                    continue;
                }
                if (f->fk_table != key.table)
                    res.errors.push_back("field " + key.from + " should reference table " + f->fk_table + " but references " + key.table);
                if (f->fk_field != key.to)
                    res.errors.push_back("field " + key.from + " should reference field " + f->fk_field + " but references " + key.to);
                if (f->fk_update_action != convert_fk_action(key.on_update))
                    res.errors.push_back("field " + key.from + " has a different on update action");
                if (f->fk_del_action != convert_fk_action(key.on_delete))
                    res.errors.push_back("field " + key.from + " has a different on delete action");
                if (key.match != "NONE")
                    res.errors.push_back("field " + key.from + " has a match clause");
            }
            for (auto &e : info.fields)
            {
                if (e.fk_table.empty())
                    continue;
                bool found = false;
                for (auto &k : fks)
                {
                    if (k.from == e.name)
                    {
                        found = true;
                        break;
                    }
                }
                if (!found)
                    res.errors.push_back("field " + e.name + " should reference " + e.fk_table + "." + e.fk_field + " but fk is missing");
            }

            auto uks = get_table_uks(db, info.schema, info.table);
            auto uks_expect = group_uks(info);
            for (auto &e : uks_expect)
            {
                bool found = false;
                for (auto &h : uks)
                {
                    if (uk_match(e, h.fields))
                    {
                        found = true;
                        break;
                    }
                }
                if (!found)
                {
                    std::string fields = "";
                    for (auto &x : e)
                    {
                        if (!fields.empty())
                            fields += ",";
                        fields += x;
                    }
                    res.errors.push_back("could not find unique key for fields " + fields + " in database");
                }
            }
            for (auto &e : uks)
            {
                bool found = false;
                for (auto &h : uks_expect)
                {
                    if (uk_match(h, e.fields))
                    {
                        found = true;
                        break;
                    }
                }
                if (!found)
                {
                    std::string fields = "";
                    for (auto &x : e.fields)
                    {
                        if (!fields.empty())
                            fields += ",";
                        fields += x.name;
                    }
                    res.errors.push_back("extra unique key for fields " + fields + " found in database");
                }
            }

            return res;
        }
    }
}