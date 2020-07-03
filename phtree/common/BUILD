package(default_visibility = ["//visibility:private"])

cc_library(
    name = "common",
    hdrs = [
        "ph_base_types.h",
        "ph_bits.h",
        "ph_common.h",
        "ph_distance.h",
        "ph_filter.h",
        "ph_flat_array_map.h",
        "ph_flat_sparse_map.h",
        "ph_preprocessor.h",
        "ph_tree_debug_helper.h",
        "ph_tree_stats.h",
    ],
    visibility = [
        "//visibility:public",
    ],
    deps = [
    ],
)

cc_test(
    name = "base_types_test",
    timeout = "long",
    srcs = [
        "ph_base_types_test.cc",
    ],
    linkstatic = True,
    deps = [
        ":common",
        "//phtree/testing/gtest_main",
    ],
)

cc_test(
    name = "bits_test",
    timeout = "long",
    srcs = [
        "ph_bits_test.cc",
    ],
    linkstatic = True,
    deps = [
        ":common",
        "//phtree/testing/gtest_main",
    ],
)

cc_test(
    name = "common_test",
    timeout = "long",
    srcs = [
        "ph_common_test.cc",
    ],
    linkstatic = True,
    deps = [
        ":common",
        "//phtree/testing/gtest_main",
    ],
)

cc_test(
    name = "distance_test",
    timeout = "long",
    srcs = [
        "ph_distance_test.cc",
    ],
    linkstatic = True,
    deps = [
        ":common",
        "//phtree/testing/gtest_main",
    ],
)

cc_test(
    name = "filter_test",
    timeout = "long",
    srcs = [
        "ph_filter_test.cc",
    ],
    linkstatic = True,
    deps = [
        ":common",
        "//phtree/testing/gtest_main",
    ],
)

cc_test(
    name = "flat_array_map_test",
    timeout = "long",
    srcs = [
        "ph_flat_array_map_test.cc",
    ],
    linkstatic = True,
    deps = [
        ":common",
        "//phtree/testing/gtest_main",
    ],
)

cc_test(
    name = "flat_sparse_map_test",
    timeout = "long",
    srcs = [
        "ph_flat_sparse_map_test.cc",
    ],
    linkstatic = True,
    deps = [
        ":common",
        "//phtree/testing/gtest_main",
    ],
)

cc_test(
    name = "preprocessor_test",
    timeout = "long",
    srcs = [
        "ph_preprocessor_test.cc",
    ],
    linkstatic = True,
    deps = [
        ":common",
        "//phtree/testing/gtest_main",
    ],
)
