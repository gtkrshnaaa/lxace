#include "test_runner.hpp"
#include "json/json_parser.hpp"
#include "json/json_writer.hpp"

using namespace lxace::json;

TEST_CASE(json_primitives) {
    auto null_val = JsonParser::parse("null");
    ASSERT_TRUE(null_val.has_value());
    ASSERT_TRUE(null_val->is_null());

    auto true_val = JsonParser::parse("true");
    ASSERT_TRUE(true_val.has_value());
    ASSERT_TRUE(true_val->is_bool());
    ASSERT_EQ(true_val->as_bool(), true);

    auto num_val = JsonParser::parse("42.5");
    ASSERT_TRUE(num_val.has_value());
    ASSERT_TRUE(num_val->is_number());
    ASSERT_EQ(num_val->as_double(), 42.5);

    auto str_val = JsonParser::parse("\"hello world\"");
    ASSERT_TRUE(str_val.has_value());
    ASSERT_TRUE(str_val->is_string());
    ASSERT_EQ(str_val->as_string(), "hello world");
}

TEST_CASE(json_object_and_array) {
    std::string json_str = R"({"name": "lxace", "version": 1, "tags": ["linux", "ai"]})";
    auto parsed = JsonParser::parse(json_str);
    ASSERT_TRUE(parsed.has_value());
    ASSERT_TRUE(parsed->is_object());

    ASSERT_EQ((*parsed)["name"].as_string(), "lxace");
    ASSERT_EQ((*parsed)["version"].as_int(), 1);

    ASSERT_TRUE((*parsed)["tags"].is_array());
    ASSERT_EQ((*parsed)["tags"].as_array().size(), 2);
    ASSERT_EQ((*parsed)["tags"][0].as_string(), "linux");
    ASSERT_EQ((*parsed)["tags"][1].as_string(), "ai");

    std::string serialized = parsed->serialize(false);
    auto reparsed = JsonParser::parse(serialized);
    ASSERT_TRUE(reparsed.has_value());
    ASSERT_EQ((*reparsed)["name"].as_string(), "lxace");
}
