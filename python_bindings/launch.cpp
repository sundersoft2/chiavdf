#include <pybind11/pybind11.h>
#include "../vdf_client.h"

namespace py = pybind11;

PYBIND11_MODULE(chiatimelord, m) {
    m.doc() = "Chia time lord";

    m.def("launch_time_lord", [] (const char *host, const char *port, int local_process_number) {
        return launch_client(host, port, local_process_number);
    });
}
