#pragma once

#include <cmath>
#include <exception>
#include <iostream>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

namespace error {
    class ValueTypeError final : std::exception {
    public:
        explicit ValueTypeError(const char* msg) : msg_(msg){};
        const char* what() const override { return msg_; }

    private:
        const char* msg_;
    };

    class UnexpectedToken final : std::exception {
    public:
        UnexpectedToken() = default;
        const char* what() const override { return "Unexpected token"; };
    };

    class UnexpectedEscape final : std::exception {
    public:
        explicit UnexpectedEscape(const char escape) : msg_("Unexpected escape \"\\") {
            msg_ += escape;
            msg_ += '"';
        };
        const char* what() const override { return msg_.c_str(); }

    private:
        std::string msg_;
    };

    class IndexOutOfRange final : std::exception {
    public:
        IndexOutOfRange() = default;
        const char* what() const override { return "Array index out of range"; }
    };

    class KeyNotExists final : std::exception {
    public:
        explicit KeyNotExists(const char* key) : msg_(R"(Object key ")") {
            msg_ += key;
            msg_ += R"(" not exist)";
        };

        const char* what() const override { return msg_.c_str(); }

    private:
        std::string msg_;
    };
} // namespace error

namespace json {
    class Value;

    using Null   = nullptr_t;
    using Bool   = bool;
    using Int    = long long;
    using Float  = double;
    using String = std::string;
    using Array  = std::vector<Value>;
    using Object = std::unordered_map<String, Value>;

    Value deserialize(const char*);
    Value deserialize(const std::string&);
    Value deserialize(std::istream&);
    void  serialize(const Value&, std::ostream& out);

    template <typename T> inline constexpr bool is_null_v   = std::is_same<std::remove_cv_t<T>, Null>::value;
    template <typename T> inline constexpr bool is_bool_v   = std::is_same<std::remove_cv_t<T>, Bool>::value;
    template <typename T> inline constexpr bool is_string_v = std::is_same<std::remove_cv_t<T>, String>::value;
    template <typename T> inline constexpr bool is_array_v  = std::is_same<std::remove_cv_t<T>, Array>::value;
    template <typename T> inline constexpr bool is_object_v = std::is_same<std::remove_cv_t<T>, Object>::value;
    template <typename T, typename... R>
    inline constexpr bool is_any_of_v = std::disjunction<std::is_same<T, R>...>::value;
    template <typename T>
    inline constexpr bool is_int_v =
        is_any_of_v<std::remove_cv_t<T>, int8_t, int16_t, int32_t, int64_t, uint8_t, uint16_t, uint32_t, uint64_t>;
    template <typename T>
    inline constexpr bool is_float_v = is_any_of_v<std::remove_cv_t<T>, float, double, long double>;

    enum Type : char { JSON_NULL, JSON_BOOL, JSON_INT, JSON_FLOAT, JSON_STRING, JSON_ARRAY, JSON_OBJECT };

    class Value {
    public:
        friend std::ostream& operator<<(std::ostream&, const Value&);

        // null
        Value(){};
        Value(Null){};
        // bool
        Value(Bool val) : type_(JSON_BOOL) { data_.bval = val; };
        // int
        template <typename R, std::enable_if_t<is_int_v<R>, int> = 0> Value(R val) : type_(JSON_INT) {
            data_.ival = val;
        };
        // float
        template <typename R, std::enable_if_t<is_float_v<R>, int> = 0> Value(R val) : type_(JSON_FLOAT) {
            data_.fval = val;
        };
        // string
        Value(const char* str) : type_(JSON_STRING) { data_.sptr = new Wrapper<String>(str); };
        Value(const String& str) : type_(JSON_STRING) { data_.sptr = new Wrapper<String>(str); };
        Value(String&& str) : type_(JSON_STRING) { data_.sptr = new Wrapper<String>(std::move(str)); };
        // array
        Value(const Array& arr) : type_(JSON_ARRAY) { data_.aptr = new Wrapper<Array>(arr); };
        Value(Array&& arr) : type_(JSON_ARRAY) { data_.aptr = new Wrapper<Array>(std::move(arr)); };
        // object
        Value(const Object& obj) : type_(JSON_OBJECT) { data_.optr = new Wrapper<Object>(obj); };
        Value(Object&& obj) : type_(JSON_OBJECT) { data_.optr = new Wrapper<Object>(std::move(obj)); };
        // copy constructor
        Value(const Value& value) : type_(value.type_), data_(value.data_) { incref(); };
        // move constructor
        Value(Value&& value) noexcept : type_(value.type_), data_(value.data_) { value.type_ = JSON_NULL; };
        // type
        Type type() const { return type_; }

        bool is_null() const { return type_ == JSON_NULL; }
        bool is_bool() const { return type_ == JSON_BOOL; }
        bool is_int() const { return type_ == JSON_INT; }
        bool is_float() const { return type_ == JSON_FLOAT; }
        bool is_string() const { return type_ == JSON_STRING; }
        bool is_array() const { return type_ == JSON_ARRAY; }
        bool is_object() const { return type_ == JSON_OBJECT; }

        template <typename R, std::enable_if_t<is_null_v<R>, int> = 0> bool   is() const { return is_null(); };
        template <typename R, std::enable_if_t<is_bool_v<R>, int> = 0> bool   is() const { return is_bool(); };
        template <typename R, std::enable_if_t<is_int_v<R>, int> = 0> bool    is() const { return is_int(); };
        template <typename R, std::enable_if_t<is_float_v<R>, int> = 0> bool  is() const { return is_float(); };
        template <typename R, std::enable_if_t<is_string_v<R>, int> = 0> bool is() const { return is_string(); };
        template <typename R, std::enable_if_t<is_array_v<R>, int> = 0> bool  is() const { return is_array(); };
        template <typename R, std::enable_if_t<is_object_v<R>, int> = 0> bool is() const { return is_object(); };

        template <typename R> std::enable_if_t<is_bool_v<R>, R>          get() const;
        template <typename R> std::enable_if_t<is_int_v<R>, R>           get() const;
        template <typename R> std::enable_if_t<is_float_v<R>, R>         get() const;
        template <typename R> std::enable_if_t<is_string_v<R>, const R&> get() const;
        template <typename R> std::enable_if_t<is_array_v<R>, const R&>  get() const;
        template <typename R> std::enable_if_t<is_object_v<R>, const R&> get() const;

        // set int value
        template <typename R, std::enable_if_t<is_int_v<R>, int> = 0> Value& operator=(R);
        // set float value
        template <typename R, std::enable_if_t<is_float_v<R>, int> = 0> Value& operator=(R);
        // set null value
        Value& operator=(Null);
        // set bool value
        Value& operator=(Bool);
        // set string value
        Value& operator=(const char*);
        Value& operator=(const String&);
        Value& operator=(String&&);
        // set array value
        Value& operator=(const Array&);
        Value& operator=(Array&&);
        // set object value
        Value& operator=(const Object&);
        Value& operator=(Object&&);
        // copy assignment operator
        Value& operator=(const Value&);
        // move assignment operator
        Value& operator=(Value&&) noexcept;

        Value&       operator[](size_t);
        const Value& operator[](size_t) const;
        Value&       operator[](const String&);
        const Value& operator[](const String&) const;

        bool         has(size_t) const;
        bool         has(const String&) const;
        const Value& at(size_t) const;
        const Value& at(const String&) const;
        Value&       at(size_t);
        Value&       at(const String&);

        template <typename R> void insert(R&&);
        template <typename R> void insert(const String&, R&&);

        ~Value() {
            decref();
            type_ = JSON_NULL;
        };

    private:
        void incref() const;
        void decref() const;

        // template <typename T> T*   create();
        template <typename T> void destory() const;

        template <typename T> class Wrapper {
        public:
            T value{};
            Wrapper() = default;
            explicit Wrapper(const T& v) : value(v){};
            explicit Wrapper(T&& v) : value(std::move(v)){};
            int incref() { return ++uses_; };
            int decref() { return --uses_; };

        private:
            int uses_ = 1;
        };

        Type type_ = JSON_NULL;

        union Data {
            Int              ival;
            Bool             bval;
            Float            fval;
            Wrapper<Array>*  aptr;
            Wrapper<Object>* optr;
            Wrapper<String>* sptr;
        } data_{};
    };
} // namespace json

namespace json {

    // get bool value
    template <typename R> std::enable_if_t<is_bool_v<R>, R> Value::get() const {
        if (is_bool()) return data_.bval;
        throw error::ValueTypeError("This is not an bool value");
    };
    // get int value
    template <typename R> std::enable_if_t<is_int_v<R>, R> Value::get() const {
        if (is_int()) return static_cast<R>(data_.ival);
        throw error::ValueTypeError("This is not an int value");
    };
    // get float value
    template <typename R> std::enable_if_t<is_float_v<R>, R> Value::get() const {
        if (is_float()) return static_cast<R>(data_.fval);
        throw error::ValueTypeError("This is not a float value");
    };
    // get string reference
    template <typename R> std::enable_if_t<is_string_v<R>, const R&> Value::get() const {
        if (is_string()) return data_.sptr->value;
        throw error::ValueTypeError("This is not a string");
    };
    //  get array reference
    template <typename R> std::enable_if_t<is_array_v<R>, const R&> Value::get() const {
        if (is_array()) return data_.aptr->value;
        throw ValueTypeError("This is not an array");
    };
    // get object reference
    template <typename R> std::enable_if_t<is_object_v<R>, const R&> Value::get() const {
        if (is_object()) return data_.optr->value;
        throw ValueTypeError("This is not an object");
    };

    inline Value& Value::operator=(const Value& value) {
        decref();
        type_ = value.type_;
        data_ = value.data_;
        incref();
        return *this;
    };

    inline Value& Value::operator=(Value&& value) noexcept {
        decref();
        type_       = value.type_;
        data_       = value.data_;
        value.type_ = JSON_NULL;
        return *this;
    };

    inline Value& Value::operator=(Null) {
        decref();
        type_ = JSON_NULL;
        return *this;
    };

    inline Value& Value::operator=(Bool val) {
        if (is_bool()) {
            data_.bval = val;
        } else {
            decref();
            type_      = JSON_BOOL;
            data_.bval = val;
        }
        return *this;
    };

    template <typename R, std::enable_if_t<is_int_v<R>, int>> inline Value& Value::operator=(R val) {
        if (is_int()) {
            data_.ival = val;
        } else {
            decref();
            type_      = JSON_INT;
            data_.ival = val;
        }
        return *this;
    };

    template <typename R, std::enable_if_t<is_float_v<R>, int>> inline Value& Value::operator=(R val) {
        if (is_float()) {
            data_.fval = val;
        } else {
            decref();
            type_      = JSON_FLOAT;
            data_.fval = val;
        }
        return *this;
    };

    inline Value& Value::operator=(const char* str) {
        if (is_string()) {
            data_.sptr->value = str;
        } else {
            decref();
            type_      = JSON_STRING;
            data_.sptr = new Wrapper<String>(str);
        }
        return *this;
    };

    inline Value& Value::operator=(const String& str) {
        if (is_string()) {
            data_.sptr->value = str;
        } else {
            decref();
            type_      = JSON_STRING;
            data_.sptr = new Wrapper<String>(str);
        }
        return *this;
    };

    inline Value& Value::operator=(String&& str) {
        if (is_string()) {
            data_.sptr->value = std::move(str);
        } else {
            decref();
            type_      = JSON_STRING;
            data_.sptr = new Wrapper<String>(std::move(str));
        }
        return *this;
    };

    inline Value& Value::operator=(const Array& arr) {
        if (is_array()) {
            data_.aptr->value = arr;
        } else {
            decref();
            type_      = JSON_ARRAY;
            data_.aptr = new Wrapper<Array>(arr);
        }
        return *this;
    };

    inline Value& Value::operator=(Array&& arr) {
        if (is_array()) {
            data_.aptr->value = std::move(arr);
        } else {
            decref();
            type_      = JSON_ARRAY;
            data_.aptr = new Wrapper<Array>(std::move(arr));
        }
        return *this;
    };

    inline Value& Value::operator=(const Object& obj) {
        if (is_object()) {
            data_.optr->value = obj;
        } else {
            decref();
            type_      = JSON_OBJECT;
            data_.optr = new Wrapper<Object>(obj);
        }
        return *this;
    };

    inline Value& Value::operator=(Object&& obj) {
        if (is_object()) {
            data_.optr->value = std::move(obj);
        } else {
            decref();
            type_      = JSON_OBJECT;
            data_.optr = new Wrapper<Object>(std::move(obj));
        }
        return *this;
    };

    inline Value& Value::operator[](size_t idx) { return at(idx); };

    inline Value& Value::operator[](const String& key) { return at(key); };

    inline const Value& Value::operator[](size_t idx) const { return at(idx); };

    inline const Value& Value::operator[](const String& key) const { return at(key); };

    inline bool Value::has(size_t idx) const { return is_array() && idx < data_.aptr->value.size(); };

    inline bool Value::has(const String& key) const {
        return is_object() && data_.optr->value.find(key) != data_.optr->value.end();
    }

    const Value& Value::at(size_t idx) const {
        if (is_array()) {
            const auto& arr = data_.aptr->value;
            if (idx < arr.size()) return arr[idx];
            throw error::IndexOutOfRange();
        }
        throw error::ValueTypeError("This is not an array");
    };

    const Value& Value::at(const String& key) const {
        if (is_object()) {
            const auto& obj = data_.optr->value;
            if (const auto it = obj.find(key); it != obj.end()) return it->second;
            throw error::KeyNotExists(key.c_str());
        }
        throw error::ValueTypeError("This is not an object");
    };

    Value& Value::at(size_t idx) {
        if (is_array()) {
            auto& arr = data_.aptr->value;
            if (idx < arr.size()) return arr[idx];
            throw error::IndexOutOfRange();
        }
        throw error::ValueTypeError("This is not an array");
    };

    Value& Value::at(const String& key) {
        if (is_object()) return data_.optr->value[key];
        throw error::ValueTypeError("This is not an object");
    };

    template <typename R> void Value::insert(R&& val) {
        if (is_array()) {
            data_.aptr->value.emplace_back(std::forward<R>(val));
        } else if (is_null()) {
            type_      = JSON_ARRAY;
            data_.aptr = new Wrapper<Array>();
            data_.aptr->value.emplace_back(std::forward<R>(val));
        } else {
            throw error::ValueTypeError("This is not an array");
        }
    };

    template <typename R> void Value::insert(const String& key, R&& val) {
        if (is_object()) {
            data_.optr->value[key] = std::forward<R>(val);
        } else if (is_null()) {
            type_                  = JSON_OBJECT;
            data_.optr             = new Wrapper<Object>();
            data_.optr->value[key] = std::forward<R>(val);
        } else {
            throw error::ValueTypeError("This is not an object");
        }
    };

    template <> inline void Value::destory<Array>() const { delete data_.sptr; };
    template <> inline void Value::destory<Object>() const { delete data_.optr; };
    template <> inline void Value::destory<String>() const { delete data_.aptr; };

    inline void Value::incref() const {
        switch (type_) {
            case JSON_ARRAY: data_.aptr->incref(); return;
            case JSON_OBJECT: data_.optr->incref(); return;
            case JSON_STRING: data_.sptr->incref(); return;
            default: return;
        }
    };

    inline void Value::decref() const {
        switch (type_) {
            case JSON_ARRAY:
                if (data_.aptr->decref() == 0) destory<Array>();
                return;
            case JSON_OBJECT:
                if (data_.optr->decref() == 0) destory<Object>();
                return;
            case JSON_STRING:
                if (data_.sptr->decref() == 0) destory<String>();
                return;
            default: return;
        }
    };

    namespace detail {
        void serialize(Null, std::ostream&);
        void serialize(Int, std::ostream&);
        void serialize(Bool, std::ostream&);
        void serialize(Float, std::ostream&);
        void serialize(const String&, std::ostream&);
        void serialize(const Array&, std::ostream&);
        void serialize(const Object&, std::ostream&);
    }; // namespace detail

    inline std::ostream& operator<<(std::ostream& out, const Value& value) {
        switch (value.type_) {
            case JSON_NULL: detail::serialize(nullptr, out); break;
            case JSON_INT: detail::serialize(value.data_.ival, out); break;
            case JSON_BOOL: detail::serialize(value.data_.bval, out); break;
            case JSON_FLOAT: detail::serialize(value.data_.fval, out); break;
            case JSON_ARRAY: detail::serialize(value.data_.aptr->value, out); break;
            case JSON_OBJECT: detail::serialize(value.data_.optr->value, out); break;
            case JSON_STRING: detail::serialize(value.data_.sptr->value, out); break;
            default: break;
        }
        return out;
    };

    inline void detail::serialize(Null, std::ostream& out) { out << "null"; };

    inline void detail::serialize(Int val, std::ostream& out) { out << val; };

    inline void detail::serialize(Bool val, std::ostream& out) { out << (val ? "true" : "false"); };

    inline void detail::serialize(Float val, std::ostream& out) { out << val; };

    inline void detail::serialize(const String& str, std::ostream& out) {
        out << '"';
        for (const char* ch = str.c_str(); *ch; ++ch) {
            switch (*ch) {
                case '"': out << R"(\")"; break;
                case '/': out << R"(\/)"; break;
                case '\\': out << R"(\\)"; break;
                case '\b': out << R"(\b)"; break;
                case '\f': out << R"(\f)"; break;
                case '\n': out << R"(\n)"; break;
                case '\r': out << R"(\r)"; break;
                case '\t': out << R"(\t)"; break;
                default: out << *ch; break;
            }
        }
        out << '"';
    };

    inline void detail::serialize(const Array& arr, std::ostream& out) {
        if (arr.empty()) {
            out << "[]";
            return;
        }
        bool is_first = true;
        for (auto&& v : arr) {
            out << (is_first ? '[' : ',') << v;
            is_first = false;
        }
        out << ']';
    };

    inline void detail::serialize(const Object& obj, std::ostream& out) {
        if (obj.empty()) {
            out << "{}";
            return;
        }
        bool is_first = true;
        for (auto&& [k, v] : obj) {
            out << (is_first ? '{' : ',');
            serialize(k, out);
            out << ':' << v;
            is_first = false;
        }
        out << '}';
    };

    namespace detail {
        class StringReader {
        public:
            explicit StringReader(const char* str) : str_(str){};
            [[nodiscard]] char rdch() { return *str_ ? *++str_ : '\0'; };

        private:
            const char* str_;
        };

        class StreamReader {
        public:
            explicit StreamReader(std::istream& in) : in_(in){};
            [[nodiscard]] char rdch() {
                if (++idx_ < len_) return buf_[idx_];
                if (in_.good()) {
                    in_.read(buf_, 256);
                    len_ = in_.gcount();
                    idx_ = 0;
                } else {
                    len_ = 0;
                    idx_ = 0;
                }
                return len_ ? buf_[0] : '\0';
            }

        private:
            std::istream& in_;
            size_t        idx_ = 0;
            size_t        len_ = 0;
            char          buf_[256]{0};
        };

        template <typename R> class Parser {
        public:
            template <typename T> explicit Parser(T&& r) : r_(std::forward<T>(r)) { skip(); };
            void parse(Value&);

        private:
            R    r_;
            char ch_;

            void skip();
            void parse_null(Value&);
            void parse_true(Value&);
            void parse_false(Value&);
            void parse_digit(Value&);
            void parse_string(Value&);
            void parse_array(Value&);
            void parse_object(Value&);

            void parse_dot(Float&);
            void parse_exp(Float&);
            void parse_string(String&);
        };
    } // namespace detail

    inline std::istream& operator>>(std::istream& in, Value& value) {
        using namespace detail;
        Parser<StreamReader> parser(in);
        parser.parse(value);
        return in;
    };

    inline Value deserialize(const char* str) {
        using namespace detail;
        Value                value;
        Parser<StringReader> parser(str);
        parser.parse(value);
        return std::move(value);
    };

    inline Value deserialize(const String& str) {
        using namespace detail;
        Value                value;
        Parser<StringReader> parser(str.c_str());
        parser.parse(value);
        return std::move(value);
    };

    inline Value deserialize(std::istream& in) {
        using namespace detail;
        Value                value;
        Parser<StreamReader> parser(in);
        parser.parse(value);
        return std::move(value);
    };

    template <typename R> inline void detail::Parser<R>::parse(Value& value) {
        for (;;) {
            switch (ch_) {
                case ' ':
                case '\t':
                case '\r':
                case '\v':
                case '\f':
                case '\n': skip(); break;
                case '-':
                case '0':
                case '1':
                case '2':
                case '3':
                case '4':
                case '5':
                case '6':
                case '7':
                case '8':
                case '9': parse_digit(value); return;
                case 'n': parse_null(value); return;
                case 't': parse_true(value); return;
                case 'f': parse_false(value); return;
                case '"': parse_string(value); return;
                case '[': parse_array(value); return;
                case '{': parse_object(value); return;
                default: throw error::UnexpectedToken();
            }
        }
    };

    template <typename R> void detail::Parser<R>::skip() { ch_ = r_.rdch(); };

    // "null"
    template <typename R> inline void detail::Parser<R>::parse_null(Value& value) {
        skip();
        const char* nameof_null = "null";
        for (size_t i = 1; i < 4; ++i, skip()) {
            if (ch_ != nameof_null[i]) throw error::UnexpectedToken();
        }
        value = nullptr;
    };

    // "true"
    template <typename R> inline void detail::Parser<R>::parse_true(Value& out) {
        skip();
        const char* nameof_true = "true";
        for (size_t i = 1; i < 4; ++i, skip()) {
            if (ch_ != nameof_true[i]) throw error::UnexpectedToken();
        }
        out = true;
    };

    // "false"
    template <typename R> inline void detail::Parser<R>::parse_false(Value& out) {
        skip();
        const char* nameof_false = "false";
        for (size_t i = 1; i < 5; ++i, skip()) {
            if (ch_ != nameof_false[i]) throw error::UnexpectedToken();
        }
        out = false;
    };

    // digit | -
    template <typename R> inline void detail::Parser<R>::parse_digit(Value& value) {
        bool is_positive = true;
        if (ch_ == '-') {
            skip();
            is_positive = false;
            if (!std::isdigit(ch_)) throw error::UnexpectedToken();
        }
        Int ival = 0;
        do {
            ival = ival * 10 + ch_ - '0';
            skip();
        } while (std::isdigit(ch_));
        if (ch_ == '.') {
            Float fval = ival;
            parse_dot(fval);
            value = is_positive ? fval : -fval;
        } else if (ch_ == 'e' || ch_ == 'E') {
            Float fval = ival;
            parse_exp(fval);
            value = is_positive ? fval : -fval;
        } else {
            value = is_positive ? ival : -ival;
        }
    };

    template <typename R> inline void detail::Parser<R>::parse_dot(Float& fval) {
        skip();
        if (!std::isdigit(ch_)) throw error::UnexpectedToken();
        for (Float e = -1;; --e) {
            fval += static_cast<Float>(ch_ - '0') * std::pow(10.0, e);
            skip();
            if (!std::isdigit(ch_)) break;
        }
        if (ch_ == 'e' || ch_ == 'E') parse_exp(fval);
    };

    template <typename R> inline void detail::Parser<R>::parse_exp(Float& fval) {
        skip();
        bool is_positive = true;
        if (ch_ == '-') {
            is_positive = false;
            skip();
        } else if (ch_ == '+') {
            skip();
        }
        if (!std::isdigit(ch_)) throw error::UnexpectedToken();
        if (fval == 0) {
            do {
                skip();
            } while (std::isdigit(ch_));
        } else {
            Int exp = 0;
            do {
                exp += exp * 10 + ch_ - '0';
                skip();
            } while (std::isdigit(ch_));
            fval *= std::pow(10.0, is_positive ? exp : -exp);
        }
    };

    template <typename R> inline void detail::Parser<R>::parse_string(Value& out) {
        String str = "";
        parse_string(str);
        out = std::move(str);
    };

    template <typename R> inline void detail::Parser<R>::parse_string(String& out) {
        skip();
        std::stringstream ss;
        for (;;) {
            switch (ch_) {
                case '\0': throw error::UnexpectedToken();
                case '"': {
                    skip();
                    out = ss.str();
                    return;
                }
                case '\\': {
                    skip();
                    switch (ch_) {
                        case '/': ss << '/'; break;
                        case '"': ss << '"'; break;
                        case '\\': ss << '\\'; break;
                        case 'b': ss << '\b'; break;
                        case 'f': ss << '\f'; break;
                        case 'v': ss << '\v'; break;
                        case 'n': ss << '\n'; break;
                        case 'r': ss << '\r'; break;
                        case 't': ss << '\t'; break;
                        case 'u': {
                            uint32_t hex = 0;
                            for (size_t i = 0; i < 4; ++i) {
                                skip();
                                hex <<= 4;
                                if ('0' <= ch_ && ch_ <= '9')
                                    hex += ch_ - '0';
                                else if ('a' <= ch_ && ch_ <= 'f')
                                    hex += ch_ - 'a';
                                else if ('A' <= ch_ && ch_ <= 'F')
                                    hex += ch_ - 'A';
                                else
                                    throw error::UnexpectedToken();
                            }
                            ss << static_cast<char>(0xE0 | (hex >> 12));
                            ss << static_cast<char>(0x80 | (hex >> 6));
                            ss << static_cast<char>(0x80 | (hex & 0x3F));
                            break;
                        }
                        default: throw error::UnexpectedEscape(ch_);
                    }
                    skip();
                    break;
                }
                default: {
                    ss << ch_;
                    skip();
                    break;
                }
            }
        }
        throw error::UnexpectedToken();
    };

    template <typename R> inline void detail::Parser<R>::parse_array(Value& out) {
        skip();
        Array arr;
        while (ch_) {
            arr.emplace_back();
            parse(arr.back());
            while (std::isspace(ch_)) skip();
            if (ch_ == ']') {
                skip();
                out = std::move(arr);
                return;
            }
            if (ch_ != ',') break;
            skip();
        }
        throw error::UnexpectedToken();
    };

    template <typename R> inline void detail::Parser<R>::parse_object(Value& out) {
        skip();
        Object obj;
        String key;
        while (ch_) {
            while (std::isspace(ch_)) skip();
            if (ch_ != '"') break;
            parse_string(key);
            while (std::isspace(ch_)) skip();
            if (ch_ != ':') break;
            skip();
            parse(obj[key]);
            while (std::isspace(ch_)) skip();
            if (ch_ == '}') {
                skip();
                out = std::move(obj);
                return;
            }
            if (ch_ != ',') break;
            skip();
        }
        throw error::UnexpectedToken();
    };
} // namespace json