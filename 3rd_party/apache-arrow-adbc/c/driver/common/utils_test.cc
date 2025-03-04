// Licensed to the Apache Software Foundation (ASF) under one
// or more contributor license agreements.  See the NOTICE file
// distributed with this work for additional information
// regarding copyright ownership.  The ASF licenses this file
// to you under the Apache License, Version 2.0 (the
// "License"); you may not use this file except in compliance
// with the License.  You may obtain a copy of the License at
//
//   http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing,
// software distributed under the License is distributed on an
// "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
// KIND, either express or implied.  See the License for the
// specific language governing permissions and limitations
// under the License.

#include <cstring>
#include <string>
#include <string_view>
#include <vector>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "utils.h"

TEST(TestStringBuilder, TestBasic) {
  struct StringBuilder str;
  int ret;
  ret = StringBuilderInit(&str, /*initial_size=*/64);
  EXPECT_EQ(ret, 0);
  EXPECT_EQ(str.capacity, 64);

  ret = StringBuilderAppend(&str, "%s", "BASIC TEST");
  EXPECT_EQ(ret, 0);
  EXPECT_EQ(str.size, 10);
  EXPECT_STREQ(str.buffer, "BASIC TEST");

  StringBuilderReset(&str);
}

TEST(TestStringBuilder, TestBoundary) {
  struct StringBuilder str;
  int ret;
  ret = StringBuilderInit(&str, /*initial_size=*/10);
  EXPECT_EQ(ret, 0);
  EXPECT_EQ(str.capacity, 10);

  ret = StringBuilderAppend(&str, "%s", "BASIC TEST");
  EXPECT_EQ(ret, 0);
  // should resize to include \0
  EXPECT_EQ(str.capacity, 11);
  EXPECT_EQ(str.size, 10);
  EXPECT_STREQ(str.buffer, "BASIC TEST");

  StringBuilderReset(&str);
}

TEST(TestStringBuilder, TestMultipleAppends) {
  struct StringBuilder str;
  int ret;
  ret = StringBuilderInit(&str, /*initial_size=*/2);
  EXPECT_EQ(ret, 0);
  EXPECT_EQ(str.capacity, 2);

  ret = StringBuilderAppend(&str, "%s", "BASIC");
  EXPECT_EQ(ret, 0);
  EXPECT_EQ(str.capacity, 6);
  EXPECT_EQ(str.size, 5);
  EXPECT_STREQ(str.buffer, "BASIC");

  ret = StringBuilderAppend(&str, "%s", " TEST");
  EXPECT_EQ(ret, 0);
  EXPECT_EQ(str.capacity, 11);
  EXPECT_EQ(str.size, 10);
  EXPECT_STREQ(str.buffer, "BASIC TEST");

  StringBuilderReset(&str);
}

TEST(ErrorDetails, Adbc100) {
  struct AdbcError error;
  std::memset(&error, 0, ADBC_ERROR_1_1_0_SIZE);

  SetError(&error, "My message");

  ASSERT_EQ(nullptr, error.private_data);
  ASSERT_EQ(nullptr, error.private_driver);

  {
    std::string detail = "detail";
    AppendErrorDetail(&error, "key", reinterpret_cast<const uint8_t*>(detail.data()),
                      detail.size());
  }

  ASSERT_EQ(0, CommonErrorGetDetailCount(&error));
  struct AdbcErrorDetail detail = CommonErrorGetDetail(&error, 0);
  ASSERT_EQ(nullptr, detail.key);
  ASSERT_EQ(nullptr, detail.value);
  ASSERT_EQ(0, detail.value_length);

  error.release(&error);
}

TEST(ErrorDetails, Adbc110) {
  struct AdbcError error = ADBC_ERROR_INIT;
  SetError(&error, "My message");

  ASSERT_NE(nullptr, error.private_data);
  ASSERT_EQ(nullptr, error.private_driver);

  {
    std::string detail = "detail";
    AppendErrorDetail(&error, "key", reinterpret_cast<const uint8_t*>(detail.data()),
                      detail.size());
  }

  ASSERT_EQ(1, CommonErrorGetDetailCount(&error));
  struct AdbcErrorDetail detail = CommonErrorGetDetail(&error, 0);
  ASSERT_STREQ("key", detail.key);
  ASSERT_EQ("detail", std::string_view(reinterpret_cast<const char*>(detail.value),
                                       detail.value_length));

  detail = CommonErrorGetDetail(&error, -1);
  ASSERT_EQ(nullptr, detail.key);
  ASSERT_EQ(nullptr, detail.value);
  ASSERT_EQ(0, detail.value_length);

  detail = CommonErrorGetDetail(&error, 2);
  ASSERT_EQ(nullptr, detail.key);
  ASSERT_EQ(nullptr, detail.value);
  ASSERT_EQ(0, detail.value_length);

  error.release(&error);
  ASSERT_EQ(nullptr, error.private_data);
  ASSERT_EQ(nullptr, error.private_driver);
}

TEST(ErrorDetails, RoundTripValues) {
  struct AdbcError error = ADBC_ERROR_INIT;
  SetError(&error, "My message");

  struct Detail {
    std::string key;
    std::vector<uint8_t> value;
  };

  std::vector<Detail> details = {
      {"x-key-1", {0, 1, 2, 3}}, {"x-key-2", {1, 1}}, {"x-key-3", {128, 129, 200, 0, 1}},
      {"x-key-4", {97, 98, 99}}, {"x-key-5", {42}},
  };

  for (const auto& detail : details) {
    AppendErrorDetail(&error, detail.key.c_str(), detail.value.data(),
                      detail.value.size());
  }

  ASSERT_EQ(details.size(), CommonErrorGetDetailCount(&error));
  for (int i = 0; i < static_cast<int>(details.size()); i++) {
    struct AdbcErrorDetail detail = CommonErrorGetDetail(&error, i);
    ASSERT_EQ(details[i].key, detail.key);
    ASSERT_EQ(details[i].value.size(), detail.value_length);
    ASSERT_THAT(std::vector<uint8_t>(detail.value, detail.value + detail.value_length),
                ::testing::ElementsAreArray(details[i].value));
  }

  error.release(&error);
}
