#include "sqlitepp/orm.h"

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
    }
}