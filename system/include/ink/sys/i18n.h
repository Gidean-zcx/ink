/*
 * Interface languages (R-069, D-116). Chinese is the source language: the code writes
 * TR("中文"), and the Chinese text is the key into one table per other language
 * (assets/i18n/<code>.tsv, generated into a const table at build time; see
 * tools/i18n/i18n_gen.py). A key a language does not have shows in Chinese and counts
 * "i18n.missing"; nothing ever shows blank or as a key name. Adding a language is one
 * file; pages do not change.
 *
 * The language is the system setting "system.language" (its choices are the languages'
 * own names), changed by hand and by the AI through settings.set; the shell redraws the
 * open page when it changes.
 */
#ifndef INK_SYS_I18N_H
#define INK_SYS_I18N_H

#ifdef __cplusplus
extern "C" {
#endif

/** `zh` in the current language: the translation, or `zh` itself. Any task. */
const char *ink_tr(const char *zh) __attribute__((format_arg(1)));
/** Code of the current language ("zh", "en"), as sent to the hub. */
const char *ink_lang_code(void);
/** The language setting's choice `index` in its own words ("中文", "English"); NULL past the last. */
const char *ink_lang_name(int index);
/** Define the language setting (after ink_settings_init). 0 or -1. */
int ink_i18n_init(void);

/* TR() translates now; TR_NOOP() only marks a key kept in a table and passed to TR() later. */
#define TR(s) ink_tr(s)
#define TR_NOOP(s) s

#ifdef __cplusplus
}
#endif
#endif
