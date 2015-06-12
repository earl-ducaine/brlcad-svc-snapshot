/*                         O P T . H
 * BRL-CAD
 *
 * Copyright (c) 2015 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
 */

#ifndef BU_OPT_H
#define BU_OPT_H

#include "common.h"
#include "bu/defines.h"
#include "bu/ptbl.h"
#include "bu/vls.h"

__BEGIN_DECLS

/** @addtogroup bu_opt
 * @brief
 * Generalized option handling.
 *
 * The usage pattern is to
 * build up a BU_OPT_DESC_NULL terminated array of option descriptions, which are
 * used by bu_opt_parse to process an argv array.
 *
 * The set_var pointer points to a user selected variable.  The type of the variable
 * needed is determined by the arg_process callback.  If no callback is present and
 * the max arg count is zero, set_var is expected to be an integer that will be set
 * to 1 if the option is present in the argv string.

 *
 * @code
 * #define help_str "Print help and exit"
 * static int ph = 0;
 * static int i = 0;
 * static fastf_t f = 0.0;
 * struct bu_opt_desc opt_defs[] = {
 *     {"h", "help",    NULL,            (void *)&ph , "", help_str},
 *     {"n", "num",     &bu_opt_int,     (void *)&i,   "#", "Read int"},
 *     {"f", "fastf_t", &bu_opt_fastf_t, (void *)&f,   "#", "Read float"},
 *     BU_OPT_DESC_NULL
 * };
 * @endcode
 *
 * If an array initialization of an option description array is provided as in
 * the above example , the variables specified by the user for setting must all
 * be static (as long as C89 is used.) This should work for executable
 * argc/argv parsing, but for libraries it is not advisable due to static
 * variables rendering the function in question thread unsafe.  For an approach
 * usable in a library see the BU_OPT macro documentation.
 *
 *
 * @code
 * #define help_str "Print help and exit"
 * int ph = 0;
 * int i = 0;
 * fastf_t f = 0.0;
 * struct bu_opt_desc opt_defs[4];
 * BU_OPT(opt_defs[0], "h", "help",     NULL,            (void *)&ph, "", help_str);
 * BU_OPT(opt_defs[1], "n", "num",      &bu_opt_ind,     (void *)&i,  "#", "Read int");
 * BU_OPT(opt_defs[2], "f", "fastf_t",  &bu_opt_fastf_t, (void *)&f,  "#", "Read float");
 * BU_OPT_NULL(opt_defs[3]);
 * @endcode
 *
 */
/** @{ */
/** @file bu/opt.h */

/**
 * Callback function signature for bu_opt_desc argument processing functions. Any
 * user defined argument processing function should match this signature and return
 * values as documented below.
 *
 * \returns
 * Val | Interpretation
 * --- | --------------
 * -1  | Invalid argument encountered, or argument expected but not found.
 *  0  | No argument processed (not an error.)
 * >0  | Number of argv elements used in valid argument processing.
 *
 */
typedef int (*bu_opt_arg_process_t)(struct bu_vls *, int argc, const char **argv, void *);

/**
 * @brief
 * "Option description" structure.
 *
 * Arrays of this structure are used to define command line options.
 *
 */
struct bu_opt_desc {
    const char *shortopt;             /**< @brief "Short" option (i.e. -h for help option) */
    const char *longopt;              /**< @brief "Long" option (i.e. --help for help option) */
    const char *arg_helpstr;          /**< @brief Documentation describing option argument, if any (i.e. "file" in --input file)*/
    bu_opt_arg_process_t arg_process; /**< @brief Argument processing function pointer */
    void *set_var;                    /**< @brief Pointer to the variable or structure that collects this option's results */
    const char *help_string;          /**< @brief Option description */
};

/** Convenience initializer for NULL bu_opt_desc array terminator */
#define BU_OPT_DESC_NULL {NULL, NULL, NULL, NULL, NULL, NULL}

/**
 * Macro for assigning values to bu_opt_desc array entries.  Use this style
 * when it isn't possible to use static variables as set_var entries
 * (such as libraries which need to be thread safe.)
 */
#define BU_OPT(_desc, _so, _lo, _ahelp, _aprocess, _var, _help) { \
    _desc.shortopt = _so; \
    _desc.longopt = _lo; \
    _desc.arg_helpstr = _ahelp; \
    _desc.arg_process = _aprocess; \
    _desc.set_var = _var; \
    _desc.help_string = _help; \
}

/* Convenience macro for setting a bu_opt_desc struct to BU_OPT_DESC_NULL */
#define BU_OPT_NULL(_desc) { \
    _desc.shortopt = NULL; \
    _desc.longopt = NULL; \
    _desc.arg_helpstr = NULL; \
    _desc.arg_process = NULL; \
    _desc.set_var = NULL; \
    _desc.help_string = NULL; \
}

/**
 * Parse argv array using option descs.
 *
 * The bu_opt_desc array ds must be null terminated with BU_OPT_DESC_NULL.
 *
 * \returns
 * Val | Interpretation
 * --- | --------------
 * -1  | fatal error in parsing.  Program must decide to recover or exit.
 *  0  | all argv options handled.
 *  >0 | number of unused argv entries returned at the begging of the argv array.
 *
 * msgs will collect any informational messages generated by the parser (typically
 * used for error reporting.)
 *
 * Note: An option definition without an arg_process callback that indicates
 * the option requires >0 arguments in its definition will be treated as a
 * fatal error, should that option be encountered while processing an argv
 * input.
 */
BU_EXPORT extern int bu_opt_parse(struct bu_vls *msgs, int ac, const char **argv, struct bu_opt_desc *ds);


/** @addtogroup bu_opt_arg_process
 *
 * Standard option validators - if a custom option argument
 * validation isn't needed, the functions below can be
 * used for most valid data types. When data conversion is successful,
 * the user_data pointer in bu_opt_data will point to the results
 * of the string->[type] translation in order to allow a calling
 * program to use the int/long/etc. without having to repeat the
 * conversion.
 */
/** @{ */
BU_EXPORT extern int bu_opt_bool(struct bu_vls *msg, int argc, const char **argv, void *set_var);
BU_EXPORT extern int bu_opt_int(struct bu_vls *msg, int argc, const char **argv, void *set_var);
BU_EXPORT extern int bu_opt_long(struct bu_vls *msg, int argc, const char **argv, void *set_var);
BU_EXPORT extern int bu_opt_fastf_t(struct bu_vls *msg, int argc, const char **argv, void *set_var);
BU_EXPORT extern int bu_opt_str(struct bu_vls *msg, int argc, const char **argv, void *set_var);
BU_EXPORT extern int bu_opt_vls(struct bu_vls *msg, int argc, const char **argv, void *set_var);
/** @} */


/** Output format options for bu_opt documentation generation */
typedef enum {
    BU_OPT_ASCII,
    BU_OPT_DOCBOOK /* TODO */
} bu_opt_format_t;

/**
 * Construct a textual description of the options defined by
 * the array.
 *
 * The structure is as follows:
 *
 * Offset    Options      Descriptions
 * ******--------------*********************
 *       --test-option This is a test option
 *
 * Opt_col specifies how wide the options column is, and desc_cols
 * specifies how wide the description column is.
 */

/* TODO - support actually using the struct... */
struct bu_opt_desc_opts {
    bu_opt_format_t format;
    int offset;
    int option_columns;
    int description_columns;
};

BU_EXPORT extern const char *bu_opt_describe(struct bu_opt_desc *ds, struct bu_opt_desc_opts *settings);



/** @} */

__END_DECLS

#endif  /* BU_OPT_H */

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
