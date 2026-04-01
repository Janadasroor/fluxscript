#include <pybind11/embed.h>
#include <pybind11/stl.h>
#include <QPointF>
#include <QString>
#include <QJsonObject>
#include <QJsonDocument>
#include <QJsonArray>
#include <QVariant>
#include <QMetaType>

namespace py = pybind11;

// Helper to convert QString to std::string for pybind11
namespace pybind11 { namespace detail {
    template <> struct type_caster<QString> {
    public:
        PYBIND11_TYPE_CASTER(QString, _("str"));
        bool load(handle src, bool) {
            if (!src) return false;
            if (PyUnicode_Check(src.ptr())) {
                value = QString::fromUtf8(PyUnicode_AsUTF8(src.ptr()));
                return true;
            }
            return false;
        }
        static handle cast(QString src, return_value_policy /* policy */, handle /* parent */) {
            return PyUnicode_FromString(src.toUtf8().constData());
        }
    };

    template <> struct type_caster<QJsonObject> {
    public:
        PYBIND11_TYPE_CASTER(QJsonObject, _("dict"));
        bool load(handle src, bool) {
            if (!src) return false;
            py::module_ json = py::module_::import("json");
            py::object dumps = json.attr("dumps");
            std::string s = dumps(src).cast<std::string>();
            value = QJsonDocument::fromJson(QByteArray::fromStdString(s)).object();
            return true;
        }
        static handle cast(QJsonObject src, return_value_policy /* policy */, handle /* parent */) {
            QJsonDocument doc(src);
            std::string s = doc.toJson(QJsonDocument::Compact).toStdString();
            py::module_ json = py::module_::import("json");
            py::object loads = json.attr("loads");
            return loads(s).release();
        }
    };
}}

PYBIND11_EMBEDDED_MODULE(flux, m) {
    m.doc() = "FluxScript Core API";

    // Bind QPointF
    py::class_<QPointF>(m, "Point")
        .def(py::init<double, double>())
        .def_property("x", &QPointF::x, &QPointF::setX)
        .def_property("y", &QPointF::y, &QPointF::setY)
        .def("__repr__", [](const QPointF &p) {
            return "Point(" + std::to_string(p.x()) + ", " + std::to_string(p.y()) + ")";
        });

    // Add math helpers directly to flux module if needed
    m.def("info", [](const QString& msg) {
        printf("FluxScript Info: %s\n", msg.toUtf8().constData());
    });
}
