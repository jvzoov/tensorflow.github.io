load("//tensorflow:strict.default.bzl", "py_strict_library", "py_strict_test")

package(
    # copybara:uncomment default_applicable_licenses = ["//tensorflow:license"],
    default_visibility = [
        "//tensorflow:internal",
        "//tensorflow_estimator:__subpackages__",
        "//tensorflow_federated:__subpackages__",
        "//third_party/py/tensorflow:__subpackages__",
    ],
    licenses = ["notice"],
)

py_strict_library(
    name = "authoring",
    srcs = [
        "authoring.py",
    ],
    srcs_version = "PY3",
    visibility = ["//visibility:public"],
    deps = [
        "//tensorflow/lite/python:convert",
        "//tensorflow/lite/python:lite",
        "//tensorflow/lite/python/metrics:converter_error_data_proto_py",
        "//tensorflow/python/util:tf_export",
    ],
)

py_strict_test(
    name = "authoring_test",
    srcs = ["authoring_test.py"],
    python_version = "PY3",
    srcs_version = "PY2AND3",
    deps = [
        ":authoring",
        "//tensorflow:tensorflow_py",
    ],
)
