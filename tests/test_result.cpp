// Ferriot - Result Type Unit Tests

#include <gtest/gtest.h>

#include "lwm2m/result.hpp"

namespace lwm2m {
namespace {

TEST(ResultTest, OkConstruction) {
    Result<int> result = Ok(42);
    EXPECT_TRUE(result.is_ok());
    EXPECT_FALSE(result.is_err());
    EXPECT_EQ(result.value(), 42);
}

TEST(ResultTest, ErrConstruction) {
    Result<int> result = Err<int>(ErrorCode::NotFound, "Item not found");
    EXPECT_FALSE(result.is_ok());
    EXPECT_TRUE(result.is_err());
    EXPECT_EQ(result.error().code(), ErrorCode::NotFound);
    EXPECT_EQ(result.error().message(), "Item not found");
}

TEST(ResultTest, ImplicitOkConstruction) {
    Result<int> result = 42;
    EXPECT_TRUE(result.is_ok());
    EXPECT_EQ(result.value(), 42);
}

TEST(ResultTest, ImplicitErrConstruction) {
    Result<int> result = Error(ErrorCode::BadRequest, "Bad");
    EXPECT_TRUE(result.is_err());
    EXPECT_EQ(result.error().code(), ErrorCode::BadRequest);
}

TEST(ResultTest, ValueOr) {
    Result<int> ok_result = Ok(42);
    Result<int> err_result = Err<int>(ErrorCode::NotFound);

    EXPECT_EQ(ok_result.value_or(0), 42);
    EXPECT_EQ(err_result.value_or(0), 0);
}

TEST(ResultTest, Map) {
    Result<int> result = Ok(21);
    auto mapped = result.map([](int x) { return x * 2; });

    EXPECT_TRUE(mapped.is_ok());
    EXPECT_EQ(mapped.value(), 42);
}

TEST(ResultTest, MapError) {
    Result<int> result = Err<int>(ErrorCode::NotFound);
    auto mapped = result.map([](int x) { return x * 2; });

    EXPECT_TRUE(mapped.is_err());
    EXPECT_EQ(mapped.error().code(), ErrorCode::NotFound);
}

TEST(ResultTest, AndThen) {
    auto half_if_even = [](int x) -> Result<int> {
        if (x % 2 == 0) {
            return Ok(x / 2);
        }
        return Err<int>(ErrorCode::BadRequest, "Odd number");
    };

    Result<int> even_result = Ok(42);
    auto result1 = even_result.and_then(half_if_even);
    EXPECT_TRUE(result1.is_ok());
    EXPECT_EQ(result1.value(), 21);

    Result<int> odd_result = Ok(43);
    auto result2 = odd_result.and_then(half_if_even);
    EXPECT_TRUE(result2.is_err());
}

TEST(ResultTest, BoolConversion) {
    Result<int> ok_result = Ok(42);
    Result<int> err_result = Err<int>(ErrorCode::NotFound);

    EXPECT_TRUE(static_cast<bool>(ok_result));
    EXPECT_FALSE(static_cast<bool>(err_result));

    if (ok_result) {
        SUCCEED();
    } else {
        FAIL() << "Expected ok_result to be truthy";
    }
}

TEST(ResultVoidTest, OkConstruction) {
    Result<void> result = Ok();
    EXPECT_TRUE(result.is_ok());
    EXPECT_FALSE(result.is_err());
}

TEST(ResultVoidTest, ErrConstruction) {
    Result<void> result = Err(ErrorCode::InternalServerError, "Oops");
    EXPECT_FALSE(result.is_ok());
    EXPECT_TRUE(result.is_err());
    EXPECT_EQ(result.error().code(), ErrorCode::InternalServerError);
}

TEST(ResultVoidTest, BoolConversion) {
    Result<void> ok_result = Ok();
    Result<void> err_result = Err(ErrorCode::NotFound);

    EXPECT_TRUE(static_cast<bool>(ok_result));
    EXPECT_FALSE(static_cast<bool>(err_result));
}

TEST(ErrorTest, Categories) {
    Error client_error(ErrorCode::NotFound);
    EXPECT_TRUE(client_error.is_client_error());
    EXPECT_FALSE(client_error.is_server_error());
    EXPECT_FALSE(client_error.is_transport_error());

    Error server_error(ErrorCode::InternalServerError);
    EXPECT_FALSE(server_error.is_client_error());
    EXPECT_TRUE(server_error.is_server_error());
    EXPECT_FALSE(server_error.is_transport_error());

    Error transport_error(ErrorCode::Timeout);
    EXPECT_FALSE(transport_error.is_client_error());
    EXPECT_FALSE(transport_error.is_server_error());
    EXPECT_TRUE(transport_error.is_transport_error());
}

TEST(ErrorCodeTest, Values) {
    EXPECT_EQ(static_cast<int>(ErrorCode::BadRequest), 400);
    EXPECT_EQ(static_cast<int>(ErrorCode::NotFound), 404);
    EXPECT_EQ(static_cast<int>(ErrorCode::InternalServerError), 500);
    EXPECT_EQ(static_cast<int>(ErrorCode::Timeout), 600);
}

}  // namespace
}  // namespace lwm2m
