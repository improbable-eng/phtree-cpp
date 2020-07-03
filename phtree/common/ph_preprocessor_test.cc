/*
 * Copyright 2020 Improbable Worlds Limited
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "ph_preprocessor.h"
#include <gtest/gtest.h>
#include <random>

using namespace improbable::phtree;

TEST(PhTreePreprocessorTest, IEEE_SmokeTest) {
    double d1 = -55;
    double d2 = 7;

    scalar_t l1 = Preprocessors::ToSortableLong(d1);
    scalar_t l2 = Preprocessors::ToSortableLong(d2);

    ASSERT_GT(l2, l1);

    ASSERT_EQ(d1, Preprocessors::ToDouble(l1));
    ASSERT_EQ(d2, Preprocessors::ToDouble(l2));
}
