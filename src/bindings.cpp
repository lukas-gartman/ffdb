#include <pybind11/pybind11.h>
#include "FFDB.h"

namespace py = pybind11;

PYBIND11_MODULE(ffdbcore, m) {
    m.doc() = "Python bindings for FFDB";

    py::class_<FFDB>(m, "FFDB")
        .def(py::init<std::string, int, int, size_t>(),
             py::arg("filepath"),
             py::arg("flags"),
             py::arg("mode"),
             py::arg("init_size"))
        .def("hello", &FFDB::hello_ffdb);
}
