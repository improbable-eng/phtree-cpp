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

#include <gmock/gmock.h>
#include <spdlog/spdlog.h>
#include <spdlog/sinks/ansicolor_sink.h>
#include <memory>

int main(int argc, char** argv) {
    auto console_sink = std::make_shared<spdlog::sinks::ansicolor_stdout_sink_mt>();
    spdlog::set_default_logger(
        std::make_shared<spdlog::logger>("", spdlog::sinks_init_list({console_sink})));
    spdlog::set_level(spdlog::level::trace);

    testing::InitGoogleMock(&argc, argv);
    return RUN_ALL_TESTS();
}
