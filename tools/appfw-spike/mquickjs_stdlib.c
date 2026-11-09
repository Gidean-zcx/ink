/* Reuse upstream class definitions, exposing only pure builtins and page calls. */
#define main ink_upstream_stdlib_main
#include "mqjs_stdlib.c"
#undef main

static const JSPropDef ink_globals[] = {
    JS_PROP_CLASS_DEF("Object", &js_object_class),
    JS_PROP_CLASS_DEF("Function", &js_function_class),
    JS_PROP_CLASS_DEF("Number", &js_number_class),
    JS_PROP_CLASS_DEF("Boolean", &js_boolean_class),
    JS_PROP_CLASS_DEF("String", &js_string_class),
    JS_PROP_CLASS_DEF("Array", &js_array_class),
    JS_PROP_CLASS_DEF("Math", &js_math_obj),
    JS_PROP_CLASS_DEF("JSON", &js_json_obj),
    JS_PROP_CLASS_DEF("RegExp", &js_regexp_class),
    JS_PROP_CLASS_DEF("Error", &js_error_class),
    JS_PROP_CLASS_DEF("EvalError", &js_eval_error_class),
    JS_PROP_CLASS_DEF("RangeError", &js_range_error_class),
    JS_PROP_CLASS_DEF("ReferenceError", &js_reference_error_class),
    JS_PROP_CLASS_DEF("SyntaxError", &js_syntax_error_class),
    JS_PROP_CLASS_DEF("TypeError", &js_type_error_class),
    JS_PROP_CLASS_DEF("URIError", &js_uri_error_class),
    JS_PROP_CLASS_DEF("InternalError", &js_internal_error_class),
    JS_PROP_CLASS_DEF("ArrayBuffer", &js_array_buffer_class),
    JS_PROP_CLASS_DEF("Uint8ClampedArray", &js_Uint8ClampedArray_class),
    JS_PROP_CLASS_DEF("Int8Array", &js_Int8Array_class),
    JS_PROP_CLASS_DEF("Uint8Array", &js_Uint8Array_class),
    JS_PROP_CLASS_DEF("Int16Array", &js_Int16Array_class),
    JS_PROP_CLASS_DEF("Uint16Array", &js_Uint16Array_class),
    JS_PROP_CLASS_DEF("Int32Array", &js_Int32Array_class),
    JS_PROP_CLASS_DEF("Uint32Array", &js_Uint32Array_class),
    JS_PROP_CLASS_DEF("Float32Array", &js_Float32Array_class),
    JS_PROP_CLASS_DEF("Float64Array", &js_Float64Array_class),
    JS_PROP_UNDEFINED_DEF("undefined", 0),
    JS_CFUNC_DEF("title", 1, ink_js_title),
    JS_CFUNC_DEF("toggle", 2, ink_js_toggle),
    JS_CFUNC_DEF("textfield", 2, ink_js_field),
    JS_CFUNC_DEF("button", 1, ink_js_button),
    JS_CFUNC_DEF("set", 2, ink_js_set),
    JS_PROP_END,
};

int main(int argc, char **argv) {
    return build_atoms("js_stdlib", ink_globals, js_c_function_decl, argc, argv);
}
