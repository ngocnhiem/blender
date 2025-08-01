/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup pythonintern
 * This file defines '_bpy_msgbus' module, exposed as 'bpy.msgbus'.
 */

#include <Python.h>

#include "../generic/py_capi_rna.hh"
#include "../generic/py_capi_utils.hh"
#include "../generic/python_compat.hh" /* IWYU pragma: keep. */
#include "../generic/python_utildefines.hh"

#include "../mathutils/mathutils.hh"

#include "BKE_context.hh"

#include "WM_message.hh"

#include "RNA_access.hh"

#include "bpy_capi_utils.hh"
#include "bpy_rna.hh"

#include "bpy_msgbus.hh" /* own include */

/* -------------------------------------------------------------------- */
/** \name Internal Utils
 * \{ */

#define BPY_MSGBUS_RNA_MSGKEY_DOC \
  "   :arg key: Represents the type of data being subscribed to\n" \
  "\n" \
  "      Arguments include\n" \
  "      - A property instance.\n" \
  "      - A struct type.\n" \
  "      - A tuple representing a (struct, property name) pair.\n" \
  "   :type key: :class:`bpy.types.Property` | " \
  ":class:`bpy.types.Struct` | " \
  "tuple[:class:`bpy.types.Struct`, str]\n"

/**
 * There are multiple ways we can get RNA from Python,
 * it's also possible to register a type instead of an instance.
 *
 * This function handles converting Python to RNA subscription information.
 *
 * \param py_sub: See #BPY_MSGBUS_RNA_MSGKEY_DOC for description.
 * \param msg_key_params: Message key with all members zeroed out.
 * \return -1 on failure, 0 on success.
 */
static int py_msgbus_rna_key_from_py(PyObject *py_sub,
                                     wmMsgParams_RNA *msg_key_params,
                                     const char *error_prefix)
{

  /* Allow common case, object rotation, location - etc. */
  if (BaseMathObject_CheckExact(py_sub)) {
    BaseMathObject *py_sub_math = (BaseMathObject *)py_sub;
    if (py_sub_math->cb_user == nullptr) {
      PyErr_Format(PyExc_TypeError, "%s: math argument has no owner", error_prefix);
      return -1;
    }
    py_sub = py_sub_math->cb_user;
    /* Common case will use BPy_PropertyRNA_Check below. */
  }

  if (BPy_PropertyRNA_Check(py_sub)) {
    BPy_PropertyRNA *data_prop = (BPy_PropertyRNA *)py_sub;
    PYRNA_PROP_CHECK_INT(data_prop);
    msg_key_params->ptr = *data_prop->ptr;
    msg_key_params->prop = data_prop->prop;
  }
  else if (BPy_StructRNA_Check(py_sub)) {
    /* NOTE: this isn't typically used since we don't edit structs directly. */
    BPy_StructRNA *data_srna = (BPy_StructRNA *)py_sub;
    PYRNA_STRUCT_CHECK_INT(data_srna);
    msg_key_params->ptr = *data_srna->ptr;
  }
  /* TODO: property / type, not instance. */
  else if (PyType_Check(py_sub)) {
    StructRNA *data_type = pyrna_struct_as_srna(py_sub, false, error_prefix);
    if (data_type == nullptr) {
      return -1;
    }
    msg_key_params->ptr.type = data_type;
  }
  else if (PyTuple_CheckExact(py_sub)) {
    if (PyTuple_GET_SIZE(py_sub) == 2) {
      PyObject *data_type_py = PyTuple_GET_ITEM(py_sub, 0);
      PyObject *data_prop_py = PyTuple_GET_ITEM(py_sub, 1);
      StructRNA *data_type = pyrna_struct_as_srna(data_type_py, false, error_prefix);
      if (data_type == nullptr) {
        return -1;
      }
      if (!PyUnicode_CheckExact(data_prop_py)) {
        PyErr_Format(PyExc_TypeError, "%s: expected property to be a string", error_prefix);
        return -1;
      }
      PointerRNA data_type_ptr{};
      data_type_ptr.type = data_type;

      const char *data_prop_str = PyUnicode_AsUTF8(data_prop_py);
      PropertyRNA *data_prop = RNA_struct_find_property(&data_type_ptr, data_prop_str);

      if (data_prop == nullptr) {
        PyErr_Format(PyExc_TypeError,
                     "%s: struct %.200s does not contain property %.200s",
                     error_prefix,
                     RNA_struct_identifier(data_type),
                     data_prop_str);
        return -1;
      }

      msg_key_params->ptr.type = data_type;
      msg_key_params->prop = data_prop;
    }
    else {
      PyErr_Format(PyExc_ValueError, "%s: Expected a pair (type, property_id)", error_prefix);
      return -1;
    }
  }
  return 0;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Internal Callbacks
 * \{ */

#define BPY_MSGBUS_USER_DATA_LEN 2

/* Follow wmMsgNotifyFn spec */
static void bpy_msgbus_notify(bContext *C,
                              wmMsgSubscribeKey * /*msg_key*/,
                              wmMsgSubscribeValue *msg_val)
{
  PyGILState_STATE gilstate;
  bpy_context_set(C, &gilstate);
  const bool is_write_ok = pyrna_write_check();
  if (!is_write_ok) {
    pyrna_write_set(true);
  }

  PyObject *user_data = static_cast<PyObject *>(msg_val->user_data);
  BLI_assert(PyTuple_GET_SIZE(user_data) == BPY_MSGBUS_USER_DATA_LEN);

  PyObject *callback_args = PyTuple_GET_ITEM(user_data, 0);
  PyObject *callback_notify = PyTuple_GET_ITEM(user_data, 1);

  PyObject *ret = PyObject_CallObject(callback_notify, callback_args);

  if (ret == nullptr) {
    PyC_Err_PrintWithFunc(callback_notify);
  }
  else {
    if (ret != Py_None) {
      PyErr_SetString(PyExc_ValueError, "the return value must be None");
      PyC_Err_PrintWithFunc(callback_notify);
    }
    Py_DECREF(ret);
  }

  if (!is_write_ok) {
    pyrna_write_set(false);
  }
  bpy_context_clear(C, &gilstate);
}

/* Follow wmMsgSubscribeValueFreeDataFn spec */
static void bpy_msgbus_subscribe_value_free_data(wmMsgSubscribeKey * /*msg_key*/,
                                                 wmMsgSubscribeValue *msg_val)
{
  const PyGILState_STATE gilstate = PyGILState_Ensure();
  Py_DECREF(msg_val->owner);
  Py_DECREF(msg_val->user_data);
  PyGILState_Release(gilstate);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Public Message Bus API
 * \{ */

PyDoc_STRVAR(
    /* Wrap. */
    bpy_msgbus_subscribe_rna_doc,
    ".. function:: subscribe_rna(key, owner, args, notify, options=set())\n"
    "\n"
    "   Register a message bus subscription. It will be cleared when another blend file is\n"
    "   loaded, or can be cleared explicitly via :func:`bpy.msgbus.clear_by_owner`.\n"
    "\n" BPY_MSGBUS_RNA_MSGKEY_DOC
    "   :arg owner: Handle for this subscription (compared by identity).\n"
    "   :type owner: Any\n"
    "   :arg options: Change the behavior of the subscriber.\n"
    "\n"
    "      - ``PERSISTENT`` when set, the subscriber will be kept when remapping ID data.\n"
    "\n"
    "   :type options: set[str]\n"
    "\n"
    ".. note::\n"
    "\n"
    "   All subscribers will be cleared on file-load. Subscribers can be re-registered on load,\n"
    "   see :mod:`bpy.app.handlers.load_post`.\n");
static PyObject *bpy_msgbus_subscribe_rna(PyObject * /*self*/, PyObject *args, PyObject *kw)
{
  const char *error_prefix = "subscribe_rna";
  PyObject *py_sub = nullptr;
  PyObject *py_owner = nullptr;
  PyObject *callback_args = nullptr;
  PyObject *callback_notify = nullptr;

  enum {
    IS_PERSISTENT = (1 << 0),
  };
  PyObject *py_options = nullptr;
  const EnumPropertyItem py_options_enum[] = {
      {IS_PERSISTENT, "PERSISTENT", 0, ""},
      {0, nullptr, 0, nullptr, nullptr},
  };
  int options = 0;

  if (PyTuple_GET_SIZE(args) != 0) {
    PyErr_Format(PyExc_TypeError, "%s: only keyword arguments are supported", error_prefix);
    return nullptr;
  }
  static const char *_keywords[] = {
      "key",
      "owner",
      "args",
      "notify",
      "options",
      nullptr,
  };
  static _PyArg_Parser _parser = {
      PY_ARG_PARSER_HEAD_COMPAT()
      "O"  /* `key` */
      "O"  /* `owner` */
      "O!" /* `args` */
      "O"  /* `notify` */
      "|$" /* Optional keyword only arguments. */
      "O!" /* `options` */
      ":subscribe_rna",
      _keywords,
      nullptr,
  };
  if (!_PyArg_ParseTupleAndKeywordsFast(args,
                                        kw,
                                        &_parser,
                                        &py_sub,
                                        &py_owner,
                                        &PyTuple_Type,
                                        &callback_args,
                                        &callback_notify,
                                        &PySet_Type,
                                        &py_options))
  {
    return nullptr;
  }

  if (py_options &&
      (pyrna_enum_bitfield_from_set(py_options_enum, py_options, &options, error_prefix) == -1))
  {
    return nullptr;
  }

  /* NOTE: we may want to have a way to pass this in. */
  bContext *C = BPY_context_get();
  wmMsgBus *mbus = CTX_wm_message_bus(C);
  wmMsgParams_RNA msg_key_params = {{}};

  wmMsgSubscribeValue msg_val_params = {nullptr};

  if (py_msgbus_rna_key_from_py(py_sub, &msg_key_params, error_prefix) == -1) {
    return nullptr;
  }

  if (!PyFunction_Check(callback_notify)) {
    PyErr_Format(PyExc_TypeError,
                 "notify expects a function, found %.200s",
                 Py_TYPE(callback_notify)->tp_name);
    return nullptr;
  }

  if (options != 0) {
    if (options & IS_PERSISTENT) {
      msg_val_params.is_persistent = true;
    }
  }

  /* owner can be anything. */
  {
    msg_val_params.owner = py_owner;
    Py_INCREF(py_owner);
  }

  {
    PyObject *user_data = PyTuple_New(2);
    PyTuple_SET_ITEMS(user_data, Py_NewRef(callback_args), Py_NewRef(callback_notify));
    msg_val_params.user_data = user_data;
  }

  msg_val_params.notify = bpy_msgbus_notify;
  msg_val_params.free_data = bpy_msgbus_subscribe_value_free_data;

  WM_msg_subscribe_rna_params(mbus, &msg_key_params, &msg_val_params, __func__);

  if (false) { /* For debugging. */
    WM_msg_dump(mbus, __func__);
  }

  Py_RETURN_NONE;
}

PyDoc_STRVAR(
    /* Wrap. */
    bpy_msgbus_publish_rna_doc,
    ".. function:: publish_rna(key)\n"
    "\n" BPY_MSGBUS_RNA_MSGKEY_DOC
    "\n"
    "   Notify subscribers of changes to this property\n"
    "   (this typically doesn't need to be called explicitly since changes will automatically "
    "publish updates).\n"
    "   In some cases it may be useful to publish changes explicitly using more general keys.\n");
static PyObject *bpy_msgbus_publish_rna(PyObject * /*self*/, PyObject *args, PyObject *kw)
{
  const char *error_prefix = "publish_rna";
  PyObject *py_sub = nullptr;

  if (PyTuple_GET_SIZE(args) != 0) {
    PyErr_Format(PyExc_TypeError, "%s: only keyword arguments are supported", error_prefix);
    return nullptr;
  }
  static const char *_keywords[] = {
      "key",
      nullptr,
  };
  static _PyArg_Parser _parser = {
      PY_ARG_PARSER_HEAD_COMPAT()
      "O" /* `key` */
      ":publish_rna",
      _keywords,
      nullptr,
  };
  if (!_PyArg_ParseTupleAndKeywordsFast(args, kw, &_parser, &py_sub)) {
    return nullptr;
  }

  /* NOTE: we may want to have a way to pass this in. */
  bContext *C = BPY_context_get();
  wmMsgBus *mbus = CTX_wm_message_bus(C);
  wmMsgParams_RNA msg_key_params = {{}};

  if (py_msgbus_rna_key_from_py(py_sub, &msg_key_params, error_prefix) == -1) {
    return nullptr;
  }

  WM_msg_publish_rna_params(mbus, &msg_key_params);

  Py_RETURN_NONE;
}

PyDoc_STRVAR(
    /* Wrap. */
    bpy_msgbus_clear_by_owner_doc,
    ".. function:: clear_by_owner(owner)\n"
    "\n"
    "   Clear all subscribers using this owner.\n");
static PyObject *bpy_msgbus_clear_by_owner(PyObject * /*self*/, PyObject *py_owner)
{
  bContext *C = BPY_context_get();
  wmMsgBus *mbus = CTX_wm_message_bus(C);
  WM_msgbus_clear_by_owner(mbus, py_owner);
  Py_RETURN_NONE;
}

#ifdef __GNUC__
#  ifdef __clang__
#    pragma clang diagnostic push
#    pragma clang diagnostic ignored "-Wcast-function-type"
#  else
#    pragma GCC diagnostic push
#    pragma GCC diagnostic ignored "-Wcast-function-type"
#  endif
#endif

static PyMethodDef BPy_msgbus_methods[] = {
    {"subscribe_rna",
     (PyCFunction)bpy_msgbus_subscribe_rna,
     METH_VARARGS | METH_KEYWORDS,
     bpy_msgbus_subscribe_rna_doc},
    {"publish_rna",
     (PyCFunction)bpy_msgbus_publish_rna,
     METH_VARARGS | METH_KEYWORDS,
     bpy_msgbus_publish_rna_doc},
    {"clear_by_owner",
     (PyCFunction)bpy_msgbus_clear_by_owner,
     METH_O,
     bpy_msgbus_clear_by_owner_doc},
    {nullptr, nullptr, 0, nullptr},
};

#ifdef __GNUC__
#  ifdef __clang__
#    pragma clang diagnostic pop
#  else
#    pragma GCC diagnostic pop
#  endif
#endif

static PyModuleDef _bpy_msgbus_def = {
    /*m_base*/ PyModuleDef_HEAD_INIT,
    /*m_name*/ "msgbus",
    /*m_doc*/ nullptr,
    /*m_size*/ 0,
    /*m_methods*/ BPy_msgbus_methods,
    /*m_slots*/ nullptr,
    /*m_traverse*/ nullptr,
    /*m_clear*/ nullptr,
    /*m_free*/ nullptr,
};

PyObject *BPY_msgbus_module()
{
  PyObject *submodule;

  submodule = PyModule_Create(&_bpy_msgbus_def);

  return submodule;
}

/** \} */
