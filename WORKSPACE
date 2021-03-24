# Bazel bootstrapping

load("//tools/build_rules:http.bzl", "http_archive", "http_file")

http_archive(
    name = "bazel_skylib",
    sha256 = "1dde365491125a3db70731e25658dfdd3bc5dbdfd11b840b3e987ecf043c7ca0",
    url = "https://github.com/bazelbuild/bazel-skylib/releases/download/0.9.0/bazel_skylib-0.9.0.tar.gz",
)

load("@bazel_skylib//lib:versions.bzl", "versions")

versions.check(
    minimum_bazel_version = "3.4.1",
    maximum_bazel_version = "3.4.1",
)

# NOTE: We make third_party/ its own bazel workspace because it allows to run `bazel build ...` without
# having all targets defined in third-party BUILD files in that directory buildable.
local_repository(
    name = "third_party",
    path = "third_party",
)

# External PH-Tree dependencies

http_archive(
    name = "spdlog",
    build_file = "@third_party//spdlog:BUILD",
    sha256 = "b38e0bbef7faac2b82fed550a0c19b0d4e7f6737d5321d4fd8f216b80f8aee8a",
    strip_prefix = "spdlog-1.5.0",
    url = "https://github.com/gabime/spdlog/archive/v1.5.0.tar.gz",
)

http_archive(
    name = "gbenchmark",
    sha256 = "3c6a165b6ecc948967a1ead710d4a181d7b0fbcaa183ef7ea84604994966221a",
    strip_prefix = "benchmark-1.5.0",
    url = "https://github.com/google/benchmark/archive/v1.5.0.tar.gz",
)

http_archive(
    name = "gtest",
    build_file = "@third_party//gtest:BUILD",
    sha256 = "9dc9157a9a1551ec7a7e43daea9a694a0bb5fb8bec81235d8a1e6ef64c716dcb",
    strip_prefix = "googletest-release-1.10.0",
    url = "https://github.com/google/googletest/archive/release-1.10.0.tar.gz",
)

# Development environment tooling

BUILDIFIER_VERSION = "0.29.0"

http_file(
    name = "buildifier_linux",
    executable = True,
    sha256 = "4c985c883eafdde9c0e8cf3c8595b8bfdf32e77571c369bf8ddae83b042028d6",
    urls = ["https://github.com/bazelbuild/buildtools/releases/download/{version}/buildifier".format(version = BUILDIFIER_VERSION)],
)

http_file(
    name = "buildifier_macos",
    executable = True,
    sha256 = "9b108decaa9a624fbac65285e529994088c5d15fecc1a30866afc03a48619245",
    urls = ["https://github.com/bazelbuild/buildtools/releases/download/{version}/buildifier.mac".format(version = BUILDIFIER_VERSION)],
)

http_file(
    name = "buildifier_windows",
    executable = True,
    sha256 = "dc5d6ed5e3e0dbe9955f7606939c627af5a2be7f9bdd8814e77a22109164394f",
    urls = ["https://github.com/bazelbuild/buildtools/releases/download/{version}/buildifier.exe".format(version = BUILDIFIER_VERSION)],
)

http_archive(
    name = "bazel_compilation_database",
    sha256 = "bb1b812396e2ee36a50a13b03ae6833173ce643e8a4bd50731067d0b4e5c6e86",
    strip_prefix = "bazel-compilation-database-0.3.5",
    url = "https://github.com/grailbio/bazel-compilation-database/archive/0.3.5.tar.gz",
)
