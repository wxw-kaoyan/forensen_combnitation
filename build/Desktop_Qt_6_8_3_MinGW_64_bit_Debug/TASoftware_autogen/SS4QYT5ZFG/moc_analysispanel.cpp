/****************************************************************************
** Meta object code from reading C++ file 'analysispanel.h'
**
** Created by: The Qt Meta Object Compiler version 68 (Qt 6.8.3)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include "../../../../modules/analysis/compare/analysispanel.h"
#include <QtCore/qmetatype.h>

#include <QtCore/qtmochelpers.h>

#include <memory>


#include <QtCore/qxptype_traits.h>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'analysispanel.h' doesn't include <QObject>."
#elif Q_MOC_OUTPUT_REVISION != 68
#error "This file was generated using the moc from 6.8.3. It"
#error "cannot be used with the include files from this version of Qt."
#error "(The moc has changed too much.)"
#endif

#ifndef Q_CONSTINIT
#define Q_CONSTINIT
#endif

QT_WARNING_PUSH
QT_WARNING_DISABLE_DEPRECATED
QT_WARNING_DISABLE_GCC("-Wuseless-cast")
namespace {
struct qt_meta_tag_ZN8Analysis13AnalysisPanelE_t {};
} // unnamed namespace


#ifdef QT_MOC_HAS_STRINGDATA
static constexpr auto qt_meta_stringdata_ZN8Analysis13AnalysisPanelE = QtMocHelpers::stringData(
    "Analysis::AnalysisPanel",
    "onBrowseTruth",
    "",
    "onTruthFormatChanged",
    "idx",
    "onTruthWeekChanged",
    "v",
    "onAddCmpFile",
    "onRemoveCmpFile",
    "onCmpFileSelected",
    "row",
    "onCmpTimeColChanged",
    "onCmpDayColChanged",
    "onCmpFormatChanged",
    "onCmpWeekChanged",
    "onAddMetric",
    "onRemoveMetric",
    "onAddScene",
    "onRemoveScene",
    "onStartCompare",
    "onCompareFinished",
    "onResultRowDoubleClicked",
    "column"
);
#else  // !QT_MOC_HAS_STRINGDATA
#error "qtmochelpers.h not found or too old."
#endif // !QT_MOC_HAS_STRINGDATA

Q_CONSTINIT static const uint qt_meta_data_ZN8Analysis13AnalysisPanelE[] = {

 // content:
      12,       // revision
       0,       // classname
       0,    0, // classinfo
      17,   14, // methods
       0,    0, // properties
       0,    0, // enums/sets
       0,    0, // constructors
       0,       // flags
       0,       // signalCount

 // slots: name, argc, parameters, tag, flags, initial metatype offsets
       1,    0,  116,    2, 0x08,    1 /* Private */,
       3,    1,  117,    2, 0x08,    2 /* Private */,
       5,    1,  120,    2, 0x08,    4 /* Private */,
       7,    0,  123,    2, 0x08,    6 /* Private */,
       8,    0,  124,    2, 0x08,    7 /* Private */,
       9,    1,  125,    2, 0x08,    8 /* Private */,
      11,    1,  128,    2, 0x08,   10 /* Private */,
      12,    1,  131,    2, 0x08,   12 /* Private */,
      13,    1,  134,    2, 0x08,   14 /* Private */,
      14,    1,  137,    2, 0x08,   16 /* Private */,
      15,    0,  140,    2, 0x08,   18 /* Private */,
      16,    0,  141,    2, 0x08,   19 /* Private */,
      17,    0,  142,    2, 0x08,   20 /* Private */,
      18,    0,  143,    2, 0x08,   21 /* Private */,
      19,    0,  144,    2, 0x08,   22 /* Private */,
      20,    0,  145,    2, 0x08,   23 /* Private */,
      21,    2,  146,    2, 0x08,   24 /* Private */,

 // slots: parameters
    QMetaType::Void,
    QMetaType::Void, QMetaType::Int,    4,
    QMetaType::Void, QMetaType::Double,    6,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void, QMetaType::Int,   10,
    QMetaType::Void, QMetaType::Int,    4,
    QMetaType::Void, QMetaType::Int,    4,
    QMetaType::Void, QMetaType::Int,    4,
    QMetaType::Void, QMetaType::Double,    6,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void, QMetaType::Int, QMetaType::Int,   10,   22,

       0        // eod
};

Q_CONSTINIT const QMetaObject Analysis::AnalysisPanel::staticMetaObject = { {
    QMetaObject::SuperData::link<QWidget::staticMetaObject>(),
    qt_meta_stringdata_ZN8Analysis13AnalysisPanelE.offsetsAndSizes,
    qt_meta_data_ZN8Analysis13AnalysisPanelE,
    qt_static_metacall,
    nullptr,
    qt_incomplete_metaTypeArray<qt_meta_tag_ZN8Analysis13AnalysisPanelE_t,
        // Q_OBJECT / Q_GADGET
        QtPrivate::TypeAndForceComplete<AnalysisPanel, std::true_type>,
        // method 'onBrowseTruth'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'onTruthFormatChanged'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<int, std::false_type>,
        // method 'onTruthWeekChanged'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<double, std::false_type>,
        // method 'onAddCmpFile'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'onRemoveCmpFile'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'onCmpFileSelected'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<int, std::false_type>,
        // method 'onCmpTimeColChanged'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<int, std::false_type>,
        // method 'onCmpDayColChanged'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<int, std::false_type>,
        // method 'onCmpFormatChanged'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<int, std::false_type>,
        // method 'onCmpWeekChanged'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<double, std::false_type>,
        // method 'onAddMetric'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'onRemoveMetric'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'onAddScene'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'onRemoveScene'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'onStartCompare'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'onCompareFinished'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'onResultRowDoubleClicked'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<int, std::false_type>,
        QtPrivate::TypeAndForceComplete<int, std::false_type>
    >,
    nullptr
} };

void Analysis::AnalysisPanel::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    auto *_t = static_cast<AnalysisPanel *>(_o);
    if (_c == QMetaObject::InvokeMetaMethod) {
        switch (_id) {
        case 0: _t->onBrowseTruth(); break;
        case 1: _t->onTruthFormatChanged((*reinterpret_cast< std::add_pointer_t<int>>(_a[1]))); break;
        case 2: _t->onTruthWeekChanged((*reinterpret_cast< std::add_pointer_t<double>>(_a[1]))); break;
        case 3: _t->onAddCmpFile(); break;
        case 4: _t->onRemoveCmpFile(); break;
        case 5: _t->onCmpFileSelected((*reinterpret_cast< std::add_pointer_t<int>>(_a[1]))); break;
        case 6: _t->onCmpTimeColChanged((*reinterpret_cast< std::add_pointer_t<int>>(_a[1]))); break;
        case 7: _t->onCmpDayColChanged((*reinterpret_cast< std::add_pointer_t<int>>(_a[1]))); break;
        case 8: _t->onCmpFormatChanged((*reinterpret_cast< std::add_pointer_t<int>>(_a[1]))); break;
        case 9: _t->onCmpWeekChanged((*reinterpret_cast< std::add_pointer_t<double>>(_a[1]))); break;
        case 10: _t->onAddMetric(); break;
        case 11: _t->onRemoveMetric(); break;
        case 12: _t->onAddScene(); break;
        case 13: _t->onRemoveScene(); break;
        case 14: _t->onStartCompare(); break;
        case 15: _t->onCompareFinished(); break;
        case 16: _t->onResultRowDoubleClicked((*reinterpret_cast< std::add_pointer_t<int>>(_a[1])),(*reinterpret_cast< std::add_pointer_t<int>>(_a[2]))); break;
        default: ;
        }
    }
}

const QMetaObject *Analysis::AnalysisPanel::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *Analysis::AnalysisPanel::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_meta_stringdata_ZN8Analysis13AnalysisPanelE.stringdata0))
        return static_cast<void*>(this);
    return QWidget::qt_metacast(_clname);
}

int Analysis::AnalysisPanel::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QWidget::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        if (_id < 17)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 17;
    }
    if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        if (_id < 17)
            *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType();
        _id -= 17;
    }
    return _id;
}
QT_WARNING_POP
