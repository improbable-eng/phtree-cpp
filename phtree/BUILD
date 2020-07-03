package(default_visibility = ["//visibility:private"])

cc_library(
    name = "phtree",
    srcs = [
    ],
    hdrs = [
        "phtree.h",
        "phtree_box_d.h",
        "phtree_d.h",
    ],
    linkstatic = True,
    visibility = [
        "//visibility:public",
    ],
    deps = [
        "//phtree/v16",
    ],
)

cc_test(
    name = "phtree_test",
    timeout = "long",
    srcs = [
        "phtree_test.cc",
    ],
    linkstatic = True,
    deps = [
        ":phtree",
        "//phtree/testing/gtest_main",
    ],
)

cc_test(
    name = "phtree_test_const_values",
    timeout = "long",
    srcs = [
        "phtree_test_const_values.cc",
    ],
    linkstatic = True,
    deps = [
        ":phtree",
        "//phtree/testing/gtest_main",
    ],
)

cc_test(
    name = "phtree_test_ptr_values",
    timeout = "long",
    srcs = [
        "phtree_test_ptr_values.cc",
    ],
    linkstatic = True,
    deps = [
        ":phtree",
        "//phtree/testing/gtest_main",
    ],
)

cc_test(
    name = "phtree_d_test",
    timeout = "long",
    srcs = [
        "phtree_d_test.cc",
    ],
    linkstatic = True,
    deps = [
        ":phtree",
        "//phtree/testing/gtest_main",
    ],
)

cc_test(
    name = "phtree_d_test_preprocessor",
    timeout = "long",
    srcs = [
        "phtree_d_test_preprocessor.cc",
    ],
    linkstatic = True,
    deps = [
        ":phtree",
        "//phtree/testing/gtest_main",
    ],
)

cc_test(
    name = "phtree_box_d_test",
    timeout = "long",
    srcs = [
        "phtree_box_d_test.cc",
    ],
    linkstatic = True,
    deps = [
        ":phtree",
        "//phtree/testing/gtest_main",
    ],
)
