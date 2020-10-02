#include "json.h"
#include <cassert>
#include <iostream>
#include <string>

using namespace std;
using namespace json;

void test_value_creation() {
    Value null_v = nullptr;
    assert(null_v.is<nullptr_t>());
    Value bool_v = true;
    assert(bool_v.get<bool>());
    Value int_v = 1;
    assert(int_v.get<int>() == 1);
    Value float_v = 0.1;
    assert(float_v.get<double>() == 0.1);
    Value string_v = "123";
    assert(string_v.get<string>() == "123");
};

void test_deserialze() {
    const char* str   = R"({
        "null": null,"true": true,"false": false,"int": 12345,"float": 1.2345,"string": "12345",
        "array": [null,true,false,12345,0.12345,1.234e5,"12345"],
        "object": {"one": 1,"two": 2,"three": 3}
    })";
    auto        value = deserialize(str);
    assert(value["array"][0].is_null());
    assert(value["array"][1].get<bool>() == true);
    assert(value["array"][2].get<bool>() == false);
    assert(value["array"][3].get<int>() == 12345);
    assert(value["array"][4].get<double>() - 0.12345 <= 1e-10);
    assert(value["array"][5].get<double>() - 1.234e5 <= 1e-5);
    assert(value["array"][6].get<string>() == "12345");
    assert(value["object"]["one"].get<int>() == 1);
    assert(value["object"]["two"].get<int>() == 2);
    assert(value["object"]["three"].get<int>() == 3);
};

void test_array() {
    Value v;

    v.insert("2");
    cout << v;
}

int main(int argc, char const* argv[]) {
    // test_value_creation();

    // test_deserialze();
    test_array();
    _CrtDumpMemoryLeaks();
};
